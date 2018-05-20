// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright (C) 2013 James Haley et al.
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:  Platform-independent sound code
//
//-----------------------------------------------------------------------------

// killough 3/7/98: modified to allow arbitrary listeners in spy mode
// killough 5/2/98: reindented, removed useless code, beautified

#include "z_zone.h"

#include "a_small.h"
#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_gi.h"
#include "d_io.h"     // SoM 3/14/2002: strncasecmp
#include "d_main.h"
#include "doomstat.h"
#include "e_reverbs.h"
#include "e_sound.h"
#include "i_sound.h"
#include "i_system.h"
#include "m_compare.h"
#include "m_random.h"
#include "m_queue.h"
#include "p_chase.h"
#include "p_info.h"
#include "p_portal.h"
#include "p_skin.h"
#include "p_spec.h"
#include "p_tick.h"
#include "r_defs.h"
#include "r_main.h"
#include "r_state.h"
#include "s_reverb.h"
#include "s_sound.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

// haleyjd 07/13/05: redefined to use sound-specific attenuation params
#define S_ATTENUATOR ((sfx->clipping_dist - sfx->close_dist) >> FRACBITS)

// Adjustable by menu.
#define NORM_PITCH 128
#define NORM_PRIORITY 64
#define NORM_SEP 128
#define S_STEREO_SWING (96<<FRACBITS)

// sf: sound/music hashing
// use sound_hash for music hash too

static inline int sound_hash(const char *s)
{
   return D_HashTableKey(s) % SOUND_HASHSLOTS;
}

//jff 1/22/98 make sound enabling variables readable here
extern int snd_card, mus_card;
extern bool nosfxparm, nomusicparm;
//jff end sound enabling variables readable here

struct channel_t
{
  sfxinfo_t *sfxinfo;      // sound information (if null, channel avail.)
  sfxinfo_t *aliasinfo;    // original sound name if using an alias
  const PointThinker *origin;    // origin of sound
  int subchannel;          // haleyjd 06/12/08: origin subchannel
  int volume;              // volume scale value for effect -- haleyjd 05/29/06
  int attenuation;         // attenuation type -- haleyjd 05/29/06
  int pitch;               // pitch modifier -- haleyjd 06/03/06
  int handle;              // handle of the sound being played
  int o_priority;          // haleyjd 09/27/06: stored priority value
  int priority;            // current priority value
  int singularity;         // haleyjd 09/27/06: stored singularity value
  int idnum;               // haleyjd 09/30/06: unique id num for sound event
  bool looping;            // haleyjd 10/06/06: is this channel looping?
};

// the set of channels available
static channel_t *channels;

// Maximum volume of a sound effect.
// Internal default is max out of 0-15.
int snd_SfxVolume = 15;

// Maximum volume of music.
int snd_MusicVolume = 15;

// precache sounds ?
int s_precache = 1;

// whether songs are mus_paused
static bool mus_paused;

// music currently being played
static musicinfo_t *mus_playing;

// following is set
//  by the defaults code in M_misc:
// number of channels available
int numChannels;
int default_numChannels;  // killough 9/98

//jff 3/17/98 to keep track of last IDMUS specified music num
int idmusnum;

// haleyjd 05/18/14: music randomization
bool s_randmusic = false;

// sf:
// haleyjd: sound hashing is now kept up by EDF
musicinfo_t *musicinfos[SOUND_HASHSLOTS];

// haleyjd 12/24/11: hi-def music support
bool s_hidefmusic;

//
// Internals.
//

//
// S_StopChannel
//
// Stops a sound channel.
//
static void S_StopChannel(int cnum)
{
#ifdef RANGECHECK
   if(cnum < 0 || cnum >= numChannels)
      I_Error("S_StopChannel: handle %d out of range\n", cnum);
#endif

   channel_t *c = &channels[cnum];

   if(c->sfxinfo)
   {
      I_StopSound(c->handle, c->idnum); // stop the sound playing
      
      // haleyjd 09/27/06: clear the entire channel
      memset(c, 0, sizeof(channel_t));
   }
}

//
// S_CheckSectorKill
//
// haleyjd: isolated code to check for sector sound killing.
// Returns true if the sound should be killed.
//
static bool S_CheckSectorKill(const sector_t *earsec, const PointThinker *src)
{
   // haleyjd 05/29/06: moved up to here and fixed a major bug
   if(gamestate == GS_LEVEL)
   { 
      // are we in a killed-sound sector?
      if(earsec && earsec->flags & SECF_KILLSOUND)
         return true;
      
      // source in a killed-sound sector?
      if(src &&
         R_PointInSubsector(src->x, src->y)->sector->flags & SECF_KILLSOUND)
         return true;
   }

   return false;
}

