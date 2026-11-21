#include "MovementObserver.h"
#include "NativeExtensions.h"
#include <Spore/ModAPI.h>
#include <Spore/Simulator/cCulturalTarget.h>
#include "MovementDiagnostics.h"
#include <windows.h>

namespace BetterCiv::Movement {
namespace {

namespace D = CombatDiagnostics;
using City = Simulator::cCity;

// Live game pointers are borrowed for this single sample. The journal keeps
// scalar identities and snapshots across ticks.
struct Candidate {
    const Vehicle* unit;
    const Civilization* civilization;
    const City* city;
    Snapshot snapshot;
};

const City* FindLiveCity(const City* wanted, const CivilizationList& civilizations)
{
    if (!wanted) return nullptr;
    for (const auto& owner : civilizations) {
        if (!owner || owner->mbIsDestroyed) continue;
        for (const auto& city : owner->mCities) {
            if (city && city.get() == wanted && !city->mbIsDestroyed) return city.get();
        }
    }
    return nullptr;
}

void ReportCity(Journal& journal, const CivilizationList& civilizations, const City& city,
                uint64_t now, unsigned count, unsigned omitted)
{
    auto& record = D::Journal::For(journal.cities, object_key(&city), now);
    record.last_seen = now;
    if (now < record.next_report || !journal.Reserve()) return;
    record.next_report = now + kReportMs;

    char name[256]{};
    const auto length = std::min<size_t>(city.mName.size(), 60);
    if (length) {
        WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(city.mName.c_str()),
            static_cast<int>(length), name, sizeof(name) - 1, nullptr, nullptr);
    }
    ModAPI::Log(
        "[Better Civ Movement] t=%llu city=%u name=\"%s\" owner-political=%u capture=%.3f conversion=%.3f"
        " in-view=%d visible-units=%u omitted-visible=%u rate-suppressed=%u",
        now, city.mID, name, city.mPoliticalID, city.mCapturePercent, city.mConvertPercent,
        city.mbInView, count, omitted, journal.suppressed);

    unsigned slot = 0;
    for (const auto& turret : city.mTurrets) {
        const unsigned index = slot++;
        if (!turret || turret->mbIsDestroyed || turret->mHealthPoints <= 0) continue;
        if (!journal.Reserve()) break;
        const auto aim = D::Resolve(turret->mpTarget.get(), true, civilizations);
        ModAPI::Log(
            "[Better Civ Movement] t=%llu city=%u turret=%u slot=%u frozen=%d health=%.1f aim=%s:%u "
            "aim-distance=%.2f",
            now, city.mID, turret->mID, index, turret->mFreezeCount, turret->mHealthPoints,
            D::Name(aim.kind), aim.id, aim.Known() ? D::Distance(turret->mPosition, aim.position) : -1.0f);
    }
}

void ReportUnit(Journal& journal, const Candidate& candidate, uint64_t now)
{
    const auto& snapshot = candidate.snapshot;
    const auto& unit = snapshot.unit;
    auto& record = D::Journal::For(journal.units, object_key(candidate.unit), now);
    record.Observe(snapshot, now);
    if (!record.Due(now) || !journal.Reserve()) return;

    const auto* weapon = candidate.unit->mpWeapon.get();
    const float range = weapon ? weapon->GetMaxRange() : -1.0f;
    ModAPI::Log(
        "[Better Civ Movement] t=%llu unit=%u civ=%u player=%d purpose=%d locomotion=%d city=%u order=%d "
        "order-target=%s:%u order-city=%u aim=%s:%u aim-city=%u aim-distance=%.2f range=%.2f "
        "position=%.2f,%.2f,%.2f velocity=%.2f,%.2f,%.2f angular=%.2f,%.2f,%.2f "
        "order-position=%.2f,%.2f,%.2f desired-speed=%.2f neighbors=%u hostiles=%u health=%.1f "
        "since-fire-ms=%.0f window-ms=%llu steps=%u path=%.2f net=%.2f velocity-turn-deg=%.1f valid=%d "
        "order-changes=%u aim-changes=%u",
        now, unit.id, unit.owner, candidate.civilization->mIsPlayerOwned, unit.purpose, unit.locomotion,
        unit.near_city, unit.order, D::Name(unit.order_target.kind), unit.order_target.id,
        unit.order_target.city, D::Name(unit.aim.kind), unit.aim.id, unit.aim.city,
        unit.aim.Known() ? D::Distance(unit.position, unit.aim.position) : -1.0f, range,
        unit.position.x, unit.position.y, unit.position.z, unit.velocity.x, unit.velocity.y, unit.velocity.z,
        snapshot.angular_velocity.x, snapshot.angular_velocity.y, snapshot.angular_velocity.z,
        snapshot.order_position.x, snapshot.order_position.y, snapshot.order_position.z, unit.desired_speed,
        snapshot.neighbors, unit.hostiles, unit.health, unit.since_fire_ms, now - record.start,
        record.samples, record.path, D::Distance(unit.position, record.anchor), record.turn_degrees,
        record.valid, record.order_changes, record.aim_changes);
    record.Reported(now);
}

} // namespace

