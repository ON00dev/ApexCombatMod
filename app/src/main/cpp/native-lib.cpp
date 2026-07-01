#include "And64InlineHook.hpp"
#include <cstdint>
#include <jni.h>
#include <string>
#include <vector>
#include <sstream>
#include <fstream>
#include <unistd.h>
#include <sys/mman.h>
#include <android/log.h>
#include <dlfcn.h>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>
#include <time.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TAG "ApexCombatMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// 5. Aimbot / FOV
uintptr_t OFFSET_MISSILE_GET_LOCK_DISTANCE = 0x331E1CC; // MissileProperty.get_LockDistance
uintptr_t OFFSET_MISSILE_GET_MAX_ROTATE_ANGLE = 0x332D964; // MissileProperty.get_MaxRotateAngle
uintptr_t OFFSET_MISSILE_GET_TRACE_ABILITY = 0x332DAA8; // MissileProperty.get_TraceAbility

// =============================================================================================
// OFFSETS ATUALIZADOS (V22 - Hooked Hit Kill)
// =============================================================================================

// 1. God Mode & One Hit Kill
uintptr_t OFFSET_APPLY_DAMAGE = 0x33203BC; // PlayerPlaneAction.ApplyDamage (Offline God Mode)
uintptr_t OFFSET_UNIT_APPLY_DAMAGE = 0x33204E8; // UnitActionBase.ApplyDamage (Offline God Mode)
uintptr_t OFFSET_GET_GOD_MODE = 0x3324480; // UnitActionBase.get_GodMode (Retornar 1)
uintptr_t OFFSET_GET_HP_PROGRESS = 0x332A034; // UnitActionBase.GetHpProgress (Retornar 1.0)
uintptr_t OFFSET_MODIFY_DAMAGE = 0x332022C; // PlayerPlaneAction.ModifyDamage (Retornar 0.0)

// 1.1 Hit Kill Aux (Patches)
uintptr_t OFFSET_WEAPON_DAMAGE = 0x332D0F4; // MissileProperty.get_Damage
uintptr_t OFFSET_CANNON_DAMAGE = 0x332B290; // CannonProperty.get_Damage
uintptr_t OFFSET_CRITICAL_DAMAGE_RATE = 0x341179C; // PlaneCommonAttrCfgData.get_CriticalStrike
uintptr_t OFFSET_CRITICAL_PROB = 0x332DBEC; // MissileProperty.get_X2DamageProbability
uintptr_t OFFSET_BULLET_GET_DAMAGE = 0x30E9588; // BulletMove.GetDamage (Return 99999.0)
uintptr_t OFFSET_MISSILE_GET_DAMAGE = 0x33B5AE4; // MissileTrace.GetDamage (Signature with out float critMultiplier)
uintptr_t OFFSET_MISSILE_GET_RELOAD_TIME = 0x3322AD0; // MissileProperty.get_ReloadTime
uintptr_t OFFSET_CANNON_GET_COLD_TIME = 0x331FCA4; // CannonProperty.get_ColdTime (Verify if correct)

// 2. Hit Kill (Hooked)
uintptr_t OFFSET_UNITMANAGER_UPDATE = 0x2DFF2FC;
uintptr_t OFFSET_UNITMANAGER_GET_ISM_YSELF = 0x2DFEBE4;
uintptr_t OFFSET_UNITMANAGER_GET_IS_CURRENT_PLAYER = 0x2DFED34; // UnitManager.get_IsCurrentPlayer
uintptr_t OFFSET_UNITMANAGER_GET_HASHCODE = 0x2DFE14C;
uintptr_t OFFSET_UNITMANAGER_GET_IS_INVINCIBLE = 0x2DFE864; // UnitManager.get_IsInvincible

uintptr_t OFFSET_UNITACTIONBASE_APPLYDAMAGE = 0x33204E8;
uintptr_t OFFSET_CLOUDCONTAINER_APPLYDAMAGEBYLUA = 0x3469380; // CloudContainer.ApplyDamageByLua
uintptr_t OFFSET_PHOTON_PLUGIN_EVENTCALL = 0x2EBDBA8; // PhotonPlugin.EventCall
uintptr_t OFFSET_PHOTON_PLUGIN_APPLYDAMAGE = 0x2EBF93C; // PhotonPlugin.ApplyDamage (Dictionary)
uintptr_t OFFSET_PHOTON_CLIENT_RAISEEVENT = 0x2E4C7EC; // PhotonClient.RaiseEvent

// 3. Energia
uintptr_t OFFSET_REDUCE_ENERGY = 0x331E648; // PlayerPlaneAction.ReduceEnergy
uintptr_t OFFSET_GET_ENERGY_PROGRESS = 0x331E268; // PlayerPlaneAction.GetEnergyProgress (Float 1.0)
uintptr_t OFFSET_HAS_ENERGY = 0x331E4EC; // PlayerPlaneAction.HasEnergy (True)
uintptr_t OFFSET_ENOUGH_ENERGY_SPECIAL = 0x3325558; // PlayerPlaneAction.EnoughEnergyToSpecialMove (True)
uintptr_t OFFSET_ENOUGH_ENERGY_CLIMB = 0x33251C0; // PlayerPlaneAction.EnoughEnergyToClimbout (True)
uintptr_t OFFSET_ENOUGH_ENERGY_BACK = 0x3325410; // PlayerPlaneAction.EnoughEnergyToBackRoll (True)
uintptr_t OFFSET_ENOUGH_ENERGY_HORI = 0x33254B4; // PlayerPlaneAction.EnoughEnergyToHoriRoll (True)
uintptr_t OFFSET_ENOUGH_ENERGY_NOT_SPECIAL = 0x3325264; // PlayerPlaneAction.EnoughEnergyAndNotInSpecialMove (True)

// 3.1 Velocidade do Aviao
uintptr_t OFFSET_PLAYERPLANEACTION_UPDATE = 0x331BFB0; // PlayerPlaneAction.Update
uintptr_t OFFSET_PLAYERPLANEACTION_UPDATE_FLYCONTROLLER_PARAMS = 0x331D298; // PlayerPlaneAction.UpdateFlyControllerParams
uintptr_t OFFSET_PLAYERPLANEACTION_SETUP_FLYCONTROLLER = 0x331DC64; // PlayerPlaneAction.SetUpFlyController
uintptr_t OFFSET_PLAYERPLANEACTION_RELOAD_MISSILE = 0x331CD54; // PlayerPlaneAction.ReloadMissile
uintptr_t OFFSET_PLAYERPLANEACTION_REFRESH_MISSILE_ATTR_VALUE = 0x331E140; // PlayerPlaneAction.RefreshMissileAttrValue
uintptr_t OFFSET_PLANEPROPERTY_GET_MAX_SPEED = 0x332F004; // PlaneProperty.get_MaxSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_MAX_SPEED = 0x332F0A0; // PlaneProperty.set_MaxSpeed
uintptr_t OFFSET_PLANEPROPERTY_GET_NORMAL_SPEED = 0x332F150; // PlaneProperty.get_NormalSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_NORMAL_SPEED = 0x332F1EC; // PlaneProperty.set_NormalSpeed
uintptr_t OFFSET_PLANEPROPERTY_GET_MIN_SPEED = 0x332F29C; // PlaneProperty.get_MinSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_MIN_SPEED = 0x332F338; // PlaneProperty.set_MinSpeed