//
// S_AdjustSoundParams
//
// Alters a playing sound's volume and stereo separation to account for
// the position and angle of the listener relative to the source.
//
// sf: listener now a camera_t for external cameras
// haleyjd: added sfx parameter for custom attenuation data
// haleyjd: added channel volume scale value
// haleyjd: added priority scaling
//
static int S_AdjustSoundParams(camera_t *listener, const PointThinker *source,
                               int chanvol, int chanattn, int *vol, int *sep,
                               int *pitch, int *pri, sfxinfo_t *sfx)
{
   fixed_t adx = 0, ady = 0, dist = 0;
   angle_t angle;
   fixed_t sx, sy;
   int attenuator = 0, basevolume;            // haleyjd
   fixed_t close_dist = 0, clipping_dist = 0; // haleyjd

   // haleyjd 08/12/04: we cannot adjust a sound for a NULL listener.
   if(!listener)
      return 1;

   // haleyjd 05/29/06: this function isn't supposed to be called for NULL sources
#ifdef RANGECHECK
   if(!source)
      I_Error("S_AdjustSoundParams: NULL source\n");
#endif
   
   // calculate the distance to sound origin
   //  and clip it if necessary
   //
   // killough 11/98: scale coordinates down before calculations start
   // killough 12/98: use exact distance formula instead of approximation

   sx = source->x;
   sy = source->y;
      
   if(useportalgroups && listener->groupid != source->groupid)
   {
      // The listener and the source are not in the same subspace, so offset
      // the sound origin so it will sound correct to the player.
      linkoffset_t *link = P_GetLinkOffset(source->groupid, listener->groupid);
      sx += link->x;
      sy += link->y;
   }

   adx = D_abs((listener->x >> FRACBITS) - (sx >> FRACBITS));
   ady = D_abs((listener->y >> FRACBITS) - (sy >> FRACBITS));
   
   if(ady > adx)
   {
      dist = adx;
      adx = ady;
      ady = dist;
   }

   dist = adx ? FixedDiv(adx, finesine[(tantoangle_acc[FixedDiv(ady,adx) >> DBITS]
                                        + ANG90) >> ANGLETOFINESHIFT]) : 0;   
   
   // haleyjd 05/29/06: allow per-channel volume scaling
   basevolume = (snd_SfxVolume * chanvol) / 15;

   // haleyjd 05/30/06: allow per-channel attenuation behavior
   switch(chanattn)
   {
   case ATTN_NORMAL: // use sfxinfo_t data
      attenuator = S_ATTENUATOR;
      close_dist = sfx->close_dist;
      clipping_dist = sfx->clipping_dist;
      break;
   case ATTN_IDLE: // use DOOM's original values
      attenuator = S_CLIPPING_DIST_I - S_CLOSE_DIST_I;
      close_dist = S_CLOSE_DIST;
      clipping_dist = S_CLIPPING_DIST;
      break;
   case ATTN_STATIC: // fade fast
      attenuator = 448;
      close_dist = 64 << FRACBITS;
      clipping_dist = 512 << FRACBITS;
      break;
   case ATTN_NONE: // Moooo! Useful for ambient sounds.
      attenuator = close_dist = clipping_dist = 0;
      break;
   }

   // killough 11/98:   handle zero-distance as special case
   // haleyjd 07/13/05: handle case of zero-or-less attenuation as well
   if(!dist || attenuator <= 0)
   {
      *sep = NORM_SEP;
      *vol = basevolume; // haleyjd 05/29/06: use scaled basevolume
      return *vol > 0;
   }

   if(dist > clipping_dist >> FRACBITS)
      return 0;

   // angle of source to listener
   // sf: use listenx, listeny
   
   angle = R_PointToAngle2(listener->x, listener->y, sx, sy);

   if(angle <= listener->angle)
      angle += 0xffffffff;
   angle -= listener->angle;
   angle >>= ANGLETOFINESHIFT;

   // stereo separation
   *sep = NORM_SEP - FixedMul(S_STEREO_SWING >> FRACBITS, finesine[angle]);
   
   // volume calculation
   *vol = dist < close_dist >> FRACBITS ? basevolume :
      basevolume * ((clipping_dist >> FRACBITS) - dist) / attenuator;

   // haleyjd 09/27/06: decrease priority with volume attenuation
   // haleyjd 04/27/10: special treatment for priorities <= 0
   if(*pri > 0)
      *pri = *pri + (127 - *vol);
   
   if(*pri > 255) // cap to 255
      *pri = 255;

   return *vol > 0;
}

//
// S_getChannel
//
//   If none available, return -1.  Otherwise channel #.
//   haleyjd 09/27/06: fixed priority/singularity bugs
//   Note that a higher priority number means lower priority!
//
static int S_getChannel(const PointThinker *origin, sfxinfo_t *sfxinfo,
                        int priority, int singularity, int schan,
                        bool nocutoff)
{
   // channel number to use
   int cnum;
   int lowestpriority = D_MININT; // haleyjd
   int lpcnum = -1;
   bool origin_equivalent;

   // haleyjd 09/28/06: moved this here. If we kill a sound already
   // being played, we can use that channel. There is no need to
   // search for a free one again because we already know of one.

   // kill old sound?
   if(nocutoff)
      cnum = numChannels;
   else
   {
      // killough 12/98: replace is_pickup hack with singularity flag
      // haleyjd 06/12/08: only if subchannel matches
      for(cnum = 0; cnum < numChannels; cnum++)
      {
         // haleyjd 04/09/11: Allow different sounds played on NULL
         // channel to not cut each other off
         if(origin == NULL)
            origin_equivalent = (channels[cnum].sfxinfo == sfxinfo);
         else
            origin_equivalent = (channels[cnum].origin == origin);

         if(channels[cnum].sfxinfo &&
            channels[cnum].singularity == singularity &&
            origin_equivalent &&
            channels[cnum].subchannel == schan)
         {
            S_StopChannel(cnum);
            break;
         }
      }
   }
   
   // Find an open channel
   if(cnum == numChannels)
   {
      // haleyjd 09/28/06: it isn't necessary to look for playing sounds in
      // the same singularity class again, as we just did that above. Here
      // we are looking for an open channel. We will also keep track of the
      // channel found with the lowest sound priority while doing this.
      for(cnum = 0; cnum < numChannels && channels[cnum].sfxinfo; cnum++)
      {
         if(channels[cnum].priority > lowestpriority)
         {
            lowestpriority = channels[cnum].priority;
            lpcnum = cnum;
         }
      }
   }

   // None available?
   if(cnum == numChannels)
   {
      // Look for lower priority
      // haleyjd: we have stored the channel found with the lowest priority
      // in the loop above
      if(priority > lowestpriority)
         return -1;                  // No lower priority.  Sorry, Charlie.
      else
      {
         S_StopChannel(lpcnum);      // Otherwise, kick out lowest priority.
         cnum = lpcnum;
      }
   }

#ifdef RANGECHECK
   if(cnum >= numChannels)
      I_Error("S_getChannel: handle %d out of range\n", cnum);
#endif
   
   return cnum;
}

//
// S_countChannels
//
// haleyjd 04/28/10: gets a count of the currently active sound channels.
//
static int S_countChannels()
{
   int numchannels = 0;

   for(int cnum = 0; cnum < numChannels; cnum++)
      if(channels[cnum].sfxinfo)
         ++numchannels;

   return numchannels;
}

