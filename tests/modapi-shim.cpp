// Offline loader fixture only. Never install this DLL in the game or Launcher Kit.
#include <cstring>

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