// 3. Munição Infinita
uintptr_t OFFSET_DO_CONSUME_WEAPON = 0x331FAA4; // PlayerPlaneAction.DoConsumeWeapon (Block)
uintptr_t OFFSET_CANNON_GET_COUNT = 0x331FB58; // CannonProperty.get_Count (Return 999)
uintptr_t OFFSET_MISSILE_GET_COUNT = 0x3321F94; // MissileProperty.get_Count (Return 999)
uintptr_t OFFSET_IS_MISSILE_READY = 0x3322C14; // PlayerPlaneAction.get_IsMissileReady (True)
uintptr_t OFFSET_GET_AIR_MISSILE_TRANSMIT_CNT_ONCE = 0x3321F18; // PlayerPlaneAction.get_AirMissileTransmitCntOnce
uintptr_t OFFSET_GET_AIR_MISSILE_CAN_LOCK_CNT_ONCE = 0x3323804; // PlayerPlaneAction.get_AirMissileCanLockCntOnce
uintptr_t OFFSET_CHECK_RELOAD_AIR_MISSILE_IDX = 0x3322C98; // PlayerPlaneAction.CheckReloadAirMissileIdx (Force Return 0, *isLeft=true)
uintptr_t OFFSET_INTERNAL_FIRE_MISSILE = 0x3323300; // PlayerPlaneAction.InternalFireMissile
uintptr_t OFFSET_INTERNAL_FIRE_MISSILE_WITH_AIR = 0x332202C; // PlayerPlaneAction.InternalFireMissileWithAirMissile

// 4. Auto Dodge / Ignore Hit
uintptr_t OFFSET_MISSILE_TRACE_CAN_HIT = 0x33B52D8; // MissileTrace.<OnTriggerEnter>g__CanHit|49_1
uintptr_t OFFSET_MISSILE_TRACE_APPLY_DAMAGE = 0x33B53B8; // MissileTrace.ApplyDamage
uintptr_t OFFSET_BULLET_MOVE_CAN_HIT = 0x30E8CB8; // BulletMove.CanHit

// 5. Moedas / Currency
uintptr_t OFFSET_GET_GOLD = 0x3499920; // UserProfile.get_Gold
uintptr_t OFFSET_GET_DIAMOND = 0x34999B4; // UserProfile.get_Diamond

// =============================================================================================

uintptr_t libIl2CppBase = 0;
std::mutex patchMutex;

// --- HOOKS ---
typedef bool (*Func_GetIsMyself)(void* _this);
typedef bool (*Func_GetIsCurrentPlayer)(void* _this);
typedef int (*Func_GetHashCode)(void* _this);
struct Vector3 {
    float x;
    float y;
    float z;
};

struct Quaternion {
    float x;
    float y;
    float z;
    float w;
};

typedef void (*Func_UnitManagerUpdate)(void* _this);
typedef bool (*Func_GetIsMyself)(void* _this);
typedef bool (*Func_GetIsInvincible)(void* _this);

Func_GetIsMyself get_IsMyself = nullptr;
typedef void (*Func_ApplyDamage)(void* _this, int attackerHashCode, float damage, void* damageSource, float* damageReduce);
typedef void (*Func_ApplyDamageByLua)(void* _this, int hashCode, uint8_t damageSource, int weaponId, float damage, int attackerHashCode, float clientTime, int fireId);
typedef void (*Func_PhotonPlugin_ApplyDamage)(void* _this, void* dict); // Dictionary<byte, object>
typedef void (*Func_PhotonClient_RaiseEvent)(uint8_t eventCode, void* eventContent, bool sendReliable, void* options);
typedef int (*Func_CheckReloadAirMissileIdx)(void* _this, bool* isLeft);
typedef void (*Func_InternalFireMissile)(void* _this, int missileType, void* missileProperty, void* missileTarget, bool isLastMissile, int missileIdx);
typedef void (*Func_InternalFireMissileWithAirMissile)(void* _this, void* missileTargetList, bool isLastMissile);
typedef bool (*Func_GetIsMissileReady)(void* _this);
typedef int (*Func_IntGetter)(void* _this);
typedef void (*Func_VoidInstance)(void* _this);
typedef float (*Func_FloatGetter)(void* _this);
typedef void (*Func_FloatSetter)(void* _this, float value);
typedef float (*Func_MissileTraceGetDamage)(void* _this, Vector3 missileForward, Vector3 targetForward, void* target, float* critMultiplier);

typedef float (*Func_GetReloadTime)(void* _this);
typedef float (*Func_GetColdTime)(void* _this);
typedef bool (*Func_MissileCanHit)(void* _this, void* defender);
typedef void (*Func_MissileTraceApplyDamage)(void* _this, void* targetUnitManager);
typedef void* (*Func_GetComponent)(void* _this, void* type);
Func_GetIsCurrentPlayer get_IsCurrentPlayer = nullptr;
Func_GetHashCode get_HashCode = nullptr;
Func_UnitManagerUpdate orig_UnitManager_Update = nullptr;
Func_ApplyDamage orig_ApplyDamage = nullptr;
Func_ApplyDamageByLua orig_ApplyDamageByLua = nullptr;
Func_PhotonPlugin_ApplyDamage orig_PhotonPlugin_ApplyDamage = nullptr;
Func_PhotonClient_RaiseEvent orig_PhotonClient_RaiseEvent = nullptr;
Func_CheckReloadAirMissileIdx orig_CheckReloadAirMissileIdx = nullptr;
Func_InternalFireMissile orig_InternalFireMissile = nullptr;
Func_InternalFireMissileWithAirMissile orig_InternalFireMissileWithAirMissile = nullptr;
Func_GetIsMissileReady orig_GetIsMissileReady = nullptr;
Func_IntGetter orig_GetAirMissileTransmitCntOnce = nullptr;
Func_IntGetter orig_GetAirMissileCanLockCntOnce = nullptr;
Func_MissileTraceGetDamage orig_MissileTrace_GetDamage = nullptr;
Func_GetReloadTime orig_Missile_GetReloadTime = nullptr;
Func_GetColdTime orig_Cannon_GetColdTime = nullptr;
Func_MissileCanHit orig_MissileTrace_CanHit = nullptr;
Func_MissileCanHit orig_BulletMove_CanHit = nullptr;
Func_MissileTraceApplyDamage orig_MissileTrace_ApplyDamage = nullptr;
Func_GetIsInvincible orig_UnitManager_GetIsInvincible = nullptr;
Func_VoidInstance orig_PlayerPlaneAction_UpdateFlyControllerParams = nullptr;
Func_VoidInstance orig_PlayerPlaneAction_SetUpFlyController = nullptr;
Func_VoidInstance orig_PlayerPlaneAction_Update = nullptr;
Func_VoidInstance playerPlaneAction_ReloadMissile = nullptr;
Func_VoidInstance playerPlaneAction_RefreshMissileAttrValue = nullptr;
Func_FloatGetter planeProperty_GetMaxSpeed = nullptr;
Func_FloatSetter planeProperty_SetMaxSpeed = nullptr;
Func_FloatGetter planeProperty_GetNormalSpeed = nullptr;
Func_FloatSetter planeProperty_SetNormalSpeed = nullptr;
Func_FloatGetter planeProperty_GetMinSpeed = nullptr;
Func_FloatSetter planeProperty_SetMinSpeed = nullptr;
Func_FloatGetter missileProperty_GetDamage = nullptr;