//
// S_StartSfxInfo
//
// The main sound starting function.
// haleyjd 05/29/06: added volume scaling value. Allows sounds to be
// started and to persist at differing volume levels. volumeScale should
// range from 0 to 127. Also added customizable attenuation types.
// haleyjd 06/03/06: added ability to loop sound samples
//
void S_StartSfxInfo(const soundparams_t &params)
{
   int  sep = 0, pitch, singularity, cnum, handle, o_priority, priority, chancount;
   int  volume         = snd_SfxVolume;
   int  volumeScale    = params.volumeScale;
   int  subchannel     = params.subchannel;
   bool priority_boost = false;
   bool extcamera      = false;
   bool nocutoff       = false;
   camera_t      playercam;
   camera_t     *listener = &playercam;
   sector_t     *earsec   = NULL;
   sfxinfo_t    *sfx      = params.sfx;
   const PointThinker *origin   = params.origin;
   const Mobj *mo;

   // haleyjd 09/03/03: allow NULL sounds to fall through
   if(!sfx)
      return;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   // haleyjd 09/24/06: Sound aliases. These are similar to links, but we skip
   // through them now, up here, instead of below. This allows aliases to simply
   // serve as alternate names for the same sounds, in contrast to links which
   // provide a way of playing the same sound with different parameters.

   // haleyjd 05/12/09: Randomized sounds. Like aliases, these are links to 
   // other sounds, but we choose one at random.

   sfxinfo_t *aliasinfo = sfx;

   while(sfx->alias || sfx->randomsounds)
   {
      if(sfx->alias)
         sfx = sfx->alias;
      else if(sfx->randomsounds)
      {
         // make sure the sound we get is valid
         if(!(sfx = sfx->randomsounds[M_Random() % sfx->numrandomsounds]))
            return;
      }
   }

   // haleyjd:  we must weed out degenMobj's before trying to 
   // dereference these fields -- a thinker check perhaps?
   if((mo = thinker_cast<const Mobj *>(origin)))
   {
      // haleyjd 08/11/13: check for no cut off flag
      if(mo->flags4 & MF4_NOSOUNDCUTOFF)
         nocutoff = true;

      if(sfx->skinsound) // check for skin sounds
      {
         const char *sndname = "";

         // haleyjd: monster skins don't support sound replacements
         if(mo->skin && mo->skin->type == SKIN_PLAYER)
         {         
            sndname = mo->skin->sounds[sfx->skinsound - 1];
            sfx = S_SfxInfoForName(sndname);
         }

         if(!sfx)
         {
            doom_printf(FC_ERROR "S_StartSfxInfo: skin sound %s not found\n", sndname);
            return;
         }
      }

      // haleyjd: give local client sounds high priority
      if(mo == players[displayplayer].mo || 
         (mo->flags & MF_MISSILE && 
          mo->target == players[displayplayer].mo))
      {
         priority_boost = true;
      }
   }

   // Initialize sound parameters
   if(sfx->link)
   {
      pitch        = sfx->pitch;
      volumeScale += sfx->volume;   // haleyjd: now modifies volumeScale
   }
   else
      pitch = NORM_PITCH;

   // haleyjd 09/29/06: rangecheck volumeScale now!
   volumeScale = eclamp(volumeScale, 0, 127);

   // haleyjd 04/28/10: adjust volume for channel overload
   if((chancount = S_countChannels()) >= 4)
   {
      // don't make 0
      if(volumeScale > chancount)
         volumeScale -= chancount;
   }
   
   // haleyjd: modified so that priority value is always used
   // haleyjd: also modified to get and store proper singularity value
   // haleyjd: allow priority boost for local client sounds
   o_priority = priority = priority_boost ? 0 : sfx->priority;
   singularity = sfx->singularity;

   // haleyjd: setup playercam
   if(gamestate == GS_LEVEL)
   {     
      if(camera) // an external camera is active
      {
         playercam = *camera; // assign directly
         extcamera = true;
      }
      else
      {
         Mobj *pmo = players[displayplayer].mo;

         // haleyjd 10/20/07: do not crash in multiplayer trying to
         // adjust sounds for a player that hasn't been spawned yet!
         if(pmo)
         {
            playercam.x = pmo->x; 
            playercam.y = pmo->y; 
            playercam.z = pmo->z;
            playercam.angle = pmo->angle;
            playercam.groupid = pmo->groupid;
         }
         else
         {
            memset(&playercam, 0, sizeof(playercam));
            listener = NULL;
         }
      }

      earsec = R_PointInSubsector(playercam.x, playercam.y)->sector;
   }

   // haleyjd 09/29/06: check for sector sound kill here.
   if(S_CheckSectorKill(earsec, origin))
      return;

   // Check to see if it is audible, modify the params
   // killough 3/7/98, 4/25/98: code rearranged slightly
   // haleyjd 08/12/04: add extcamera check
   
   if(!origin || (!extcamera && origin == players[displayplayer].mo))
   {
      sep = NORM_SEP;
      volume = (volume * volumeScale) / 15; // haleyjd 05/29/06: scale volume
      volume = eclamp(volume, 0, 127);
      if(volume < 1) // clip due to inaudibility
         return;
   }
   else
   {     
      // use an external cam?
      if(!S_AdjustSoundParams(listener, origin, volumeScale, params.attenuation,
                              &volume, &sep, &pitch, &priority, sfx))
         return;
      else if(origin->x == playercam.x && origin->y == playercam.y)
         sep = NORM_SEP;
   }
  
   if(pitched_sounds)
   {
      switch(sfx->pitch_type)
      {
      case sfxinfo_t::pitch_doom:
         pitch += 16 - (M_Random() & 31);
         break;
      case sfxinfo_t::pitch_doomsaw:
         pitch += 8 - (M_Random() & 15);
         break;
      case sfxinfo_t::pitch_heretic:
         pitch += M_Random() & 31;
         pitch -= M_Random() & 31;
         break;
      case sfxinfo_t::pitch_hticamb:
         pitch += M_Random() & 15;
         pitch -= M_Random() & 15;
         break;
      case sfxinfo_t::pitch_none:
      default:
         break;
      }

      pitch = eclamp(pitch, 0, 255);
   }

   // haleyjd 06/12/08: determine subchannel. If auto, try using the sound's
   // preferred subchannel (which is also auto by default).
   if(subchannel == CHAN_AUTO)
      subchannel = sfx->subchannel;

   // try to find a channel
   if((cnum = S_getChannel(origin, sfx, priority, singularity, subchannel, nocutoff)) < 0)
      return;

#ifdef RANGECHECK
   if(cnum < 0 || cnum >= numChannels)
      I_Error("S_StartSfxInfo: handle %d out of range\n", cnum);
#endif

   channels[cnum].sfxinfo = sfx;
   channels[cnum].aliasinfo = aliasinfo;
   channels[cnum].origin  = origin;

   while(sfx->link)
      sfx = sfx->link;     // sf: skip thru link(s)

   // Assigns the handle to one of the channels in the mix/output buffer.
   handle = I_StartSound(sfx, cnum, volume, sep, pitch, priority, params.loop, params.reverb);

   // haleyjd: check to see if the sound was started
   if(handle >= 0)
   {
      channels[cnum].handle = handle;
      
      // haleyjd 05/29/06: record volume scale value and attenuation type
      // haleyjd 06/03/06: record pitch too (wtf is going on here??)
      // haleyjd 09/27/06: store priority and singularity values (!!!)
      // haleyjd 06/12/08: store subchannel
      channels[cnum].volume      = volumeScale;
      channels[cnum].attenuation = params.attenuation;
      channels[cnum].pitch       = pitch;
      channels[cnum].o_priority  = o_priority;  // original priority
      channels[cnum].priority    = priority;    // scaled priority
      channels[cnum].singularity = singularity;
      channels[cnum].looping     = params.loop;
      channels[cnum].subchannel  = subchannel;
      channels[cnum].idnum       = I_SoundID(handle); // unique instance id
   }
   else // haleyjd: the sound didn't start, so clear the channel info
   {
      memset(&channels[cnum], 0, sizeof(channel_t));
   }
}

