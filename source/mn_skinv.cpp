//
// The Eternity Engine
// Copyright (C) 2025 James Haley et al.
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
// Purpose: Skin viewer widget for the menu. Why? Because I can!
// Authors: James Haley
//

#include "z_zone.h"

#include "a_args.h"
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

static state_t *skview_state     = nullptr;
static int      skview_tics      = 0;
static int      skview_action    = SKV_WALKING;
static int      skview_rot       = 0;
static bool     skview_halfspeed = false;
static int      skview_typenum;         // 07/12/03
static bool     skview_haswdth = false; // 03/29/08
static bool     skview_gibbed  = false;
static int      skview_light   = 0; // 01/22/12

// haleyjd 09/29/07: rewrites for player class engine
static statenum_t skview_atkstate2;

// haleyjd 01/22/12: for special death states
static PODCollection<MetaState *> skview_metadeaths;
static MetaState                 *skview_metadeath = nullptr;

extern void A_PlaySoundEx(actionargs_t *);
extern void A_Pain(actionargs_t *);
extern void A_Scream(actionargs_t *);
extern void A_PlayerScream(actionargs_t *);
extern void A_RavenPlayerScream(actionargs_t *);
extern void A_XScream(actionargs_t *);
extern void A_FlameSnd(actionargs_t *);

//
// MN_skinEmulateAction
//
// Some actions can be emulated here.
//
static void MN_skinEmulateAction(state_t *state)
{
    if(state->action->codeptr == A_PlaySoundEx) // AEON_FIXME
    {
        S_StartInterfaceSound(E_ArgAsSound(state->args, 0));
    }
    else if(state->action->codeptr == A_Pain) // AEON_FIXME
    {
        S_StartInterfaceSound(players[consoleplayer].skin->sounds[sk_plpain]);
    }
    else if(state->action->codeptr == A_Scream) // AEON_FIXME
    {
        S_StartInterfaceSound(players[consoleplayer].skin->sounds[sk_pldeth]);
    }
    else if(state->action->codeptr == A_PlayerScream || state->action->codeptr == A_RavenPlayerScream) // AEON_FIXME
    {
        if(skview_gibbed) // may gib when using Raven player behavior
            S_StartInterfaceSound(players[consoleplayer].skin->sounds[sk_slop]);
        else if(skview_haswdth)
        {
            // 03/29/08: sometimes play wimpy death if the player skin specifies one
            int rnd    = M_Random() % ((GameModeInfo->flags & GIF_NODIEHI) ? 2 : 3);
            int sndnum = 0;

            switch(rnd)
            {
            case 0: sndnum = sk_pldeth; break;
            case 1: sndnum = sk_plwdth; break;
            case 2: sndnum = sk_pdiehi; break;
            }

            S_StartInterfaceSound(players[consoleplayer].skin->sounds[sndnum]);
        }
        else
        {
            S_StartInterfaceSound(((GameModeInfo->flags & GIF_NODIEHI) || M_Random() % 2) ?
                                      players[consoleplayer].skin->sounds[sk_pldeth] :
                                      players[consoleplayer].skin->sounds[sk_pdiehi]);
        }
    }
    else if(state->action->codeptr == A_XScream) // AEON_FIXME
    {
        S_StartInterfaceSound(players[consoleplayer].skin->sounds[sk_slop]);
    }
    else if(state->action->codeptr == A_FlameSnd) // AEON_FIXME
    {
        S_StartInterfaceSound("ht_hedat1");
    }

    skview_gibbed = false;
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

    tics        = skview_state->tics;
    skview_tics = menutime + (skview_halfspeed ? 2 * tics : tics);

    MN_skinEmulateAction(skview_state);
}

//
// MN_SkinResponder
//
// The skin viewer widget responder function. Sorta long and
// hackish, but cool.
//
static bool MN_SkinResponder(event_t *ev, int action)
{
    // only interested in keydown events
    if(ev->type != ev_keydown)
        return false;

    if(action == ka_menu_toggle || action == ka_menu_previous)
    {
        // kill the widget
        skview_metadeaths.clear();
        S_StartInterfaceSound(GameModeInfo->menuSounds[MN_SND_DEACTIVATE]);
        MN_PopWidget();
        return true;
    }

    if(action == ka_menu_left)
    {
        // rotate sprite left
        if(skview_rot == 7)
            skview_rot = 0;
        else
            skview_rot++;

        return true;
    }

    if(action == ka_menu_right)
    {
        // rotate sprite right
        if(skview_rot == 0)
            skview_rot = 7;
        else
            skview_rot--;

        return true;
    }

    if(action == ka_menu_up)
    {
        // increase light level
        if(skview_light != 0)
            --skview_light;

        return true;
    }

    if(action == ka_menu_down)
    {
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
            S_StartInterfaceSound(GameModeInfo->skvAtkSound);
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
            MN_SkinSetState(skview_metadeath->state);
            skview_action = SKV_DEAD;
        }
        break;
    case 'x':
    case 'X':
        // gib
        if(skview_action != SKV_DEAD)
        {
            skview_gibbed = true;
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
            S_StartInterfaceSound(GameModeInfo->teleSound);
            MN_SkinSetState(states[mobjinfo[skview_typenum]->seestate]);
            skview_action = SKV_WALKING;
        }
        break;
    default: //
        return false;
    }

    return true;
}

