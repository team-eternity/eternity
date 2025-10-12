//
// The Eternity Engine
// Copyright (C) 2025 David Hill, James Haley, et al.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
// Additional terms and conditions compatible with the GPLv3 apply. See the
// file COPYING-EE for details.
//
//------------------------------------------------------------------------------
//
// Purpose: Original 100% GPL ACS interpreter.
// Authors: James Haley, David Hill
//

#ifndef ACS_INTR_H__
#define ACS_INTR_H__

#include "m_dllist.h"
#include "p_tick.h"
#include "r_defs.h"

#include "ACSVM/Environment.hpp"
#include "ACSVM/Thread.hpp"

class qstring;
class Mobj;
class WadDirectory;

struct line_t;
struct polyobj_t;

//
// Defines
//

constexpr int32_t ACS_NUM_THINGTYPES = 256;

#define ACS_CF_ARGS ACSVM::Thread *thread, const ACSVM::Word *argV, ACSVM::Word argC

//
// Misc. Enums
//

// ACS game mode flags for EDF thingtypes

enum
{
    ACS_MODE_DOOM = 0x00000001,
    ACS_MODE_HTIC = 0x00000002,
    ACS_MODE_ALL  = ACS_MODE_DOOM | ACS_MODE_HTIC,
};

// Script types.
enum acs_stype_t
{
    ACS_STYPE_Closed,
    ACS_STYPE_Enter,
    ACS_STYPE_Open,
};

// Tag types.
enum acs_tagtype_t
{
    ACS_TAGTYPE_POLYOBJ,
    ACS_TAGTYPE_SECTOR,
};

// Level Properties.
enum
{
    ACS_LP_ParTime        = 0,
    ACS_LP_ClusterNumber  = 1,
    ACS_LP_LevelNumber    = 2,
    ACS_LP_TotalSecrets   = 3,
    ACS_LP_FoundSecrets   = 4,
    ACS_LP_TotalItems     = 5,
    ACS_LP_FoundItems     = 6,
    ACS_LP_TotalMonsters  = 7,
    ACS_LP_KilledMonsters = 8,
    ACS_LP_SuckTime       = 9,

    ACS_LPMAX
};

// Thing properties.
enum
{
    ACS_TP_Health       = 0,
    ACS_TP_Speed        = 1,
    ACS_TP_Damage       = 2,
    ACS_TP_Alpha        = 3,
    ACS_TP_RenderStyle  = 4,
    ACS_TP_SeeSound     = 5,
    ACS_TP_AttackSound  = 6,
    ACS_TP_PainSound    = 7,
    ACS_TP_DeathSound   = 8,
    ACS_TP_ActiveSound  = 9,
    ACS_TP_Ambush       = 10,
    ACS_TP_Invulnerable = 11,
    ACS_TP_JumpZ        = 12,
    ACS_TP_ChaseGoal    = 13,
    ACS_TP_Frightened   = 14,
    ACS_TP_Gravity      = 15,
    ACS_TP_Friendly     = 16,
    ACS_TP_SpawnHealth  = 17,
    ACS_TP_Dropped      = 18,
    ACS_TP_NoTarget     = 19,
    ACS_TP_Species      = 20,
    ACS_TP_NameTag      = 21,
    ACS_TP_Score        = 22,
    ACS_TP_NoTrigger    = 23,
    ACS_TP_DamageFactor = 24,
    ACS_TP_MasterTID    = 25,
    ACS_TP_TargetTID    = 26,
    ACS_TP_TracerTID    = 27,
    ACS_TP_WaterLevel   = 28,
    ACS_TP_ScaleX       = 29,
    ACS_TP_ScaleY       = 30,
    ACS_TP_Dormant      = 31,
    ACS_TP_Mass         = 32,
    ACS_TP_Accuracy     = 33,
    ACS_TP_Stamina      = 34,
    ACS_TP_Height       = 35,
    ACS_TP_Radius       = 36,
    ACS_TP_ReactionTime = 37,
    ACS_TP_MeleeRange   = 38,
    ACS_TP_ViewHeight   = 39,
    ACS_TP_AttackZOff   = 40,
    ACS_TP_StencilColor = 41,
    ACS_TP_Friction     = 42,
    ACS_TP_DamageMult   = 43,

