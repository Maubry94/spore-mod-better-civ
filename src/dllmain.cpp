#include <Spore/ModAPI.h>
#include "BetterCiv.h"

namespace {
HMODULE moduleHandle;
void Initialize() { BetterCiv::Initialize(moduleHandle); }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        moduleHandle = module;
        ModAPI::AddPostInitFunction(Initialize);
    }
    return TRUE;
}
