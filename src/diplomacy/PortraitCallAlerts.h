#pragma once
#include "PortraitPages.h"

namespace BetterCiv {
using ApplyPortraitMaterial = void (__cdecl*)(UTFWin::IWindow*, const char16_t*, int);
extern ApplyPortraitMaterial native_apply_portrait_material;
inline constexpr char16_t kPortraitCallMaterial[] =
    u"ui_material_blink -color (2,2,2) -frequency 1 -intensity 1 -func 1 -func_inv 1";
inline constexpr char16_t kPortraitIdleMaterial[] = u"ui_material_default";

struct PortraitCallAlerts {
    uintptr_t panel_identity = 0;
    struct Entry {
        uint64_t civilization = 0;
        uintptr_t window_identity = 0;
        uint32_t event = UINT32_MAX;
    } windows[4][2];
    unsigned log_lines = 0;
};
extern PortraitCallAlerts portrait_call_alerts;
void reset_portrait_call_alerts();
void repair_portrait_call_alerts(Native::RivalPortraitPanel* panel, const CivilizationList& civs);
}
