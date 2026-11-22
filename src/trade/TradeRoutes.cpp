#include <Spore/ModAPI.h>
#include <Spore/Simulator/SubSystem/PlanetModel.h>
#include <Spore/Simulator/cCityWalls.h>
#include <Spore/Simulator/cCommodityNode.h>
#include <Spore/Simulator/cCulturalTarget.h>
#include <Spore/Simulator/cTurret.h>
#include <Spore/Terrain/ITerrain.h>
// The pinned SDK's noun-container template refers to intrusive_ptr unqualified.
namespace Simulator { using eastl::intrusive_ptr; }
#include <Spore/Simulator/SubSystem/GameNounManager.h>
#include "TradeRoutes.h"
#include "engine/NativeExtensions.h"
#include "engine/ReferenceCode.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace BetterCiv::TradeRoutes {

using City = Simulator::cCity;
constexpr uintptr_t kReferenceBase = 0x400000;
static_assert(sizeof(Simulator::cPlanetModel::cLandTradeRoute) == 0x1c);
static_assert(offsetof(Simulator::cPlanetModel, mLandTradeRoutes) == 0xd4);
static_assert(offsetof(Simulator::cPlanetModel, mLandTradeRoutesDirty) == 0xea);

static uint64_t CityKey(const City& city)
{
    return (uint64_t(city.mID) << 32) | reinterpret_cast<uintptr_t>(&city);
}

struct NativeRange {
    uint32_t address, size, expected_hash;
};

template<size_t N>
static bool ValidateRanges(const unsigned char* base, uint32_t relocation, const NativeRange (&ranges)[N])
{
    if (!base) return false;
    for (const auto& range : ranges) {
        uint32_t hash;
        const auto* code = base + range.address - kReferenceBase;
        if (!Native::HashReferenceCode(code, range.size, relocation, Native::CodeRelocations(range.address), hash) ||
            hash != range.expected_hash) return false;
    }
    return true;
}

bool ValidateNative(const unsigned char* base, uint32_t relocation)
{
    static constexpr NativeRange ranges[] = {
        {0xb87410,    48, 0xa763fd81},
        {0xb87cea,    10, 0xe8215410},
        {0xac68a0,   171, 0x0ff6a7f4}, // Path search
        {0xac6510,   912, 0xe95ead5c}, // Path request
        {0xac53c0,    10, 0x0aabdfd2}, // Path completion
        {0xb3d390,     6, 0x32e5eb8f}, // Path manager
        {0x1565b54,   20, 0x83747623}, // Land profile
        {0xc414c0,  1943, 0xe2ee0198}, // Road path layout
        {0xb8d2e0,   263, 0x2ed294c4}, // Ribbon cache layout
        {0xb8cfd4,   767, 0x8f586850}, // Ribbon consumer
    };
    return ValidateRanges(base, relocation, ranges);
}

bool ValidateGroundPaint(const unsigned char* base, uint32_t relocation)
{
    static constexpr NativeRange ranges[] = {
        {0xb8d570, 816, 0x035815a5}, // Publish arrows and ground road
        {0xbd89b0,  37, 0x25a0a89d}, // Land partner stores the road identity in its second word
        {0xfacea0,  40, 0x9312cee2}, // Take ownership of terrain stamps
        {0xf98890,  89, 0x78f37836}, // Remove by road identity
        {0xfaced0, 156, 0x60854742}, // Release stamps and invalidate terrain
        {0xf988f0,  16, 0xaf743cfd}, // Read a slot
        {0xf98900,  16, 0x5eefdd73}, // Count slots, including holes
    };
    return ValidateRanges(base, relocation, ranges);
}

bool Finite(const Point& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

float Length(const Point& point)
{
    return std::sqrt(point.x * point.x + point.y * point.y + point.z * point.z);
}

float Distance(const Point& a, const Point& b)
{
    return Length(a - b);
}

uint64_t HashBytes(uint64_t hash, const void* data, size_t size)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * 1099511628211ull;
    return hash;
}

uint64_t HashPath(const Path& path)
{
    return HashBytes(14695981039346656037ull, path.data(), path.size() * sizeof(Point));
}