// ... Variáveis de Controle ...
int g_MyHashCode = 0;
bool isHitKillEnabled = false;
std::atomic<bool> isRapidFireEnabled{false};
std::atomic<int> g_RapidFireToggleSerial{0};
int g_RapidFireHandledToggleSerial = 0;
bool isAutoDodgeEnabled = false;
bool isMissileFovEnabled = false;
bool g_MissileToggle = false;
int g_FakeMissileIdx = 1000;
bool g_PlaneSpeedHackEnabled = false;
int g_PlaneSpeedMultiplier = 2;
int g_RapidFireShotsThisFrame = 0;
int g_RapidFireProjectilesPerTrigger = 1;
int g_RapidFireMaxProjectilesPerFrame = 1;
int g_RapidFireFrameId = 0;
int g_RapidFireLastFireFrameId = -1;
int g_RapidFireShotsFrameId = -1;
int g_FakeMissileFireId = 1000;
constexpr uintptr_t OFFSET_PLANEACTIONBASE_PLANEPROPERTY_FIELD = 0xE0;
void* g_CurrentPlayerPlaneAction = nullptr;
constexpr uintptr_t OFFSET_UNITACTIONBASE_UNITMANAGER_FIELD = 0x50;
constexpr uintptr_t OFFSET_OFFENSIVE_LAST_CANNON_FIRE_ID_FIELD = 0xA8;
constexpr uintptr_t OFFSET_OFFENSIVE_LAST_MISSILE_FIRE_ID_FIELD = 0xAC;
constexpr uintptr_t OFFSET_OFFENSIVE_LAST_MISSILE_GR_FIRE_ID_FIELD = 0xB0;

constexpr uintptr_t OFFSET_MISSILETRACE_MISSILEPROPERTY_FIELD = 0x40;
constexpr uintptr_t OFFSET_MISSILETRACE_ATTACKER_HASHCODE_FIELD = 0x60;
constexpr uintptr_t OFFSET_MISSILETRACE_CAN_REPORT_DAMAGE_FIELD = 0x78;

constexpr uintptr_t OFFSET_PLANEACTIONBASE_LEFT_MISSILE_FIRE_TIME_FIELD = 0x100;
constexpr uintptr_t OFFSET_PLANEACTIONBASE_RIGHT_MISSILE_FIRE_TIME_FIELD = 0x104;
constexpr uintptr_t OFFSET_PLANEACTIONBASE_LEFT_MISSILE_GR_FIRE_TIME_FIELD = 0x108;
constexpr uintptr_t OFFSET_PLANEACTIONBASE_RIGHT_MISSILE_GR_FIRE_TIME_FIELD = 0x10C;

static inline void* get_UnitManager_FromAction(void* unitAction) {
    if (unitAction == nullptr) return nullptr;
    return *(void**)((uintptr_t)unitAction + OFFSET_UNITACTIONBASE_UNITMANAGER_FIELD);
}

static inline bool is_CurrentPlayerPlaneAction(void* planeAction) {
    if (planeAction == nullptr) return false;
    if (g_MyHashCode == 0 || get_HashCode == nullptr) return false;
    void* unitManager = get_UnitManager_FromAction(planeAction);
    if (unitManager == nullptr) return false;
    return get_HashCode(unitManager) == g_MyHashCode;
}

static inline void reset_MissileFireTimes(void* planeAction) {
    if (planeAction == nullptr) return;
    *(float*)((uintptr_t)planeAction + OFFSET_PLANEACTIONBASE_LEFT_MISSILE_FIRE_TIME_FIELD) = 0.0f;
    *(float*)((uintptr_t)planeAction + OFFSET_PLANEACTIONBASE_RIGHT_MISSILE_FIRE_TIME_FIELD) = 0.0f;
    *(float*)((uintptr_t)planeAction + OFFSET_PLANEACTIONBASE_LEFT_MISSILE_GR_FIRE_TIME_FIELD) = 0.0f;
    *(float*)((uintptr_t)planeAction + OFFSET_PLANEACTIONBASE_RIGHT_MISSILE_GR_FIRE_TIME_FIELD) = 0.0f;
}

static inline void reset_OffensiveFireIds(void* planeAction) {
    if (planeAction == nullptr) return;
    *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_CANNON_FIRE_ID_FIELD) = 0;
    *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_MISSILE_FIRE_ID_FIELD) = 0;
    *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_MISSILE_GR_FIRE_ID_FIELD) = 0;
}

static inline void bump_MissileFireIds_ForPlayer(void* planeAction) {
    if (planeAction == nullptr) return;
    int last = *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_MISSILE_FIRE_ID_FIELD);
    if (last < g_FakeMissileFireId) last = g_FakeMissileFireId;
    last++;
    *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_MISSILE_FIRE_ID_FIELD) = last;
    *(int*)((uintptr_t)planeAction + OFFSET_OFFENSIVE_LAST_MISSILE_GR_FIRE_ID_FIELD) = last;
    g_FakeMissileFireId = last;
}

static inline void refresh_RapidFireFrameState() {
    if (g_RapidFireShotsFrameId != g_RapidFireFrameId) {
        g_RapidFireShotsFrameId = g_RapidFireFrameId;
        g_RapidFireShotsThisFrame = 0;
    }
}

struct PlaneSpeedBackup {
    float maxSpeed;
    float normalSpeed;
    float minSpeed;
    bool captured;
};

std::map<void*, PlaneSpeedBackup> g_PlaneSpeedBackups;

// ... (previous hooks) ...

