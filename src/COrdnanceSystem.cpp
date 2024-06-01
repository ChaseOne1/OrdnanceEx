#include "COrdnanceSystem.hpp"
#include "plugin.h"
#include "CWorld.h"
#include "CWeaponEffects.h"
#include "CProjectileInfo.h"
#include "NodeName.h"
#include "CAudioEngine.h"

using namespace plugin;
using namespace std;

static CVehicle* gs_pVehicle;
static constexpr short GUN_RATE = 30;
static unsigned int gs_nGunFiringTime;
static constexpr short ROCKET_RATE = 250;
static unsigned int gs_nRocketFiringTime;
static unsigned int gs_nTargetLockStartTime;
static CEntity* gs_pTargetEntity;
static constexpr short MISSILE_RATE = 500;
static unsigned int gs_nMissileFiringTime;
static constexpr short FLARE_RATE = 1000;
static unsigned int gs_nFlareFiringTime;

void COrdnanceSystem::InstallPatches()
{
    injector::MakeRangedNOP(0x6D3F30, 0x6D3F39);    injector::MakeJMP(0x6D3F30, MyGetPlaneNumGuns);
    //remove the mark restriction that is only used by hydra
    injector::MakeRangedNOP(0x527058, 0x52705F);    injector::MakeJMP(0x527058, 0x52706A);
}

void COrdnanceSystem::reInit()
{
    gs_nGunFiringTime = gs_nRocketFiringTime = gs_nTargetLockStartTime = gs_nMissileFiringTime = gs_nFlareFiringTime = NULL;
    gs_pTargetEntity = nullptr;
}

void COrdnanceSystem::MyProcessWeapons(injector::reg_pack& regs)
{
    regs.ecx = regs.esi;

    gs_pVehicle = reinterpret_cast<CVehicle*>(regs.esi);
    LoadOrdnanceInfo();
    ProcessOrdnance();

    gs_pVehicle->ProcessWeapons();
}

static RwFrame* FindNodeCB(RwFrame* frame, void* oi)
{
    constexpr char  frame_prefix[] = "OEx_", gun[] = "OEx_gun", rocket[] = "OEx_rocket", missile[] = "OEx_missile", flare[] = "OEx_flare";

    string_view name(GetFrameNodeName(frame));
    if (name.find(frame_prefix) == string_view::npos)   return RwFrameForAllChildren(frame, FindNodeCB, nullptr);

    COrdnanceInfo& info = *static_cast<COrdnanceInfo*>(oi);
    switch (name[4]) {
    case 'g':   info.guns.push_back(frame);     break;
    case 'r':   info.rockets.push_back(frame);  if (!info.flags.rockets_cyclic)  info.flags.rockets_cyclic = frame->child; break;
    case 'm':   info.missiles.push_back(frame); if (!info.flags.missiles_cyclic) info.flags.missiles_cyclic = frame->child; break;
    case 'f':   info.flares.push_back(frame);   break;
    default:    break;
    }

    return frame;
}

void COrdnanceSystem::LoadOrdnanceInfo()
{
    auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);
    if (oi.flags.processed) return;

    RwFrameForAllChildren((RwFrame*)gs_pVehicle->m_pRwObject->parent, FindNodeCB, &oi);

    oi.flags.processed = true;
    oi.flags.needProcess = oi.guns.size() + oi.rockets.size() + oi.missiles.size() + oi.flares.size();
}

int32_t __fastcall COrdnanceSystem::MyGetPlaneNumGuns(CVehicle* vehicle, int)
{
    //the second parameter is edx, and we need to protect it
    __asm push edx

    int32_t nums = ms_OrdnanceInfo.Get(vehicle).guns.size();
    if (!nums) {
        switch (vehicle->m_nModelIndex) {
        case MODEL_HUNTER:
        case MODEL_SEASPAR:
        case MODEL_RCBARON:
        case MODEL_MAVERICK:
        case MODEL_POLMAV:
        case MODEL_CARGOBOB:
            nums = 1;   break;
        case MODEL_RUSTLER:
            nums = 6;   break;
        case MODEL_HYDRA:
        case MODEL_TORNADO:
            nums = 2;   break;
        default:
            nums = 0;   break;
        }
    }

    __asm pop edx
    return nums;
}