// haleyjd 05/29/06: got rid of cascading macros in preference of this
#define INSTR_Y ((SCREENHEIGHT - 1) - (menu_font_normal->cy * 5))

//
// MN_SkinInstructions
//
// Draws some v_font strings to the screen to tell the user how
// to operate this beast.
//
static void MN_SkinInstructions()
{
    const char *msg = FC_RED "skin viewer";

    int x     = 160 - V_FontStringWidth(menu_font_big, msg) / 2;
    int y     = 8;
    int color = GameModeInfo->titleColor;

    // draw a title at the top, too
    if(GameModeInfo->flags & GIF_SHADOWTITLES)
    {
        V_FontWriteTextShadowed(menu_font_big, msg, x, y, &subscreen43, color);
    }
    else
    {
        V_FontWriteTextColored(menu_font_big, msg, color, x, y, &subscreen43);
    }

    // haleyjd 05/29/06: rewrote to be binding neutral and to draw all of
    // it with one call to V_FontWriteText instead of five.
    V_FontWriteText(menu_font_normal,
                    "Instructions:\n" FC_GRAY "left" FC_RED " = rotate left, " FC_GRAY "right" FC_RED
                    " = rotate right\n" FC_GRAY "ctrl" FC_RED " = fire, " FC_GRAY "p" FC_RED " = pain, " FC_GRAY
                    "d" FC_RED " = die, " FC_GRAY "x" FC_RED " = gib\n" FC_GRAY "space" FC_RED " = respawn, " FC_GRAY
                    "h" FC_RED " = half-speed\n" FC_GRAY "toggle or previous" FC_RED " = exit",
                    4, INSTR_Y, &subscreen43);
}

//
// MN_SkinDrawer
//
// The skin viewer widget drawer function. Puts the player sprite on
// screen with the currently active properties (frame, rotation, and
// lighting).
//
static void MN_SkinDrawer()
{
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    int            lump, rot = 0;
    bool           flip;
    patch_t       *patch;
    int            pctype;
    int            lighttouse;
    byte          *translate = nullptr;

    // draw the normal menu background
    V_DrawBackground(mn_background_flat, &vbscreen);

    // draw instructions and title
    MN_SkinInstructions();

    pctype = players[consoleplayer].pclass->type;

    // get the player skin sprite definition
    if(skview_state->sprite == mobjinfo[pctype]->defsprite)
        sprdef = &sprites[players[consoleplayer].skin->sprite];
    else
        sprdef = &sprites[skview_state->sprite];

    if(!(sprdef->spriteframes))
        return;

    // get the current frame, using the skin state and rotation vars
    sprframe = &sprdef->spriteframes[skview_state->frame & FF_FRAMEMASK];
    if(sprframe->rotate)
        rot = skview_rot;
    lump = sprframe->lump[rot];
    flip = !!sprframe->flip[rot];

    lighttouse = (skview_state->frame & FF_FULLBRIGHT ? 0 : skview_light);

    if(players[consoleplayer].colormap)
        translate = translationtables[players[consoleplayer].colormap - 1];

    // cache the sprite patch -- watch out for "firstspritelump"!
    patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);

    // draw the sprite, with color translation and proper flipping
    // 01/12/04: changed translation handling
    V_DrawPatchTranslatedLit(160, 120, &subscreen43, patch, translate, colormaps[0] + 256 * lighttouse, flip);
}

//
// MN_SkinTicker
//
// haleyjd 05/29/06: separated out from the drawer and added ticker
// support to widgets to enable precise state transition timing.
//
static void MN_SkinTicker()
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
    MetaState     *state  = nullptr;

    skview_metadeaths.clear();

    while((state = meta->getNextTypeEx(state)))
    {
        // NB: also matches XDeath implicitly.
        if(M_StrCaseStr(state->getKey(), "Death."))
            skview_metadeaths.add(state);
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
void MN_InitSkinViewer()
{
    playerclass_t *pclass = players[consoleplayer].pclass; // haleyjd 09/29/07

    // reset all state variables
    skview_action    = SKV_WALKING;
    skview_rot       = 0;
    skview_halfspeed = false;
    skview_typenum   = pclass->type;
    skview_light     = 0;
    skview_gibbed    = false;

    // haleyjd 09/29/07: save alternate attack state number
    skview_atkstate2 = pclass->altattack;

    // haleyjd 03/29/08: determine if player skin has wimpy death sound
    skview_haswdth = (strcasecmp(players[consoleplayer].skin->sounds[sk_plwdth], "none") != 0);

    MN_SkinSetState(states[mobjinfo[skview_typenum]->seestate]);

    MN_initMetaDeaths();

    // set the widget
    MN_PushWidget(&skinviewer);
}

// EOF