//
// S_StartSoundAtVolume
//
// haleyjd 05/29/06: Actually, DOOM had a routine named this, but it was
// removed, apparently by the BOOM team, because it was never used for 
// anything useful (it was always called with snd_SfxVolume...).
//
void S_StartSoundAtVolume(const PointThinker *origin, int sfx_id,
                          int volume, int attn, int subchannel)
{
   soundparams_t params;

   // haleyjd: changed to use EDF DeHackEd number hashing,
   // to enable full use of dynamically defined sounds ^_^
   if((params.sfx = E_SoundForDEHNum(sfx_id)))
   {
      params.origin      = origin;
      params.volumeScale = volume;
      params.attenuation = attn;
      params.loop        = false;
      params.subchannel  = subchannel;
      params.reverb      = true;

      S_StartSfxInfo(params);
   }
}

//
// S_StartSound
//
// The classic DOOM sound routine, which now accepts an EDF-defined sound
// DeHackEd number. This allows it to extend to all sounds seamlessly yet
// retains full compatibility.
// haleyjd 05/29/06: reimplemented in terms of the above function.
//
void S_StartSound(const PointThinker *origin, int sfx_id)
{
   S_StartSoundAtVolume(origin, sfx_id, 127, ATTN_NORMAL, CHAN_AUTO);
}

//
// S_StartSoundNameAtVolume
//
// haleyjd 05/29/06: as below, but allows volume scaling.
//
void S_StartSoundNameAtVolume(const PointThinker *origin, const char *name,
                              int volume, int attn, int subchannel)
{
   soundparams_t params;
   
   // haleyjd 03/17/03: allow NULL sound names to fall through
   if(!name)
      return;

   if((params.sfx = S_SfxInfoForName(name)))
   {
      params.origin      = origin;
      params.volumeScale = volume;
      params.attenuation = attn;
      params.loop        = false;
      params.subchannel  = subchannel;
      params.reverb      = true;
      S_StartSfxInfo(params);
   }
}

//
// S_StartSoundName
//
// Starts a sound by its EDF mnemonic. This is used by some newer
// engine features, particularly ones that use implicitly defined
// WAD sounds.
// haleyjd 05/29/06: reimplemented in terms of the above function.
//
void S_StartSoundName(const PointThinker *origin, const char *name)
{
   S_StartSoundNameAtVolume(origin, name, 127, ATTN_NORMAL, CHAN_AUTO);
}

//
// S_StartSoundLooped
//
// haleyjd 06/03/06: support playing looped sounds.
//
// FIXME: unused?
//
static void S_StartSoundLooped(PointThinker *origin, char *name, int volume,
                               int attn, int subchannel)
{
   soundparams_t params;
   
   if(!name)
      return;

   if((params.sfx = S_SfxInfoForName(name)))
   {
      params.origin      = origin;
      params.volumeScale = volume;
      params.attenuation = attn;
      params.loop        = true;
      params.subchannel  = subchannel;
      params.reverb      = true;
      S_StartSfxInfo(params);
   }
}

//
// S_StartInterfaceSound(int)
//
// Start an interface sound by DeHackEd number.
//
void S_StartInterfaceSound(int sound_id)
{
   soundparams_t params;

   if((params.sfx = E_SoundForDEHNum(sound_id)))
   {
      params.setNormalDefaults(NULL);
      params.reverb = false;
      S_StartSfxInfo(params);
   }
}

//
// S_StartInterfaceSound(const char *)
//
// Start an interface sound by name.
//
void S_StartInterfaceSound(const char *name)
{
   soundparams_t params;

   if((params.sfx = E_SoundForName(name)))
   {
      params.setNormalDefaults(NULL);
      params.reverb = false;
      S_StartSfxInfo(params);
   }
}

//
// S_StopSound
//
void S_StopSound(const PointThinker *origin, int subchannel)
{
   int cnum;
   
   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   for(cnum = 0; cnum < numChannels; cnum++)
   {
      if(channels[cnum].sfxinfo && channels[cnum].origin == origin &&
         channels[cnum].idnum == I_SoundID(channels[cnum].handle) &&
         (subchannel == CHAN_ALL || channels[cnum].subchannel == subchannel))
      {
         S_StopChannel(cnum);
      }
   }
}

