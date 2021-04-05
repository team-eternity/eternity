//
// The Eternity Engine
// Copyright(C) 2020 James Haley, Max Waine, et al.
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
//----------------------------------------------------------------------------
//
// Purpose: Aeon wrapper for Mobj
// Authors: Max Waine
//

#include "aeon_common.h"
#include "aeon_math.h"
#include "aeon_mobj.h"
#include "aeon_system.h"
#include "d_dehtbl.h"
#include "e_things.h"
#include "m_qstr.h"
#include "p_enemy.h"
#include "p_mobj.h"
#include "p_pspr.h"
#include "s_sound.h"

#include "p_map.h"

namespace Aeon
{
   static Mobj *mobjFactory()
   {
      return new Mobj();
   }

   static Mobj *mobjFactoryFromOther(const Mobj &in)
   {
      return new Mobj(in);
   }

   //
   // Sanity checked getter for mo->counters[ctrnum]
   // Returns 0 on failure
   //
   static int getMobjCounter(const unsigned int ctrnum, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         return mo->counters[ctrnum];
      else
         return 0; // TODO: C_Printf warning?
   }

   //
   // Sanity checked setter for mo->counters[ctrnum]
   // Doesn't set on failure
   //
   static void setMobjCounter(const unsigned int ctrnum, const int val, Mobj *mo)
   {
      if(ctrnum >= 0 && ctrnum < NUMMOBJCOUNTERS)
         mo->counters[ctrnum] = val;
      // TODO: else C_Printf warning?
   }

   template <int flag, typename T, unsigned int T::* flagMember>
   static bool getFlag(const T *const mobjOrInfo)
   {
      return !!(mobjOrInfo->*flagMember & flag);
   }

   template <int flag, unsigned int Mobj::* flagMember>
   static void setFlag(const bool isTrue, Mobj *const mo)
   {
      if(isTrue)
         mo->*flagMember |= flag;
      else
         mo->*flagMember &= ~flag;
   }

   static fixed_t floatBobOffsets(const int in)
   {
      static constexpr int NUMFLOATBOBOFFSETS = earrlen(FloatBobOffsets);
      if(in < 0 || in > NUMFLOATBOBOFFSETS)
         return 0;
      return FloatBobOffsets[in];
   }

   static Mobj *spawnMobj(fixed_t x, fixed_t y, fixed_t z, const qstring &name)
   {
      return P_SpawnMobj(x, y, z, E_SafeThingName(name.constPtr()));
   }

   static Mobj *spawnMissile(Mobj *source, Mobj *dest, const qstring &type, fixed_t z)
   {
      return P_SpawnMissile(source, dest, E_SafeThingName(type.constPtr()), z);
   }

   static Mobj *spawnMissileAngle(Mobj *source, const qstring &type, angle_t angle, fixed_t momz, fixed_t z)
   {
      return P_SpawnMissileAngle(source, E_SafeThingName(type.constPtr()),
                                 angle, momz, z);
   }

   static void startSound(const PointThinker *origin, const sfxinfo_t &sfxinfo)
   {
      S_StartSound(origin, sfxinfo.dehackednum);
   }

   static void weaponSound(Mobj *t1, const sfxinfo_t &sfxinfo)
   {
      P_WeaponSound(t1, sfxinfo.dehackednum);
   }

   static fixed_t aimLineAttack(Mobj *t1, angle_t angle, fixed_t distance, int flags)
   {
      return P_AimLineAttack(t1, angle, distance, flags & 1 ? true : false);
   }

   static void lineAttack(Mobj *t1, angle_t angle, fixed_t distance, fixed_t slope,
                          int damage, const qstring &pufftype)
   {
      P_LineAttack(t1, angle, distance, slope, damage, pufftype.constPtr());
   }