// Sample the display line every four units so a long segment cannot skip a lake or cliff.
bool GroundPath(const Path& input, float sea, const SurfaceHeight& height, Path& output,
    const char*& reason, float* surface_error)
{
    output.clear();
    reason = "invalid-path";
    if (surface_error) *surface_error = 0;
    if (input.size() < 2 || input.size() > kMaxPoints || !std::isfinite(sea) || sea < 100) return false;

    for (size_t i = 0; i < input.size(); ++i) {
        const auto& from = i ? input[i - 1] : input[i];
        const auto& to = input[i];
        if (!Finite(from) || !Finite(to) || Length(from) < 100 || Length(to) < 100) return false;
        const float length = Distance(from, to);
        if (!std::isfinite(length) || length > 10000) return false;

        const unsigned steps = i ? unsigned(std::ceil(length / 4.0f)) : 1u;
        if (steps > kMaxPoints || output.size() + steps > kMaxPoints) {
            reason = "path-limit";
            return false;
        }
        for (unsigned step = 1; step <= steps; ++step) {
            const auto raw = from + (to - from) * (float(step) / float(steps));
            const float radius = Length(raw);
            if (!std::isfinite(radius) || radius < 100) return false;
            const auto direction = raw * (1.0f / radius);
            const float ground = height(direction);
            if (!std::isfinite(ground) || ground < 100) {
                reason = "invalid-terrain";
                return false;
            }
            if (ground <= sea + 0.15f) {
                reason = "water-crossing";
                return false;
            }
            if (surface_error) *surface_error = std::max(*surface_error, std::fabs(radius - ground));

            const auto projected = direction * (ground + 0.35f);
            if (!output.empty()) {
                const auto& previous = output.back();
                const float radial = std::fabs(Length(previous) - Length(projected));
                const float chord = Distance(previous, projected);
                const float tangent = std::sqrt(std::max(0.0f, chord * chord - radial * radial));
                if (radial > 2.0f && radial > 1.3f * tangent) {
                    reason = "steep-crossing";
                    return false;
                }
            }
            output.push_back(projected);
        }
    }
    if (output.size() < 2) return false;
    reason = "ground-path";
    return true;
}

static bool NearEdge(const Point& point, const Ends& city)
{
    const float distance = Distance(point, city.center);
    return std::isfinite(distance) && distance >= std::max(5.0f, city.inner * 0.5f) && distance <= city.outer + 35.0f;
}

bool HasCityEnds(const Path& path, const Ends& a, const Ends& b)
{
    if (path.size() < 2) return false;
    return (NearEdge(path.front(), a) && NearEdge(path.back(), b)) ||
        (NearEdge(path.front(), b) && NearEdge(path.back(), a));
}

bool Crosses(const Path& path, const Exclusion& zone)
{
    for (size_t i = 1; i < path.size(); ++i) {
        const auto& from = path[i - 1];
        const auto delta = path[i] - from;
        const auto to_center = zone.center - from;
        const float squared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        const float dot = to_center.x * delta.x + to_center.y * delta.y + to_center.z * delta.z;
        const float along = squared > 0 ? std::clamp(dot / squared, 0.0f, 1.0f) : 0;
        if (Distance(from + delta * along, zone.center) < zone.radius) return true;
    }
    return false;
}