//
// S_PauseSound
//
// Stop music, during game PAUSE.
//
void S_PauseSound()
{
   if(mus_playing && !mus_paused)
   {
      I_PauseSong(mus_playing->handle);
      mus_paused = true;
   }
}

//
// S_ResumeSound
//
// Start music back up.
//
void S_ResumeSound()
{
   if(mus_playing && mus_paused)
   {
      I_ResumeSong(mus_playing->handle);
      mus_paused = false;
   }
}

// Currently active reverberation environment
static ereverb_t *s_currentEnvironment;

//
// S_updateEnvironment
//
// Update the active sound environment.
//
static void S_updateEnvironment(sector_t *earsec)
{
   ereverb_t *reverb;
   
   if(!earsec || gamestate != GS_LEVEL)
      reverb = E_GetDefaultReverb();
   else
      reverb = soundzones[earsec->soundzone].reverb;

   if(reverb != s_currentEnvironment)
   {
      s_currentEnvironment = reverb;
      S_ReverbSetState(reverb);
   }
}

//
// S_UpdateSounds
//
// Called from main loop. Updates digital sound positional effects.
//
void S_UpdateSounds(const Mobj *listener)
{
   // sf: a camera_t holding the information about the player
   camera_t playercam = { 0 }; 
   sector_t *earsec = NULL;

   //jff 1/22/98 return if sound is not enabled
   if(!snd_card || nosfxparm)
      return;

   if(listener)
   {
      // haleyjd 08/12/04: fix possible bugs with external cameras
      if(camera) // an external camera is active
      {
         playercam = *camera; // assign directly
      }
      else
      {
         playercam.x = listener->x;
         playercam.y = listener->y;
         playercam.z = listener->z;
         playercam.angle = listener->angle;
         playercam.groupid = listener->groupid;
      }
      earsec = R_PointInSubsector(playercam.x, playercam.y)->sector;
   }

   // update sound environment
   S_updateEnvironment(earsec);

   // now update each individual channel
   for(int cnum = 0; cnum < numChannels; cnum++)
   {
      channel_t *c = &channels[cnum];
      sfxinfo_t *sfx = c->sfxinfo;

      if(!sfx)
         continue;

      // haleyjd: has this software channel lost its hardware channel?
      if(c->idnum != I_SoundID(c->handle))
      {
         // clear the channel and keep going
         memset(c, 0, sizeof(channel_t));
         continue;
      }

      if(I_SoundIsPlaying(c->handle))
      {
         // initialize parameters
         int volume = snd_SfxVolume; // haleyjd: this gets scaled below.
         int pitch = c->pitch; // haleyjd 06/03/06: use channel's pitch!
         int sep = NORM_SEP;
         int pri = c->o_priority; // haleyjd 09/27/06: priority

         // check non-local sounds for distance clipping
         // or modify their params

         // sf again: use external camera if there is one
         // fix afterglows bug: segv because of NULL listener

         // haleyjd 09/29/06: major bug fix. fraggle's change to remove the
         // listener != origin check here causes player sounds to be adjusted
         // inappropriately. The only reason he changed this was to get to
         // the code in S_AdjustSoundParams that checks for sector sound
         // killing. We do that here now instead.
         if(listener && S_CheckSectorKill(earsec, c->origin))
            S_StopChannel(cnum);
         else if(c->origin && static_cast<const PointThinker *>(listener) != c->origin) // killough 3/20/98
         {
            // haleyjd 05/29/06: allow per-channel volume scaling
            // and attenuation type selection
            if(!S_AdjustSoundParams(listener ? &playercam : NULL,
                                    c->origin,
                                    c->volume,
                                    c->attenuation,
                                    &volume, &sep, &pitch, &pri, sfx))
            {
               S_StopChannel(cnum);
            }
            else
            {
               I_UpdateSoundParams(c->handle, volume, sep, pitch);
               c->priority = pri; // haleyjd
            }
         }
      }
      else   // if channel is allocated but sound has stopped, free it
         S_StopChannel(cnum);
   }
}

//
// S_CheckSoundPlaying
//
// haleyjd: rudimentary sound checking function
//
bool S_CheckSoundPlaying(const PointThinker *mo, sfxinfo_t *aliasinfo)
{
   int cnum;

   if(mo && aliasinfo)
   {
      for(cnum = 0; cnum < numChannels; cnum++)
      {
         if(channels[cnum].origin == mo && channels[cnum].aliasinfo == aliasinfo)
         {
            if(I_SoundIsPlaying(channels[cnum].handle))
               return true;
         }
      }
   }
   
   return false;
}

//
// S_StopSounds
//
// Stops only sourced sounds, or all sounds if "killall" is true. This is called
// on transitions between levels, when going into console gamestate, and when
// resetting the state of EDF definitions.
//
void S_StopSounds(bool killall)
{
   int cnum;
   // kill all playing sounds at start of level
   //  (trust me - a good idea)

   // jff 1/22/98 skip sound init if sound not enabled
   // haleyjd 08/29/07: kill only sourced sounds.
   if(snd_card && !nosfxparm)
      for(cnum = 0; cnum < numChannels; ++cnum)
         if(channels[cnum].sfxinfo && (killall || channels[cnum].origin))
            S_StopChannel(cnum);
}

//
// S_StopLoopedSounds
//
// haleyjd 10/06/06: Looped sounds need to stop playing when the intermission
// starts up, or else they'll play all the way til the next level.
//
void S_StopLoopedSounds()
{
   int cnum;

   if(snd_card && !nosfxparm)
      for(cnum = 0; cnum < numChannels; ++cnum)
         if(channels[cnum].sfxinfo && channels[cnum].looping)
            S_StopChannel(cnum);
}

void S_SetSfxVolume(int volume)
{
   //jff 1/22/98 return if sound is not enabled
   if (!snd_card || nosfxparm)
      return;

#ifdef RANGECHECK
   if(volume < 0 || volume > 127)
      I_Error("Attempt to set sfx volume at %d\n", volume);
#endif

   snd_SfxVolume = volume;
}

