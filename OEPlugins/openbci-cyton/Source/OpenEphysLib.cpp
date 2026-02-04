/*
    OpenBCI Cyton Plugin
    
    Plugin registration for Open Ephys GUI
*/

#include <PluginInfo.h>
#include "OpenBCICytonThread.h"

#include <string>

#ifdef _WIN32
#include <Windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

using namespace Plugin;

#define NUM_PLUGINS 1

extern "C" EXPORT void getLibInfo(Plugin::LibraryInfo* info)
{
    // Plugin library info
    info->apiVersion = PLUGIN_API_VER;
    info->name = "OpenBCI Cyton";
    info->libVersion = "0.1.0";
    info->numPlugins = NUM_PLUGINS;
}

extern "C" EXPORT int getPluginInfo(int index, Plugin::PluginInfo* info)
{
    switch (index)
    {
        case 0:
            // OpenBCI Cyton DataThread
            info->type = Plugin::Type::DATA_THREAD;
            info->dataThread.name = "OpenBCI Cyton";
            info->dataThread.creator = &createDataThread<OpenBCICytonThread>;
            break;
            
        default:
            return -1;
    }
    
    return 0;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    return TRUE;
}
#endif
