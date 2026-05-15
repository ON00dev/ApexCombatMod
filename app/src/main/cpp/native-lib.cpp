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
#include <cstdio>
#include <cstdlib>
#include <cstring>

#define TAG "ApexCombatMod"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// 5. Aimbot / FOV
uintptr_t OFFSET_MISSILE_GET_LOCK_DISTANCE = 0x1C063E4; // MissileProperty.get_LockDistance
uintptr_t OFFSET_MISSILE_GET_MAX_ROTATE_ANGLE = 0x1C06694; // MissileProperty.get_MaxRotateAngle
uintptr_t OFFSET_MISSILE_GET_TRACE_ABILITY = 0x1C067E8; // MissileProperty.get_TraceAbility

// =============================================================================================
// OFFSETS ATUALIZADOS (V22 - Hooked Hit Kill)
// =============================================================================================

// 1. God Mode & One Hit Kill
uintptr_t OFFSET_APPLY_DAMAGE = 0x1BF77C0; // PlayerPlaneAction.ApplyDamage (Offline God Mode)
uintptr_t OFFSET_UNIT_APPLY_DAMAGE = 0x1C031D8; // UnitActionBase.ApplyDamage (Offline God Mode)
uintptr_t OFFSET_GET_GOD_MODE = 0x1C01704; // UnitActionBase.get_GodMode (Retornar 1)
uintptr_t OFFSET_GET_HP_PROGRESS = 0x1C01DF0; // UnitActionBase.GetHpProgress (Retornar 1.0)
uintptr_t OFFSET_MODIFY_DAMAGE = 0x1BF7628; // PlayerPlaneAction.ModifyDamage (Retornar 0.0)

// 1.1 Hit Kill Aux (Patches)
uintptr_t OFFSET_WEAPON_DAMAGE = 0x1C05D18; // MissileProperty.get_Damage
uintptr_t OFFSET_CANNON_DAMAGE = 0x1C035D0; // CannonProperty.get_Damage
uintptr_t OFFSET_CRITICAL_DAMAGE_RATE = 0x1C01398; // UnitActionBase.get_CriticalDamageRate
uintptr_t OFFSET_CRITICAL_PROB = 0x1C01328; // UnitActionBase.get_CriticalProb
uintptr_t OFFSET_BULLET_GET_DAMAGE = 0x1869168; // BulletMove.GetDamage (Return 99999.0)
uintptr_t OFFSET_MISSILE_GET_DAMAGE = 0x1C866B4; // Missile.GetDamage (Return 99999.0)
uintptr_t OFFSET_MISSILE_GET_RELOAD_TIME = 0x1C06D38; // MissileProperty.get_ReloadTime
uintptr_t OFFSET_CANNON_GET_COLD_TIME = 0x1BFFFC8; // CannonProperty.get_ColdTime (Verify if correct)

// 2. Hit Kill (Hooked)
uintptr_t OFFSET_UNITMANAGER_UPDATE = 0x16C2A24;
uintptr_t OFFSET_UNITMANAGER_GET_ISM_YSELF = 0x16C22B4;
uintptr_t OFFSET_UNITMANAGER_GET_IS_CURRENT_PLAYER = 0x16C2414; // UnitManager.get_IsCurrentPlayer
uintptr_t OFFSET_UNITMANAGER_GET_HASHCODE = 0x16C176C;
uintptr_t OFFSET_UNITMANAGER_GET_IS_INVINCIBLE = 0x16C1EFC; // UnitManager.get_IsInvincible

uintptr_t OFFSET_UNITACTIONBASE_APPLYDAMAGE = 0x1C031D8;
uintptr_t OFFSET_CLOUDCONTAINER_APPLYDAMAGEBYLUA = 0x1D7DA88; // CloudContainer.ApplyDamageByLua
uintptr_t OFFSET_PHOTON_PLUGIN_EVENTCALL = 0x1780D3C; // PhotonPlugin.EventCall
uintptr_t OFFSET_PHOTON_PLUGIN_APPLYDAMAGE = 0x178285C; // PhotonPlugin.ApplyDamage (Dictionary)
uintptr_t OFFSET_PHOTON_CLIENT_RAISEEVENT = 0x170FAE0; // PhotonClient.RaiseEvent

