/*
    ------------------------------------------------------------------

    BioSerial-Pro Source Plugin for Open Ephys
    
    Plugin registration and library information.

    ------------------------------------------------------------------
*/

#include <PluginInfo.h>
#include "BioSerialProThread.h"

#include <string>

#ifdef _WIN32
#include <Windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C" EXPORT void getLibInfo(Plugin::LibraryInfo* info)
{
    info->apiVersion = PLUGIN_API_VER;
    info->name = "BioSerial-Pro Source";
    info->libVersion = "1.0.0";
    info->numPlugins = 1;
}

extern "C" EXPORT int getPluginInfo(int index, Plugin::PluginInfo* info)
{
    switch (index)
    {
        case 0:
            info->type = Plugin::Type::DATA_THREAD;
            info->dataThread.name = "BioSerial-Pro";
            info->dataThread.creator = &BioSerialProThread::createDataThread;
            break;
            
        default:
            return -1;
    }
    
    return 0;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    return TRUE;
}
#endif
