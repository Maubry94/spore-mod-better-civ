#pragma once
#include <Spore/MathUtils.h>
#include <EASTL/vector.h>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace BetterCiv::TradeRoutes {

using Point = Math::Vector3;
using Path = eastl::vector<Point>;
using SurfaceHeight = std::function<float(const Point&)>;
constexpr unsigned kMaxPoints = 4096;

// These small primitives are shared by the land and sea display paths.
bool Finite(const Point& point);
float Length(const Point& point);
float Distance(const Point& a, const Point& b);
uint64_t HashBytes(uint64_t hash, const void* data, size_t size);
uint64_t HashPath(const Path& path);

struct Ends {
    Point center;
    float inner = 0;
    float outer = 0;
};

struct Exclusion {
    Point center;
    float radius;
};
using Exclusions = eastl::vector<Exclusion>;

bool GroundPath(const Path& input, float sea, const SurfaceHeight& height, Path& output,
                const char*& reason, float* surface_error = nullptr);
bool HasCityEnds(const Path& path, const Ends& a, const Ends& b);
bool Crosses(const Path& path, const Exclusion& zone);
bool AvoidSpice(Path& path, const Exclusions& nodes, const Ends& a, const Ends& b,
                float sea, const SurfaceHeight& height);

} // namespace BetterCiv::TradeRoutes
