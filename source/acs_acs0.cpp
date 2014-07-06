// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 David Hill et al.
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
//----------------------------------------------------------------------------
//
// ACS0 loader.
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "acs_intr.h"

#include "i_system.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "w_wad.h"
#include "doomstat.h"


//----------------------------------------------------------------------------|
// Types                                                                      |
//

typedef enum acs0_op_e
{
   // ZDoom was first, so their extensions are what will be used for ACS0 lumps.
   #define ACSE_OP(OPDATA,OP,ARGC,COMP) ACS0_OP(OPDATA, OP, ARGC, COMP)
   #define ACS0_OP(OPDATA,OP,ARGC,COMP) ACS0_OP_##OP,
   #include "acs_op.h"
   #undef ACS0_OP

   ACS0_OPMAX
} acs0_op_t;

typedef struct acs0_opdata_s
{
   acs_opdata_t const *opdata;
   acs0_op_t op;
   int args;
   bool compressed;
} acs0_opdata_t;

struct acs0_tracer_t
{
   ACSVM *vm;
   const byte *data;
   bool *codeTouched;
   uint32_t jumpCount;
   uint32_t lumpLength;
   bool compressed;
};


//----------------------------------------------------------------------------|
// Static Variables                                                           |
//

static acs0_opdata_t const ACS0opdata[ACS0_OPMAX] =
{
   #define ACS0_OP(OPDATA,OP,ARGC,COMP) {&ACSopdata[ACS_OP_##OPDATA], ACS0_OP_##OP, ARGC, COMP},
   #include "acs_op.h"
   #undef ACS0_OP
};


//----------------------------------------------------------------------------|
// Static Functions                                                           |
//

//
// ACS_countOpSizeACS0
//
// Some instructions require special handling to find next index.
//
static uint32_t ACS_countOpSizeACS0(acs0_tracer_t *tracer, uint32_t index,
                                    uint32_t opSize, const acs0_opdata_t *opdata)
{
   switch(opdata->op)
   {
   case ACS0_OP_LINESPEC1_IMM_BYTE:
   case ACS0_OP_LINESPEC2_IMM_BYTE:
   case ACS0_OP_LINESPEC3_IMM_BYTE:
   case ACS0_OP_LINESPEC4_IMM_BYTE:
   case ACS0_OP_LINESPEC5_IMM_BYTE:
   case ACS0_OP_GET_IMM_BYTE:
   case ACS0_OP_GET2_IMM_BYTE:
   case ACS0_OP_GET3_IMM_BYTE:
   case ACS0_OP_GET4_IMM_BYTE:
   case ACS0_OP_GET5_IMM_BYTE:
   case ACS0_OP_DELAY_IMM_BYTE:
   case ACS0_OP_RANDOM_IMM_BYTE:
      return opSize + opdata->args;

   case ACS0_OP_GETARR_IMM_BYTE:
      // Need room for the byte count.
      if(index + opSize > tracer->lumpLength - 4) return opSize;

      return opSize + 1 + tracer->data[index + opSize];

   case ACS0_OP_BRANCH_CASETABLE:
      // Set opSize to the op plus alignment.
      opSize = ((index + opSize + 3) & ~3) - index;

      // Need room for the case count.
      if(index + opSize > tracer->lumpLength - 4) return opSize;

      // Read the number of cases.
      return opSize + (SwapULong(*(uint32_t *)(tracer->data + index + opSize)) * 8) + 4;

   case ACS0_OP_CALLFUNC:
      return opSize + (tracer->compressed ? 3 : 8);

   default:
      return opSize + opdata->args * (tracer->compressed && opdata->compressed ? 1 : 4);
   }
}

//
// ACS_readOpACS0
//
static int32_t ACS_readOpACS0(acs0_tracer_t *tracer, uint32_t *opSize, uint32_t index)
{
   int32_t op;

   if(tracer->compressed)
   {
      *opSize = 1;
      if((op = tracer->data[index]) >= 240)
      {
         ++*opSize;
         op = 240 + ((op - 240) << 8) + tracer->data[index + 1];
      }
   }
   else
   {
      *opSize = 4;
      op = SwapLong(*(int32_t *)(tracer->data + index));
   }

   return op;
}

//
// ACS_touchScriptACS0
//
static bool ACS_touchScriptACS0(bool *begin, bool *end)
{
   bool *itr;

   int32_t touchCount = 0;

   for (itr = begin; itr != end; ++itr)
      touchCount += *itr;

   if(!touchCount)
   {
      for (itr = begin; itr != end; ++itr)
         *itr = true;

      return false;
   }

   return true;
}

