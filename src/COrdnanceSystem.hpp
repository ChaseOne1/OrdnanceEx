#pragma once
#include "COrdnanceInfo.hpp"
#include "extender\VehicleExtender.h"

class CVehicle;
class CEntity;

class COrdnanceSystem
{
private:
    inline static plugin::VehicleExtendedData<COrdnanceInfo> ms_OrdnanceInfo;

    static void LoadOrdnanceInfo();
    static void ProcessOrdnance();

    static int32_t __fastcall MyGetPlaneNumGuns(CVehicle*, int);
    static void FirePlaneGuns();
    static void FireUnguidedMissile();
    static void ScanAndMarkTargetForHeatSeekingMissile(unsigned int& targetLockStartTime, CEntity*& targetEntity);
    static void FireHeatSeakingMissile(unsigned int& targetLockStartTime, CEntity*& targetEntity);
    static void FireHeatSeakingMissile(CEntity* target);
    static void FireFlares();

public:
    static void InstallPatches();
    static void reInit();
    static void MyProcessWeapons(injector::reg_pack& regs);
};