void hook_InternalFireMissile(void* _this, int missileType, void* missileProperty, void* missileTarget, bool isLastMissile, int missileIdx) {
    if (orig_InternalFireMissile) {
        if (!isRapidFireEnabled.load(std::memory_order_relaxed) || !is_CurrentPlayerPlaneAction(_this)) {
            orig_InternalFireMissile(_this, missileType, missileProperty, missileTarget, isLastMissile, missileIdx);
            return;
        }

        refresh_RapidFireFrameState();

        if (g_RapidFireLastFireFrameId == g_RapidFireFrameId) return;

        int remainingThisFrame = g_RapidFireMaxProjectilesPerFrame - g_RapidFireShotsThisFrame;
        if (remainingThisFrame <= 0) return;

        int spawnCount = g_RapidFireProjectilesPerTrigger;
        if (spawnCount < 1) spawnCount = 1;
        if (spawnCount > remainingThisFrame) spawnCount = remainingThisFrame;

        for (int i = 0; i < spawnCount; i++) {
            int idx = missileIdx;
            if (idx < g_FakeMissileIdx) {
                idx = g_FakeMissileIdx++;
            } else {
                g_FakeMissileIdx = idx + 1;
            }
            bool last = (i == (spawnCount - 1)) ? isLastMissile : false;
            bump_MissileFireIds_ForPlayer(_this);
            orig_InternalFireMissile(_this, missileType, missileProperty, missileTarget, last, idx);
        }

        g_RapidFireShotsThisFrame += spawnCount;
        g_RapidFireLastFireFrameId = g_RapidFireFrameId;
    }
}

int hook_CheckReloadAirMissileIdx(void* _this, bool* isLeft) {
    if (isRapidFireEnabled.load(std::memory_order_relaxed) && is_CurrentPlayerPlaneAction(_this)) {
        bool localIsLeft = false;
        bool* outIsLeft = (isLeft != nullptr) ? isLeft : &localIsLeft;

        int idx = -1;
        if (orig_CheckReloadAirMissileIdx) {
            idx = orig_CheckReloadAirMissileIdx(_this, outIsLeft);
        }

        if (idx < 0) {
            idx = g_FakeMissileIdx++;
        } else {
            if (g_FakeMissileIdx <= idx) g_FakeMissileIdx = idx + 1;
        }

        return idx;
    }
    if (orig_CheckReloadAirMissileIdx) return orig_CheckReloadAirMissileIdx(_this, isLeft);
    return -1;
}

void hook_InternalFireMissileWithAirMissile(void* _this, void* missileTargetList, bool isLastMissile) {
    if (orig_InternalFireMissileWithAirMissile) {
        if (!isRapidFireEnabled.load(std::memory_order_relaxed) || !is_CurrentPlayerPlaneAction(_this)) {
            orig_InternalFireMissileWithAirMissile(_this, missileTargetList, isLastMissile);
            return;
        }

        refresh_RapidFireFrameState();

        if (g_RapidFireLastFireFrameId == g_RapidFireFrameId) return;

        int remainingThisFrame = g_RapidFireMaxProjectilesPerFrame - g_RapidFireShotsThisFrame;
        if (remainingThisFrame <= 0) return;

        int spawnCount = g_RapidFireProjectilesPerTrigger;
        if (spawnCount < 1) spawnCount = 1;
        if (spawnCount > remainingThisFrame) spawnCount = remainingThisFrame;

        for (int i = 0; i < spawnCount; i++) {
            bool last = (i == (spawnCount - 1)) ? isLastMissile : false;
            bump_MissileFireIds_ForPlayer(_this);
            orig_InternalFireMissileWithAirMissile(_this, missileTargetList, last);
        }

        g_RapidFireShotsThisFrame += spawnCount;
        g_RapidFireLastFireFrameId = g_RapidFireFrameId;
    }
}

bool hook_GetIsMissileReady(void* _this) {
    if (isRapidFireEnabled.load(std::memory_order_relaxed) && is_CurrentPlayerPlaneAction(_this)) return true;
    if (orig_GetIsMissileReady) return orig_GetIsMissileReady(_this);
    return true;
}

int hook_GetAirMissileTransmitCntOnce(void* _this) {
    if (isRapidFireEnabled.load(std::memory_order_relaxed) && is_CurrentPlayerPlaneAction(_this)) return 999;
    if (orig_GetAirMissileTransmitCntOnce) return orig_GetAirMissileTransmitCntOnce(_this);
    return 2;
}

int hook_GetAirMissileCanLockCntOnce(void* _this) {
    if (isRapidFireEnabled.load(std::memory_order_relaxed) && is_CurrentPlayerPlaneAction(_this)) return 999;
    if (orig_GetAirMissileCanLockCntOnce) return orig_GetAirMissileCanLockCntOnce(_this);
    return 2;
}

float hook_MissileTrace_GetDamage(void* _this, Vector3 missileForward, Vector3 targetForward, void* target, float* critMultiplier) {
    if (orig_MissileTrace_GetDamage) {
        return orig_MissileTrace_GetDamage(_this, missileForward, targetForward, target, critMultiplier);
    }
    return 0.0f;
}

void hook_UnitManager_Update(void* _this) {
    if (get_HashCode) {
        bool isMe = false;
        if (get_IsMyself && get_IsMyself(_this)) isMe = true;
        else if (get_IsCurrentPlayer && get_IsCurrentPlayer(_this)) isMe = true;
        
        if (isMe) {
            g_MyHashCode = get_HashCode(_this);
        }
    }
    if (orig_UnitManager_Update) orig_UnitManager_Update(_this);
}

void hook_PlayerPlaneAction_Update(void* _this) {
    if (is_CurrentPlayerPlaneAction(_this)) {
        int currentSerial = g_RapidFireToggleSerial.load(std::memory_order_relaxed);
        if (g_RapidFireHandledToggleSerial != currentSerial) {
            g_RapidFireHandledToggleSerial = currentSerial;
            reset_OffensiveFireIds(_this);
            g_FakeMissileIdx = 1000;
            g_FakeMissileFireId = 1000;
        }

        if (isRapidFireEnabled.load(std::memory_order_relaxed)) {
            g_RapidFireFrameId++;
            refresh_RapidFireFrameState();
            reset_MissileFireTimes(_this);
        }
    }

    if (orig_PlayerPlaneAction_Update) {
        orig_PlayerPlaneAction_Update(_this);
    }

    if (isRapidFireEnabled.load(std::memory_order_relaxed) && is_CurrentPlayerPlaneAction(_this)) {
        reset_MissileFireTimes(_this);
    }
}

bool hook_UnitManager_GetIsInvincible(void* _this) {
    if (isAutoDodgeEnabled && get_HashCode && g_MyHashCode != 0) {
        int hash = get_HashCode(_this);
        if (hash == g_MyHashCode) {
            return true; // Força Invencibilidade nativa do jogo (God Mode interno)
        }
    }
    if (orig_UnitManager_GetIsInvincible) return orig_UnitManager_GetIsInvincible(_this);
    return false;
}