void COrdnanceSystem::FirePlaneGuns()
{
    if (!gs_pVehicle->GetPlaneNumGuns())    return;
    if (CTimer::m_snTimeInMilliseconds <= gs_nGunFiringTime + GUN_RATE)  return;


    const auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);
    CWeapon weapon(WEAPON_MINIGUN, 5000);
    const CVector velocityOffset = gs_pVehicle->m_vecMoveSpeed * CTimer::ms_fTimeStep;

    for (size_t i = 0; i < oi.guns.size(); ++i) {
        CVector gunShellPos = velocityOffset + *RwMatrixGetPos(RwFrameGetLTM(oi.guns[i]));
        CVector planeGunPosMS(*RwMatrixGetPos(RwFrameGetMatrix(oi.guns[i])));//Model Space
        gs_pVehicle->DoPlaneGunFireFX(&weapon, planeGunPosMS, gunShellPos, i);
        weapon.FireInstantHit(gs_pVehicle, &gunShellPos, &gunShellPos, nullptr, nullptr, nullptr, false, false);
    }

    int32_t player_slot = CallAndReturn<int32_t, 0x564000, CEntity*>(gs_pVehicle);
    CPad* pad = CPad::GetPad(player_slot);
    pad->StartShake(240, 123u, 0);//magic number

    CallMethod <0x506F40, CAudioEngine*, eAudioEvents, eWeaponType, CEntity*>(&AudioEngine, eAudioEvents::AE_WEAPON_FIRE_PLANE, WEAPON_M4, gs_pVehicle);
    gs_nGunFiringTime = CTimer::m_snTimeInMilliseconds;
}

void COrdnanceSystem::FireUnguidedMissile()
{
    if (CTimer::m_snTimeInMilliseconds <= gs_nRocketFiringTime + ROCKET_RATE) return;

    const auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);
    CWeapon weapon(WEAPON_RLAUNCHER, 5000);

    auto dot = [](const CVector& left, const CVector& right)->float {return left.z * right.z + left.y * right.y + left.x * right.x;};

    if (oi.flags.rockets_cyclic) {
        static char index = -1;
        const CVector rocket_pos = *RwMatrixGetPos(RwFrameGetLTM(oi.rockets[++index %= oi.rockets.size()])); //don't worry about div 0
        CVector origin = rocket_pos + gs_pVehicle->GetMatrix()->GetForward()
            * (max(0.f, dot(gs_pVehicle->GetMatrix()->GetForward(), gs_pVehicle->m_vecMoveSpeed)) * CTimer::ms_fTimeStep);  //fuck max of windows
        weapon.FireProjectile(gs_pVehicle, &origin, nullptr, nullptr, 0.f);
    }
    else {
        for (size_t i = 0; i < oi.rockets.size(); i++) {
            const CVector rocket_pos = *RwMatrixGetPos(RwFrameGetLTM(oi.rockets[i]));
            CVector origin = rocket_pos + gs_pVehicle->GetMatrix()->GetForward()
                * (max(0.f, dot(gs_pVehicle->GetMatrix()->GetForward(), gs_pVehicle->m_vecMoveSpeed)) * CTimer::ms_fTimeStep);  //fuck max of windows
            weapon.FireProjectile(gs_pVehicle, &origin, nullptr, nullptr, 0.f);
        }
    }


    int32_t player_slot = CallAndReturn<int32_t, 0x564000, CEntity*>(gs_pVehicle);
    CPad* pad = CPad::GetPad(player_slot);
    pad->StartShake(240, 160u, 0);//magic number too

    gs_nRocketFiringTime = CTimer::m_snTimeInMilliseconds;
}