   // Flags for both Mobj and mobjinfo_t
#define MOBJ_FLAG_FIELDS \
   FLAG_ACCESSOR(special,          MF_SPECIAL,           flags),  FLAG_ACCESSOR(solid,              MF_SOLID,               flags),  \
   FLAG_ACCESSOR(shootable,        MF_SHOOTABLE,         flags),  FLAG_ACCESSOR(nosector,           MF_NOSECTOR,            flags),  \
   FLAG_ACCESSOR(noblockmap,       MF_NOBLOCKMAP,        flags),  FLAG_ACCESSOR(ambush,             MF_AMBUSH,              flags),  \
   FLAG_ACCESSOR(justhit,          MF_JUSTHIT,           flags),  FLAG_ACCESSOR(justattack,         MF_JUSTATTACKED,        flags),  \
   FLAG_ACCESSOR(spawnceiling,     MF_SPAWNCEILING,      flags),  FLAG_ACCESSOR(nogravity,          MF_NOGRAVITY,           flags),  \
   FLAG_ACCESSOR(dropoff,          MF_DROPOFF,           flags),  FLAG_ACCESSOR(pickup,             MF_PICKUP,              flags),  \
   FLAG_ACCESSOR(noclip,           MF_NOCLIP,            flags),  /* MF_SLIDE does nothing so it isn't exposed*/                     \
   FLAG_ACCESSOR(float,            MF_FLOAT,             flags),  FLAG_ACCESSOR(teleport,           MF_TELEPORT,            flags),  \
   FLAG_ACCESSOR(missile,          MF_MISSILE,           flags),  FLAG_ACCESSOR(dropped,            MF_DROPPED,             flags),  \
   FLAG_ACCESSOR(shadow,           MF_SHADOW,            flags),  FLAG_ACCESSOR(noblood,            MF_NOBLOOD,             flags),  \
   FLAG_ACCESSOR(corpse,           MF_CORPSE,            flags),  FLAG_ACCESSOR(infloat,            MF_INFLOAT,             flags),  \
   FLAG_ACCESSOR(countkill,        MF_COUNTKILL,         flags),  FLAG_ACCESSOR(countitem,          MF_COUNTITEM,           flags),  \
   FLAG_ACCESSOR(skullfly,         MF_SKULLFLY,          flags),  FLAG_ACCESSOR(notdmatch,          MF_NOTDMATCH,           flags),  \
   FLAG_ACCESSOR(translation1,     0x04000000,           flags),  FLAG_ACCESSOR(translation2,       0x08000000,             flags),  \
   FLAG_ACCESSOR(touchy,           MF_TOUCHY,            flags),  FLAG_ACCESSOR(bounces,            MF_BOUNCES,             flags),  \
   FLAG_ACCESSOR(friend,           MF_FRIEND,            flags),  FLAG_ACCESSOR(translucent,        MF_TRANSLUCENT,         flags),  \
                                                                                                                                     \
   FLAG_ACCESSOR(lograv,           MF2_LOGRAV,           flags2), FLAG_ACCESSOR(nosplash,           MF2_NOSPLASH,           flags2), \
   FLAG_ACCESSOR(nostrafe,         MF2_NOSTRAFE,         flags2), FLAG_ACCESSOR(norespawn,          MF2_NORESPAWN,          flags2), \
   FLAG_ACCESSOR(alwaysrespawn,    MF2_ALWAYSRESPAWN,    flags2), FLAG_ACCESSOR(removedead,         MF2_REMOVEDEAD,         flags2), \
   FLAG_ACCESSOR(nothrust,         MF2_NOTHRUST,         flags2), FLAG_ACCESSOR(nocross,            MF2_NOCROSS,            flags2), \
   FLAG_ACCESSOR(jumpdown,         MF2_JUMPDOWN,         flags2), FLAG_ACCESSOR(pushable,           MF2_PUSHABLE,           flags2), \
   FLAG_ACCESSOR(map07boss1,       MF2_MAP07BOSS1,       flags2), FLAG_ACCESSOR(map07boss2,         MF2_MAP07BOSS2,         flags2), \
   FLAG_ACCESSOR(e1m8boss,         MF2_E1M8BOSS,         flags2), FLAG_ACCESSOR(e2m8boss,           MF2_E2M8BOSS,           flags2), \
   FLAG_ACCESSOR(e3m8boss,         MF2_E3M8BOSS,         flags2), FLAG_ACCESSOR(boss,               MF2_BOSS,               flags2), \
   FLAG_ACCESSOR(e4m6boss,         MF2_E4M6BOSS,         flags2), FLAG_ACCESSOR(e4m8boss,           MF2_E4M8BOSS,           flags2), \
   FLAG_ACCESSOR(footclip,         MF2_FOOTCLIP,         flags2), FLAG_ACCESSOR(floatbob,           MF2_FLOATBOB,           flags2), \
   FLAG_ACCESSOR(dontdraw,         MF2_DONTDRAW,         flags2), FLAG_ACCESSOR(shortmrange,        MF2_SHORTMRANGE,        flags2), \
   FLAG_ACCESSOR(longmelee,        MF2_LONGMELEE,        flags2), FLAG_ACCESSOR(rangehalf,          MF2_RANGEHALF,          flags2), \
   FLAG_ACCESSOR(highermprob,      MF2_HIGHERMPROB,      flags2), FLAG_ACCESSOR(cantleavefloorpic,  MF2_CANTLEAVEFLOORPIC,  flags2), \
   FLAG_ACCESSOR(spawnfloat,       MF2_SPAWNFLOAT,       flags2), FLAG_ACCESSOR(invulnerable,       MF2_INVULNERABLE,       flags2), \
   FLAG_ACCESSOR(dormant,          MF2_DORMANT,          flags2), FLAG_ACCESSOR(seekermissile,      MF2_SEEKERMISSILE,      flags2), \
   FLAG_ACCESSOR(deflective,       MF2_DEFLECTIVE,       flags2), FLAG_ACCESSOR(reflective,         MF2_REFLECTIVE,         flags2), \
                                                                                                                                     \
   FLAG_ACCESSOR(ghost,            MF3_GHOST,            flags3), FLAG_ACCESSOR(thrughost,          MF3_THRUGHOST,          flags3), \
   FLAG_ACCESSOR(nodmgthrust,      MF3_NODMGTHRUST,      flags3), FLAG_ACCESSOR(actseesound,        MF3_ACTSEESOUND,        flags3), \
   FLAG_ACCESSOR(loudactive,       MF3_LOUDACTIVE,       flags3), FLAG_ACCESSOR(e5m8boss,           MF3_E5M8BOSS,           flags3), \
   FLAG_ACCESSOR(dmgignored,       MF3_DMGIGNORED,       flags3), FLAG_ACCESSOR(bossignore,         MF3_BOSSIGNORE,         flags3), \
   FLAG_ACCESSOR(slide,            MF3_SLIDE,            flags3), FLAG_ACCESSOR(telestomp,          MF3_TELESTOMP,          flags3), \
   FLAG_ACCESSOR(windthrust,       MF3_WINDTHRUST,       flags3), FLAG_ACCESSOR(firedamage,         MF3_FIREDAMAGE,         flags3), \
   FLAG_ACCESSOR(killable,         MF3_KILLABLE,         flags3), FLAG_ACCESSOR(deadfloat,          MF3_DEADFLOAT,          flags3), \
   FLAG_ACCESSOR(nothreshold,      MF3_NOTHRESHOLD,      flags3), FLAG_ACCESSOR(floormissile,       MF3_FLOORMISSILE,       flags3), \
   FLAG_ACCESSOR(superitem,        MF3_SUPERITEM,        flags3), FLAG_ACCESSOR(noitemresp,         MF3_NOITEMRESP,         flags3), \
   FLAG_ACCESSOR(superfriend,      MF3_SUPERFRIEND,      flags3), FLAG_ACCESSOR(invulncharge,       MF3_INVULNCHARGE,       flags3), \
   FLAG_ACCESSOR(explocount,       MF3_EXPLOCOUNT,       flags3), FLAG_ACCESSOR(cannotpush,         MF3_CANNOTPUSH,         flags3), \
   FLAG_ACCESSOR(tlstyleadd,       MF3_TLSTYLEADD,       flags3), FLAG_ACCESSOR(spacmonster,        MF3_SPACMONSTER,        flags3), \
   FLAG_ACCESSOR(spacmissile,      MF3_SPACMISSILE,      flags3), FLAG_ACCESSOR(nofrienddmg,        MF3_NOFRIENDDMG,        flags3), \
   FLAG_ACCESSOR(3ddecoration,     MF3_3DDECORATION,     flags3), FLAG_ACCESSOR(alwaysfast,         MF3_ALWAYSFAST,         flags3), \
   FLAG_ACCESSOR(passmobj,         MF3_PASSMOBJ,         flags3), FLAG_ACCESSOR(dontoverlap,        MF3_DONTOVERLAP,        flags3), \
   FLAG_ACCESSOR(cyclealpha,       MF3_CYCLEALPHA,       flags3), FLAG_ACCESSOR(rip,                MF3_RIP,                flags3), \
                                                                                                                                     \
   FLAG_ACCESSOR(autotranslate,    MF4_AUTOTRANSLATE,    flags4), FLAG_ACCESSOR(noradiusdmg,        MF4_NORADIUSDMG,        flags4), \
   FLAG_ACCESSOR(forceradiusdmg,   MF4_FORCERADIUSDMG,   flags4), FLAG_ACCESSOR(lookallaround,      MF4_LOOKALLAROUND,      flags4), \
   FLAG_ACCESSOR(nodamage,         MF4_NODAMAGE,         flags4), FLAG_ACCESSOR(synchronized,       MF4_SYNCHRONIZED,       flags4), \
   FLAG_ACCESSOR(norandomize,      MF4_NORANDOMIZE,      flags4), FLAG_ACCESSOR(bright,             MF4_BRIGHT,             flags4), \
   FLAG_ACCESSOR(fly,              MF4_FLY,              flags4), FLAG_ACCESSOR(noradiushack,       MF4_NORADIUSHACK,       flags4), \
   FLAG_ACCESSOR(nosoundcutoff,    MF4_NOSOUNDCUTOFF,    flags4), FLAG_ACCESSOR(ravenrespawn,       MF4_RAVENRESPAWN,       flags4), \
   FLAG_ACCESSOR(notshareware,     MF4_NOTSHAREWARE,     flags4), FLAG_ACCESSOR(notorque,           MF4_NOTORQUE,           flags4), \
   FLAG_ACCESSOR(alwaystorque,     MF4_ALWAYSTORQUE,     flags4), FLAG_ACCESSOR(nozerodamage,       MF4_NOZERODAMAGE,       flags4), \
   FLAG_ACCESSOR(tlstylesub,       MF4_TLSTYLESUB,       flags4), FLAG_ACCESSOR(totalinvisible,     MF4_TOTALINVISIBLE,     flags4), \
   FLAG_ACCESSOR(drawsblood,       MF4_DRAWSBLOOD,       flags4), FLAG_ACCESSOR(spacpushwall,       MF4_SPACPUSHWALL,       flags4), \
   FLAG_ACCESSOR(nospeciesinfight, MF4_NOSPECIESINFIGHT, flags4), FLAG_ACCESSOR(harmspeciesmissile, MF4_HARMSPECIESMISSILE, flags4), \
   FLAG_ACCESSOR(friendfoemissile, MF4_FRIENDFOEMISSILE, flags4), FLAG_ACCESSOR(bloodlessimpact,    MF4_BLOODLESSIMPACT,    flags4), \
   FLAG_ACCESSOR(hereticbounces,   MF4_HERETICBOUNCES,   flags4), FLAG_ACCESSOR(monsterpass,        MF4_MONSTERPASS,        flags4), \
   FLAG_ACCESSOR(lowaimprio,       MF4_LOWAIMPRIO,       flags4), FLAG_ACCESSOR(stickycarry,        MF4_STICKYCARRY,        flags4), \
   FLAG_ACCESSOR(settargetondeath, MF4_SETTARGETONDEATH, flags4), FLAG_ACCESSOR(slideroverthings,   MF4_SLIDEOVERTHINGS,    flags4), \
   FLAG_ACCESSOR(unsteppable,      MF4_UNSTEPPABLE,      flags4), FLAG_ACCESSOR(rangeeighth,        MF4_RANGEEIGHTH,        flags4), \
                                                                                                                                     \
   FLAG_ACCESSOR(notautoaimed,     MF5_NOTAUTOAIMED,     flags5)

#define FLAG_ACCESSOR(name, flag, member) \
   { "bool get_" #name "() const property",         WRAP_OBJ_LAST((getFlag<flag, Mobj, &Mobj:: ##member>)) }, \
   { "void set_" #name "(const bool val) property", WRAP_OBJ_LAST((setFlag<flag, &Mobj:: ##member>))       }

   static const aeonfuncreg_t mobjFuncs[] =
   {
      // Native Mobj methods
      { "void backupPosition()",              WRAP_MFN(Mobj, backupPosition)                  },
      { "void copyPosition(Mobj @other)",     WRAP_MFN(Mobj, copyPosition)                    },
      { "int getModifiedSpawnHealth() const", WRAP_MFN(Mobj, getModifiedSpawnHealth)          },
      { "void remove()",                      WRAP_MFN(Mobj, remove)                          },

      // Non-methods that are used like methods in Aeon
      { "bool checkMissileRange()",                            WRAP_OBJ_FIRST(P_CheckMissileRange) },
      { "bool checkMissileSpawn()",                            WRAP_OBJ_FIRST(P_CheckMissileSpawn) },
      { "bool checkSight(Mobj @other)",                        WRAP_OBJ_FIRST(P_CheckSight)        },
      { "bool hitFriend()",                                    WRAP_OBJ_FIRST(P_HitFriend)         },
      { "void startSound(const EE::Sound &in)",                WRAP_OBJ_FIRST(startSound)          },
      { "void weaponSound(const EE::Sound &in)",               WRAP_OBJ_FIRST(weaponSound)         },
      { "bool tryMove(fixed_t x, fixed_t y, int dropoff = 0)", WRAP_OBJ_FIRST(P_TryMove)           },
      { "fixed_t doAutoAim(angle_t angle, fixed_t distance)",  WRAP_OBJ_FIRST(P_DoAutoAim)         },
      { "bool lookForPlayers(lftype_e looktype)",              WRAP_OBJ_FIRST(P_LookForPlayers)    },
      { "bool lookForTargets(lftype_e looktype)",              WRAP_OBJ_FIRST(P_LookForTargets)    },
      {
         "EE::Mobj @spawnMissile(EE::Mobj @dest, const String &type, fixed_t z)",
         WRAP_OBJ_FIRST(spawnMissile),
      },
      {
         "EE::Mobj @spawnMissileAngle(const String &type, angle_t angle, fixed_t momz, fixed_t z)",
         WRAP_OBJ_FIRST(spawnMissileAngle)
      },
      {
         "fixed_t aimLineAttack(angle_t angle, fixed_t distance, alaflags_e flags = 0)",
         WRAP_OBJ_FIRST(aimLineAttack)
      },
      {
         "void lineAttack(angle_t angle, fixed_t distance, fixed_t slope, int damage, const String &pufftype)",
         WRAP_OBJ_FIRST(lineAttack)
      },

      // Indexed property accessors (enables [] syntax for counters)
      { "int get_counters(const uint ctrnum) const property",           WRAP_OBJ_LAST(getMobjCounter)  },
      { "void set_counters(const uint ctrnum, const int val) property", WRAP_OBJ_LAST(setMobjCounter)  },

      // Property accessors for the many, many, many flags
      MOBJ_FLAG_FIELDS,

   };

   static const aeonpropreg_t mobjProps[] =
   {
      { "fixed_t x",                  asOFFSET(Mobj, x)      },
      { "fixed_t y",                  asOFFSET(Mobj, y)      },
      { "fixed_t z",                  asOFFSET(Mobj, z)      },
      { "angle_t angle",              asOFFSET(Mobj, angle)  },

      { "fixed_t radius",             asOFFSET(Mobj, radius) },
      { "fixed_t height",             asOFFSET(Mobj, height) },
      { "vector_t mom",               asOFFSET(Mobj, mom)    },

      { "int health",                 asOFFSET(Mobj, health) },

      { "Mobj @target",               asOFFSET(Mobj, target) },
      { "Mobj @tracer",               asOFFSET(Mobj, tracer) },

      { "Player @const player",       asOFFSET(Mobj, player) },

      { "const MobjInfo @const info", asOFFSET(Mobj, info)   },
   };

   static const aeonbehaviorreg_t mobjBehaviors[] =
   {
      { asBEHAVE_FACTORY, "Mobj @f()",               WRAP_FN(mobjFactory)          },
      { asBEHAVE_FACTORY, "Mobj @f(const Mobj &in)", WRAP_FN(mobjFactoryFromOther) },
      { asBEHAVE_ADDREF,  "void f()",                WRAP_MFN(Mobj, addReference)  },
      { asBEHAVE_RELEASE, "void f()",                WRAP_MFN(Mobj, delReference)  },
   };

   template <int mobjinfo_t::* state>
   static state_t *getState(const mobjinfo_t *info)
   {
      if(info->spawnstate == -1)
         return nullptr;
      return states[info->*state];
   }

#undef FLAG_ACCESSOR
#define FLAG_ACCESSOR(name, flag, member) \
   { "bool get_" #name "() const property", WRAP_OBJ_LAST((getFlag<flag, mobjinfo_t, &mobjinfo_t:: ##member>)) }

   static const aeonfuncreg_t mobjinfoFuncs[] =
   {
      { "const EE::State @const get_spawnstate()    const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::spawnstate>)    },
      { "const EE::State @const get_seestate()      const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::seestate>)      },
      { "const EE::State @const get_painstate()     const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::painstate>)     },
      { "const EE::State @const get_meleestate()    const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::meleestate>)    },
      { "const EE::State @const get_missilestate()  const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::missilestate>)  },
      { "const EE::State @const get_deathstate()    const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::deathstate>)    },
      { "const EE::State @const get_xdeathstate()   const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::xdeathstate>)   },
      { "const EE::State @const get_raisestate()    const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::raisestate>)    },
      { "const EE::State @const get_crashstate()    const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::crashstate>)    },
      { "const EE::State @const get_activestate()   const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::activestate>)   },
      { "const EE::State @const get_inactivestate() const property", WRAP_OBJ_LAST(getState<&mobjinfo_t::inactivestate>) },
      { "const fixed_t get_height() const property",                 WRAP_OBJ_LAST(P_ThingInfoHeight)                    },

      // Getters for the many, many, many flags
      MOBJ_FLAG_FIELDS,
   };

   static const aeonpropreg_t mobjinfoProps[] =
   {
      { "const int speed",      asOFFSET(mobjinfo_t, speed)      },
      { "const int painchance", asOFFSET(mobjinfo_t, painchance) },
      { "const fixed_t radius", asOFFSET(mobjinfo_t, radius)     },
      { "const int mass",       asOFFSET(mobjinfo_t, mass)       },
   };

   void ScriptObjMobj::PreInit()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("Mobj", sizeof(Mobj), asOBJ_REF);

      e->SetDefaultNamespace("");
   }

   void ScriptObjMobj::Init()
   {
      asIScriptEngine *const e = ScriptManager::Engine();

      e->SetDefaultNamespace("EE");

      e->RegisterObjectType("MobjInfo", sizeof(mobjinfo_t), asOBJ_REF | asOBJ_NOCOUNT);

      for(const aeonpropreg_t &prop : mobjinfoProps)
         e->RegisterObjectProperty("MobjInfo", prop.declaration, prop.byteOffset);

      for(const aeonfuncreg_t &fn : mobjinfoFuncs)
         e->RegisterObjectMethod("MobjInfo", fn.declaration, fn.funcPointer, asCALL_GENERIC);


      for(const aeonbehaviorreg_t &behavior : mobjBehaviors)
         e->RegisterObjectBehaviour("Mobj", behavior.behavior, behavior.declaration, behavior.funcPointer, asCALL_GENERIC);

      for(const aeonpropreg_t &prop : mobjProps)
         e->RegisterObjectProperty("Mobj", prop.declaration, prop.byteOffset);

      // Flags used by EE::Mobj::lookForPlayers/lookForTargets
      e->RegisterEnum("lftype_e");
      e->RegisterEnumValue("lftype_e", "LF_INFRONT",  0);
      e->RegisterEnumValue("lftype_e", "LF_ALLAROUND", 1);


      // Flags used by EE::Mobj::aimLineAttack
      e->RegisterEnum("alaflags_e");
      e->RegisterEnumValue("alaflags_e", "ALF_SKIPFRIEND", 1);

      // Register all Aeon Mobj methods
      for(const aeonfuncreg_t &fn : mobjFuncs)
         e->RegisterObjectMethod("Mobj", fn.declaration, fn.funcPointer, asCALL_GENERIC);


      // It's Mobj-related, OK?
      e->SetDefaultNamespace("EE::Mobj");
      e->RegisterGlobalFunction(
         "fixed_t FloatBobOffsets(const int index)",
         WRAP_FN(floatBobOffsets), asCALL_GENERIC
      );
      e->RegisterGlobalFunction(
         "EE::Mobj @Spawn(fixed_t x, fixed_t y, fixed_t z, const String &type)",
         WRAP_FN(spawnMobj), asCALL_GENERIC
      );

      e->SetDefaultNamespace("");
   }
}


// EOF