float hook_Missile_GetReloadTime(void* _this) {
    float originalValue = 3.0f;
    if (orig_Missile_GetReloadTime) {
        originalValue = orig_Missile_GetReloadTime(_this);
    }
    if (!isRapidFireEnabled.load(std::memory_order_relaxed)) return originalValue;
    return 0.01f;
}

bool hook_MissileTrace_CanHit(void* _this, void* defender) {
    if (_this != nullptr && g_MyHashCode != 0) {
        int attackerHash = *(int*)((uintptr_t)_this + OFFSET_MISSILETRACE_ATTACKER_HASHCODE_FIELD);
        if (attackerHash == g_MyHashCode) {
        *(bool*)((uintptr_t)_this + OFFSET_MISSILETRACE_CAN_REPORT_DAMAGE_FIELD) = true;
        }
    }
    if (isAutoDodgeEnabled && defender != nullptr) {
        if (get_HashCode) {
            int targetHash = get_HashCode(defender);
            if (targetHash == g_MyHashCode && g_MyHashCode != 0) {
                // Auto Dodge: Se o míssil for me acertar, ele ignora (retorna falso).
                return false; 
            }
        }
    }
    if (orig_MissileTrace_CanHit) return orig_MissileTrace_CanHit(_this, defender);
    return true;
}

bool hook_BulletMove_CanHit(void* _this, void* defender) {
    if (isAutoDodgeEnabled && defender != nullptr) {
        if (get_HashCode) {
            int targetHash = get_HashCode(defender);
            if (targetHash == g_MyHashCode && g_MyHashCode != 0) {
                // Auto Dodge: Se a bala (canhão) for me acertar, ele ignora (retorna falso).
                return false; 
            }
        }
    }
    if (orig_BulletMove_CanHit) return orig_BulletMove_CanHit(_this, defender);
    return true;
}

void hook_MissileTrace_ApplyDamage(void* _this, void* targetUnitManager) {
    if (_this != nullptr && g_MyHashCode != 0) {
        int attackerHash = *(int*)((uintptr_t)_this + OFFSET_MISSILETRACE_ATTACKER_HASHCODE_FIELD);
        if (attackerHash == g_MyHashCode) {
            *(bool*)((uintptr_t)_this + OFFSET_MISSILETRACE_CAN_REPORT_DAMAGE_FIELD) = true;
        }
    }
    if (orig_MissileTrace_ApplyDamage) {
        orig_MissileTrace_ApplyDamage(_this, targetUnitManager);
    }
}

void hook_ApplyDamage(void* _this, int attackerHashCode, float damage, void* damageSource, float* damageReduce) {
    // 1. Hit Kill
    if (isHitKillEnabled && g_MyHashCode != 0 && attackerHashCode == g_MyHashCode) {
        damage = 999999.0f; 
    }
    // 2. God Mode Híbrido (Anula dano recebido)
    if (isAutoDodgeEnabled && g_MyHashCode != 0 && get_HashCode) {
        int targetHash = get_HashCode(_this); // O UnitManager recebendo o dano
        if (targetHash == g_MyHashCode) {
            damage = 0.0f; // Zera o dano se for contra nós
        }
    }
    if (orig_ApplyDamage) orig_ApplyDamage(_this, attackerHashCode, damage, damageSource, damageReduce);
}

void hook_ApplyDamageByLua(void* _this, int hashCode, uint8_t damageSource, int weaponId, float damage, int attackerHashCode, float clientTime, int fireId) {
    // 1. Hit Kill
    if (isHitKillEnabled && g_MyHashCode != 0 && attackerHashCode == g_MyHashCode) {
        damage = 999999.0f; // Força dano alto no script Lua
    }
    // 2. God Mode Híbrido (Anula dano recebido via LUA)
    if (isAutoDodgeEnabled && g_MyHashCode != 0 && hashCode == g_MyHashCode) {
        damage = 0.0f; 
    }
    if (orig_ApplyDamageByLua) {
        orig_ApplyDamageByLua(_this, hashCode, damageSource, weaponId, damage, attackerHashCode, clientTime, fireId);
    }
}

// Hook direto no envio da rede (Photon)
void hook_PhotonPlugin_ApplyDamage(void* _this, void* dict) {
    // Removido o loop for(20x) porque causa travamento no servidor (Flood)
    if (orig_PhotonPlugin_ApplyDamage) {
        orig_PhotonPlugin_ApplyDamage(_this, dict);
    }
}

void hook_PhotonClient_RaiseEvent(uint8_t eventCode, void* eventContent, bool sendReliable, void* options) {
    // Se for um evento de tiro/dano (precisaria saber o código exato, mas vamos spammar se for ativado)
    // Para não crashar, vamos apenas interceptar e deixar passar
    if (orig_PhotonClient_RaiseEvent) {
        orig_PhotonClient_RaiseEvent(eventCode, eventContent, sendReliable, options);
    }
}

static void* get_PlaneProperty_FromAction(void* playerPlaneAction) {
    if (playerPlaneAction == nullptr) return nullptr;
    return *(void**)((uintptr_t)playerPlaneAction + OFFSET_PLANEACTIONBASE_PLANEPROPERTY_FIELD);
}

static void apply_PlaneSpeed_ToProperty(void* planeProperty) {
    if (planeProperty == nullptr ||
        planeProperty_GetMaxSpeed == nullptr || planeProperty_SetMaxSpeed == nullptr ||
        planeProperty_GetNormalSpeed == nullptr || planeProperty_SetNormalSpeed == nullptr ||
        planeProperty_GetMinSpeed == nullptr || planeProperty_SetMinSpeed == nullptr) {
        return;
    }

    std::lock_guard<std::mutex> lock(patchMutex);
    auto& backup = g_PlaneSpeedBackups[planeProperty];
    if (!backup.captured) {
        backup.maxSpeed = planeProperty_GetMaxSpeed(planeProperty);
        backup.normalSpeed = planeProperty_GetNormalSpeed(planeProperty);
        backup.minSpeed = planeProperty_GetMinSpeed(planeProperty);
        backup.captured = true;
    }

    float maxSpeed = backup.maxSpeed;
    float normalSpeed = backup.normalSpeed;
    float minSpeed = backup.minSpeed;

    if (g_PlaneSpeedHackEnabled) {
        float multiplier = static_cast<float>(g_PlaneSpeedMultiplier);
        maxSpeed *= multiplier;
        normalSpeed *= multiplier;
        minSpeed *= multiplier;
    }

    planeProperty_SetMaxSpeed(planeProperty, maxSpeed);
    planeProperty_SetNormalSpeed(planeProperty, normalSpeed);
    planeProperty_SetMinSpeed(planeProperty, minSpeed);
}

