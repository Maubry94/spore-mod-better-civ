#pragma once
#include "TradeRouteGeometry.h"

namespace Terrain { class ITerrain; }
namespace Simulator { class cCity; }

namespace BetterCiv::TradeRoutes {

// Native terrain records. Spore owns a stroke and its buffer once it accepts it.
struct GroundStamp {
    Point position;
    float width;
    uint32_t brush;
    float red, green, blue;
    int tile;
};
struct GroundStroke {
    const void* road;
    eastl::vector<GroundStamp> stamps;
};
static_assert(sizeof(GroundStamp) == 0x24 && offsetof(GroundStamp, tile) == 0x20);
static_assert(sizeof(GroundStroke) == 0x18 && offsetof(GroundStroke, stamps) == 4);
constexpr uint32_t kRoadBrush = 0x53496b14;

bool ValidateGroundPaint(const unsigned char* base, uint32_t relocation);
bool GroundMethods(const Terrain::ITerrain& terrain, uintptr_t base);
bool AlignGroundStroke(Terrain::ITerrain& terrain, const void* road, const Path& path);
const void* LandRoadKey(const Simulator::cCity& from, const Simulator::cCity& to);

} // namespace BetterCiv::TradeRoutes