void COrdnanceSystem::ScanAndMarkTargetForHeatSeekingMissile(unsigned int& targetLockStartTime, CEntity*& targetEntity)
{
    if (!targetLockStartTime)    targetLockStartTime = CTimer::m_snTimeInMilliseconds;
    CVehicle* target = (CVehicle*)gs_pVehicle->ScanAndMarkTargetForHeatSeekingMissile(targetEntity);
    if (!target || target != targetEntity)  targetLockStartTime = CTimer::m_snTimeInMilliseconds;
    unsigned int passed_time = CTimer::m_snTimeInMilliseconds - targetLockStartTime;
    gCrossHair[0].m_nTimeWhenToDeactivate = 0;
    gCrossHair[0].m_color.r = -1;
    if (passed_time <= 1500) {
        gCrossHair[0].m_color.g = -1;
        gCrossHair[0].m_color.b = -1;
        gCrossHair[0].m_fRotation = 0.0;
    }
    else {
        gCrossHair[0].m_color.g = 0;
        gCrossHair[0].m_color.b = 0;
        gCrossHair[0].m_fRotation = 1.0;
    }
    targetEntity = target;
}

void COrdnanceSystem::FireHeatSeakingMissile(unsigned int& targetLockStartTime, CEntity*& targetEntity)
{
    if (CVehicle* target = nullptr;
        CWeaponEffects::IsLockedOn(0)
        && targetLockStartTime
        && (target = (CVehicle*)gs_pVehicle->ScanAndMarkTargetForHeatSeekingMissile(targetEntity))
        && target == targetEntity
        && (CTimer::m_snTimeInMilliseconds - targetLockStartTime) > 1500u) {
        gCrossHair[0].m_color.r = -1;
        gCrossHair[0].m_color.g = 0;
        gCrossHair[0].m_color.b = 0;
        gCrossHair[0].m_fRotation = 1.0;
        gCrossHair[0].m_nTimeWhenToDeactivate = 0;
        FireHeatSeakingMissile(target);
    }
    else {
        if (targetLockStartTime && ms_OrdnanceInfo.Get(gs_pVehicle).rockets.size())
            FireUnguidedMissile();
    }
}

void COrdnanceSystem::FireHeatSeakingMissile(CEntity* target)
{
    if (CTimer::m_snTimeInMilliseconds <= gs_nMissileFiringTime + MISSILE_RATE)    return;

    const auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);

    auto dot = [](const CVector& left, const CVector& right)->float {return left.z * right.z + left.y * right.y + left.x * right.x;};

    if (oi.flags.missiles_cyclic) {
        static char index = -1;
        const CVector missile_pos = *RwMatrixGetPos(RwFrameGetLTM(oi.missiles[++index %= oi.missiles.size()])); //don't worry about div 0
        CVector origin = missile_pos + gs_pVehicle->GetMatrix()->GetForward()
            * (max(0.f, dot(gs_pVehicle->GetMatrix()->GetForward(), gs_pVehicle->m_vecMoveSpeed)) * CTimer::ms_fTimeStep);  //fuck max of windows
        CProjectileInfo::AddProjectile(gs_pVehicle, WEAPON_ROCKET_HS, origin, 0.f, nullptr, target);
    }
    else {
        // for (size_t i = 0; i < oi.rockets.size(); i++) {
        const CVector missile_pos = *RwMatrixGetPos(RwFrameGetLTM(oi.missiles.front()));
        CVector origin = missile_pos + gs_pVehicle->GetMatrix()->GetForward()
            * (max(0.f, dot(gs_pVehicle->GetMatrix()->GetForward(), gs_pVehicle->m_vecMoveSpeed)) * CTimer::ms_fTimeStep);  //fuck max of windows
        CProjectileInfo::AddProjectile(gs_pVehicle, WEAPON_ROCKET_HS, origin, 0.f, nullptr, target);
        // }
    }

    int32_t player_slot = CallAndReturn<int32_t, 0x564000, CEntity*>(gs_pVehicle);
    CPad* pad = CPad::GetPad(player_slot);
    pad->StartShake(240, 160u, 0);//magic number too

    gs_nMissileFiringTime = CTimer::m_snTimeInMilliseconds;
}