void Observe(Journal& journal, const CivilizationList& civilizations, CurrentVehicleOrder read_order, uint64_t now)
{
    Candidate candidates[kMaxUnits];
    unsigned count = 0, omitted = 0;
    for (const auto& civilization : civilizations) {
        if (!civilization || civilization->mbIsDestroyed) continue;
        for (const auto& unit : civilization->mVehicles) {
            if (!unit || unit->mbIsDestroyed || unit->mbDead || !unit->mbInView) continue;
            if (count == kMaxUnits) {
                ++omitted;
                continue;
            }

            Snapshot snapshot;
            snapshot.unit = D::ReadUnit(*unit, civilization->mID);
            snapshot.in_view = unit->mbInView;
            const auto* order = read_order ? read_order(unit.get()) : nullptr;
            if (order) {
                snapshot.unit.order = order->kind;
                snapshot.order_position = order->position;
                snapshot.unit.order_target = D::Resolve(order->target, false, civilizations);
            }
            snapshot.unit.aim = D::Resolve(unit->mpTarget.get(), true, civilizations);
            snapshot.angular_velocity = unit->mAngularVelocity;
            snapshot.neighbors = static_cast<unsigned>(unit->mNeighbors.size());
            const City* city = FindLiveCity(unit->mpNearestCity.get(), civilizations);
            if (city) {
                snapshot.unit.near_city = city->mID;
                snapshot.city_under_attack = city->mCapturePercent > 0 || city->mConvertPercent > 0;
            }
            if (snapshot.Eligible())
                candidates[count++] = {unit.get(), civilization.get(), city, snapshot};
        }
    }

    // Turrets get the first reports. Rotate vehicle order so the same units do
    // not consume the shared reporting budget every time.
    for (unsigned i = 0; i < count; ++i) {
        if (candidates[i].city)
            ReportCity(journal, civilizations, *candidates[i].city, now, count, omitted);
    }
    for (unsigned offset = 0; offset < count; ++offset)
        ReportUnit(journal, candidates[(journal.cursor + offset) % count], now);
    if (count) journal.cursor = (journal.cursor + 17) % count;

    if (omitted && !journal.capacity_notified && journal.Reserve()) {
        journal.capacity_notified = true;
        ModAPI::Log("[Better Civ Movement] Visible-unit capacity reached; detail covers at most 128 units per sample.");
    }
    if (journal.lines >= kMaxLines && !journal.limit_notified) {
        journal.limit_notified = true;
        ModAPI::Log("[Better Civ Movement] Log limit reached; copy spore_log.txt before restarting for another movement test.");
    }
}
} // namespace BetterCiv::Movement