    ACS_TP_Counter0 = 100,
    ACS_TP_Counter1 = 101,
    ACS_TP_Counter2 = 102,
    ACS_TP_Counter3 = 103,
    ACS_TP_Counter4 = 104,
    ACS_TP_Counter5 = 105,
    ACS_TP_Counter6 = 106,
    ACS_TP_Counter7 = 107,

    // Internal properties, not meant for external use.
    ACS_TP_Angle,
    ACS_TP_Armor,
    ACS_TP_CeilTex,
    ACS_TP_CeilZ,
    ACS_TP_FloorTex,
    ACS_TP_FloorZ,
    ACS_TP_Frags,
    ACS_TP_LightLevel,
    ACS_TP_MomX,
    ACS_TP_MomY,
    ACS_TP_MomZ,
    ACS_TP_Pitch,
    ACS_TP_PlayerNumber,
    ACS_TP_SigilPieces,
    ACS_TP_TID,
    ACS_TP_Type,
    ACS_TP_X,
    ACS_TP_Y,
    ACS_TP_Z,

    ACS_TPMAX
};

//
// Structures
//

//
// ACSEnvironment
//
class ACSEnvironment : public ACSVM::Environment
{
protected:
    virtual ACSVM::Thread *allocThread();

    virtual ACSVM::Word callSpecImpl(ACSVM::Thread *thread, ACSVM::Word spec, const ACSVM::Word *argV,
                                     ACSVM::Word argC);

public:
    using ACSVM::Environment::getModuleName;

    ACSEnvironment();

    virtual bool checkTag(ACSVM::Word type, ACSVM::Word tag);

    virtual ACSVM::ModuleName getModuleName(char const *str, size_t len);

    virtual ACSVM::ScopeID getScopeID(ACSVM::Word mapnum) const;

    virtual std::pair<ACSVM::Word /*type*/, ACSVM::Word /*name*/> getScriptTypeACS0(ACSVM::Word name);
    virtual ACSVM::Word                                           getScriptTypeACSE(ACSVM::Word type);

    virtual void loadModule(ACSVM::Module *module);

    virtual void loadState(ACSVM::Serial &in);

    virtual ACSVM::ModuleName readModuleName(ACSVM::Serial &in) const;

    virtual void refStrings();

    WadDirectory       *dir;
    ACSVM::GlobalScope *global;
    ACSVM::HubScope    *hub;
    ACSVM::MapScope    *map;

    size_t errors;
};

//
// ACSThreadInfo
//
class ACSThreadInfo : public ACSVM::ThreadInfo
{
public:
    ACSThreadInfo() : mo{ nullptr }, line{ nullptr }, side{ 0 }, po{ nullptr } {}

    ACSThreadInfo(const ACSThreadInfo &info) : mo{ nullptr }, line{ info.line }, side{ info.side }, po{ info.po }
    {
        P_SetTarget(&mo, info.mo);
    }

    ACSThreadInfo(Mobj *mo_, line_t *line_, int side_, polyobj_t *po_)
        : mo{ nullptr }, line{ line_ }, side{ side_ }, po{ po_ }
    {
        P_SetTarget(&mo, mo_);
    }

    ~ACSThreadInfo() { P_SetTarget<Mobj>(&mo, nullptr); }

    ACSThreadInfo &operator=(const ACSThreadInfo &info)
    {
        P_SetTarget(&mo, info.mo);
        line = info.line;
        side = info.side;
        po   = info.po;

        return *this;
    }

    Mobj      *mo;   // Mobj that activated.
    line_t    *line; // Line that activated.
    int        side; // Side of actived line.
    polyobj_t *po;
};

