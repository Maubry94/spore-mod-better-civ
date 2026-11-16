// Offline loader fixture only. Never install this DLL in the game or Launcher Kit.
#include <cstring>
#include <cstdint>
#include <cstdlib>

// Fixture addresses are already absolute; the SDK adds this relocation base.
__declspec(dllexport) uintptr_t baseAddress = 0;

// Minimal scalar-property implementation for offline SDK calls. These test
// records use the independently checked 32-bit engine layout, not game data.
namespace {
struct ScalarProperty { uint32_t value[4]; uint16_t flags; uint16_t type; };
using FindProperty = bool (__thiscall*)(const void*, uint32_t, ScalarProperty*&);
bool findProperty(const void* list, uint32_t id, ScalarProperty*& result)
{
    if (!list) return false;
    auto table = *static_cast<void* const* const*>(list);
    return reinterpret_cast<FindProperty>(table[9])(list, id, result);
}
bool __cdecl getKeyInstance(const void* list, uint32_t id, uint32_t& result)
{
    ScalarProperty* value = nullptr;
    if (!findProperty(list, id, value) || !value || value->type != 0x20) return false;
    result = value->value[0];
    return true;
}
bool __cdecl getInt(const void* list, uint32_t id, int32_t& result)
{
    ScalarProperty* value = nullptr;
    if (!findProperty(list, id, value) || !value || value->type != 9) return false;
    std::memcpy(&result, value->value, sizeof result);
    return true;
}
int32_t* __fastcall getIntValue(ScalarProperty* value, void*)
{
    if (!value || value->type != 9 || value->flags) std::abort();
    return reinterpret_cast<int32_t*>(value->value);
}
ScalarProperty& __fastcall setIntValue(ScalarProperty* value, void*, const int32_t& number)
{
    if (!value || value->flags) std::abort();
    std::memcpy(value->value, &number, sizeof number);
    value->type = 9;
    return *value;
}
void __fastcall unexpectedClear(void*, void*, bool) { std::abort(); }
}

// These exported address constants have the same ABI as the official SDK.
namespace App::Property_addresses {
__declspec(dllexport) extern const uintptr_t GetKeyInstanceID = reinterpret_cast<uintptr_t>(getKeyInstance);
__declspec(dllexport) extern const uintptr_t GetInt32 = reinterpret_cast<uintptr_t>(getInt);
__declspec(dllexport) extern const uintptr_t GetValueInt32 = reinterpret_cast<uintptr_t>(getIntValue);
__declspec(dllexport) extern const uintptr_t SetValueInt32 = reinterpret_cast<uintptr_t>(setIntValue);
__declspec(dllexport) extern const uintptr_t Clear = reinterpret_cast<uintptr_t>(unexpectedClear);
}

namespace {
void (*postInit)() = nullptr;
bool rejectedHost = false;
}

namespace ModAPI {
__declspec(dllexport) void AddPostInitFunction(void (*callback)()) { postInit = callback; }
__declspec(dllexport) void Log(const char* format, ...)
{
    rejectedHost = std::strstr(format, "Rules inactive:") != nullptr;
}
}

extern "C" __declspec(dllexport) int RunRegisteredInitialization()
{
    if (!postInit) return 0;
    postInit();
    return rejectedHost ? 1 : 0;
}
