/*
    Eternity Engine Small Header
    Native functions for sound
*/

#if defined _snd_included
  #endinput
#endif
#define _snd_included

//
// Global Sound Functions
//
// These play sounds at full-volume on the global channel. The first takes an
// EDF sound mnemonic, and can play any sound defined in EDF.
//
// The latter takes a sound DeHackEd number. Only sounds with a unique DeHackEd
// number can be played with this function.
//

native _SoundGlobal(const name[]);
native _SoundGlobalNum(sndnum);

//
// Sector Sound Functions
//
// These play sounds from sector sound origins.
//

native _SectorSound(const name[], tag);
native _SectorSoundNum(sndnum, tag);

//
// Mapthing Sound Functions
//
// As above, but for Mapthings. ThingInfoSound allows playing a thingtype's
// predefined EDF sound values.
//

native _ThingSound(const name[], tid);
native _ThingSoundNum(sndnum, tid);

// enum values for ThingInfoSound type parameter
enum _ti_sounds
{
   _TI_SEESOUND,
   _TI_ACTIVESOUND,
   _TI_ATTACKSOUND,
   _TI_PAINSOUND,
   _TI_DEATHSOUND
}

native _ThingInfoSound(_ti_sounds:type, tid);