//=============================================================================
//
// Sound Hashing
//

sfxinfo_t *S_SfxInfoForName(const char *name)
{   
   // haleyjd 09/03/03: now calls down to master EDF sound hash   
   sfxinfo_t *sfx = E_SoundForName(name);

   // haleyjd 03/26/11: if not found, check for an implicit wad sound
   if(!sfx)
   {
      char lumpname[16];
      if(strncasecmp(name, "DS", 2))
         psnprintf(lumpname, sizeof(lumpname), "DS%s", name);
      else
         psnprintf(lumpname, sizeof(lumpname), "%s", name);

      if(W_CheckNumForName(lumpname) > 0)
         sfx = E_NewWadSound(lumpname);
   }

   return sfx;
}

//
// S_Chgun
//
// Delinks the chgun sound effect when a DSCHGUN lump has been
// detected. This allows the sound to be used separately without 
// use of EDF.
//
void S_Chgun()
{
   sfxinfo_t *s_chgun = E_SoundForName("chgun");

   if(!s_chgun)
      return;

   s_chgun->link = NULL;
   s_chgun->pitch = -1;
   s_chgun->volume = -1;
}

// free sound and reload
// also check to see if a new sound name has been found
// (ie. not one in the original game). If so, we create
// a new sfxinfo_t and hook it into the hashtable for use
// by scripting and skins

// haleyjd 03/26/2011: this is now done on-demand rather than by scanning
// through the wad directory.

//=============================================================================
//
// Music
//

void S_SetMusicVolume(int volume)
{
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;

#ifdef RANGECHECK
   if(volume < 0 || volume > 16)
      I_Error("Attempt to set music volume at %d\n", volume);
#endif

   I_SetMusicVolume(volume);
   snd_MusicVolume = volume;
}

//
// S_ChangeMusic
//
void S_ChangeMusic(musicinfo_t *music, int looping)
{
   int lumpnum;
   char namebuf[16];

   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;

   // same as the one playing ?
   if(mus_playing == music)
      return;  

   // shutdown old music
   S_StopMusic();

   if(music->prefix)
   {
      psnprintf(namebuf, sizeof(namebuf), "%s%s", 
                GameModeInfo->musPrefix, music->name);
   }
   else
      psnprintf(namebuf, sizeof(namebuf), "%s", music->name);

   if((lumpnum = W_CheckNumForName(namebuf)) == -1)
   {
      doom_printf(FC_ERROR "bad music name '%s'\n", music->name);
      return;
   }

   // load & register it
   // haleyjd: changed to PU_STATIC
   // julian: added lump length

   music->data = wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC);   

   if(music->data)
   {
      music->handle = I_RegisterSong(music->data, W_LumpLength(lumpnum));
      
      // play it
      if(music->handle)
      {
         I_PlaySong(music->handle, looping);
         mus_playing = music;
      }
      else
      {
         Z_Free(music->data);
         music->data = NULL;
         mus_playing = NULL;
      }
   }
   else
      mus_playing = NULL;
}

//
// S_ChangeMusicNum
//
// sf: created changemusicnum, not limited to original musics
// change by music number
// removed mus_new
//
void S_ChangeMusicNum(int musnum, int looping)
{
   musicinfo_t *music;

   // haleyjd 05/18/14: check for Tarnsman's random music
   if(s_randmusic && 
      musnum >= GameModeInfo->randMusMin && musnum <= GameModeInfo->randMusMax)
   {
      musnum = M_RangeRandomEx(GameModeInfo->randMusMin, GameModeInfo->randMusMax);
   }
   
   if(musnum <= GameModeInfo->musMin || musnum >= GameModeInfo->numMusic)
   {
      doom_printf(FC_ERROR "Bad music number %d\n", musnum);
      return;
   }
   
   music = &(GameModeInfo->s_music[musnum]);
   
   S_ChangeMusic(music, looping);
}

//
// Starts some music with the music id found in sounds.h.
//
void S_StartMusic(int m_id)
{
   S_ChangeMusicNum(m_id, false);
}

//
// S_ChangeMusicName
//
// change by name
//
void S_ChangeMusicName(const char *name, int looping)
{
   musicinfo_t *music;
   
   music = S_MusicForName(name);
   
   if(music)
      S_ChangeMusic(music, looping);
   else
   {
      C_Printf(FC_ERROR "music not found: %s\n", name);
      S_StopMusic(); // stop music anyway
   }
}

//
// S_StopMusic
//
void S_StopMusic()
{
   if(!mus_playing || !mus_playing->data)
      return;

   if(mus_paused)
      I_ResumeSong(mus_playing->handle);

   I_StopSong(mus_playing->handle);
   I_UnRegisterSong(mus_playing->handle);
   Z_Free(mus_playing->data);
   
   mus_playing->data = NULL;
   mus_playing = NULL;
}

//=============================================================================
//
// S_Start Music Handlers
//
// haleyjd 07/04/09: These are pointed to by GameModeInfo.
//

//
// Doom
//
// Probably the most complicated music determination, between the 
// original episodes and the addition of episode 4, which is all 
// over the place.
//
int S_MusicForMapDoom()
{
   static const int spmus[] =     // Song - Who? - Where?
   {
      mus_e3m4,     // American     e4m1
      mus_e3m2,     // Romero       e4m2
      mus_e3m3,     // Shawn        e4m3
      mus_e1m5,     // American     e4m4
      mus_e2m7,     // Tim          e4m5
      mus_e2m4,     // Romero       e4m6
      mus_e2m6,     // J.Anderson   e4m7 CHIRON.WAD
      mus_e2m5,     // Shawn        e4m8
      mus_e1m9      // Tim          e4m9
   };

   int episode = eclamp(gameepisode, 1, 4);
   int map     = eclamp(gamemap,     1, 9);
            
   // sf: simplified
   return episode < 4 ? mus_e1m1 + (episode-1)*9 + map-1 : spmus[map-1];
}

