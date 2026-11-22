// Offline loader fixture only. Never install this DLL in the game or Launcher Kit.
#include <cstring>
#include <cstdint>
#include <cstdlib>

// Fixture addresses are already absolute; the SDK adds this relocation base.
__declspec(dllexport) uintptr_t baseAddress = 0;

namespace {
uintptr_t fixtureAllocator = 0;
void unexpectedEngineAllocation() { std::abort(); }
void* fixtureSimulator() { return nullptr; }
}
namespace Internal_addresses {
__declspec(dllexport) extern const uintptr_t Allocator_ptr = reinterpret_cast<uintptr_t>(&fixtureAllocator);
__declspec(dllexport) extern const uintptr_t new_ = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
__declspec(dllexport) extern const uintptr_t delete_ = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
}
namespace Resource::IResourceManager_addresses {
__declspec(dllexport) extern const uintptr_t Get = reinterpret_cast<uintptr_t>(fixtureSimulator);
}
namespace Simulator::cSimulatorSystem_addresses {
__declspec(dllexport) extern const uintptr_t Get = reinterpret_cast<uintptr_t>(fixtureSimulator);
}
namespace Simulator::cGameNounManager_addresses {
// No real game noun manager exists in the offline fixture. Portrait tests
// inject their own SDK-typed snapshot provider before invoking the UI adapter.
__declspec(dllexport) extern const uintptr_t Get = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
__declspec(dllexport) extern const uintptr_t GetData = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
}
namespace Simulator::cCommManager_addresses {
__declspec(dllexport) extern const uintptr_t Get = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
__declspec(dllexport) extern const uintptr_t IsCommScreenActive = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
}
namespace Simulator::cPlanetModel_addresses {
__declspec(dllexport) extern const uintptr_t Get = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
__declspec(dllexport) extern const uintptr_t GetHeightAt = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
}
namespace UTFWin::IButton_addresses {
__declspec(dllexport) extern const uintptr_t Create = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation);
}

// The pager now owns SDK image/drawable objects. Match the SDK's static UI
// allocator ABI with local test memory; rendering/serialization remain fatal.
namespace {
void* __cdecl fixtureUiAllocator() { return nullptr; }
void* __cdecl fixtureUiNew(size_t size, size_t alignment, const char*, void*) {
    if (alignment != 4) std::abort();
    return std::malloc(size);
}
void __cdecl fixtureUiDelete(void* pointer) { std::free(pointer); }
}
namespace UTFWin_addresses {
__declspec(dllexport) extern const uintptr_t GetAllocator = reinterpret_cast<uintptr_t>(fixtureUiAllocator);
}
namespace UTFWin::UTFWinObject_addresses {
__declspec(dllexport) extern const uintptr_t new_ = reinterpret_cast<uintptr_t>(fixtureUiNew);
__declspec(dllexport) extern const uintptr_t delete_ = reinterpret_cast<uintptr_t>(fixtureUiDelete);
}
#define UNEXPECTED_UI_ADDRESS(name) __declspec(dllexport) extern const uintptr_t name = reinterpret_cast<uintptr_t>(unexpectedEngineAllocation)
namespace UTFWin::Image_addresses {
UNEXPECTED_UI_ADDRESS(GetImage);
UNEXPECTED_UI_ADDRESS(SetSerializer);
UNEXPECTED_UI_ADDRESS(GetProxyID);
}
namespace UTFWin::StdDrawable_addresses {
UNEXPECTED_UI_ADDRESS(Paint);
UNEXPECTED_UI_ADDRESS(IsColliding);
UNEXPECTED_UI_ADDRESS(GetDimensions);
UNEXPECTED_UI_ADDRESS(UseCollision);
UNEXPECTED_UI_ADDRESS(SetSerializer);
UNEXPECTED_UI_ADDRESS(GetProxyID);
UNEXPECTED_UI_ADDRESS(GetImage);
UNEXPECTED_UI_ADDRESS(SetImage);
UNEXPECTED_UI_ADDRESS(GetScaleType);
UNEXPECTED_UI_ADDRESS(SetScaleType);
UNEXPECTED_UI_ADDRESS(GetScaleArea);
UNEXPECTED_UI_ADDRESS(SetScaleArea);
UNEXPECTED_UI_ADDRESS(GetScaleFactor);
UNEXPECTED_UI_ADDRESS(SetScaleFactor);
UNEXPECTED_UI_ADDRESS(GetHitMask);
UNEXPECTED_UI_ADDRESS(SetHitMask);
UNEXPECTED_UI_ADDRESS(GetHitFactor);
UNEXPECTED_UI_ADDRESS(SetHitFactor);
}
#undef UNEXPECTED_UI_ADDRESS

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

namespace Simulator { class ISimulatorStrategy; }
namespace ModAPI {
__declspec(dllexport) bool AddSimulatorStrategy(Simulator::ISimulatorStrategy*, uint32_t) { return true; }
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