static void apply_PlaneSpeed_ToAction(void* playerPlaneAction) {
    if (playerPlaneAction == nullptr) return;
    g_CurrentPlayerPlaneAction = playerPlaneAction;
    apply_PlaneSpeed_ToProperty(get_PlaneProperty_FromAction(playerPlaneAction));
}

void hook_PlayerPlaneAction_UpdateFlyControllerParams(void* _this) {
    if (_this != nullptr) {
        g_CurrentPlayerPlaneAction = _this;
    }
    if (isRapidFireEnabled && is_CurrentPlayerPlaneAction(_this)) {
        reset_MissileFireTimes(_this);
    }
    if (orig_PlayerPlaneAction_UpdateFlyControllerParams) {
        orig_PlayerPlaneAction_UpdateFlyControllerParams(_this);
    }
    apply_PlaneSpeed_ToAction(_this);
}

void hook_PlayerPlaneAction_SetUpFlyController(void* _this) {
    if (_this != nullptr) {
        g_CurrentPlayerPlaneAction = _this;
    }
    if (isRapidFireEnabled && is_CurrentPlayerPlaneAction(_this)) {
        g_RapidFireShotsThisFrame = 0;
    }
    if (orig_PlayerPlaneAction_SetUpFlyController) {
        orig_PlayerPlaneAction_SetUpFlyController(_this);
    }
    apply_PlaneSpeed_ToAction(_this);
}

void init_hooks(uintptr_t base) {
    get_IsMyself = (Func_GetIsMyself)(base + OFFSET_UNITMANAGER_GET_ISM_YSELF);
    get_IsCurrentPlayer = (Func_GetIsCurrentPlayer)(base + OFFSET_UNITMANAGER_GET_IS_CURRENT_PLAYER);
    get_HashCode = (Func_GetHashCode)(base + OFFSET_UNITMANAGER_GET_HASHCODE);
    planeProperty_GetMaxSpeed = (Func_FloatGetter)(base + OFFSET_PLANEPROPERTY_GET_MAX_SPEED);
    planeProperty_SetMaxSpeed = (Func_FloatSetter)(base + OFFSET_PLANEPROPERTY_SET_MAX_SPEED);
    planeProperty_GetNormalSpeed = (Func_FloatGetter)(base + OFFSET_PLANEPROPERTY_GET_NORMAL_SPEED);
    planeProperty_SetNormalSpeed = (Func_FloatSetter)(base + OFFSET_PLANEPROPERTY_SET_NORMAL_SPEED);
    planeProperty_GetMinSpeed = (Func_FloatGetter)(base + OFFSET_PLANEPROPERTY_GET_MIN_SPEED);
    planeProperty_SetMinSpeed = (Func_FloatSetter)(base + OFFSET_PLANEPROPERTY_SET_MIN_SPEED);
    missileProperty_GetDamage = (Func_FloatGetter)(base + OFFSET_WEAPON_DAMAGE);
    playerPlaneAction_ReloadMissile = (Func_VoidInstance)(base + OFFSET_PLAYERPLANEACTION_RELOAD_MISSILE);
    playerPlaneAction_RefreshMissileAttrValue = (Func_VoidInstance)(base + OFFSET_PLAYERPLANEACTION_REFRESH_MISSILE_ATTR_VALUE);
    
    A64HookFunction((void*)(base + OFFSET_UNITMANAGER_UPDATE), (void*)hook_UnitManager_Update, (void**)&orig_UnitManager_Update);
    A64HookFunction((void*)(base + OFFSET_PLAYERPLANEACTION_UPDATE), (void*)hook_PlayerPlaneAction_Update, (void**)&orig_PlayerPlaneAction_Update);
    A64HookFunction((void*)(base + OFFSET_UNITACTIONBASE_APPLYDAMAGE), (void*)hook_ApplyDamage, (void**)&orig_ApplyDamage);
    A64HookFunction((void*)(base + OFFSET_CLOUDCONTAINER_APPLYDAMAGEBYLUA), (void*)hook_ApplyDamageByLua, (void**)&orig_ApplyDamageByLua);
    A64HookFunction((void*)(base + OFFSET_PHOTON_PLUGIN_APPLYDAMAGE), (void*)hook_PhotonPlugin_ApplyDamage, (void**)&orig_PhotonPlugin_ApplyDamage);
    A64HookFunction((void*)(base + OFFSET_CHECK_RELOAD_AIR_MISSILE_IDX), (void*)hook_CheckReloadAirMissileIdx, (void**)&orig_CheckReloadAirMissileIdx);
    A64HookFunction((void*)(base + OFFSET_INTERNAL_FIRE_MISSILE), (void*)hook_InternalFireMissile, (void**)&orig_InternalFireMissile);
    A64HookFunction((void*)(base + OFFSET_INTERNAL_FIRE_MISSILE_WITH_AIR), (void*)hook_InternalFireMissileWithAirMissile, (void**)&orig_InternalFireMissileWithAirMissile);
    A64HookFunction((void*)(base + OFFSET_IS_MISSILE_READY), (void*)hook_GetIsMissileReady, (void**)&orig_GetIsMissileReady);
    A64HookFunction((void*)(base + OFFSET_GET_AIR_MISSILE_TRANSMIT_CNT_ONCE), (void*)hook_GetAirMissileTransmitCntOnce, (void**)&orig_GetAirMissileTransmitCntOnce);
    A64HookFunction((void*)(base + OFFSET_GET_AIR_MISSILE_CAN_LOCK_CNT_ONCE), (void*)hook_GetAirMissileCanLockCntOnce, (void**)&orig_GetAirMissileCanLockCntOnce);
    A64HookFunction((void*)(base + OFFSET_MISSILE_GET_RELOAD_TIME), (void*)hook_Missile_GetReloadTime, (void**)&orig_Missile_GetReloadTime);
    A64HookFunction((void*)(base + OFFSET_MISSILE_GET_DAMAGE), (void*)hook_MissileTrace_GetDamage, (void**)&orig_MissileTrace_GetDamage);
    A64HookFunction((void*)(base + OFFSET_MISSILE_TRACE_CAN_HIT), (void*)hook_MissileTrace_CanHit, (void**)&orig_MissileTrace_CanHit);
    A64HookFunction((void*)(base + OFFSET_MISSILE_TRACE_APPLY_DAMAGE), (void*)hook_MissileTrace_ApplyDamage, (void**)&orig_MissileTrace_ApplyDamage);
    A64HookFunction((void*)(base + OFFSET_BULLET_MOVE_CAN_HIT), (void*)hook_BulletMove_CanHit, (void**)&orig_BulletMove_CanHit);
    A64HookFunction((void*)(base + OFFSET_UNITMANAGER_GET_IS_INVINCIBLE), (void*)hook_UnitManager_GetIsInvincible, (void**)&orig_UnitManager_GetIsInvincible);
    A64HookFunction((void*)(base + OFFSET_PLAYERPLANEACTION_UPDATE_FLYCONTROLLER_PARAMS), (void*)hook_PlayerPlaneAction_UpdateFlyControllerParams, (void**)&orig_PlayerPlaneAction_UpdateFlyControllerParams);
    A64HookFunction((void*)(base + OFFSET_PLAYERPLANEACTION_SETUP_FLYCONTROLLER), (void*)hook_PlayerPlaneAction_SetUpFlyController, (void**)&orig_PlayerPlaneAction_SetUpFlyController);
    
    LOGI("[MOD] Hooks Initialized");
}