//
// Doom 2
//
// Drastically simpler.
//
int S_MusicForMapDoom2()
{
   int map = eclamp(gamemap, 1, 35);
   return (mus_runnin + map - 1);
}

//
// Heretic
//
// Also simple, thanks to H_Mus_Matrix, which is my own invention.
//
int S_MusicForMapHtic()
{
   // ensure bounds just for safety
   int gep = eclamp(gameepisode, 1, 6);
   int gmp = eclamp(gamemap,     1, 9);
     
   return H_Mus_Matrix[gep - 1][gmp - 1];
}

//
// S_Start
//
// Per level startup code.
// Kills playing sounds at start of level,
//  determines music if any, changes music.
//
void S_Start()
{
   int mnum;
   
   S_StopSounds(false);
   
   //jff 1/22/98 return if music is not enabled
   if(!mus_card || nomusicparm)
      return;
   
   // start new music for the level
   mus_paused = 0;
   
   if(!*LevelInfo.musicName && gamemap == 0)
   {
      // dont know what music to play
      // we need a default
      LevelInfo.musicName = GameModeInfo->defMusName;
   }
   
   // sf: replacement music
   if(*LevelInfo.musicName)
      S_ChangeMusicName(LevelInfo.musicName, true);
   else
   {
      if(idmusnum != -1)
         mnum = idmusnum; //jff 3/17/98 reload IDMUS music if not -1
      else
         mnum = GameModeInfo->MusicForMap();
         
      // start music
      S_ChangeMusicNum(mnum, true);
   }
}

//=============================================================================
//
// Music Cheat Routines
//

int S_DoomMusicCheat(const char *buf)
{
   int input = (buf[0] - '1') * 9 + (buf[1] - '1');
          
   // jff 4/11/98: prevent IDMUS0x IDMUSx0 in DOOM I and greater than introa
   if(buf[0] < '1' || buf[1] < '1' || input > 31)
      return -1;
   else
      return mus_e1m1 + input;
}

int S_Doom2MusicCheat(const char *buf)
{
   int input = (buf[0] - '0') * 10 + buf[1] - '0';

   // jff 4/11/98: prevent IDMUS00 in DOOM II and IDMUS36 or greater
   if(input < 1 || input > 35)
      return -1;
   else
      return mus_runnin + input - 1;
}

int S_HereticMusicCheat(const char *buf)
{
   // haleyjd 03/10/03: heretic support
   // use H_Mus_Matrix for easy access
   int episodenum = (buf[0] - '0') - 1;
   int mapnum     = (buf[1] - '0') - 1;

   if(episodenum < 0 || episodenum > 5 || mapnum < 0 || mapnum > 8)
      return -1;
   else
      return H_Mus_Matrix[episodenum][mapnum];
}

//=============================================================================
//
// Sound Initialization
//

static void S_HookMusic(musicinfo_t *);

//
// S_Init
// 
// Initializes sound stuff, including volume
// Sets channels, SFX and music volume,
//  allocates channel buffer, sets S_sfx lookup.
//
void S_Init(int sfxVolume, int musicVolume)
{
   // haleyjd 09/03/03: the sound hash is now maintained by EDF

   //jff 1/22/98 skip sound init if sound not enabled
   if(snd_card && !nosfxparm)
   {
      usermsg("\tdefault sfx volume %d", sfxVolume);  // killough 8/8/98

      S_SetSfxVolume(sfxVolume);

      // Allocating the internal channels for mixing
      // (the maximum numer of sounds rendered
      // simultaneously) within zone memory.

      // killough 10/98:
      numChannels = default_numChannels;
      channels = ecalloc(channel_t *, numChannels, sizeof(channel_t));
   }

   if(s_precache)        // sf: option to precache sounds
   {
      E_PreCacheSounds();
      usermsg("\tprecached all sounds.");
   }
   else
      usermsg("\tsounds to be cached dynamically.");

   if(!mus_card || nomusicparm)
      return;

   S_SetMusicVolume(musicVolume);
   
   // no sounds are playing, and they are not mus_paused
   mus_paused = 0;

   // hook in all natively defined music
   for(int i = 0; i < GameModeInfo->numMusic; i++)
      S_HookMusic(&(GameModeInfo->s_music[i]));
}

//=============================================================================
//
// Music Hashing
//

static void S_HookMusic(musicinfo_t *music)
{
   int hashslot;
   
   if(!music || !music->name) 
      return;
   
   hashslot = sound_hash(music->name);
   
   music->next = musicinfos[hashslot];
   musicinfos[hashslot] = music;
}

//
// S_MusicForName
//
// Returns a musicinfo_t structure given a music entry name.
// Returns NULL if not found.
//
musicinfo_t *S_MusicForName(const char *name)
{
   int hashnum, lumpnum;
   musicinfo_t *mus = NULL;
   bool nameHasPrefix, lumpHasPrefix;
   const char *nameToUse;
   qstring tempname;
   size_t prefixlen = strlen(GameModeInfo->musPrefix);
   size_t namelen   = strlen(name);

   tempname = name;
   tempname.toUpper(); // uppercase for insensitivity

   // If the gamemode-dependent music prefix is present, skip past it
   if(prefixlen > 0 && namelen > prefixlen && 
      tempname.findSubStr(GameModeInfo->musPrefix) == tempname.constPtr())
   {      
      nameToUse = name + prefixlen;
      nameHasPrefix = true;
   }
   else
   {
      nameToUse = name;
      nameHasPrefix = false;
   }

   tempname = nameToUse;

   hashnum = sound_hash(tempname.constPtr());
  
   for(mus = musicinfos[hashnum]; mus; mus = mus->next)
   {
      if(!tempname.strCaseCmp(mus->name))
         return mus;
   }

   if(nameHasPrefix) // name is prefixed? use it unconditionally as provided.
   {
      lumpnum = W_CheckNumForName(name);
      lumpHasPrefix = true;
   }
   else // name is unprefixed; try with prefix first, then without.
   {
      tempname.clear() << GameModeInfo->musPrefix << name;
      tempname.toUpper();
      lumpnum = W_CheckNumForName(tempname.constPtr());

      if(lumpnum < 0) // try, try again...
      {
         lumpnum = W_CheckNumForName(name);
         lumpHasPrefix = false;
      }
      else
         lumpHasPrefix = true;
   }
   
   // Not found? Create a new musicinfo if the indicated lump is found.
   if(lumpnum >= 0)
   {
      mus = estructalloc(musicinfo_t, 1);
      mus->name   = estrdup(nameToUse);
      mus->prefix = lumpHasPrefix;
      S_HookMusic(mus);
   }
   
   return mus;
}

