#include "VehiclePrices.h"
#include "engine/Runtime.h"
#include <Spore/Palettes/PaletteItem.h>
#include <cstring>

namespace BetterCiv {
ReadFloatProperty read_float_property;
ReadLocalizedText read_localized_text;

const float* configured_price::detoured()
{
    const float* vanilla = read_float_property(this);
    static thread_local float value;
    if (!vanilla || settings.cost_percent == 100 || !in_civilization())
        return vanilla;
    value = *vanilla * static_cast<float>(settings.cost_percent) / 100.0f;
    return &value;
}

// The planner and tooltips read paletteItemCost, not cCivilization's float
// vehicle cost. Keep both paths on the same multiplier without editing the
// shared property list (which can also be used outside Civilization).
static bool is_discounted_vehicle_item(const App::PropertyList* list, uint32_t property_id)
{
    if (property_id != kPaletteItemCost || settings.cost_percent == 100 || !in_civilization())
        return false;
    uint32_t type = 0;
    return App::Property::GetKeyInstanceID(list, kPaletteItemType, type) &&
        type == Palettes::PaletteItem::kItemCityVehicle;
}

static int32_t palette_vehicle_price(int32_t native)
{
    // Zero/negative values are UI sentinels; the offered percentages are <= 100.
    return native > 0 ? static_cast<int32_t>(static_cast<int64_t>(native) * settings.cost_percent / 100)
                      : native;
}

bool configured_palette_cost::detoured(uint32_t property_id, App::Property*& result)
{
    const bool found = this->GetProperty(property_id, result);
    if (found && result && result->mnType == App::PropertyType::Int32 &&
        is_discounted_vehicle_item(this, property_id)) {
        // Each checked caller consumes the scalar immediately. Never give
        // the engine a stack pointer or modify the original property.
        static thread_local App::Property adjusted;
        adjusted.SetValueInt32(palette_vehicle_price(*result->GetValueInt32()));
        result = &adjusted;
    }
    return found;
}

bool __cdecl configured_palette_int(const App::PropertyList* list, uint32_t property_id, int32_t& result)
{
    const bool found = App::Property::GetInt32(list, property_id, result);
    if (found && is_discounted_vehicle_item(list, property_id))
        result = palette_vehicle_price(result);
    return found;
}

const char16_t* discount_hover_text(const char16_t* native, char16_t (&buffer)[512])
{
    if (!native || settings.cost_percent == 100 || !in_civilization()) return native;
    size_t length = 0;
    while (length < 512 && native[length]) ++length;
    if (length == 512) return native;
    size_t begin = 0;
    while (begin < length && (native[begin] < u'0' || native[begin] > u'9')) ++begin;
    if (begin == length) return native;
    size_t end = begin;
    int64_t price = 0;
    char16_t separator = 0;
    while (end < length) {
        const char16_t ch = native[end];
        if (ch >= u'0' && ch <= u'9') {
            price = price * 10 + ch - u'0';
            if (price > INT32_MAX) return native;
        } else if ((ch == u' ' || ch == u'\u00a0' || ch == u'\u202f' || ch == u',' || ch == u'.' || ch == u'\'') &&
                   end + 1 < length && native[end + 1] >= u'0' && native[end + 1] <= u'9') {
            separator = ch;
        } else break;
        ++end;
    }
    int32_t discounted = palette_vehicle_price(static_cast<int32_t>(price));
    char16_t reverse[24];
    size_t digits = 0, count = 0;
    do {
        if (separator && digits && digits % 3 == 0) reverse[count++] = separator;
        reverse[count++] = static_cast<char16_t>(u'0' + discounted % 10);
        discounted /= 10;
        ++digits;
    } while (discounted);
    if (begin + count + length - end >= 512) return native;
    memcpy(buffer, native, begin * sizeof(char16_t));
    for (size_t i = 0; i < count; ++i) buffer[begin + i] = reverse[count - i - 1];
    memcpy(buffer + begin + count, native + end, (length - end + 1) * sizeof(char16_t));
    return buffer;
}

// The three world-view purchase buttons use localized text with a literal
// vanilla price. These exact GetText calls are separate from paletteItemCost.
const char16_t* configured_hover_text::detoured()
{
    static thread_local char16_t buffer[512];
    return discount_hover_text(read_localized_text(this), buffer);
}
} // namespace BetterCiv
