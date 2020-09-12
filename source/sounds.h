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
//--------------------------------------------------------------------------
//
// DESCRIPTION:
//      Created by the sound utility written by Dave Taylor.
//      Kept as a sample, DOOM2 sounds. Frozen.
//
//      haleyjd: erm... ::lights a fire to warm it up::
//
//-----------------------------------------------------------------------------

#ifndef __SOUNDS__
#define __SOUNDS__

#include "doomtype.h"
#include "m_dllist.h"

//
// SoundFX struct.
//

// haleyjd 06/12/08: origin subchannels
typedef enum
{
   CHAN_ALL = -1, // to S_StopSound, this means stop everything
   CHAN_AUTO,
   CHAN_WEAPON,
   CHAN_VOICE,
   CHAN_ITEM,
   CHAN_BODY,
   CHAN_SLOT5,
   CHAN_SLOT6,
   CHAN_SLOT7,
   NUMSCHANNELS
} schannel_e;

// haleyjd 03/27/11: sound flags
enum
{
   SFXF_PREFIX    = 0x00000001, // lumpname should be prefixed with "DS"
   SFXF_NOPCSOUND = 0x00000002, // sound won't play under PC speaker emulation
   SFXF_EDF       = 0x00000004, // sfxinfo was born via EDF
   SFXF_SNDINFO   = 0x00000008, // sfxinfo was born via SNDINFO lump
   SFXF_WAD       = 0x00000010  // sfxinfo was born as an implicit wad sound
};

struct sfxinfo_t
{
   // Sfx singularity (only one at a time)
   enum
   {
      sg_none,
      sg_itemup,
      sg_wpnup,
      sg_oof,
      sg_getpow
   };

   // haleyjd 09/23/06: pitch variance types
   enum
   {
      pitch_none,    // none: always at normal pitch.
      pitch_doom,    // normal variance for DOOM v1.1
      pitch_doomsaw, // variance for DOOM chainsaw
      pitch_heretic, // normal variance for Heretic
      pitch_hticamb, // variance for Heretic ambient sounds
   };

   char name[9];       // haleyjd: up to 8-character lump name
   char pcslump[9];    // haleyjd: explicitly provided PC speaker effect lump
   int  singularity;   // killough 12/98: implement separate classes of singularity
   int  priority;      // Sfx priority
   int  pitch;         // pitch if a link
   int  volume;        // volume if a link
   int  pitch_type;    // haleyjd 09/23/06: pitch variance type
   int  skinsound;     // sf: skin sound number to use in place
   int  subchannel;    // haleyjd 06/12/08: origin subchannels - default = CHAN_AUTO.
   unsigned int flags; // haleyjd 03/27/11: sound effect flags

   // haleyjd 07/13/05: sound attenuation properties now customizable
   // on a per-sound basis to allow differing behaviors.
   int clipping_dist;   // distance when sound is clipped entirely
   int close_dist;      // distance when sound is at maximum volume

   // links to other sound definitions
   sfxinfo_t  *link;         // referenced sound if a link
   sfxinfo_t  *alias;        // haleyjd 09/24/06: referenced sound if an alias
   sfxinfo_t **randomsounds; // haleyjd 05/12/09: random sounds
   int numrandomsounds;

   // haleyjd 04/23/08: additional caching data
   void *data;        // sound data
   int length;        // lump length
   unsigned int alen; // length of converted sound pointed to by data

   // this is checked every second to see if sound
   // can be thrown out (if 0, then decrement, if -1,
   // then throw out, if > 0, then it is in use)
   int usefulness;

   // haleyjd: EDF mnemonic
   char mnemonic[129];

   char *lfn;    // alison: long file name
   char *pcslfn; // alison: long file name for PC speaker sound

   // haleyjd 09/03/03: revised for dynamic EDF sounds
   DLListItem<sfxinfo_t> numlinks; // haleyjd 04/13/08: numeric hash links
   sfxinfo_t *next;                // next in mnemonic hash chain
   int dehackednum;                // dehacked number
};

//
// MusicInfo struct.
//

struct musicinfo_t
{
   // up to 6-character name
   const char *name;

   // haleyjd 04/10/11: whether to apply prefix or not
   bool prefix;

   // lump number of music
   //  int lumpnum;

   // music data
   void *data;

   // music handle once registered
   int handle;

   // sf: for hashing
   musicinfo_t *next;
};

// the complete set of sound effects

// haleyjd 11/05/03: made dynamic with EDF
extern int NUMSFX;

// the complete set of music
extern musicinfo_t  S_music[];

// heretic music
extern musicinfo_t  H_music[];

// haleyjd: clever indirection for heretic maps
extern int H_Mus_Matrix[6][9];

// strife music
extern musicinfo_t  Strf_music[];