//
// ACSThread
//
class ACSThread : public ACSVM::Thread
{
public:
    explicit ACSThread(ACSVM::Environment *env_) : ACSVM::Thread{ env_ } {}

    virtual ACSVM::ThreadInfo const *getInfo() const { return &info; }

    virtual void loadState(ACSVM::Serial &in);

    virtual void saveState(ACSVM::Serial &out) const;

    virtual void start(ACSVM::Script *script, ACSVM::MapScope *map, const ACSVM::ThreadInfo *info,
                       const ACSVM::Word *argV, ACSVM::Word argC);

    virtual void stop();

    ACSThreadInfo info;

    static int saveLoadVersion; // context information stored when saving and loading
};

// Global function prototypes

// Environment control.
void ACS_Init();
void ACS_NewGame();
void ACS_InitLevel();
void ACS_LoadLevelScript(WadDirectory *dir, int lump);
void ACS_Exec();

void ACS_Archive(SaveArchive &arc);

// Script control.
bool     ACS_ExecuteScriptI(uint32_t name, uint32_t mapnum, const uint32_t *argv, uint32_t argc, Mobj *mo, line_t *line,
                            int side, polyobj_t *po);
bool     ACS_ExecuteScriptIAlways(uint32_t name, uint32_t mapnum, const uint32_t *argv, uint32_t argc, Mobj *mo,
                                  line_t *line, int side, polyobj_t *po);
uint32_t ACS_ExecuteScriptIResult(uint32_t name, const uint32_t *argv, uint32_t argc, Mobj *mo, line_t *line, int side,
                                  polyobj_t *po);
bool ACS_ExecuteScriptS(const char *name, uint32_t mapnum, const uint32_t *argv, uint32_t argc, Mobj *mo, line_t *line,
                        int side, polyobj_t *po);
bool ACS_ExecuteScriptSAlways(const char *name, uint32_t mapnum, const uint32_t *argv, uint32_t argc, Mobj *mo,
                              line_t *line, int side, polyobj_t *po);
uint32_t ACS_ExecuteScriptSResult(const char *name, const uint32_t *argv, uint32_t argc, Mobj *mo, line_t *line,
                                  int side, polyobj_t *po);
bool     ACS_SuspendScriptI(uint32_t name, uint32_t mapnum);
bool     ACS_SuspendScriptS(const char *name, uint32_t mapnum);
bool     ACS_TerminateScriptI(uint32_t name, uint32_t mapnum);
bool     ACS_TerminateScriptS(const char *name, uint32_t mapnum);

// Utilities.
uint32_t ACS_GetLevelProp(uint32_t prop);
bool     ACS_ChkThingProp(Mobj *mo, uint32_t prop, uint32_t val);
uint32_t ACS_GetThingProp(Mobj *mo, uint32_t prop);
void     ACS_SetThingProp(Mobj *mo, uint32_t prop, uint32_t val);

