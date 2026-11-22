#pragma once
#include <Spore/Simulator/cCivilization.h>
#include <cstdint>

namespace BetterCiv {
using Civilization = Simulator::cCivilization;
using CivilizationList = eastl::vector<cCivilizationPtr>;

// A companion file remembers first-conquest order for each planet and player.
struct PortraitOrder {
    bool initialized = false, has_key = false;
    uint32_t key[4]{};
    uint32_t ids[9]{};
    unsigned count = 0;
};

struct PortraitOrderFile {
    uint32_t magic = 0x324f4342, version = 1;
    uint32_t key[4]{};
    uint32_t count = 0;
    uint32_t ids[9]{};
    uint32_t checksum = 0;
};
static_assert(sizeof(PortraitOrderFile) == 68);

extern PortraitOrder portrait_order;
extern bool (*portrait_order_key)(const CivilizationList&, uint32_t*);
extern bool (*read_portrait_order)(const uint32_t*, PortraitOrderFile&);
extern bool (*write_portrait_order)(const uint32_t*, const PortraitOrderFile&);

uint32_t portrait_order_checksum(const PortraitOrderFile& record);
bool valid_portrait_order_file(const PortraitOrderFile& file, const uint32_t* key);
bool read_portrait_order_file(const wchar_t* path, const uint32_t* key, PortraitOrderFile& result);
bool write_portrait_order_file(const wchar_t* path, const PortraitOrderFile& data);
void note_portrait_qualification(const Civilization* civ);
void reset_portrait_order();
void save_portrait_order();
}