//
// ACS_traceFuncACS0
//
// Determines how many internal codes a CALLFUNC function counts for. In
// particular, in the event it is translated to something else.
//
static uint32_t ACS_traceFuncACS0(uint32_t func, uint32_t argc)
{
   switch(func)
   {
   case   9: // ACS_FUNC_GetThingMomX
   case  10: // ACS_FUNC_GetThingMomY
   case  11: // ACS_FUNC_GetThingMomZ
      if(argc == 1)
         return 2;
      break;

   case  15: // ACS_FUNC_GetChar
      if(argc == 2)
         return 1;
   }

   return 3;
}

//
// ACS_traceScriptACS0
//
static void ACS_traceScriptACS0(acs0_tracer_t *tracer, uint32_t index)
{
   uint32_t indexNext, opSize;
   int32_t op;
   acs0_opdata_t const *opdata;

   for(;;)
   {
      // Read next instruction from file.
      op = ACS_readOpACS0(tracer, &opSize, index);

      // Invalid opcode terminates tracer.
      if(op >= ACS0_OPMAX || op < 0)
      {
         // But flag it so that a KILL gets generated by the translator.
         ACS_touchScriptACS0(tracer->codeTouched + index, tracer->codeTouched + index + opSize);
         ++tracer->vm->numCode;
         return;
      }

      opdata = &ACS0opdata[op];

      // Calculate next index.
      indexNext = index + ACS_countOpSizeACS0(tracer, index, opSize, opdata);

      // Leaving the bounds of the lump also terminates the tracer.
      if(indexNext > tracer->lumpLength) return;

      // If already touched this instruction, stop tracing.
      if(ACS_touchScriptACS0(tracer->codeTouched + index, tracer->codeTouched + indexNext))
         return;

      // Determine how many internal codes this counts for.
      switch(op)
      {
      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
         tracer->vm->numCode += opdata->args + 2;
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         tracer->vm->numCode += *(tracer->data + index + opSize) + 2;
         break;

      case ACS0_OP_ACTIVATORHEALTH:
      case ACS0_OP_ACTIVATORARMOR:
      case ACS0_OP_ACTIVATORFRAGS:
      case ACS0_OP_BRANCH_RETURNVOID:
      case ACS0_OP_PLAYERNUMBER:
      case ACS0_OP_ACTIVATORTID:
      case ACS0_OP_SIGILPIECES:
         tracer->vm->numCode += opdata->opdata->args + 1 + 2; // GET_IMM 0
         break;

      case ACS0_OP_GAMETYPE_ONEFLAGCTF:
         tracer->vm->numCode += 2; // GET_IMM
         break;
      case ACS0_OP_GAMETYPE_SINGLEPLAYER:
         tracer->vm->numCode += 4; // GAMETYPE + GET_IMM + CMP_EQ
         break;

      case ACS0_OP_BRANCH_CALLDISCARD:
         tracer->vm->numCode += opdata->opdata->args + 1 + 1; // DROP
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         tracer->vm->numCode += 2;
         // More alignment stuff. (Wow, that's a mouthfull.)
         tracer->vm->numCode += SwapULong(*(uint32_t *)(((uintptr_t)tracer->data + index + opSize + 3) & ~3)) * 2;
         break;

      case ACS0_OP_CALLFUNC:
         if(tracer->compressed)
         {
            uint8_t  argc = tracer->data[index + opSize];
            uint16_t func = SwapUShort(*(uint16_t *)(tracer->data + index + opSize + 1));
            tracer->vm->numCode += ACS_traceFuncACS0(func, argc);
         }
         else
         {
            uint32_t argc = SwapULong(*(uint32_t *)(tracer->data + index + opSize + 0));
            uint32_t func = SwapULong(*(uint32_t *)(tracer->data + index + opSize + 4));
            tracer->vm->numCode += ACS_traceFuncACS0(func, argc);
         }
         break;

      default:
         // Translation to CALLFUNC.
         if(opdata->opdata->op == ACS_OP_CALLFUNC_IMM)
         {
            // Adds the func and argc arguments.
            tracer->vm->numCode += opdata->args + 1 + 2;
            break;
         }

         // Direct translation.
#ifdef RANGECHECK
         // This should never happen.
         if(opdata->opdata->args == -1)
            I_Error("Unknown translation for opcode. opcode %i", (int)op);
#endif

         tracer->vm->numCode += opdata->opdata->args + 1;
         break;
      }

      // Advance the index past the instruction.
      switch(op)
      {
      case ACS0_OP_SCRIPT_TERMINATE:
      case ACS0_OP_BRANCH_RETURN:
      case ACS0_OP_BRANCH_RETURNVOID:
         return;

      case ACS0_OP_BRANCH_IMM:
         ++tracer->jumpCount;
         index = SwapLong(*(int32_t *)(tracer->data + index + opSize));
         continue;

      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         ++tracer->jumpCount;
         ACS_traceScriptACS0(tracer, SwapLong(*(int32_t *)(tracer->data + index + opSize)));
         break;

      case ACS0_OP_BRANCH_CASE:
         ++tracer->jumpCount;
         ACS_traceScriptACS0(tracer, SwapLong(*(int32_t *)(tracer->data + index + opSize + 4)));
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         {
            uint32_t jumps, *rover;

            rover = (uint32_t *)(tracer->data + index + opSize);
            // And alignment again.
            rover = (uint32_t *)(((uintptr_t)rover + 3) & ~3);
            jumps = SwapULong(*rover++);

            tracer->jumpCount += jumps;

            // Trace all of the jump targets.
            // Start by incrementing rover to point to address.
            for(++rover; jumps--; rover += 2)
               ACS_traceScriptACS0(tracer, SwapULong(*rover));
         }
         break;
      }

      index = indexNext;
   }
}