// --- MEMORY PATCHES ---
class MemoryPatch {
public:
    uintptr_t offset;
    uintptr_t absoluteAddr;
    std::vector<uint8_t> originalBytes;
    bool isActive;
    const char* name;
    size_t patchSize;

    MemoryPatch(const char* patchName, uintptr_t offsetAddr) : name(patchName), offset(offsetAddr), absoluteAddr(0), isActive(false), patchSize(4) {}

    // Patch: RET (void)
    void Apply(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0xD65F03C0; // RET
        }, 4);
    }

    // Patch: RET TRUE (bool/int 1) -> MOV W0, #1; RET
    void ApplyTrue(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x52800020;     // MOV W0, #1
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    // Patch: RET FALSE (bool/int 0) -> MOV W0, #0; RET
    void ApplyFalse(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x52800000;     // MOV W0, #0
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    // Patch: RET FLOAT 1.0 (float) -> FMOV S0, #1.0; RET
    void ApplyFloat1(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x1E201000;     // FMOV S0, #1.0 (Little Endian: 00 10 20 1E)
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    // Patch: RET FLOAT 0.0 (float) -> FMOV S0, #0.0; RET
    void ApplyFloat0(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x1E200000;     // FMOV S0, #0.0 (Little Endian: 00 00 20 1E)
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    // Patch: RET FLOAT 99999.0 (0x47C34F80)
    void ApplyFloat99999(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x1C000040;       // LDR S0, .+8
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
            *(unsigned int*)(addr + 8) = 0x47C34F80; // 99999.0f
        }, 12);
    }

    // Patch: RET INT 999 -> MOV W0, #999; RET
    void ApplyInt999(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x52807CE0;     
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    // Patch: RET INT 9999 -> MOV W0, #9999; RET
    void ApplyInt9999(uintptr_t baseAddr) {
        ApplyPatch(baseAddr, [](uintptr_t addr) {
            *(unsigned int*)addr = 0x5284E1E0;     
            *(unsigned int*)(addr + 4) = 0xD65F03C0; // RET
        }, 8);
    }

    void Restore() {
        std::lock_guard<std::mutex> lock(patchMutex);
        if (!isActive || absoluteAddr == 0) return;

        memcpy((void*)absoluteAddr, originalBytes.data(), patchSize);
        __builtin___clear_cache((char*)absoluteAddr, (char*)absoluteAddr + patchSize);
        isActive = false;
        LOGI("[MOD] Patch RESTAURADO: %s", name);
    }

private:
    template<typename Func>
    void ApplyPatch(uintptr_t baseAddr, Func patchFunc, size_t size) {
        std::lock_guard<std::mutex> lock(patchMutex);
        if (isActive) return;
        if (offset == 0) {
            LOGE("[MOD] Erro: Offset 0x0 para %s", name);
            return;
        }

        absoluteAddr = baseAddr + offset;
        patchSize = size;
        
        originalBytes.resize(patchSize);
        memcpy(originalBytes.data(), (void*)absoluteAddr, patchSize);

        size_t page_size = sysconf(_SC_PAGESIZE);
        uintptr_t page_start = absoluteAddr & -page_size;
        mprotect((void*)page_start, page_size * 2, PROT_READ | PROT_WRITE | PROT_EXEC);

        patchFunc(absoluteAddr);

        __builtin___clear_cache((char*)absoluteAddr, (char*)absoluteAddr + patchSize);
        isActive = true;
        LOGI("[MOD] Patch APLICADO: %s", name);
    }
};

// --- Instâncias dos Patches ---

// 1. God Mode
MemoryPatch patchGodOffline("GodModeOffline", OFFSET_APPLY_DAMAGE);
MemoryPatch patchUnitApplyDamage("UnitApplyDamage", OFFSET_UNIT_APPLY_DAMAGE);
MemoryPatch patchModifyDamage("ModifyDamage", OFFSET_MODIFY_DAMAGE);
MemoryPatch patchGetGodMode("GetGodMode", OFFSET_GET_GOD_MODE);
MemoryPatch patchGetHpProgress("GetHpProgress", OFFSET_GET_HP_PROGRESS);
MemoryPatch patchCannonDamage("CannonDamage", OFFSET_CANNON_DAMAGE);
MemoryPatch patchMissileDamage("MissileDamage", OFFSET_WEAPON_DAMAGE);
MemoryPatch patchCriticalRate("CriticalRate", OFFSET_CRITICAL_DAMAGE_RATE);
MemoryPatch patchCriticalProb("CriticalProb", OFFSET_CRITICAL_PROB);
MemoryPatch patchBulletGetDamage("BulletGetDamage", OFFSET_BULLET_GET_DAMAGE);
MemoryPatch patchMissileGetDamage("MissileGetDamage", OFFSET_MISSILE_GET_DAMAGE);
MemoryPatch patchMissileReload("MissileReload", OFFSET_MISSILE_GET_RELOAD_TIME);
MemoryPatch patchCannonCold("CannonCold", OFFSET_CANNON_GET_COLD_TIME);

// 2. Energia
MemoryPatch patchEnergyReduce("EnergyReduce", OFFSET_REDUCE_ENERGY);
MemoryPatch patchEnergyProgress("EnergyProgress", OFFSET_GET_ENERGY_PROGRESS);
MemoryPatch patchEnergyHas("HasEnergy", OFFSET_HAS_ENERGY);
MemoryPatch patchEnergySpecial("EnergySpecial", OFFSET_ENOUGH_ENERGY_SPECIAL);
MemoryPatch patchEnergyClimb("EnergyClimb", OFFSET_ENOUGH_ENERGY_CLIMB);
MemoryPatch patchEnergyBack("EnergyBack", OFFSET_ENOUGH_ENERGY_BACK);
MemoryPatch patchEnergyHori("EnergyHori", OFFSET_ENOUGH_ENERGY_HORI);
MemoryPatch patchEnergyNotSpecial("EnergyNotSpecial", OFFSET_ENOUGH_ENERGY_NOT_SPECIAL);

// 3. Munição Infinita
MemoryPatch patchConsumeWeapon("ConsumeWeapon", OFFSET_DO_CONSUME_WEAPON);
MemoryPatch patchCannonCount("CannonCount", OFFSET_CANNON_GET_COUNT);
MemoryPatch patchMissileCount("MissileCount", OFFSET_MISSILE_GET_COUNT);
MemoryPatch patchMissileReady("MissileReady", OFFSET_IS_MISSILE_READY);

// 4. Aimbot / FOV
MemoryPatch patchAimbotDistance("AimbotDistance", OFFSET_MISSILE_GET_LOCK_DISTANCE);
MemoryPatch patchAimbotAngle("AimbotAngle", OFFSET_MISSILE_GET_MAX_ROTATE_ANGLE);
MemoryPatch patchAimbotTrace("AimbotTrace", OFFSET_MISSILE_GET_TRACE_ABILITY);


// --- JNI ---

uintptr_t get_libBase(const char* libName) {
    FILE* fp = fopen("/proc/self/maps", "rt");
    if (fp == nullptr) return 0;

    uintptr_t bestBase = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libName) == nullptr) continue;

        uintptr_t start = 0;
        uintptr_t fileOffset = 0;
        if (sscanf(line, "%lx-%*lx %*4s %lx %*5s %*lu %*s", &start, &fileOffset) == 2) {
            uintptr_t baseCandidate = start - fileOffset;
            if (bestBase == 0 || baseCandidate < bestBase) {
                bestBase = baseCandidate;
            }
        }
    }
    fclose(fp);
    return bestBase;
}

