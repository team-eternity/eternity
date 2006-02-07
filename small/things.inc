/*
    Eternity Engine Small Header
    Native functions for mapthing interaction
*/

#if defined _things_included
  #endinput
#endif
#define _things_included

// special reserved TID values
#define _TID_PLAYER1 -1
#define _TID_PLAYER2 -2
#define _TID_PLAYER3 -3
#define _TID_PLAYER4 -4
#define _TID_TRIGGER -10

// method of death types
enum _mods
{
   _MOD_UNKNOWN,
   _MOD_FIST,
   _MOD_PISTOL,
   _MOD_SHOTGUN,
   _MOD_CHAINGUN,
   _MOD_ROCKET,
   _MOD_R_SPLASH,
   _MOD_PLASMA,
   _MOD_BFG,
   _MOD_BFG_SPLASH,
   _MOD_CHAINSAW,
   _MOD_SSHOTGUN,
   _MOD_SLIME,
   _MOD_LAVA,
   _MOD_CRUSH,
   _MOD_TELEFRAG,
   _MOD_FALLING,
   _MOD_SUICIDE,
   _MOD_BARREL,
   _MOD_SPLASH,
   _MOD_HIT,
   _MOD_BFG11K_SPLASH,
   _MOD_BETABFG,
   _MOD_BFGBURST,
   _MOD_PLAYERMISC,
   _MOD_GRENADE,
}

// damage types for ThingKill function
enum _killtypes
{
   _KILL_HEALTH,
   _KILL_TELEFRAG,
}

// Kills all things with the given TID
// * must be called only while in a level
native _ThingKill(tid, _killtypes:dmgtype = _KILL_HEALTH, _mods:mod = _MOD_UNKNOWN);

// Hurts all things with the given tid
// * must be called only while in a level
native _ThingHurt(tid, damage, _mods:mod = _MOD_UNKNOWN, inflictor = 0, source = 0);

// massacre types
enum _massacres
{
   _MASSACRE_ALL,
   _MASSACRE_FRIENDS,
   _MASSACRE_ENEMIES
}

// Kills all monsters on the map that fit the class in massacreType.
// Note that EDF nuke specials are NOT invoked by a massacre.
// * must be called only while in a level
native _ThingMassacre(_massacres:massacreType);

// Makes all things with the first TID hate the first thing with the
// second. If the second TID is 0, or no monster is found with that TID,
// the monsters may fall asleep.
// * must be called only while in a level
native _ThingHate(object, target);

// Thing type lookup functions:

// Resolves an EDF thingtype mnemonic to a type number. "name" is case-
// insensitive.
native _ThingNumForName(const name[]);

// Resolves an EDF DeHackEd number to a type number.
native _ThingNumForDEHNum(num);

// Returns the type number of the required "Unknown" thingtype.
native _ThingUnknown();

// Compares a thing type number against that of the "Unknown" type.
stock bool:_ThingCheckType(num)
   return num != _ThingUnknown();

// Thing spawning functions -- only in levels:
// Spawn a thing at an arbitary location with optional tid and angle.
native _ThingSpawn(type, x, y, z, tid = 0, angle = 0);

// Spawn a thing at an existing thing that has tid 'spottid'; optional
// tid and angle will be assigned to new thing.
native _ThingSpawnSpot(type, spottid, tid = 0, angle = 0);

// Thing fields for query/set functions
enum _thing_fields
{
   _THING_TYPE,        // NOTE: type is read-only
   _THING_TICS,
   _THING_HEALTH,
   _THING_COUNTER0,
   _THING_COUNTER1,
   _THING_COUNTER2,
   _THING_EFFECTS,
   _THING_TRANSLUCENCY
}

// Allows getting the value of various thing properties from a numeric
// field id. The fields that can be examined are listed in the enumeration
// above. The first thing with the given tid's value is returned.
// * must be called only while in a level
native _ThingGetProperty(tid, _thing_fields:field);

// Sets the requested property on ALL things with the given tid. NOTE:
// the TF_TYPE property is read-only and cannot be set via this function.
// * must be called only while in a level
native _ThingSetProperty(tid, _thing_fields:field, value);

// flag operation values
enum _flagops
{
   _FLAGS_SET,
   _FLAGS_ADD,
   _FLAGS_REMOVE
}

// Can set, add, or remove thing flag values given a BEX/EDF flag string.
// See the DeHackEd/BEX documentation for full information. Use one of the
// above operation enumeration values to select what to do with the flags.
// This function is equivalent to the EDF "cflags" field, and will set or
// remove flags in all of a thing's internal flags fields at once.
// * must be called only while in a level
native _ThingFlagsFromStr(tid, _flagops:op, str[]);

// particle effects flag values -- note: particle fountain effects cannot
// be combined with each other.
enum
{
   _PFX_ROCKET         = 0x00000001,
   _PFX_GRENADE        = 0x00000002,
   _PFX_FLIES          = 0x00000004,
   _PFX_BFG            = 0x00000008,
   _PFX_FLIESONDEATH   = 0x00000010,
   _PFX_DRIP           = 0x00000020,
   
   _PFX_REDFOUNTAIN    = 0x00010000,
   _PFX_GREENFOUNTAIN  = 0x00020000,
   _PFX_BLUEFOUNTAIN   = 0x00030000,
   _PFX_YELLOWFOUNTAIN = 0x00040000,
   _PFX_PURPLEFOUNTAIN = 0x00050000,
   _PFX_BLACKFOUNTAIN  = 0x00060000,
   _PFX_WHITEFOUNTAIN  = 0x00070000
}
