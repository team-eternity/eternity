// Emacs style mode select -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2003 James Haley
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//--------------------------------------------------------------------------
//
// Skin Viewer Widget for the Menu
//
//    Why? Because I can!
//
// haleyjd 04/20/03
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "doomtype.h"
#include "e_args.h"
#include "e_fonts.h"
#include "e_metastate.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "g_bind.h"
#include "info.h"
#include "m_collection.h"
#include "m_random.h"
#include "m_strcasestr.h"
#include "mn_engin.h"
#include "p_pspr.h"
#include "p_skin.h"
#include "r_defs.h"
#include "r_draw.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

// player skin sprite states
enum
{
   SKV_WALKING,
   SKV_FIRING,
   SKV_PAIN,
   SKV_DEAD,
};

// static state variables

static state_t *skview_state     = NULL;
static int      skview_tics      = 0;
static int      skview_action    = SKV_WALKING;
static int      skview_rot       = 0;
static bool     skview_halfspeed = false;
static int      skview_typenum;                 // 07/12/03
static bool     skview_haswdth   = false;       // 03/29/08
static int      skview_light     = 0;          // 01/22/12

// haleyjd 09/29/07: rewrites for player class engine
static statenum_t skview_atkstate2;

// haleyjd 01/22/12: for special death states
static PODCollection<MetaState *> skview_metadeaths;
static MetaState *skview_metadeath = NULL;

extern void A_PlaySoundEx(Mobj *);
extern void A_Pain(Mobj *);
extern void A_Scream(Mobj *);
extern void A_PlayerScream(Mobj *);
extern void A_XScream(Mobj *);

//
// MN_skinEmulateAction
//
// Some actions can be emulated here.
//
static void MN_skinEmulateAction(state_t *state)
{
   if(state->action == A_PlaySoundEx)
   {
      sfxinfo_t *snd;

      if((snd = E_ArgAsSound(state->args, 0)))
         S_StartSfxInfo(NULL, snd, 127, ATTN_NORMAL, false, CHAN_AUTO);
   }
   else if(state->action == A_Pain)
   {
      S_StartSoundName(NULL, players[consoleplayer].skin->sounds[sk_plpain]);
   }
   else if(state->action == A_Scream)
   {
      S_StartSoundName(NULL, players[consoleplayer].skin->sounds[sk_pldeth]);
   }
   else if(state->action == A_PlayerScream)
   {
      // 03/29/08: sometimes play wimpy death if the player skin specifies one
      if(skview_haswdth)
      {
         int rnd = M_Random() % ((GameModeInfo->id == shareware) ? 2 : 3);
         int sndnum = 0;

         switch(rnd)
         {
         case 0:
            sndnum = sk_pldeth;
            break;
         case 1:
            sndnum = sk_plwdth;
            break;
         case 2:
            sndnum = sk_pdiehi;
            break;
         }

         S_StartSoundName(NULL, players[consoleplayer].skin->sounds[sndnum]);
      }
      else
      {
         S_StartSoundName(NULL,
            (GameModeInfo->id == shareware || M_Random() % 2) ? 
             players[consoleplayer].skin->sounds[sk_pldeth] :
             players[consoleplayer].skin->sounds[sk_pdiehi]);
      }
   }
   else if(state->action == A_XScream)
   {
      S_StartSoundName(NULL, players[consoleplayer].skin->sounds[sk_slop]);
   }
}

//
// MN_SkinSetState
//
// Sets the skin sprite to a state, and sets its tics appropriately.
// The tics will be set to 2x normal if half speed mode is active.
//
static void MN_SkinSetState(state_t *state)
{
   int tics;
   
   skview_state = state;
   
   tics = skview_state->tics;
   skview_tics = menutime + (skview_halfspeed ? 2*tics : tics);

   MN_skinEmulateAction(skview_state);
}