//
// ACS_translateFuncACS0
//
// Translates a ZDoom CALLFUNC function to either an internal CALLFUNC function
// or other internal instruction.
//
static void ACS_translateFuncACS0(int32_t *&codePtr, uint32_t func, uint32_t argc)
{
   acs_funcnum_t funcnum;

   // Handle special translations here to keep the switch below clean.
   switch(func)
   {
   case   9: // ACS_FUNC_GetThingMomX
   case  10: // ACS_FUNC_GetThingMomY
   case  11: // ACS_FUNC_GetThingMomZ
      if(argc == 1)
      {
         *codePtr++ = ACS_OP_GET_THINGVAR;
         *codePtr++ = ACS_THINGVAR_MomX + (func - 9);
         return;
      }
      break;

   case  15: // ACS_FUNC_GetChar
      if(argc == 2)
      {
         *codePtr++ = ACS_OP_GET_STRINGARR;
         return;
      }
      break;
   }

   switch(func)
   {
   default:
      *codePtr++ = ACS_OP_KILL;
      *codePtr++ = 0;
      *codePtr++ = 0;
      return;

   case   0: funcnum = ACS_FUNC_NOP;                     break;
 //case   1: funcnum = ACS_FUNC_GetLineUDMFInt;          break;
 //case   2: funcnum = ACS_FUNC_GetLineUDMFFixed;        break;
 //case   3: funcnum = ACS_FUNC_GetThingUDMFInt;         break;
 //case   4: funcnum = ACS_FUNC_GetThingUDMFFixed;       break;
 //case   5: funcnum = ACS_FUNC_GetSectorUDMFInt;        break;
 //case   6: funcnum = ACS_FUNC_GetSectorUDMFFixed;      break;
 //case   7: funcnum = ACS_FUNC_GetSideUDMFInt;          break;
 //case   8: funcnum = ACS_FUNC_GetSideUDMFFixed;        break;
 //case   9: funcnum = ACS_FUNC_GetThingMomX;            break;
 //case  10: funcnum = ACS_FUNC_GetThingMomY;            break;
 //case  11: funcnum = ACS_FUNC_GetThingMomZ;            break;
   case  12: funcnum = ACS_FUNC_SetActivator;            break;
   case  13: funcnum = ACS_FUNC_SetActivatorToTarget;    break;
 //case  14: funcnum = ACS_FUNC_GetThingViewHeight;      break;
 //case  15: funcnum = ACS_FUNC_GetChar;                 break;
 //case  16: funcnum = ACS_FUNC_GetAirSupply;            break;
 //case  17: funcnum = ACS_FUNC_SetAirSupply;            break;
   case  18: funcnum = ACS_FUNC_SetSkyDelta;             break;
 //case  19: funcnum = ACS_FUNC_GetArmorType;            break;
   case  20: funcnum = ACS_FUNC_SpawnSpotForced;         break;
   case  21: funcnum = ACS_FUNC_SpawnSpotAngleForced;    break;
   case  22: funcnum = ACS_FUNC_ChkThingVar;             break;
   case  23: funcnum = ACS_FUNC_SetThingMomentum;        break;
 //case  24: funcnum = ACS_FUNC_SetUserVar;              break;
 //case  25: funcnum = ACS_FUNC_GetUserVar;              break;
   case  26: funcnum = ACS_FUNC_RadiusQuake;             break;
   case  27: funcnum = ACS_FUNC_CheckThingType;          break;
 //case  28: funcnum = ACS_FUNC_SetUserArr;              break;
 //case  29: funcnum = ACS_FUNC_GetUserArr;              break;
   case  30: funcnum = ACS_FUNC_SoundSequenceThing;      break;
 //case  31: funcnum = ACS_FUNC_SoundSequenceSector;     break;
 //case  32: funcnum = ACS_FUNC_SoundSequencePolyobj;    break;
   case  33: funcnum = ACS_FUNC_GetPolyobjX;             break;
   case  34: funcnum = ACS_FUNC_GetPolyobjY;             break;
   case  35: funcnum = ACS_FUNC_CheckSight;              break;
   case  36: funcnum = ACS_FUNC_SpawnPointForced;        break;
 //case  37: funcnum = ACS_FUNC_AnnouncerSound;          break;
 //case  38: funcnum = ACS_FUNC_SetPointer;              break;
   case  39: funcnum = ACS_FUNC_ExecuteScriptName;       break;
   case  40: funcnum = ACS_FUNC_SuspendScriptName;       break;
   case  41: funcnum = ACS_FUNC_TerminateScriptName;     break;
 //case  42: funcnum = ACS_FUNC_ExecuteScriptLockedName; break;
 //case  43: funcnum = ACS_FUNC_ExecuteScriptDoorName;   break;
   case  44: funcnum = ACS_FUNC_ExecuteScriptResultName; break;
   case  45: funcnum = ACS_FUNC_ExecuteScriptAlwaysName; break;
   case  46: funcnum = ACS_FUNC_UniqueTID;               break;
   case  47: funcnum = ACS_FUNC_IsTIDUsed;               break;
   }

   *codePtr++ = ACS_OP_CALLFUNC_ZD;
   *codePtr++ = funcnum;
   *codePtr++ = argc;
}