// Try a local bend on each side of the node. Both gates and the rest of the route stay in place.
bool AvoidSpice(Path& path, const Exclusions& nodes, const Ends& a, const Ends& b,
    float sea, const SurfaceHeight& height)
{
    if (path.size() < 3 || path.size() > kMaxPoints || nodes.size() > 64) return false;
    bool changed = false;
    for (const auto& node : nodes) {
        if (!Finite(node.center) || !std::isfinite(node.radius) || node.radius <= 0 || node.radius > 40 ||
            !Crosses(path, node)) continue;

        size_t first = path.size(), last = 0;
        const float influence = node.radius * 2 + 10;
        for (size_t i = 0; i < path.size(); ++i) {
            if (Distance(path[i], node.center) < influence) {
                first = std::min(first, i);
                last = i;
            }
        }
        if (first >= last || first == 0 || last + 1 >= path.size()) continue;

        const auto tangent = path[last + 1] - path[first - 1];
        Point side{
            node.center.y * tangent.z - node.center.z * tangent.y,
            node.center.z * tangent.x - node.center.x * tangent.z,
            node.center.x * tangent.y - node.center.y * tangent.x
        };
        const float length = Length(side);
        if (!std::isfinite(length) || length < 0.01f) continue;
        side = side * (1 / length);
        for (float sign : {1.0f, -1.0f}) {
            Path bend, ground;
            bend.push_back(path[first - 1]);
            for (size_t i = first; i <= last; ++i) {
                const float weight = std::max(0.0f, 1 - Distance(path[i], node.center) / influence);
                bend.push_back(path[i] + side * (sign * (node.radius * 2 + 4) * weight));
            }
            bend.push_back(path[last + 1]);
            const char* reason = nullptr;
            if (!GroundPath(bend, sea, height, ground, reason) || Crosses(ground, node) ||
                Crosses(ground, {a.center, a.inner * 0.8f}) || Crosses(ground, {b.center, b.inner * 0.8f}) ||
                path.size() - (last - first + 3) + ground.size() > kMaxPoints) continue;

            const auto conflicts = [&](const Exclusion& other) { return &other != &node && Crosses(ground, other); };
            if (std::any_of(nodes.begin(), nodes.end(), conflicts)) continue;

            Path result;
            result.insert(result.end(), path.begin(), path.begin() + first - 1);
            result.insert(result.end(), ground.begin(), ground.end());
            result.insert(result.end(), path.begin() + last + 2, path.end());
            path.swap(result);
            changed = true;
            break;
        }
    }
    return changed;
}

void State::ResetSession()
{
    *this = {};
}

Record& State::Get(uint64_t a, uint64_t b, uint64_t now)
{
    Record* oldest = &records[0];
    for (auto& record : records) {
        if (record.city1 == a && record.city2 == b) {
            record.seen = now;
            return record;
        }
        if (record.seen < oldest->seen) oldest = &record;
    }
    *oldest = {};
    oldest->city1 = a;
    oldest->city2 = b;
    oldest->seen = now;
    return *oldest;
}

bool GroundMethods(const Terrain::ITerrain& terrain, uintptr_t base)
{
    const auto* table = *reinterpret_cast<const uintptr_t* const*>(&terrain);
    return table &&
        table[0xb8 / 4] == base + 0xfacea0 - kReferenceBase &&
        table[0xbc / 4] == base + 0xf98890 - kReferenceBase &&
        table[0xc0 / 4] == base + 0xfaced0 - kReferenceBase &&
        table[0xc4 / 4] == base + 0xf988f0 - kReferenceBase &&
        table[0xc8 / 4] == base + 0xf98900 - kReferenceBase;
}

// The native publisher bisects long chords, then projects the samples to the
// radius-500 sphere used by terrain paint. Arrows themselves use terrain height.
static bool AppendGroundLeg(const Point& from, const Point& to, Path& positions, unsigned depth = 0)
{
    if (positions.size() >= kMaxPoints) return false;
    const auto delta = to - from;
    const float squared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (!std::isfinite(squared)) return false;
    if (squared > 100.0f) {
        if (depth >= 12) return false;
        const auto midpoint = (from + to) * 0.5f;
        return AppendGroundLeg(from, midpoint, positions, depth + 1) &&
            AppendGroundLeg(midpoint, to, positions, depth + 1);
    }

    const float radius = Length(to);
    if (!std::isfinite(radius) || radius < 100 || radius > 10000) return false;
    positions.push_back(to * (500.0f / radius));
    return true;
}