//=============================================================================
//
// Console Commands
//

VARIABLE_BOOLEAN(s_precache,      NULL, onoff);
VARIABLE_BOOLEAN(pitched_sounds,  NULL, onoff);
VARIABLE_INT(default_numChannels, NULL, 1, 32,  NULL);
VARIABLE_INT(snd_SfxVolume,       NULL, 0, 15,  NULL);
VARIABLE_INT(snd_MusicVolume,     NULL, 0, 15,  NULL);
VARIABLE_BOOLEAN(forceFlipPan,    NULL, onoff);
VARIABLE_TOGGLE(s_hidefmusic,     NULL, onoff);

CONSOLE_VARIABLE(s_precache, s_precache, 0) {}
CONSOLE_VARIABLE(s_pitched, pitched_sounds, 0) {}
CONSOLE_VARIABLE(snd_channels, default_numChannels, 0) {}

CONSOLE_VARIABLE(sfx_volume, snd_SfxVolume, 0)
{
   S_SetSfxVolume(snd_SfxVolume);
}

CONSOLE_VARIABLE(music_volume, snd_MusicVolume, 0)
{
   S_SetMusicVolume(snd_MusicVolume);
}

// haleyjd 12/08/01: allow user to force reversal of audio channels
CONSOLE_VARIABLE(s_flippan, forceFlipPan, 0) {}

CONSOLE_VARIABLE(s_hidefmusic, s_hidefmusic, 0) {}

VARIABLE_TOGGLE(s_randmusic,      NULL, onoff);
CONSOLE_VARIABLE(s_randmusic, s_randmusic, 0) {}

CONSOLE_COMMAND(s_playmusic, 0)
{
   musicinfo_t *music;
   char namebuf[16];

   if(Console.argc < 1)
   {
      C_Printf(FC_ERROR "A music name is required.\a\n");
      return;
   }

   // check to see if there's a music by this name
   if(!(music = S_MusicForName(Console.argv[0]->constPtr())))
   {
      C_Printf(FC_ERROR "Unknown music %s\a\n", Console.argv[0]->constPtr());
      return;
   }

   // check to see if the lump exists to avoid stopping the current music if it
   // is missing
   if(music->prefix)
   {
      psnprintf(namebuf, sizeof(namebuf), "%s%s", 
                GameModeInfo->musPrefix, music->name);
   }
   else
      psnprintf(namebuf, sizeof(namebuf), "%s", music->name);

   if(W_CheckNumForName(namebuf) < 0)
   {
      C_Printf(FC_ERROR "Lump %s not found for music %s\a\n", namebuf, 
               Console.argv[0]->constPtr());
      return;
   }

   S_ChangeMusic(music, true);
}

#if 0
//
// Small native functions
//

static cell AMX_NATIVE_CALL sm_globalsound(AMX *amx, cell *params)
{
   int err;
   char *sndname;

   // get sound name
   if((err = SM_GetSmallString(amx, &sndname, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   S_StartSoundName(NULL, sndname);

   Z_Free(sndname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_glblsoundnum(AMX *amx, cell *params)
{
   int soundnum = (int)params[1];

   S_StartSound(NULL, soundnum);

   return 0;
}

static cell AMX_NATIVE_CALL sm_sectorsound(AMX *amx, cell *params)
{
   int err, tag, secnum = -1;
   char *sndname;

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   if((err = SM_GetSmallString(amx, &sndname, params[1])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   tag = params[2];

   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sector_t *sector = &sectors[secnum];
      
      S_StartSoundName(&sector->soundorg, sndname);
   }

   Z_Free(sndname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_sectorsoundnum(AMX *amx, cell *params)
{
   int tag, sndnum;
   int secnum = -1;

   sndnum  = params[1];
   tag     = params[2];

   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }
   
   while((secnum = P_FindSectorFromTag(tag, secnum)) >= 0)
   {
      sector_t *sector = &sectors[secnum];
      
      S_StartSound(&sector->soundorg, sndnum);
   }

   return 0;
}

AMX_NATIVE_INFO sound_Natives[] =
{
   { "_SoundGlobal",    sm_globalsound },
   { "_SoundGlobalNum", sm_glblsoundnum },
   { "_SectorSound",    sm_sectorsound },
   { "_SectorSoundNum", sm_sectorsoundnum },
   { NULL, NULL }
};
#endif

//----------------------------------------------------------------------------
//
// $Log: s_sound.c,v $
// Revision 1.11  1998/05/03  22:57:06  killough
// beautification, #include fix
//
// Revision 1.10  1998/04/27  01:47:28  killough
// Fix pickups silencing player weapons
//
// Revision 1.9  1998/03/23  03:39:12  killough
// Fix spy-mode sound effects
//
// Revision 1.8  1998/03/17  20:44:25  jim
// fixed idmus non-restore, space bug
//
// Revision 1.7  1998/03/09  07:32:57  killough
// ATTEMPT to support hearing with displayplayer's hears
//
// Revision 1.6  1998/03/04  07:46:10  killough
// Remove full-volume sound hack from MAP08
//
// Revision 1.5  1998/03/02  11:45:02  killough
// Make missing sounds non-fatal
//
// Revision 1.4  1998/02/02  13:18:48  killough
// stop incorrect looping of music (e.g. bunny scroll)
//
// Revision 1.3  1998/01/26  19:24:52  phares
// First rev with no ^Ms
//
// Revision 1.2  1998/01/23  01:50:49  jim
// Added music/sound options, and enables
//
// Revision 1.1.1.1  1998/01/19  14:03:04  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
