#include "plugin.h"
#include "COrdnanceSystem.hpp"

using namespace plugin;

class OrdnanceEx
{
public:
    OrdnanceEx()
    {
        Events::initGameEvent += []() {
            injector::MakeInline <0x6C7915, 0x6C7915 + 7>(COrdnanceSystem::MyProcessWeapons);
            injector::MakeInline <0x6C9346, 0x6C9346 + 7>(COrdnanceSystem::MyProcessWeapons);
            COrdnanceSystem::InstallPatches();
            };
        Events::reInitGameEvent += COrdnanceSystem::reInit;
    }
}ordnanceEx;