void hack_thread() {
    LOGI("[MOD] Thread iniciada. Aguardando libil2cpp.so...");
    int tries = 0;
    while (libIl2CppBase == 0 && tries < 60) {
        libIl2CppBase = get_libBase("libil2cpp.so");
        if (libIl2CppBase != 0) break;
        sleep(1);
        tries++;
    }
    if (libIl2CppBase != 0) {
        LOGI("[MOD] libil2cpp.so: %p", (void*)libIl2CppBase);
        init_hooks(libIl2CppBase);
    }
    else LOGE("[MOD] libil2cpp.so NAO encontrada!");
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_InitMod(JNIEnv *env, jclass type) {
    std::thread(hack_thread).detach();
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetGodMode(JNIEnv *env, jclass type, jboolean isEnabled) {
    if (libIl2CppBase == 0) return;
    if (isEnabled) {
        patchGodOffline.Apply(libIl2CppBase);
    } else {
        patchGodOffline.Restore();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetGodModeOnline(JNIEnv *env, jclass type, jboolean isEnabled) {
    // V33: Rapid Fire corrigido para afetar APENAS o jogador
    isRapidFireEnabled.store(isEnabled, std::memory_order_relaxed);
    if (libIl2CppBase == 0) return;
    g_RapidFireToggleSerial.fetch_add(1, std::memory_order_relaxed);
    
    if (isEnabled) {
        g_RapidFireShotsThisFrame = 0;
        g_RapidFireFrameId = 0;
        g_RapidFireLastFireFrameId = -1;
        g_RapidFireShotsFrameId = -1;
        g_FakeMissileIdx = 1000;
    } else {
        g_RapidFireShotsThisFrame = 0;
        g_RapidFireFrameId = 0;
        g_RapidFireLastFireFrameId = -1;
        g_RapidFireShotsFrameId = -1;
    }
    
    LOGI("[MOD] Rapid Fire: %s", isEnabled ? "ON" : "OFF");
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetAutoDodge(JNIEnv *env, jclass type, jboolean isEnabled) {
    isAutoDodgeEnabled = isEnabled;
    LOGI("[MOD] Auto Dodge (Missile & Cannon): %s", isEnabled ? "ON" : "OFF");
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetMissileFOV(JNIEnv *env, jclass type, jboolean isEnabled) {
    if (libIl2CppBase == 0) return;
    
    isMissileFovEnabled = isEnabled;
    if (isEnabled) {
        patchAimbotDistance.ApplyFloat99999(libIl2CppBase);
        patchAimbotAngle.ApplyFloat99999(libIl2CppBase);
        patchAimbotTrace.ApplyFloat99999(libIl2CppBase);
    } else {
        patchAimbotDistance.Restore();
        patchAimbotAngle.Restore();
        patchAimbotTrace.Restore();
    }
    LOGI("[MOD] Missile FOV: %s", isEnabled ? "ON" : "OFF");
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetInfiniteEnergy(JNIEnv *env, jclass type, jboolean isEnabled) {
    if (libIl2CppBase == 0) return;
    if (isEnabled) {
        patchEnergyReduce.Apply(libIl2CppBase);
        patchEnergyProgress.ApplyFloat1(libIl2CppBase);
        patchEnergyHas.ApplyTrue(libIl2CppBase);
        patchEnergySpecial.ApplyTrue(libIl2CppBase);
        patchEnergyClimb.ApplyTrue(libIl2CppBase);
        patchEnergyBack.ApplyTrue(libIl2CppBase);
        patchEnergyHori.ApplyTrue(libIl2CppBase);
        patchEnergyNotSpecial.ApplyTrue(libIl2CppBase);
    } else {
        patchEnergyReduce.Restore();
        patchEnergyProgress.Restore();
        patchEnergyHas.Restore();
        patchEnergySpecial.Restore();
        patchEnergyClimb.Restore();
        patchEnergyBack.Restore();
        patchEnergyHori.Restore();
        patchEnergyNotSpecial.Restore();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetPlaneSpeedHackEnabled(JNIEnv *env, jclass type, jboolean isEnabled) {
    g_PlaneSpeedHackEnabled = isEnabled;

    if (g_CurrentPlayerPlaneAction != nullptr) {
        apply_PlaneSpeed_ToAction(g_CurrentPlayerPlaneAction);
    }

    LOGI("[MOD] Plane Speed (%dx Battle): %s", g_PlaneSpeedMultiplier, isEnabled ? "ON" : "OFF");
}

extern "C" JNIEXPORT void JNICALL
Java_com_on00dev_apexcombatmod_Native_SetPlaneSpeedMultiplier(JNIEnv *env, jclass type, jint multiplier) {
    if (multiplier < 1) multiplier = 1;
    if (multiplier > 10) multiplier = 10;

    g_PlaneSpeedMultiplier = multiplier;

    if (g_CurrentPlayerPlaneAction != nullptr) {
        apply_PlaneSpeed_ToAction(g_CurrentPlayerPlaneAction);
    }

    LOGI("[MOD] Plane Speed multiplier set to: %dx", g_PlaneSpeedMultiplier);
}

// Funções Removidas: SetInfiniteAmmo e SetAimbot