//
// ACS_translateFuncACS0
//
// Translates an ACS0 opdata into a CALLFUNC func/argc pair.
//
static void ACS_translateFuncACS0(int32_t *&codePtr, const acs0_opdata_t *opdata)
{
   acs_funcnum_t funcnum;
   uint32_t argc;

   #define CASE(OP,FUNC,ARGC) \
      case ACS0_OP_##OP: funcnum = ACS_FUNC_##FUNC; argc = ARGC; break
   #define CASE_IMM(OP,FUNC,ARGC) \
      case ACS0_OP_##OP##_IMM: CASE(OP, FUNC, ARGC)

   switch(opdata->op)
   {
   CASE(ACTIVATORSOUND,         ActivatorSound,         2);
   CASE(AMBIENTSOUND,           AmbientSound,           2);
   CASE(AMBIENTSOUNDLOCAL,      AmbientSoundLocal,      2);
   CASE(CLASSIFYTHING,          ClassifyThing,          1);
   CASE(GETPLAYERINPUT,         GetPlayerInput,         2);
   CASE(GET_THINGARR,           GetThingVar,            2);
   CASE(GETSECTORCEILINGZ,      GetSectorCeilingZ,      3);
   CASE(GETSECTORFLOORZ,        GetSectorFloorZ,        3);
   CASE(GETSECTORLIGHTLEVEL,    GetSectorLightLevel,    1);
   CASE(REPLACETEXTURES,        ReplaceTextures,        3);
   CASE(SECTORDAMAGE,           SectorDamage,           5);
   CASE(SECTORSOUND,            SectorSound,            2);
   CASE(SETLINEBLOCKING,        SetLineBlocking,        2);
   CASE(SETLINEMONSTERBLOCKING, SetLineMonsterBlocking, 2);
   CASE(SETLINESPECIAL,         SetLineSpecial,         7);
   CASE(SETLINETEXTURE,         SetLineTexture,         4);
   CASE(SETMUSIC_ST,            SetMusic,               1);
   CASE(SETTHINGPOSITION,       SetThingPosition,       5);
   CASE(SETTHINGSPECIAL,        SetThingSpecial,        7);
   CASE(SETTHINGSTATE,          SetThingState,          3);
   CASE(SET_THINGARR,           SetThingVar,            3);
   CASE(SOUNDSEQUENCE,          SoundSequence,          1);
   CASE(SPAWNPROJECTILE,        SpawnProjectile,        7);
   CASE(SPAWNSPOTANGLE,         SpawnSpotAngle,         3);
   CASE(THINGCOUNTNAME,         ThingCountName,         2);
   CASE(THINGCOUNTNAMESECTOR,   ThingCountNameSector,   3);
   CASE(THINGCOUNTSECTOR,       ThingCountSector,       3);
   CASE(THINGDAMAGE,            ThingDamage,            3);
   CASE(THINGPROJECTILE,        ThingProjectile,        7);
   CASE(THINGSOUND,             ThingSound,             3);

   CASE_IMM(CHANGECEILING, ChangeCeiling, 2);
   CASE_IMM(CHANGEFLOOR,   ChangeFloor,   2);
   case ACS0_OP_RANDOM_IMM_BYTE:
   CASE_IMM(RANDOM,        Random,        2);
   CASE_IMM(SETMUSIC,      SetMusic,      3);
   CASE_IMM(SETMUSICLOCAL, SetMusicLocal, 3);
   CASE_IMM(SPAWNPOINT,    SpawnPoint,    6);
   CASE_IMM(SPAWNSPOT,     SpawnSpot,     4);
   CASE_IMM(THINGCOUNT,    ThingCount,    2);

   default: opdata = &ACS0opdata[ACS_OP_KILL]; CASE(NOP, NOP, 0);
   }

   #undef CASE_IMM
   #undef CASE

   *codePtr++ = opdata->opdata->op;
   *codePtr++ = funcnum;
   *codePtr++ = argc;
}