//
// MN_SkinResponder
//
// The skin viewer widget responder function. Sorta long and
// hackish, but cool.
//
static bool MN_SkinResponder(event_t *ev)
{
   // only interested in keydown events
   if(ev->type != ev_keydown)
      return false;

   if(action_menu_toggle || action_menu_previous)
   {
      action_menu_toggle = action_menu_previous = false;

      // kill the widget
      skview_metadeaths.clear();
      S_StartSound(NULL, GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
      current_menuwidget = NULL;
      return true;
   }

   if(action_menu_left)
   {
      action_menu_left = false;

      // rotate sprite left
      if(skview_rot == 7)
         skview_rot = 0;
      else
         skview_rot++;

      return true;
   }

   if(action_menu_right)
   {
      action_menu_right = false;

      // rotate sprite right
      if(skview_rot == 0)
         skview_rot = 7;
      else
         skview_rot--;

      return true;
   }

   if(action_menu_up)
   {
      action_menu_up = false;

      // increase light level
      if(skview_light != 0)
         --skview_light;

      return true;
   }

   if(action_menu_down)
   {
      action_menu_down = false;

      // decrease light level
      if(skview_light != 31)
         ++skview_light;

      return true;
   }

   switch(ev->data1)
   {
   case KEYD_RCTRL:
      // attack!
      if(skview_action == SKV_WALKING)
      {
         S_StartSound(NULL, GameModeInfo->skvAtkSound);
         MN_SkinSetState(states[skview_atkstate2]);
         skview_action = SKV_FIRING;
      }
      break;
   case 'p':
   case 'P':
      // act hurt
      if(skview_action == SKV_WALKING)
      {
         MN_SkinSetState(states[mobjinfo[skview_typenum]->painstate]);
         skview_action = SKV_PAIN;
      }
      break;
   case 'd':
   case 'D':
      // die normally
      if(skview_action != SKV_DEAD)
      {
         MN_SkinSetState(states[mobjinfo[skview_typenum]->deathstate]);
         skview_action = SKV_DEAD;
      }
      break;
   case 'm':
   case 'M':
      // "meta" death (elemental death states)
      if(skview_action != SKV_DEAD && !skview_metadeaths.isEmpty())
      {
         skview_metadeath = skview_metadeaths.wrapIterator();
         MN_SkinSetState(skview_metadeath->getValue());
         skview_action = SKV_DEAD;
      }
      break;
   case 'x':
   case 'X':
      // gib
      if(skview_action != SKV_DEAD)
      {
         MN_SkinSetState(states[mobjinfo[skview_typenum]->xdeathstate]);
         skview_action = SKV_DEAD;
      }
      break;
   case 'h':
   case 'H':
      // toggle half-speed animation
      skview_halfspeed ^= true;
      break;
   case ' ':
      // "respawn" the player if dead
      if(skview_action == SKV_DEAD)
      {
         S_StartSound(NULL, GameModeInfo->teleSound);
         MN_SkinSetState(states[mobjinfo[skview_typenum]->seestate]);
         skview_action = SKV_WALKING;
      }
      break;
   default:
      return false;
   }

   return true;
}

// haleyjd 05/29/06: got rid of cascading macros in preference of this
#define INSTR_Y ((SCREENHEIGHT - 1) - (menu_font_normal->cy * 5))

extern vfont_t *menu_font_big;
extern vfont_t *menu_font_normal;

//
// MN_SkinInstructions
//
// Draws some v_font strings to the screen to tell the user how
// to operate this beast.
//
static void MN_SkinInstructions(void)
{
   const char *msg = FC_GOLD "skin viewer";

   void (*textfunc)(vfont_t *, const char *, int, int) = 
      GameModeInfo->flags & GIF_SHADOWTITLES ? 
        V_FontWriteTextShadowed : V_FontWriteText;

   // draw a title at the top, too

   textfunc(menu_font_big, msg, 
            160 - V_FontStringWidth(menu_font_big, msg)/2, 8);

   // haleyjd 05/29/06: rewrote to be binding neutral and to draw all of
   // it with one call to V_FontWriteText instead of five.
   V_FontWriteText(menu_font_normal, 
               "instructions:\n"
               FC_GRAY "left" FC_RED " = rotate left, "
               FC_GRAY "right" FC_RED " = rotate right\n"
               FC_GRAY "ctrl" FC_RED " = fire, "
               FC_GRAY "p" FC_RED " = pain, "
               FC_GRAY "d" FC_RED " = die, "
               FC_GRAY "x" FC_RED " = gib\n"
               FC_GRAY "space" FC_RED " = respawn, "
               FC_GRAY "h" FC_RED " = half-speed\n"
               FC_GRAY "toggle or previous" FC_RED " = exit", 4, INSTR_Y);
}

//
// MN_SkinDrawer
//
// The skin viewer widget drawer function. Basically implements a
// small state machine and sprite drawer all in one function. Since
// state transition timing is done through the drawer and not a ticker,
// it's not absolutely precise, but it's good enough.
//
static void MN_SkinDrawer(void)
{
   spritedef_t *sprdef;
   spriteframe_t *sprframe;
   int lump;
   bool flip;
   patch_t *patch;
   int pctype;
   int lighttouse;
   byte *translate = NULL;

   // draw the normal menu background
   V_DrawBackground(mn_background_flat, &vbscreen);

   // draw instructions and title
   MN_SkinInstructions();

   pctype = players[consoleplayer].pclass->type;

   if(skview_state->sprite == mobjinfo[pctype]->defsprite)
   {
      // get the player skin sprite definition
      sprdef = &sprites[players[consoleplayer].skin->sprite];
      if(!(sprdef->spriteframes))
         return;
   }
   else
   {
      sprdef = &sprites[skview_state->sprite];
      if(!(sprdef->spriteframes))
         return;
   }

   // get the current frame, using the skin state and rotation vars
   sprframe = &sprdef->spriteframes[skview_state->frame&FF_FRAMEMASK];
   if(sprframe->rotate)
   {
      lump = sprframe->lump[skview_rot];
      flip = !!sprframe->flip[skview_rot];
   }
   else
   {
      lump = sprframe->lump[0];
      flip = !!sprframe->flip[0];
   }

   lighttouse = (skview_state->frame & FF_FULLBRIGHT ? 0 : skview_light);

   if(players[consoleplayer].colormap)
      translate = translationtables[players[consoleplayer].colormap - 1];

   // cache the sprite patch -- watch out for "firstspritelump"!
   patch = PatchLoader::CacheNum(wGlobalDir, lump+firstspritelump, PU_CACHE);

   // draw the sprite, with color translation and proper flipping
   // 01/12/04: changed translation handling
   V_DrawPatchTranslatedLit(160, 120, &vbscreen, patch, translate, 
                            colormaps[0] + 256 * lighttouse, flip);
}

//
// MN_SkinTicker
//
// haleyjd 05/29/06: separated out from the drawer and added ticker
// support to widgets to enable precise state transition timing.
//
void MN_SkinTicker(void)
{
   if(skview_tics != -1 && menutime >= skview_tics)
   {
      // hack states: these need special nextstate handling so
      // that the player will start walking again afterward
      if(skview_state->nextstate == mobjinfo[skview_typenum]->spawnstate)
      {
         MN_SkinSetState(states[mobjinfo[skview_typenum]->seestate]);
         skview_action = SKV_WALKING;
      }
      else
      {
         // normal state transition
         MN_SkinSetState(states[skview_state->nextstate]);

         // if the state has -1 tics, reset skview_tics
         if(skview_state->tics == -1)
            skview_tics = -1;
      }
   }
}

//
// MN_initMetaDeaths
//
// haleyjd 01/22/12: Pull all meta death states out of the player class
// object's metatable.
//
static void MN_initMetaDeaths()
{
   playerclass_t *pclass = players[consoleplayer].pclass;
   MetaTable     *meta   = mobjinfo[pclass->type]->meta;
   MetaObject    *obj    = NULL;
   
   skview_metadeaths.clear();

   while((obj = meta->getNextType(obj, METATYPE(MetaState))))
   {
      // NB: also matches XDeath implicitly.
      if(M_StrCaseStr(obj->getKey(), "Death."))
         skview_metadeaths.add(static_cast<MetaState *>(obj));
   }
}

// the skinviewer menu widget

menuwidget_t skinviewer = { MN_SkinDrawer, MN_SkinResponder, MN_SkinTicker, true };

//
// MN_InitSkinViewer
//
// Called by the skinviewer console command, this function resets
// all the skview internal state variables to their defaults, and
// activates the skinviewer menu widget.
//
void MN_InitSkinViewer(void)
{
   playerclass_t *pclass = players[consoleplayer].pclass; // haleyjd 09/29/07

   // reset all state variables
   skview_action    = SKV_WALKING;
   skview_rot       = 0;
   skview_halfspeed = false;
   skview_typenum   = pclass->type;
   skview_light     = 0;

   // haleyjd 09/29/07: save alternate attack state number
   skview_atkstate2 = pclass->altattack;

   // haleyjd 03/29/08: determine if player skin has wimpy death sound
   skview_haswdth = 
      (strcasecmp(players[consoleplayer].skin->sounds[sk_plwdth], "none") != 0);

   MN_SkinSetState(states[mobjinfo[skview_typenum]->seestate]);

   MN_initMetaDeaths();

   // set the widget
   current_menuwidget = &skinviewer;
}

// EOF