// CallFuncs.
bool ACS_CF_PlayerArmorPoints(ACS_CF_ARGS);
bool ACS_CF_PlayerFrags(ACS_CF_ARGS);
bool ACS_CF_PlayerHealth(ACS_CF_ARGS);
bool ACS_CF_GetSigilPieces(ACS_CF_ARGS);
bool ACS_CF_ActivatorSound(ACS_CF_ARGS);
bool ACS_CF_ActivatorTID(ACS_CF_ARGS);
bool ACS_CF_AmbientSound(ACS_CF_ARGS);
bool ACS_CF_LocalAmbientSound(ACS_CF_ARGS);
bool ACS_CF_VectorAngle(ACS_CF_ARGS);
bool ACS_CF_ChangeCeiling(ACS_CF_ARGS);
bool ACS_CF_ChangeFloor(ACS_CF_ARGS);
bool ACS_CF_CheckProximity(ACS_CF_ARGS);
bool ACS_CF_CheckSight(ACS_CF_ARGS);
bool ACS_CF_CheckWeapon(ACS_CF_ARGS);
bool ACS_CF_CheckActorCeilingTexture(ACS_CF_ARGS);
bool ACS_CF_CheckFlag(ACS_CF_ARGS);
bool ACS_CF_SetLineActivation(ACS_CF_ARGS);
bool ACS_CF_CheckActorFloorTexture(ACS_CF_ARGS);
bool ACS_CF_CheckActorProperty(ACS_CF_ARGS);
bool ACS_CF_CheckActorClass(ACS_CF_ARGS);
bool ACS_CF_ClassifyActor(ACS_CF_ARGS);
bool ACS_CF_ClearLineSpecial(ACS_CF_ARGS);
bool ACS_CF_Cos(ACS_CF_ARGS);
bool ACS_CF_EndLog(ACS_CF_ARGS);
bool ACS_CF_EndPrint(ACS_CF_ARGS);
bool ACS_CF_EndPrintBold(ACS_CF_ARGS);
bool ACS_CF_GameSkill(ACS_CF_ARGS);
bool ACS_CF_GameType(ACS_CF_ARGS);
bool ACS_CF_GetCVar(ACS_CF_ARGS);
bool ACS_CF_GetCVarString(ACS_CF_ARGS);
bool ACS_CF_CheckInventory(ACS_CF_ARGS);
bool ACS_CF_GetLevelInfo(ACS_CF_ARGS);
bool ACS_CF_GetLineX(ACS_CF_ARGS);
bool ACS_CF_GetLineY(ACS_CF_ARGS);
bool ACS_CF_GetPlayerInput(ACS_CF_ARGS);
bool ACS_CF_GetPolyobjX(ACS_CF_ARGS);
bool ACS_CF_GetPolyobjY(ACS_CF_ARGS);
bool ACS_CF_SetPolyobjXY(ACS_CF_ARGS);
bool ACS_CF_GetScreenH(ACS_CF_ARGS);
bool ACS_CF_GetScreenW(ACS_CF_ARGS);
bool ACS_CF_GetSectorCeilingZ(ACS_CF_ARGS);
bool ACS_CF_GetSectorFloorZ(ACS_CF_ARGS);
bool ACS_CF_GetSectorLightLevel(ACS_CF_ARGS);
bool ACS_CF_GetActorAngle(ACS_CF_ARGS);
bool ACS_CF_GetActorCeilingZ(ACS_CF_ARGS);
bool ACS_CF_GetActorFloorZ(ACS_CF_ARGS);
bool ACS_CF_GetActorLightLevel(ACS_CF_ARGS);
bool ACS_CF_GetActorVelX(ACS_CF_ARGS);
bool ACS_CF_GetActorVelY(ACS_CF_ARGS);
bool ACS_CF_GetActorVelZ(ACS_CF_ARGS);
bool ACS_CF_GetActorPitch(ACS_CF_ARGS);
bool ACS_CF_GetActorProperty(ACS_CF_ARGS);
bool ACS_CF_GetActorX(ACS_CF_ARGS);
bool ACS_CF_GetActorY(ACS_CF_ARGS);
bool ACS_CF_GetActorZ(ACS_CF_ARGS);
bool ACS_CF_GetWeapon(ACS_CF_ARGS);
bool ACS_CF_VectorLength(ACS_CF_ARGS);
bool ACS_CF_IsTIDUsed(ACS_CF_ARGS);
bool ACS_CF_GetLineRowOffset(ACS_CF_ARGS);
bool ACS_CF_LineSide(ACS_CF_ARGS);
bool ACS_CF_PlaySound(ACS_CF_ARGS);
bool ACS_CF_PlayActorSound(ACS_CF_ARGS);
bool ACS_CF_PlayerCount(ACS_CF_ARGS);
bool ACS_CF_PlayerNumber(ACS_CF_ARGS);
bool ACS_CF_PrintName(ACS_CF_ARGS);
bool ACS_CF_Radius_Quake2(ACS_CF_ARGS);
bool ACS_CF_Random(ACS_CF_ARGS);
bool ACS_CF_ReplaceTextures(ACS_CF_ARGS);
bool ACS_CF_SectorDamage(ACS_CF_ARGS);
bool ACS_CF_SectorSound(ACS_CF_ARGS);
bool ACS_CF_SetActivator(ACS_CF_ARGS);
bool ACS_CF_SetActivatorToTarget(ACS_CF_ARGS);
bool ACS_CF_SetAirControl(ACS_CF_ARGS);
bool ACS_CF_SetAirFriction(ACS_CF_ARGS);
bool ACS_CF_SetGravity(ACS_CF_ARGS);
bool ACS_CF_SetLineBlocking(ACS_CF_ARGS);
bool ACS_CF_SetLineMonsterBlocking(ACS_CF_ARGS);
bool ACS_CF_SetLineSpecial(ACS_CF_ARGS);
bool ACS_CF_SetLineTexture(ACS_CF_ARGS);
bool ACS_CF_SetMusic(ACS_CF_ARGS);
bool ACS_CF_LocalSetMusic(ACS_CF_ARGS);
bool ACS_CF_SetSectorDamage(ACS_CF_ARGS);
bool ACS_CF_SetSkyScrollSpeed(ACS_CF_ARGS);
bool ACS_CF_SetActorAngle(ACS_CF_ARGS);
bool ACS_CF_ChangeActorAngle(ACS_CF_ARGS);
bool ACS_CF_SetActorVelocity(ACS_CF_ARGS);
bool ACS_CF_SetActorPitch(ACS_CF_ARGS);
bool ACS_CF_ChangeActorPitch(ACS_CF_ARGS);
bool ACS_CF_SetActorPosition(ACS_CF_ARGS);
bool ACS_CF_SetActorProperty(ACS_CF_ARGS);
bool ACS_CF_SetThingSpecial(ACS_CF_ARGS);
bool ACS_CF_SetActorState(ACS_CF_ARGS);
bool ACS_CF_SetWeapon(ACS_CF_ARGS);
bool ACS_CF_Sin(ACS_CF_ARGS);
bool ACS_CF_SinglePlayer(ACS_CF_ARGS);
bool ACS_CF_SoundSequence(ACS_CF_ARGS);
bool ACS_CF_SpawnProjectile(ACS_CF_ARGS);
bool ACS_CF_Spawn(ACS_CF_ARGS);
bool ACS_CF_SpawnForced(ACS_CF_ARGS);
bool ACS_CF_SpawnSpot(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotForced(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotFacing(ACS_CF_ARGS);
bool ACS_CF_SpawnSpotFacingForced(ACS_CF_ARGS);
bool ACS_CF_Sqrt(ACS_CF_ARGS);
bool ACS_CF_FixedSqrt(ACS_CF_ARGS);
bool ACS_CF_StopSound(ACS_CF_ARGS);
bool ACS_CF_GiveInventory(ACS_CF_ARGS);
bool ACS_CF_TakeInventory(ACS_CF_ARGS);
bool ACS_CF_ThingCount(ACS_CF_ARGS);
bool ACS_CF_ThingCountName(ACS_CF_ARGS);
bool ACS_CF_ThingCountSector(ACS_CF_ARGS);
bool ACS_CF_ThingCountNameSector(ACS_CF_ARGS);
bool ACS_CF_Thing_Damage2(ACS_CF_ARGS);
bool ACS_CF_Thing_Projectile2(ACS_CF_ARGS);
bool ACS_CF_ThingSound(ACS_CF_ARGS);
bool ACS_CF_SoundSequenceOnActor(ACS_CF_ARGS);
bool ACS_CF_Timer(ACS_CF_ARGS);
bool ACS_CF_UniqueTID(ACS_CF_ARGS);
bool ACS_CF_PolyWait(ACS_CF_ARGS);
bool ACS_CF_TagWait(ACS_CF_ARGS);

// extern vars.

extern ACSEnvironment ACSenv;

extern int ACS_thingtypes[ACS_NUM_THINGTYPES];

#endif

// EOF