//
// ACS_translateThingVarACS0
//
// Translates an ACS0 opdata into a THINGVAR to access.
//
static int32_t ACS_translateThingVarACS0(const acs0_opdata_t *opdata)
{
   #define CASE(OP,VAR) case ACS0_OP_##OP: return ACS_THINGVAR_##VAR

   switch(opdata->op)
   {
   CASE(ACTIVATORARMOR,          Armor);
   CASE(ACTIVATORFRAGS,          Frags);
   CASE(ACTIVATORHEALTH,         Health);
   CASE(ACTIVATORTID,            TID);
   CASE(CHK_THINGCEILINGTEXTURE, CeilingTexture);
   CASE(CHK_THINGFLOORTEXTURE,   FloorTexture);
   CASE(GET_THINGANGLE,          Angle);
   CASE(GET_THINGCEILINGZ,       CeilingZ);
   CASE(GET_THINGFLOORZ,         FloorZ);
   CASE(GET_THINGLIGHTLEVEL,     LightLevel);
   CASE(GET_THINGPITCH,          Pitch);
   CASE(GET_THINGX,              X);
   CASE(GET_THINGY,              Y);
   CASE(GET_THINGZ,              Z);
   CASE(PLAYERNUMBER,            PlayerNumber);
   CASE(SET_THINGANGLE,          Angle);
   CASE(SET_THINGPITCH,          Pitch);
   CASE(SIGILPIECES,             SigilPieces);

   default: return ACS_THINGVARMAX;
   }

   #undef CASE
}