static const GroundStroke* FindRoadStroke(Terrain::ITerrain& terrain, const void* road)
{
    const size_t count = terrain.funcC8h();
    if (count > 256) return nullptr;

    const GroundStroke* current = nullptr;
    for (size_t i = 0; i < count; ++i) {
        const auto* stroke = reinterpret_cast<const GroundStroke*>(terrain.funcC4h(int(i)));
        if (!stroke || stroke->road != road) continue;
        if (current) return nullptr; // Another layer has the same ownership key.
        current = stroke;
    }
    if (!current || current->stamps.size() < 2 || current->stamps.size() > kMaxPoints) return nullptr;
    for (const auto& stamp : current->stamps) {
        if (stamp.brush != kRoadBrush) return nullptr;
    }
    return current;
}

static bool SameStroke(const GroundStroke& current, const Path& positions)
{
    if (positions.size() != current.stamps.size()) return false;
    bool forward = true, backward = true;
    for (size_t i = 0; i < positions.size() && (forward || backward); ++i) {
        if (forward) forward = Distance(positions[i], current.stamps[i].position) < 0.02f;
        if (backward) backward = Distance(positions[i], current.stamps[positions.size() - 1 - i].position) < 0.02f;
    }
    return forward || backward;
}

bool AlignGroundStroke(Terrain::ITerrain& terrain, const void* road, const Path& path)
{
    if (!road || path.size() < 2 || path.size() > kMaxPoints) return false;
    const auto* current = FindRoadStroke(terrain, road);
    if (!current) return false;

    Path positions;
    if (!AppendGroundLeg(path.front(), path.front(), positions)) return false;
    for (size_t i = 1; i < path.size(); ++i) {
        if (!AppendGroundLeg(path[i - 1], path[i], positions)) return false;
    }
    if (SameStroke(*current, positions)) return false;

    // Stage all stamps before releasing the old layer. The terrain takes ownership
    // of the descriptor and its EASTL buffer, both supplied by Spore's allocator.
    GroundStroke staged{road, {}};
    staged.stamps.reserve(positions.size());
    GroundStamp stamp = current->stamps.front();
    stamp.tile = -1; // The native publisher requests fresh tile assignments.
    for (const auto& position : positions) {
        stamp.position = position;
        staged.stamps.push_back(stamp);
    }
    auto* replacement = new GroundStroke{road, {}};
    replacement->stamps.swap(staged.stamps);
    terrain.funcBCh(reinterpret_cast<int>(road));
    terrain.funcB8h(reinterpret_cast<int>(replacement));
    return true;
}

const void* LandRoadKey(const City& from, const City& to)
{
    if (from.mTradePartners.size() > 64) return nullptr;
    for (const auto& partner : from.mTradePartners) {
        if (partner.field_08 || Native::TradePartnerCity(partner) != &to) continue;
        // This word is a borrowed identity. Compare it with terrain entries without dereferencing it.
        const void* key;
        std::memcpy(&key, reinterpret_cast<const unsigned char*>(&partner) + 4, sizeof key);
        if (key) return key;
    }
    return nullptr;
}

static void AlignTradeGround(State& state, Simulator::cPlanetModel& planet,
    City& a, City& b, Record& record, const Path& path, uint64_t now, const Services& services)
{
    if (!services.paint_available || now < record.next_ground_check ||
        now < state.next_ground_paint || !planet.mpTerrain ||
        !GroundMethods(*planet.mpTerrain, services.image_base)) return;

    record.next_ground_check = now + 10000;
    const void* road = LandRoadKey(a, b);
    if (!road) road = LandRoadKey(b, a);
    if (!AlignGroundStroke(*planet.mpTerrain, road, path)) return;
    state.next_ground_paint = now + 2000;
    if (services.log_allowed()) {
        ModAPI::Log("[Better Civ Routes] t=%llu event=ground-road-aligned cities=%u,%u points=%u",
            now, a.mID, b.mID, unsigned(path.size()));
    }
}

Simulator::cPlanetModel* CurrentPlanet()
{
    return Simulator::cPlanetModel::Get();
}

float TerrainHeight(Simulator::cPlanetModel* planet, const Point& point)
{
    return planet->GetHeightAt(point);
}