void COrdnanceSystem::FireFlares()
{
    const auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);

    for (const auto& flare : oi.flares) {
        RwMatrix* matrix = RwFrameGetLTM(flare);
        CProjectileInfo::AddProjectile(gs_pVehicle, WEAPON_FLARE, matrix->pos, 0.f, nullptr, nullptr);
    }
    gs_nFlareFiringTime = CTimer::m_snTimeInMilliseconds;
}

void COrdnanceSystem::ProcessOrdnance()
{
    const auto& oi = ms_OrdnanceInfo.Get(gs_pVehicle);
    if (!oi.flags.needProcess || gs_pVehicle->m_nStatus) return;

    int32_t player_slot = CallAndReturn<int32_t, 0x564000, CEntity*>(gs_pVehicle);
    CPad* pad = CPad::GetPad(player_slot);

    if (gs_pVehicle->m_nVehicleSubClass == VEHICLE_HELI) {
        if (oi.missiles.size()) {
            CWeaponEffects::ClearCrossHairImmediately(0);
            if (pad->CarGunJustDown() == 2) {
                FireHeatSeakingMissile(gs_nTargetLockStartTime, gs_pTargetEntity);
            }
            else if (CallMethodAndReturn<int8_t, 0x5406B0, CPad*>(pad)) {//pad->TargetJustDown()
                gs_nTargetLockStartTime = CTimer::m_snTimeInMilliseconds;
                gs_pTargetEntity = nullptr;
            }
            else if (pad->GetTarget()) {
                ScanAndMarkTargetForHeatSeekingMissile(gs_nTargetLockStartTime, gs_pTargetEntity);
            }
            else {
                gs_nTargetLockStartTime = NULL;
                gs_pTargetEntity = nullptr;
            }
        }
        if (oi.rockets.size() && pad->CarGunJustDown() == 1 && !gs_nTargetLockStartTime) {
            FireUnguidedMissile();
        }

        if (oi.guns.size() && pad->GetCarGunFired() == 2 && !gs_nTargetLockStartTime) {
            FirePlaneGuns();
        }
        if (oi.flares.size() && pad->HornJustDown()) {
            FireFlares();
        }
    }
    else {
        if (oi.missiles.size()) {
            CWeaponEffects::ClearCrossHairImmediately(0);
            if (pad->CarGunJustDown() == 2) {
                FireHeatSeakingMissile(gs_nTargetLockStartTime, gs_pTargetEntity);
            }
            else if (CallMethodAndReturn<int8_t, 0x5406B0, CPad*>(pad)) {//pad->TargetJustDown()
                gs_nTargetLockStartTime = CTimer::m_snTimeInMilliseconds;
                gs_pTargetEntity = nullptr;
            }
            else if (pad->GetTarget()) {
                ScanAndMarkTargetForHeatSeekingMissile(gs_nTargetLockStartTime, gs_pTargetEntity);
            }
            else {
                gs_nTargetLockStartTime = NULL;
                gs_pTargetEntity = nullptr;
            }
        }
        if (oi.rockets.size() && pad->CarGunJustDown() == 2 && !gs_nTargetLockStartTime) {
            FireUnguidedMissile();
        }
        if (oi.guns.size() && pad->GetHorn()) {
            FirePlaneGuns();
        }
        if (oi.flares.size() && pad->CarGunJustDown() == 1) {
            FireFlares();
        }
    }
}