//
// ACS_translateScriptACS0
//
static void ACS_translateScriptACS0(acs0_tracer_t *tracer, int32_t *codeIndexMap)
{
   byte *brover;
   int32_t *codePtr = tracer->vm->code, *rover;
   uint32_t index, opSize;
   int32_t op, temp;
   acs0_opdata_t const *opdata;
   int32_t **jumps, **jumpItr;

   // This is used to store all of the places that need to have a jump target
   // translated. The count was determined by the tracer.
   jumps = (int32_t **)Z_Malloc(tracer->jumpCount * sizeof(int32_t *), PU_STATIC, NULL);
   jumpItr = jumps;

   // Set the first instruction to a KILL.
   *codePtr++ = ACS_OP_KILL;

   for(index = 0; index < tracer->lumpLength;)
   {
      // Search for code.
      if(!tracer->codeTouched[index])
      {
         ++index;
         continue;
      }

      // Read next instruction from file.
      op = ACS_readOpACS0(tracer, &opSize, index);

      rover = (int32_t *)(tracer->data + index + opSize);

      // Unrecognized instructions need to stop execution.
      if(op >= ACS0_OPMAX || op < 0)
      {
         *codePtr++ = ACS_OP_KILL;
         index += opSize;
         continue;
      }

      opdata = &ACS0opdata[op];

      // Record jump target.
      codeIndexMap[index] = static_cast<int32_t>(codePtr - tracer->vm->code);

      // Calculate next index.
      index += ACS_countOpSizeACS0(tracer, index, opSize, opdata);

      switch(op)
      {
      case ACS0_OP_LINESPEC1:
      case ACS0_OP_LINESPEC2:
      case ACS0_OP_LINESPEC3:
      case ACS0_OP_LINESPEC4:
      case ACS0_OP_LINESPEC5:
         temp = op - ACS0_OP_LINESPEC1 + 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         break;

      case ACS0_OP_LINESPEC1_IMM:
      case ACS0_OP_LINESPEC2_IMM:
      case ACS0_OP_LINESPEC3_IMM:
      case ACS0_OP_LINESPEC4_IMM:
      case ACS0_OP_LINESPEC5_IMM:
         temp = opdata->args - 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_LINESPEC5_RET:
         temp = op - ACS0_OP_LINESPEC5_RET + 5;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = temp;
         break;

      case ACS0_OP_LINESPEC1_IMM_BYTE:
      case ACS0_OP_LINESPEC2_IMM_BYTE:
      case ACS0_OP_LINESPEC3_IMM_BYTE:
      case ACS0_OP_LINESPEC4_IMM_BYTE:
      case ACS0_OP_LINESPEC5_IMM_BYTE:
         brover = (byte *)rover;
         temp = opdata->args - 1;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = *brover++;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_GET_IMM_BYTE:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = *(byte *)rover;
         break;

      case ACS0_OP_GET2_IMM_BYTE:
      case ACS0_OP_GET3_IMM_BYTE:
      case ACS0_OP_GET4_IMM_BYTE:
      case ACS0_OP_GET5_IMM_BYTE:
         brover = (byte *)rover;
         temp = op - ACS0_OP_GET2_IMM_BYTE + 2;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_GET_THINGX:
      case ACS0_OP_GET_THINGY:
      case ACS0_OP_GET_THINGZ:
      case ACS0_OP_GET_THINGFLOORZ:
      case ACS0_OP_GET_THINGANGLE:
      case ACS0_OP_SET_THINGANGLE:
      case ACS0_OP_GET_THINGCEILINGZ:
      case ACS0_OP_GET_THINGPITCH:
      case ACS0_OP_SET_THINGPITCH:
      case ACS0_OP_CHK_THINGCEILINGTEXTURE:
      case ACS0_OP_CHK_THINGFLOORTEXTURE:
      case ACS0_OP_GET_THINGLIGHTLEVEL:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_translateThingVarACS0(opdata);
         break;

      case ACS0_OP_GETARR_IMM_BYTE:
         brover = (byte *)rover;
         temp = *brover++;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;
         while(temp--)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_BRANCH_IMM:
      case ACS0_OP_BRANCH_NOTZERO:
      case ACS0_OP_BRANCH_ZERO:
         *jumpItr++ = codePtr + 1;
         goto case_direct;

      case ACS0_OP_BRANCH_CASE:
         *jumpItr++ = codePtr + 2;
         goto case_direct;

      case ACS0_OP_CHANGEFLOOR_IMM:
      case ACS0_OP_CHANGECEILING_IMM:
         ACS_translateFuncACS0(codePtr, opdata);
         *codePtr++ = SwapLong(*rover++);
         *codePtr++ = tracer->vm->getStringIndex(SwapLong(*rover++)); // tag string
         for(int i = opdata->args - 2; i--;)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_DELAY_IMM_BYTE:
         brover = (byte *)rover;
         *codePtr++ = opdata->opdata->op;
         for(int i = opdata->args; i--;)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_ACTIVATORHEALTH:
      case ACS0_OP_ACTIVATORARMOR:
      case ACS0_OP_ACTIVATORFRAGS:
      case ACS0_OP_PLAYERNUMBER:
      case ACS0_OP_ACTIVATORTID:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_translateThingVarACS0(opdata);
         break;

      case ACS0_OP_GAMETYPE_ONEFLAGCTF:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         break;

      case ACS0_OP_GAMETYPE_SINGLEPLAYER:
         *codePtr++ = opdata->opdata->op;
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = gt_single;
         *codePtr++ = ACS_OP_CMP_EQ;
         break;

      case ACS0_OP_RANDOM_IMM_BYTE:
         brover = (byte *)rover;
         ACS_translateFuncACS0(codePtr, opdata);
         for(int i = opdata->args; i--;)
            *codePtr++ = *brover++;
         break;

      case ACS0_OP_SETMUSIC_IMM:
      case ACS0_OP_SETMUSICLOCAL_IMM:
      case ACS0_OP_SPAWNPOINT_IMM:
      case ACS0_OP_SPAWNSPOT_IMM:
         ACS_translateFuncACS0(codePtr, opdata);
         *codePtr++ = tracer->vm->getStringIndex(SwapLong(*rover++)); // tag string
         for(int i = opdata->args - 1; i--;)
            *codePtr++ = SwapLong(*rover++);
         break;

      case ACS0_OP_BRANCH_CALLDISCARD:
         *codePtr++ = opdata->opdata->op;
         if(tracer->compressed)
            *codePtr++ = *(byte *)rover;
         else
            *codePtr++ = SwapLong(*rover);
         *codePtr++ = ACS_OP_STACK_DROP;
         break;

      case ACS0_OP_BRANCH_RETURNVOID:
         *codePtr++ = ACS_OP_GET_IMM;
         *codePtr++ = 0;
         *codePtr++ = opdata->opdata->op;
         break;

      case ACS0_OP_BRANCH_CASETABLE:
         // Align rover.
         rover = (int32_t *)(((uintptr_t)rover + 3) & ~3);
         temp = SwapLong(*rover++);

         *codePtr++ = opdata->opdata->op;
         *codePtr++ = temp;

         while(temp--)
         {
            *codePtr++ = SwapLong(*rover++);
            *jumpItr++ = codePtr;
            *codePtr++ = SwapLong(*rover++);
         }
         break;

      case ACS0_OP_CALLFUNC:
         if(tracer->compressed)
         {
            brover = (uint8_t *)rover;
            uint8_t  argc = *brover++;
            uint16_t func = SwapUShort(*(uint16_t *)brover);
            ACS_translateFuncACS0(codePtr, func, argc);
         }
         else
            ACS_translateFuncACS0(codePtr, SwapLong(rover[1]), SwapLong(rover[0]));
         break;

      default: case_direct:
         // Direct translation.
         if(opdata->opdata->op == ACS_OP_CALLFUNC ||
            opdata->opdata->op == ACS_OP_CALLFUNC_IMM)
         {
            ACS_translateFuncACS0(codePtr, opdata);
         }
         else
            *codePtr++ = opdata->opdata->op;

         if(tracer->compressed && opdata->compressed)
         {
            brover = (byte *)rover;
            for(int i = opdata->args; i--;)
               *codePtr++ = *brover++;
         }
         else
         {
            for(int i = opdata->args; i--;)
               *codePtr++ = SwapLong(*rover++);
         }

         break;
      }
   }

   // Set the last instruction to a KILL.
   *codePtr++ = ACS_OP_KILL;

#ifdef RANGECHECK
   // If this isn't true, something very wrong has happened internally.
   if(codePtr != tracer->vm->code + tracer->vm->numCode)
   {
      I_Error("Incorrect code count. %i/%i/%i", (int)(codePtr - tracer->vm->code),
              (int)tracer->vm->numCode, (int)(tracer->lumpLength / 4));
   }

   // Same here, this is just a sanity check.
   if(jumpItr != jumps + tracer->jumpCount)
   {
      I_Error("Incorrect jump count. %i/%i", (int)(jumpItr - jumps),
              (int)tracer->jumpCount);
   }
#endif

   // Translate jumps. Has to be done after code in order to jump forward.
   while(jumpItr != jumps)
   {
      codePtr = *--jumpItr;

      if ((uint32_t)*codePtr < tracer->lumpLength)
         *codePtr = codeIndexMap[*codePtr];
      else
         *codePtr = 0;
   }
}


