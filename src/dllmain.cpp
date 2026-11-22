#include <Spore/ModAPI.h>
#include "BetterCiv.h"

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
        ModAPI::AddPostInitFunction(BetterCiv::Initialize);
    return TRUE;
}