// 3. Energia
uintptr_t OFFSET_REDUCE_ENERGY = 0x1BF5D58; // PlayerPlaneAction.ReduceEnergy
uintptr_t OFFSET_GET_ENERGY_PROGRESS = 0x1BF5958; // PlayerPlaneAction.GetEnergyProgress (Float 1.0)
uintptr_t OFFSET_HAS_ENERGY = 0x1BF5BEC; // PlayerPlaneAction.HasEnergy (True)
uintptr_t OFFSET_ENOUGH_ENERGY_SPECIAL = 0x1BFC0E4; // PlayerPlaneAction.EnoughEnergyToSpecialMove (True)
uintptr_t OFFSET_ENOUGH_ENERGY_CLIMB = 0x1BFBD5C; // PlayerPlaneAction.EnoughEnergyToClimbout (True)
uintptr_t OFFSET_ENOUGH_ENERGY_BACK = 0x1BFBF8C; // PlayerPlaneAction.EnoughEnergyToBackRoll (True)
uintptr_t OFFSET_ENOUGH_ENERGY_HORI = 0x1BFC038; // PlayerPlaneAction.EnoughEnergyToHoriRoll (True)
uintptr_t OFFSET_ENOUGH_ENERGY_NOT_SPECIAL = 0x1BFBE08; // PlayerPlaneAction.EnoughEnergyAndNotInSpecialMove (True)

// 3.1 Velocidade do Aviao
uintptr_t OFFSET_PLAYERPLANEACTION_UPDATE_FLYCONTROLLER_PARAMS = 0x1BF4E8C; // PlayerPlaneAction.UpdateFlyControllerParams
uintptr_t OFFSET_PLAYERPLANEACTION_SETUP_FLYCONTROLLER = 0x1BF5454; // PlayerPlaneAction.SetUpFlyController
uintptr_t OFFSET_PLANEPROPERTY_GET_MAX_SPEED = 0x1C08348; // PlaneProperty.get_MaxSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_MAX_SPEED = 0x1C083EC; // PlaneProperty.set_MaxSpeed
uintptr_t OFFSET_PLANEPROPERTY_GET_NORMAL_SPEED = 0x1C084A4; // PlaneProperty.get_NormalSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_NORMAL_SPEED = 0x1C08548; // PlaneProperty.set_NormalSpeed
uintptr_t OFFSET_PLANEPROPERTY_GET_MIN_SPEED = 0x1C08600; // PlaneProperty.get_MinSpeed
uintptr_t OFFSET_PLANEPROPERTY_SET_MIN_SPEED = 0x1C086A4; // PlaneProperty.set_MinSpeed

// 3. Munição Infinita
uintptr_t OFFSET_DO_CONSUME_WEAPON = 0x1BF709C; // PlayerPlaneAction.DoConsumeWeapon (Block)
uintptr_t OFFSET_CANNON_GET_COUNT = 0x1C03C9C; // CannonProperty.get_Count (Return 999)
uintptr_t OFFSET_MISSILE_GET_COUNT = 0x1C06A90; // MissileProperty.get_Count (Return 999)
uintptr_t OFFSET_IS_MISSILE_READY = 0x1BF9A40; // PlayerPlaneAction.get_IsMissileReady (True)
uintptr_t OFFSET_CHECK_RELOAD_AIR_MISSILE_IDX = 0x1BF9ACC; // PlayerPlaneAction.CheckReloadAirMissileIdx (Force Return 0, *isLeft=true)
uintptr_t OFFSET_INTERNAL_FIRE_MISSILE = 0x1BFA0D4; // PlayerPlaneAction.InternalFireMissile
uintptr_t OFFSET_INTERNAL_FIRE_MISSILE_WITH_AIR = 0x1BF8EB8; // PlayerPlaneAction.InternalFireMissileWithAirMissile

// 4. Auto Dodge / Ignore Hit
uintptr_t OFFSET_MISSILE_TRACE_CAN_HIT = 0x1C85EAC; // MissileTrace.<OnTriggerEnter>g__CanHit|49_1
uintptr_t OFFSET_BULLET_MOVE_CAN_HIT = 0x1868888; // BulletMove.CanHit

// 5. Moedas / Currency
uintptr_t OFFSET_GET_GOLD = 0x1DC4714; // UserProfile.get_Gold
uintptr_t OFFSET_GET_DIAMOND = 0x1DC47B0; // UserProfile.get_Diamond

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
typedef void (*Func_VoidInstance)(void* _this);
typedef float (*Func_FloatGetter)(void* _this);
typedef void (*Func_FloatSetter)(void* _this, float value);

