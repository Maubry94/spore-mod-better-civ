#pragma once
#include <Spore/ModAPI.h>
#include <Spore/App/PropertyList.h>
#include <Spore/LocalizedString.h>

namespace BetterCiv {
inline constexpr uint32_t kPaletteItemCost = 0x02166464;
inline constexpr uint32_t kPaletteItemType = 0x02196ad5;

typedef const float *(__thiscall *ReadFloatProperty)(App::Property*);
using ReadLocalizedText = const char16_t* (__thiscall*)(LocalizedString*);
extern ReadFloatProperty read_float_property;
extern ReadLocalizedText read_localized_text;

member_detour(configured_price, App::Property, const float *()) {
    const float* detoured();
};

member_detour(configured_palette_cost, App::PropertyList, bool(uint32_t property_id, App::Property*& result)) {
    bool detoured(uint32_t property_id, App::Property*& result);
};

member_detour(configured_hover_text, LocalizedString, const char16_t*()) {
    const char16_t* detoured();
};

bool __cdecl configured_palette_int(const App::PropertyList* list, uint32_t property_id, int32_t& result);
const char16_t* discount_hover_text(const char16_t* native, char16_t (&buffer)[512]);
}
