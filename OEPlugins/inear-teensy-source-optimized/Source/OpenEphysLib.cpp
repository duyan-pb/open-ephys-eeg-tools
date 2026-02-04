/*
    ------------------------------------------------------------------

    InEar Teensy Optimized Source Plugin for Open Ephys
    
    Plugin registration.

    ------------------------------------------------------------------
*/

#include <PluginInfo.h>
#include "InEarTeensyOptimizedThread.h"

#ifdef _WIN32
#include <Windows.h>
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

extern "C" EXPORT void getLibInfo(Plugin::LibraryInfo* info)
{
    info->apiVersion = PLUGIN_API_VER;
    info->name = "InEar Teensy Optimized";
    info->libVersion = "1.0.0";
    info->numPlugins = 1;
}

extern "C" EXPORT int getPluginInfo(int index, Plugin::PluginInfo* info)
{
    if (index == 0)
    {
        info->type = Plugin::Type::DATA_THREAD;
        info->dataThread.name = "InEar Teensy Opt";
        info->dataThread.creator = &createDataThread<InEarTeensyOptimizedThread>;
        return 0;
    }
    return -1;
}