typedef float (*Func_GetReloadTime)(void* _this);
typedef float (*Func_GetColdTime)(void* _this);
typedef bool (*Func_MissileCanHit)(void* _this, void* defender);
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
Func_GetReloadTime orig_Missile_GetReloadTime = nullptr;
Func_GetColdTime orig_Cannon_GetColdTime = nullptr;
Func_MissileCanHit orig_MissileTrace_CanHit = nullptr;
Func_MissileCanHit orig_BulletMove_CanHit = nullptr;
Func_GetIsInvincible orig_UnitManager_GetIsInvincible = nullptr;
Func_VoidInstance orig_PlayerPlaneAction_UpdateFlyControllerParams = nullptr;
Func_VoidInstance orig_PlayerPlaneAction_SetUpFlyController = nullptr;
Func_FloatGetter planeProperty_GetMaxSpeed = nullptr;
Func_FloatSetter planeProperty_SetMaxSpeed = nullptr;
Func_FloatGetter planeProperty_GetNormalSpeed = nullptr;
Func_FloatSetter planeProperty_SetNormalSpeed = nullptr;
Func_FloatGetter planeProperty_GetMinSpeed = nullptr;
Func_FloatSetter planeProperty_SetMinSpeed = nullptr;

// ... Variáveis de Controle ...
int g_MyHashCode = 0;
bool isHitKillEnabled = false;
bool isAutoDodgeEnabled = false;
bool isMissileFovEnabled = false;
bool g_MissileToggle = false;
int g_FakeMissileIdx = 1000;
bool g_PlaneSpeedHackEnabled = false;
int g_PlaneSpeedMultiplier = 2;
constexpr uintptr_t OFFSET_PLANEACTIONBASE_PLANEPROPERTY_FIELD = 0xD8;
void* g_CurrentPlayerPlaneAction = nullptr;

struct PlaneSpeedBackup {
    float maxSpeed;
    float normalSpeed;
    float minSpeed;
    bool captured;
};

std::map<void*, PlaneSpeedBackup> g_PlaneSpeedBackups;

// ... (previous hooks) ...

void hook_InternalFireMissile(void* _this, int missileType, void* missileProperty, void* missileTarget, bool isLastMissile, int missileIdx) {
    if (isHitKillEnabled) {
        // V30: Fake Missile Idx para gerar fireIds únicos e forçar o servidor a aceitar danos separados.
        // O jogo usa o missileIdx para calcular o FireId (normalmente 0, 1, 2, 3...)
        // Vamos incrementar para que cada míssil seja "único"
        g_FakeMissileIdx++;
        if (g_FakeMissileIdx > 99999) g_FakeMissileIdx = 1000;
        missileIdx = g_FakeMissileIdx;
    }
    if (orig_InternalFireMissile) {
        orig_InternalFireMissile(_this, missileType, missileProperty, missileTarget, isLastMissile, missileIdx);
    }
}

int hook_CheckReloadAirMissileIdx(void* _this, bool* isLeft) {
    if (isHitKillEnabled) {
        // V33: Player-only Rapid Fire check
        // Se a função não nos dá IsMyself fácil, usamos o hack de forçar true apenas quando
        // não usamos o patch global de ReloadTime. A função CheckReloadAirMissileIdx é 
        // normalmente chamada pelo controle do jogador (PlayerPlaneAction).
        // Assim, o bot que usa AIPlaneAction não passará por aqui frequentemente ou usará 
        // a lógica de IA que obedece ao tempo de recarga normal.
        if (isLeft) {
            g_MissileToggle = !g_MissileToggle;
            *isLeft = g_MissileToggle;
        }
        return 0; // Retorna índice válido (0 = sucesso)
    }
    if (orig_CheckReloadAirMissileIdx) return orig_CheckReloadAirMissileIdx(_this, isLeft);
    return -1;
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
    // Para evitar que os bots atirem rápido, não usamos o MemoryPatch global.
    // Usamos um hook que zera o tempo APENAS se o mod estiver ativo.
    // Não conseguimos checar IsMyself facilmente dentro de MissileProperty, 
    // mas remover o patch global e usar o hook já ajuda a controlar se deixarmos de usar o patch.
    // Na verdade, o ideal seria interceptar o tiro em si ou UnitActionBase.
    if (isHitKillEnabled) {
        return 0.0f; // Todos atiram rápido? Vamos manter assim por enquanto, 
                     // mas a verdadeira correção é auto-dodge!
    }
    if (orig_Missile_GetReloadTime) return orig_Missile_GetReloadTime(_this);
    return 3.0f;
}