Exclusions SpiceNodes()
{
    Exclusions nodes;
    for (const auto& node : Simulator::GetData<Simulator::cCommodityNode>().data) {
        if (!node || node->mbIsDestroyed || !Finite(node->mPosition) ||
            !std::isfinite(node->mFootprintRadius) || node->mFootprintRadius < 0 || node->mFootprintRadius > 37) continue;
        nodes.push_back({node->mPosition, std::max(5.0f, node->mFootprintRadius) + 3});
        if (nodes.size() > 64) break;
    }
    return nodes;
}

static bool ReadCityGeometry(const City& city, Ends& end, uint64_t& hash)
{
    const auto* walls = city.mpCityWalls.get();
    if (!walls || walls->mbIsDestroyed || !Finite(city.mPosition) || Length(city.mPosition) < 100 ||
        !std::isfinite(walls->mInnerRadius) || !std::isfinite(walls->mOuterRadius) ||
        walls->mInnerRadius < 5 || walls->mOuterRadius < walls->mInnerRadius || walls->mOuterRadius > 100 ||
        walls->mGatesTransformed.size() < 3 || walls->mGatesTransformed.size() > 16) return false;
    end = {city.mPosition, walls->mInnerRadius, walls->mOuterRadius};
    hash = HashBytes(hash, &city.mPoliticalID, sizeof(city.mPoliticalID));
    hash = HashBytes(hash, &end, sizeof(end));
    hash = HashBytes(hash, walls->mGatesTransformed.data(), 3 * sizeof(Point));
    return true;
}

struct GatePair {
    Point from, to;
    float distance;
};

static unsigned GatePairs(const City& from, const City& to, const Ends& from_end,
    const Ends& to_end, GatePair (&pairs)[9])
{
    unsigned count = 0;
    // Only the first three gates serve land traffic. Gate 3 is the dock and gate 5 serves freight.
    for (unsigned i = 0; i < 3; ++i) {
        for (unsigned j = 0; j < 3; ++j) {
            const auto& a = from.mpCityWalls->mGatesTransformed[i];
            const auto& b = to.mpCityWalls->mGatesTransformed[j];
            if (!Finite(a) || !Finite(b) || !NearEdge(a, from_end) || !NearEdge(b, to_end)) continue;
            pairs[count++] = {a, b, Distance(a, b)};
        }
    }
    std::sort(pairs, pairs + count, [](const GatePair& a, const GatePair& b) { return a.distance < b.distance; });
    return count;
}

struct GateSearchResult {
    Path path;
    size_t node_count = 0;
    const char* reason = "native-path-unavailable";
    bool valid = false;
};

static GateSearchResult SearchGatePair(void* manager, const Services& services, const GatePair& pair,
    const Ends& from, const Ends& to, float sea, const SurfaceHeight& height)
{
    // C414C0 uses the same synchronous search, profile and range.
    NavPath nodes;
    GateSearchResult result;
    const bool found = services.path_search(manager, services.land_profile, pair.from, pair.to, &nodes, 0.0f, 1.0f);
    result.node_count = nodes.size();
    if (!found || nodes.size() < 2 || nodes.size() > kMaxPoints) return result;

    Path input;
    for (const auto& node : nodes) input.push_back(node.position);
    result.reason = "partial-path";
    const bool reached_gates = Distance(input.front(), pair.from) <= 8 && Distance(input.back(), pair.to) <= 8;
    if (!reached_gates || !GroundPath(input, sea, height, result.path, result.reason) ||
        !HasCityEnds(result.path, from, to)) return result;

    if (AvoidSpice(result.path, services.spice_nodes(), from, to, sea, height)) result.reason = "spice-detour";
    result.valid = true;
    return result;
}

static void PublishReplacement(Simulator::cPlanetModel& planet, Path& displayed, Record& record, const Path& replacement)
{
    displayed = replacement;
    record.replacement = replacement;
    record.path_hash = HashPath(replacement);
    planet.mLandTradeRoutesDirty = true;
}