//----------------------------------------------------------------------------|
// Global Functions                                                           |
//

//
// ACS_LoadScriptACS0
//
void ACS_LoadScriptACS0(ACSVM *vm, WadDirectory *dir, int lump, byte *data)
{
   uint32_t lumpAvail; // Used in bounds checking.
   uint32_t lumpLength = dir->lumpLength(lump);
   int32_t *rover;
   uint32_t tableIndex;

   // Header + table index + script count + string count = 16 bytes.
   if (lumpLength < 16) return;
   lumpAvail = lumpLength - 16;

   rover = (int32_t *)data + 1;

   // Find script table.
   tableIndex = SwapLong(*rover);

   // Aha, but there might really be an ACSE header here!
   if(tableIndex >= 8 && tableIndex <= lumpLength)
   {
      uint32_t fakeHeader = SwapULong(*(uint32_t *)(data + tableIndex - 4));

      if(fakeHeader == ACS_CHUNKID('A', 'C', 'S', 'E'))
      {
         ACS_LoadScriptACSE(vm, dir, lump, data, tableIndex - 8);
         return;
      }
      else if(fakeHeader == ACS_CHUNKID('A', 'C', 'S', 'e'))
      {
         ACS_LoadScriptACSe(vm, dir, lump, data, tableIndex - 8);
         return;
      }
   }

   // At the index there must be at least the script count and string count.
   // Subtracting from lumpLength instead of adding to tableIndex in case the
   // latter would cause an overflow.
   if (tableIndex > lumpLength - 8) return;

   rover = (int32_t *)(data + tableIndex);

   // Read script count.
   vm->numScripts = SwapLong(*rover++);

   // Verify that there is enough space for the given number of scripts.
   if (vm->numScripts * 12 > lumpAvail) return;
   lumpAvail -= vm->numScripts * 12;

   // Also verify that the string count will be inside the lump.
   if (tableIndex + 8 > lumpLength - (vm->numScripts * 12)) return;
   tableIndex += 8 + (vm->numScripts * 12);

   // Read scripts.
   vm->scripts = estructalloctag(ACSScript, vm->numScripts, PU_LEVEL);

   for(ACSScript *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
   {
      itr->number    = SwapLong(*rover++);
      itr->codeIndex = SwapLong(*rover++);
      itr->numArgs   = SwapLong(*rover++);

      itr->numVars   = ACS_NUM_LOCALVARS;
      itr->vm        = vm;

      if(itr->number >= 1000)
      {
         itr->type = ACS_STYPE_OPEN;
         itr->number -= 1000;
      }
      else
         itr->type = ACS_STYPE_CLOSED;
   }

   // Read string count.
   vm->numStrings = SwapLong(*rover++);

   // Again, verify that there is enough space for the table first.
   if (vm->numStrings * 4 > lumpAvail) return;
   lumpAvail -= vm->numStrings * 4;

   // This time, just verify the end of the table is in bounds.
   if (tableIndex > lumpLength - (vm->numStrings * 4)) return;

   // Read strings.
   vm->strings = (uint32_t *)Z_Malloc(vm->numStrings * sizeof(uint32_t), PU_LEVEL, NULL);

   for(uint32_t *itr = vm->strings, *end = itr + vm->numStrings; itr != end; ++itr)
   {
      tableIndex = SwapLong(*rover++);

      if (tableIndex < lumpLength)
         *itr = ACS_LoadStringACS0(data + tableIndex, data + lumpLength);
      else
         *itr = ACS_LoadStringACS0(data + lumpLength, data + lumpLength);
   }

   // The first part of the global string table must match VM-0 for compatibility.
   if(vm->id == 0 && ACSVM::GlobalNumStrings < vm->numStrings)
      vm->addStrings();

   // Read code.
   ACS_LoadScriptCodeACS0(vm, data, lumpLength, false);

   vm->loaded = true;
}

//
// ACS_LoadScriptCodeACS0
//
void ACS_LoadScriptCodeACS0(ACSVM *vm, byte *data, uint32_t lumpLength, bool compressed)
{
   acs0_tracer_t tracer = {vm, data, NULL, 0, lumpLength, compressed};

   int32_t *codeIndexMap;

   vm->numCode = 1; // Start at 1 so that 0 is an invalid index.

   // Find where there is code by tracing potential execution paths...
   tracer.codeTouched = ecalloc(bool *, lumpLength, sizeof(bool));

   // ... from scripts.
   for(ACSScript *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
   {
      ACS_traceScriptACS0(&tracer, itr->codeIndex);
   }

   // ... from functions.
   for(ACSFunc *itr = vm->funcs, *end = itr + vm->numFuncs; itr!= end; ++itr)
   {
      // Functions with code index 0 are extern.
      if(itr->codeIndex == 0) continue;

      ACS_traceScriptACS0(&tracer, itr->codeIndex);
   }

   // ... from dynamic jump targets.
   for(ACSJump *itr = vm->jumps, *end = itr + vm->numJumps; itr != end; ++itr)
   {
      ACS_traceScriptACS0(&tracer, itr->codeIndex);
   }

   ++vm->numCode; // Add a KILL to the end as well, to avoid running off the end.

   // Translate all the instructions found.
   vm->code = (int32_t *)Z_Malloc(vm->numCode * sizeof(int32_t), PU_LEVEL, NULL);
   codeIndexMap = ecalloc(int32_t *, lumpLength, sizeof(int32_t));
   ACS_translateScriptACS0(&tracer, codeIndexMap);

   efree(tracer.codeTouched);

   // Process script indexes.
   for(ACSScript *itr = vm->scripts, *end = itr + vm->numScripts; itr != end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = vm->code;

      // And names, too!
      if(itr->number < 0 && (uint32_t)(-itr->number - 1) < vm->numScriptNames)
         itr->name = ACSVM::GlobalStrings[vm->scriptNames[-itr->number - 1]]->data.s;
      else
         itr->name = "";
   }

   // Process function indexes.
   for(ACSFunc *itr = vm->funcs, *end = itr + vm->numFuncs; itr!= end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = vm->code;
   }

   // Process dynamic jump targets.
   for(ACSJump *itr = vm->jumps, *end = itr + vm->numJumps; itr != end; ++itr)
   {
      if(itr->codeIndex < lumpLength)
         itr->codePtr = vm->code + codeIndexMap[itr->codeIndex];
      else
         itr->codePtr = 0;
   }

   efree(codeIndexMap);
}

//
// ACS_LoadStringACS0
//
// Returns the index into GlobalStrings.
//
uint32_t ACS_LoadStringACS0(const byte *begin, const byte *end)
{
   qstring buf;
   const byte *itr = begin;
   char c;

   // Find either a null-terminator or the end of the lump.
   while(itr != end && *itr)
   {
      // Process escapes.
      if(*itr == '\\')
      {
         // If it's at the end of data, just append the backslash and break.
         if(++itr == end || !*itr)
         {
            buf += '\\';
            break;
         }

         switch(*itr)
         {
         default:
         case '\\':
            buf += *itr++;
            break;

         case 'a': buf += '\a';   break;
         case 'b': buf += '\b';   break;
         case 'c': buf += '\x1C'; break; // ZDoom color escape
         case 'f': buf += '\f';   break;
         case 'r': buf += '\r';   break;
         case 'n': buf += '\n';   break;
         case 't': buf += '\t';   break;
         case 'v': buf += '\v';   break;

         case '0': case '1': case '2': case '3':
         case '4': case '5': case '6': case '7':
            c = 0;

            for(int i = 3; i--;)
            {
               if(itr != end && *itr >= '0' && *itr <= '7')
                  c = c << 3 | (*itr++ - '0');
               else
                  break;
            }

            buf += c;
            break;

         case 'x':
            c = 0; ++itr;

            for(int i = 2; i--;)
            {
               if(itr == end) break;

               if(*itr >= '0' && *itr <= '9')
                  c = c << 4 | (*itr++ - '0');
               else if(*itr >= 'A' && *itr <= 'F')
                  c = c << 4 | (*itr++ - 'A' + 10);
               else if(*itr >= 'a' && *itr <= 'a')
                  c = c << 4 | (*itr++ - 'a' + 10);
               else
                  break;
            }

            buf += c;
            break;
         }
      }
      else
         buf += *itr++;
   }

   return ACSVM::AddString(buf.constPtr(), static_cast<uint32_t>(buf.length()));
}

// EOF