//
// Identifiers for all music in game.
//

typedef enum {
  mus_None,
  mus_e1m1,
  mus_e1m2,
  mus_e1m3,
  mus_e1m4,
  mus_e1m5,
  mus_e1m6,
  mus_e1m7,
  mus_e1m8,
  mus_e1m9,
  mus_e2m1,
  mus_e2m2,
  mus_e2m3,
  mus_e2m4,
  mus_e2m5,
  mus_e2m6,
  mus_e2m7,
  mus_e2m8,
  mus_e2m9,
  mus_e3m1,
  mus_e3m2,
  mus_e3m3,
  mus_e3m4,
  mus_e3m5,
  mus_e3m6,
  mus_e3m7,
  mus_e3m8,
  mus_e3m9,
  mus_inter,
  mus_intro,
  mus_bunny,
  mus_victor,
  mus_introa,
  mus_runnin,
  mus_stalks,
  mus_countd,
  mus_betwee,
  mus_doom,
  mus_the_da,
  mus_shawn,
  mus_ddtblu,
  mus_in_cit,
  mus_dead,
  mus_stlks2,
  mus_theda2,
  mus_doom2,
  mus_ddtbl2,
  mus_runni2,
  mus_dead2,
  mus_stlks3,
  mus_romero,
  mus_shawn2,
  mus_messag,
  mus_count2,
  mus_ddtbl3,
  mus_ampie,
  mus_theda3,
  mus_adrian,
  mus_messg2,
  mus_romer2,
  mus_tense,
  mus_shawn3,
  mus_openin,
  mus_evil,
  mus_ultima,
  mus_read_m,
  mus_dm2ttl,
  mus_dm2int,
  NUMMUSIC
} doom_musicenum_t;

// haleyjd: heretic music
typedef enum
{
   hmus_None,
   hmus_e1m1,
   hmus_e1m2,
   hmus_e1m3,
   hmus_e1m4,
   hmus_e1m5,
   hmus_e1m6,
   hmus_e1m7,
   hmus_e1m8,
   hmus_e1m9,
   hmus_e2m1,
   hmus_e2m2,
   hmus_e2m3,
   hmus_e2m4,
   hmus_e2m6,
   hmus_e2m7,
   hmus_e2m8,
   hmus_e2m9,
   hmus_e3m2,
   hmus_e3m3,
   hmus_titl,
   hmus_intr,
   hmus_cptd,
   NUMHTICMUSIC
} htic_musicenum_t;

// villsa [STRIFE]
typedef enum
{
   smus_None,
   smus_logo,
   smus_action,
   smus_tavern,
   smus_danger,
   smus_fast,
   smus_intro,
   smus_darker,
   smus_strike,
   smus_slide,
   smus_tribal,
   smus_march,
   smus_danger2,
   smus_mood,
   smus_castle,
   smus_darker2,
   smus_action2,
   smus_fight,
   smus_spense,
   smus_slide2,
   smus_strike2,
   smus_dark,
   smus_tech,
   smus_slide3,
   smus_drone,
   smus_panthr,
   smus_sad,
   smus_instry,
   smus_tech2,
   smus_action3,
   smus_instry2,
   smus_drone2,
   smus_fight2,
   smus_happy,
   smus_end,
   NUMSTRFMUSIC
} strf_musicenum_t;

//
// Identifiers for all sfx in game.
//