void Repair(State& state, Simulator::cPlanetModel& planet, uint64_t now, const Services& services)
{
    float sea = 0;
    if (planet.mLandTradeRoutes.size() > 64 || !services.terrain_ready(&planet, sea)) return;
    for (auto& route : planet.mLandTradeRoutes) {
        if (route.city1 == route.city2 || !services.live_city(route.city1) || !services.live_city(route.city2) ||
            route.path.size() > kMaxPoints) continue;
        auto& a = *route.city1;
        auto& b = *route.city2;
        Ends from, to;
        uint64_t topology = 14695981039346656037ull;
        if (!ReadCityGeometry(a, from, topology) || !ReadCityGeometry(b, to, topology)) continue;

        auto& record = state.Get(CityKey(a), CityKey(b), now);
        const auto source = HashPath(route.path);
        const SurfaceHeight height = [&](const Point& direction) { return services.height(&planet, direction * sea); };
        const bool new_source = source != record.path_hash && source != record.source_hash;
        if (record.topology != topology || new_source) {
            if (route.path.empty()) continue;
            record.topology = topology;
            record.source_hash = record.path_hash = source;
            record.pending = false;
            record.attempt = 0;
            record.next_ground_check = 0;
            record.replacement.clear();

            Path projected;
            const char* reason = nullptr;
            float surface_error = 0;
            const bool grounded = GroundPath(route.path, sea, height, projected, reason, &surface_error);
            if (!grounded && !std::strcmp(reason, "invalid-terrain")) {
                record.topology = 0; // Retry when terrain sampling becomes available.
                continue;
            }
            if (grounded && surface_error <= 3.0f && HasCityEnds(route.path, from, to)) {
                if (AvoidSpice(projected, services.spice_nodes(), from, to, sea, height)) {
                    PublishReplacement(planet, route.path, record, projected);
                    if (services.log_allowed()) {
                        ModAPI::Log("[Better Civ Routes] t=%llu event=spice-detour cities=%u,%u", now, a.mID, b.mID);
                    }
                }
                AlignTradeGround(state, planet, a, b, record, route.path, now, services);
                continue;
            }

            if (grounded) reason = surface_error > 3.0f ? "off-terrain" : "city-endpoint";
            record.pending = true;
            if (services.log_allowed()) {
                ModAPI::Log("[Better Civ Routes] t=%llu event=invalid-ribbon cities=%u,%u owners=%u,%u points=%u reason=%s",
                    now, a.mID, b.mID, a.mPoliticalID, b.mPoliticalID, unsigned(route.path.size()), reason);
            }
            route.path.clear();
            record.path_hash = HashPath(route.path);
            planet.mLandTradeRoutesDirty = true;
        } else if (source == record.source_hash && source != record.path_hash) {
            // The engine republished the same rejected path. Reuse its cached replacement.
            route.path = record.replacement;
            planet.mLandTradeRoutesDirty = true;
        }

        if (!record.pending) {
            if (!route.path.empty()) AlignTradeGround(state, planet, a, b, record, route.path, now, services);
            continue;
        }
        if (now < state.next_search) continue;
        auto* manager = services.path_manager();
        if (!manager) continue;

        GatePair pairs[9];
        const unsigned count = GatePairs(a, b, from, to, pairs);
        if (record.attempt >= count) {
            record.pending = false;
            if (services.log_allowed()) {
                ModAPI::Log("[Better Civ Routes] t=%llu event=ribbon-hidden cities=%u,%u reason=no-verified-land-path attempts=%u",
                    now, a.mID, b.mID, record.attempt);
            }
            continue;
        }
        const auto& pair = pairs[record.attempt++];
        state.next_search = now + 2000;
        const auto result = SearchGatePair(manager, services, pair, from, to, sea, height);
        if (result.valid) {
            PublishReplacement(planet, route.path, record, result.path);
            record.pending = false;
            AlignTradeGround(state, planet, a, b, record, route.path, now, services);
        }
        if (services.log_allowed()) {
            ModAPI::Log("[Better Civ Routes] t=%llu event=%s cities=%u,%u attempt=%u nodes=%u points=%u reason=%s",
                now, result.valid ? "ribbon-rebuilt" : "path-rejected", a.mID, b.mID, record.attempt,
                unsigned(result.node_count), unsigned(result.path.size()), result.reason);
        }
    }
}

} // namespace BetterCiv::TradeRoutes