bool hook_MissileTrace_CanHit(void* _this, void* defender) {
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
    if (orig_PlayerPlaneAction_UpdateFlyControllerParams) {
        orig_PlayerPlaneAction_UpdateFlyControllerParams(_this);
    }
    apply_PlaneSpeed_ToAction(_this);
}

void hook_PlayerPlaneAction_SetUpFlyController(void* _this) {
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
    
    A64HookFunction((void*)(base + OFFSET_UNITMANAGER_UPDATE), (void*)hook_UnitManager_Update, (void**)&orig_UnitManager_Update);
    A64HookFunction((void*)(base + OFFSET_UNITACTIONBASE_APPLYDAMAGE), (void*)hook_ApplyDamage, (void**)&orig_ApplyDamage);
    A64HookFunction((void*)(base + OFFSET_CLOUDCONTAINER_APPLYDAMAGEBYLUA), (void*)hook_ApplyDamageByLua, (void**)&orig_ApplyDamageByLua);
    A64HookFunction((void*)(base + OFFSET_PHOTON_PLUGIN_APPLYDAMAGE), (void*)hook_PhotonPlugin_ApplyDamage, (void**)&orig_PhotonPlugin_ApplyDamage);
    A64HookFunction((void*)(base + OFFSET_CHECK_RELOAD_AIR_MISSILE_IDX), (void*)hook_CheckReloadAirMissileIdx, (void**)&orig_CheckReloadAirMissileIdx);
    A64HookFunction((void*)(base + OFFSET_INTERNAL_FIRE_MISSILE), (void*)hook_InternalFireMissile, (void**)&orig_InternalFireMissile);
    A64HookFunction((void*)(base + OFFSET_MISSILE_TRACE_CAN_HIT), (void*)hook_MissileTrace_CanHit, (void**)&orig_MissileTrace_CanHit);
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
    FILE *fp;
    uintptr_t addr = 0;
    char filename[32], buffer[1024];
    snprintf(filename, sizeof(filename), "/proc/self/maps");
    fp = fopen(filename, "rt");
    if (fp != NULL) {
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, libName)) {
                addr = (uintptr_t)strtoul(buffer, NULL, 16);
                break;
            }
        }
        fclose(fp);
    }
    return addr;
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
    isHitKillEnabled = isEnabled;
    if (libIl2CppBase == 0) return;
    
    if (isEnabled) {
        // Ativa Patches de Dano/Crítico para reforçar
        patchCannonDamage.ApplyFloat99999(libIl2CppBase);
        patchMissileDamage.ApplyFloat99999(libIl2CppBase);
        patchCriticalRate.ApplyFloat99999(libIl2CppBase);
        patchCriticalProb.ApplyFloat1(libIl2CppBase); // 100% Crítico
        patchBulletGetDamage.ApplyFloat99999(libIl2CppBase);
        patchMissileGetDamage.ApplyFloat99999(libIl2CppBase);
        
        // V33: Removido os patches globais de ReloadTime e ColdTime
        // para que inimigos online e offline não atirem rápido.
        // O Rapid Fire agora depende 100% dos hooks em CheckReloadAirMissileIdx
        
        patchConsumeWeapon.ApplyFalse(libIl2CppBase); 
        patchCannonCount.ApplyInt999(libIl2CppBase);
        patchMissileCount.ApplyInt999(libIl2CppBase);
        patchMissileReady.ApplyTrue(libIl2CppBase);
    } else {
        patchCannonDamage.Restore();
        patchMissileDamage.Restore();
        patchCriticalRate.Restore();
        patchCriticalProb.Restore();
        patchBulletGetDamage.Restore();
        patchMissileGetDamage.Restore();
        
        patchConsumeWeapon.Restore();
        patchCannonCount.Restore();
        patchMissileCount.Restore();
        patchMissileReady.Restore();
    }
    
    LOGI("[MOD] Hit Kill + Rapid Fire (Player Only) + Munição: %s", isEnabled ? "ON" : "OFF");
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