typedef enum {
  sfx_None,
  sfx_pistol,
  sfx_shotgn,
  sfx_sgcock,
  sfx_dshtgn,
  sfx_dbopn,
  sfx_dbcls,
  sfx_dbload,
  sfx_plasma,
  sfx_bfg,
  sfx_sawup,
  sfx_sawidl,
  sfx_sawful,
  sfx_sawhit,
  sfx_rlaunc,
  sfx_rxplod,
  sfx_firsht,
  sfx_firxpl,
  sfx_pstart,
  sfx_pstop,
  sfx_doropn,
  sfx_dorcls,
  sfx_stnmov,
  sfx_swtchn,
  sfx_swtchx,
  sfx_plpain,
  sfx_dmpain,
  sfx_popain,
  sfx_vipain,
  sfx_mnpain,
  sfx_pepain,
  sfx_slop,
  sfx_itemup,
  sfx_wpnup,
  sfx_oof,
  sfx_telept,
  sfx_posit1,
  sfx_posit2,
  sfx_posit3,
  sfx_bgsit1,
  sfx_bgsit2,
  sfx_sgtsit,
  sfx_cacsit,
  sfx_brssit,
  sfx_cybsit,
  sfx_spisit,
  sfx_bspsit,
  sfx_kntsit,
  sfx_vilsit,
  sfx_mansit,
  sfx_pesit,
  sfx_sklatk,
  sfx_sgtatk,
  sfx_skepch,
  sfx_vilatk,
  sfx_claw,
  sfx_skeswg,
  sfx_pldeth,
  sfx_pdiehi,
  sfx_podth1,
  sfx_podth2,
  sfx_podth3,
  sfx_bgdth1,
  sfx_bgdth2,
  sfx_sgtdth,
  sfx_cacdth,
  sfx_skldth,
  sfx_brsdth,
  sfx_cybdth,
  sfx_spidth,
  sfx_bspdth,
  sfx_vildth,
  sfx_kntdth,
  sfx_pedth,
  sfx_skedth,
  sfx_posact,
  sfx_bgact,
  sfx_dmact,
  sfx_bspact,
  sfx_bspwlk,
  sfx_vilact,
  sfx_noway,
  sfx_barexp,
  sfx_punch,
  sfx_hoof,
  sfx_metal,
  sfx_chgun,
  sfx_tink,
  sfx_bdopn,
  sfx_bdcls,
  sfx_itmbk,
  sfx_flame,
  sfx_flamst,
  sfx_getpow,
  sfx_bospit,
  sfx_boscub,
  sfx_bossit,
  sfx_bospn,
  sfx_bosdth,
  sfx_manatk,
  sfx_mandth,
  sfx_sssit,
  sfx_ssdth,
  sfx_keenpn,
  sfx_keendt,
  sfx_skeact,
  sfx_skesit,
  sfx_skeatk,
  sfx_radio,

  // killough 11/98: dog sounds
  sfx_dgsit,
  sfx_dgatk,
  sfx_dgact,
  sfx_dgdth,
  sfx_dgpain,

  // haleyjd: Eternity Engine sounds
  sfx_eefly,  // buzzing flies
  sfx_gloop,  // water terrain sound
  sfx_thundr, // global lightning sound effect
  sfx_muck,   // swamp terrain sound
  sfx_plfeet, // player feet hit
  sfx_plfall, // player falling scream
  sfx_fallht, // player fall hit
  sfx_burn,   // lava terrain sound
  sfx_eehtsz, // heat sizzle
  sfx_eedrip, // drip
  sfx_quake2, // earthquake
  sfx_plwdth, // player wimpy death
  sfx_jump,

  // haleyjd 10/08/02: heretic sounds
  sfx_gldhit = 300,
  sfx_htelept,
  sfx_chat,
  sfx_keyup,
  sfx_hitemup,
  sfx_mumsit,
  sfx_mumat1,
  sfx_mumat2,
  sfx_mumpai,
  sfx_mumdth,
  sfx_mumhed,
  sfx_clksit,
  sfx_clkatk,
  sfx_clkpai,
  sfx_clkdth,
  sfx_clkact,
  sfx_wizsit,
  sfx_wizatk,
  sfx_wizdth,
  sfx_wizact,
  sfx_wizpai,
  sfx_sorzap,
  sfx_sorrise,
  sfx_sorsit,
  sfx_soratk,
  sfx_sorpai,
  sfx_soract,
  sfx_sordsph,
  sfx_sordexp,
  sfx_sordbon,
  sfx_wind,
  sfx_waterfl,
  sfx_podexp,
  sfx_newpod,
  sfx_kgtsit,
  sfx_kgtatk,
  sfx_kgtat2,
  sfx_kgtdth,
  sfx_kgtpai,
  sfx_hrnhit,
  sfx_bstsit,
  sfx_bstatk,
  sfx_bstpai,
  sfx_bstdth,
  sfx_bstact,
  sfx_snksit,
  sfx_snkatk,
  sfx_snkpai,
  sfx_snkdth,
  sfx_snkact,
  sfx_hdoropn,
  sfx_hdorcls,
  sfx_hswitch,
  sfx_hpstart,
  sfx_hpstop,
  sfx_hstnmov,
  sfx_sbtpai,
  sfx_sbtdth,
  sfx_sbtact,
  sfx_lobhit,
  sfx_minsit,
  sfx_minat1,
  sfx_minat2,
  sfx_minpai,
  sfx_mindth,
  sfx_minact,
  sfx_stfpow,
  sfx_phohit,
  sfx_hedsit,
  sfx_hedat1,
  sfx_hedat2,
  sfx_hedat3,
  sfx_heddth,
  sfx_hedact,
  sfx_hedpai,
  sfx_impsit,
  sfx_impat1,
  sfx_impat2,
  sfx_imppai,
  sfx_impdth,
  sfx_htamb1,
  sfx_htamb2,
  sfx_htamb3,
  sfx_htamb4,
  sfx_htamb5,
  sfx_htamb6,
  sfx_htamb7,
  sfx_htamb8,
  sfx_htamb9,
  sfx_htamb10,
  sfx_htamb11,
  sfx_htdormov,
  sfx_hplroof,
  sfx_hplrpai,
  sfx_hplrdth,
  sfx_hgibdth,
  sfx_hplrwdth,
  sfx_hplrcdth,
  sfx_hstfhit,
  sfx_hchicatk,
  sfx_hhrnsht,
  sfx_hphosht,
  sfx_hwpnup,
  sfx_hrespawn,
  sfx_artiup,
  sfx_artiuse,
  sfx_blssht,
  sfx_bowsht,
  sfx_gntful,
  sfx_gnthit,
  sfx_gntpow,
  sfx_gntact,
  sfx_gntuse,
  sfx_stfcrk,
  sfx_lobsht,
  sfx_bounce,
  sfx_phopow,
  sfx_blshit,
  sfx_hnoway,

  // Strife sounds
  sfx_swish = 500,
  sfx_meatht,
  sfx_mtalht,
  sfx_swpnup,
  sfx_rifle,
  sfx_mislht,
  sfx_sbarexp,
  sfx_flburn,
  sfx_flidl,
  sfx_agrsee,
  sfx_splpain,
  sfx_pcrush,
  sfx_pespna,
  sfx_pespnb,
  sfx_pespnc,
  sfx_pespnd,
  sfx_agrdpn,
  sfx_spldeth,
  sfx_plxdth,
  sfx_sslop,
  sfx_rebdth,
  sfx_agrdth,
  sfx_lgfire,
  sfx_smfire,
  sfx_alarm,
  sfx_drlmto,
  sfx_drlmtc,
  sfx_drsmto,
  sfx_drsmtc,
  sfx_drlwud,
  sfx_drswud,
  sfx_drston,
  sfx_sbdopn,
  sfx_sbdcls,
  sfx_sswtchn,
  sfx_swbolt,
  sfx_swscan,
  sfx_yeah,
  sfx_mask,
  sfx_spstart,
  sfx_spstop,
  sfx_sitemup,
  sfx_bglass,
  sfx_wriver,
  sfx_wfall,
  sfx_wdrip,
  sfx_wsplsh,
  sfx_rebact,
  sfx_agrac1,
  sfx_agrac2,
  sfx_agrac3,
  sfx_agrac4,
  sfx_ambppl,
  sfx_ambbar,
  sfx_stelept,
  sfx_ratact,
  sfx_sitmbk,
  sfx_xbow,
  sfx_burnme,
  sfx_soof,
  sfx_wbrldt,
  sfx_psdtha,
  sfx_psdthb,
  sfx_psdthc,
  sfx_rb2pn,
  sfx_rb2dth,
  sfx_rb2see,
  sfx_rb2act,
  sfx_sfirxpl,
  sfx_sstnmov,
  sfx_snoway,
  sfx_srlaunc,
  sfx_rflite,
  sfx_sradio,
  sfx_pulchn,
  sfx_swknob,
  sfx_keycrd,
  sfx_swston,
  sfx_sntsee,
  sfx_sntdth,
  sfx_sntact,
  sfx_pgrdat,
  sfx_pgrsee,
  sfx_pgrdpn,
  sfx_pgrdth,
  sfx_pgract,
  sfx_proton,
  sfx_protfl,
  sfx_splasma,
  sfx_dsrptr,
  sfx_reavat,
  sfx_revbld,
  sfx_revsee,
  sfx_reavpn,
  sfx_revdth,
  sfx_revact,
  sfx_sspisit,
  sfx_spdwlk,
  sfx_sspidth,
  sfx_spdatk,
  sfx_chant,
  sfx_static,
  sfx_chain,
  sfx_tend,
  sfx_phoot,
  sfx_explod,
  sfx_sigil,
  sfx_sglhit,
  sfx_siglup,
  sfx_prgpn,
  sfx_progac,
  sfx_lorpn,
  sfx_lorsee,
  sfx_difool,
  sfx_inqdth,
  sfx_inqact,
  sfx_inqsee,
  sfx_inqjmp,
  sfx_amaln1,
  sfx_amaln2,
  sfx_amaln3,
  sfx_amaln4,
  sfx_amaln5,
  sfx_amaln6,
  sfx_mnalse,
  sfx_alnsee,
  sfx_alnpn,
  sfx_alnact,
  sfx_alndth,
  sfx_mnaldt,
  sfx_reactr,
  sfx_airlck,
  sfx_drchno,
  sfx_drchnc,
  sfx_valve,

  // haleyjd 11/05/03: NUMSFX is a variable now
  // NUMSFX
} sfxenum_t;

#endif

//----------------------------------------------------------------------------
//
// $Log: sounds.h,v $
// Revision 1.3  1998/05/03  22:44:30  killough
// beautification
//
// Revision 1.2  1998/01/26  19:27:53  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:03  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
