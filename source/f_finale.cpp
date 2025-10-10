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
//------------------------------------------------------------------------------
//
// Purpose: Game completion, final screen animation.
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine, FozzeY
//

#include "z_zone.h"
#include "i_system.h"
#include "i_video.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_deh.h" // Ty 03/22/98 - externalizations
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"
#include "d_main.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_fonts.h"
#include "e_player.h"
#include "e_states.h"
#include "f_finale.h"
#include "m_swap.h"
#include "mn_engin.h"
#include "p_info.h"
#include "p_skin.h"
#include "r_draw.h"
#include "r_patch.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_png.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_auto.h"

// Stage of animation:
//  0 = text, 1 = art screen, 2 = character cast, 3 = Heretic underwater scene
static int finalestage;
static int finalecount;

// defines for the end mission display text                     // phares

#define TEXTSPEED    3     // original value                    // phares
#define TEXTWAIT     250   // original value                    // phares
#define NEWTEXTSPEED 0.01  // new value                         // phares
#define NEWTEXTWAIT  1000  // new value                         // phares

static void F_StartCast();
static void F_CastTicker();
static bool F_CastResponder(const event_t *ev);
static void F_CastDrawer();

void       IN_checkForAccelerate(); // killough 3/28/98: used to
extern int acceleratestage;         // accelerate intermission screens
static int midstage;                // whether we're in "mid-stage"

static void F_InitDemonScroller();

struct demonscreen_t
{
    byte *buffer; // haleyjd 08/23/02
    int   width;
};

static demonscreen_t DemonScreen;

vfont_t *f_font;
vfont_t *f_titlefont;
char    *f_fontname;
char    *f_titlefontname;

static const char *finaletext;
static int         finaletype;

//
// F_setupFinale
//
// haleyjd 01/26/14: Determine finale parameters to use.
//
static void F_setupFinale(bool secret)
{
    if(secret)
    {
        if(LevelInfo.finaleSecretType != FINALE_UNSPECIFIED)
            finaletype = LevelInfo.finaleSecretType;
        else
            finaletype = LevelInfo.finaleType;

        if(LevelInfo.interTextSecret)
            finaletext = LevelInfo.interTextSecret;
        else
            finaletext = LevelInfo.interText;
    }
    else
    {
        finaletype = LevelInfo.finaleType;
        finaletext = LevelInfo.interText;
    }
}

//
// F_Init
//
// haleyjd 02/25/09: Added to resolve finale font.
//
void F_Init()
{
    if(!(f_font = E_FontForName(f_fontname)))
        I_Error("F_Init: bad EDF font name %s\n", f_fontname);

    // title font is optional
    f_titlefont = E_FontForName(f_titlefontname);
}

//
// F_StartFinale
//
void F_StartFinale(bool secret)
{
    gameaction    = ga_nothing;
    gamestate     = GS_FINALE;
    automapactive = false;

    // killough 3/28/98: clear accelerative text flags
    acceleratestage = midstage = 0;

    // haleyjd 07/17/04: level-dependent initialization moved to MapInfo

    S_ChangeMusicName(LevelInfo.interMusic, true);

    finalestage = 0;
    finalecount = 0;

    F_setupFinale(secret);
}

//
// F_Responder
//
bool F_Responder(const event_t *event)
{
    if(finalestage == 2)
        return F_CastResponder(event);

    // haleyjd: Heretic underwater hack for E2 end
    if(finalestage == 3 && event->type == ev_keydown)
    {
        // restore normal palette and kick out to title screen
        finalestage = 4;
        I_SetPalette((byte *)(wGlobalDir.cacheLumpName("PLAYPAL", PU_CACHE)));
        return true;
    }

    return false;
}

//
// Get_TextSpeed()
//
// Returns the value of the text display speed  // phares
// Rewritten to allow user-directed acceleration -- killough 3/28/98
//
static float Get_TextSpeed()
{
    return (float)(midstage ? NEWTEXTSPEED : (midstage = acceleratestage) ? acceleratestage = 0,
                   NEWTEXTSPEED : TEXTSPEED);
}

//
// F_Ticker
//
// killough 3/28/98: almost totally rewritten, to use
// player-directed acceleration instead of constant delays.
// Now the player can accelerate the text display by using
// the fire/use keys while it is being printed. The delay
// automatically responds to the user, and gives enough
// time to read.
//
// killough 5/10/98: add back v1.9 demo compatibility
// haleyjd 10/12/01: reformatted, added cast call for any level
//
void F_Ticker()
{
    int i;

    if(!demo_compatibility)
    {
        // killough 3/28/98: check for acceleration
        IN_checkForAccelerate();
    }
    else if(finaletype == FINALE_TEXT && finalecount > 50)
    {
        // haleyjd 05/26/06: do this for all FINALE_TEXT finales
        // check for skipping
        for(i = 0; i < MAXPLAYERS; i++)
        {
            if(players[i].cmd.buttons)
            {
                // go on to the next level
                if(LevelInfo.endOfGame)
                    F_StartCast(); // cast of Doom 2 characters
                else
                    gameaction = ga_worlddone; // next level, e.g. MAP07
                return;
            }
        }
    }

    // advance animation
    finalecount++;

    if(finalestage == 2)
        F_CastTicker();

    if(!finalestage)
    {
        float speed = demo_compatibility ? TEXTSPEED : Get_TextSpeed();

        if(finalecount > strlen(finaletext) * speed +               // phares
                             (midstage ? NEWTEXTWAIT : TEXTWAIT) || // killough 2/28/98:
           (midstage && acceleratestage))                           // changed to allow acceleration
        {
            // Doom 1 / Ultimate Doom episode end: with enough time, it's automatic
            // haleyjd 05/26/06: all finales except just text use this
            if(finaletype != FINALE_TEXT)
            {
                finalecount = 0;
                finalestage = 1;

                // no wipe before Heretic E2 or E3 finales
                if(finaletype != FINALE_HTIC_WATER && finaletype != FINALE_HTIC_DEMON)
                    wipegamestate = GS_NOSTATE; // force a wipe

                // special actions
                switch(finaletype)
                {
                case FINALE_DOOM_BUNNY: // bunny scroller
                    S_StartMusic(mus_bunny);
                    break;
                case FINALE_HTIC_DEMON: // demon scroller
                    F_InitDemonScroller();
                    break;
                default: //
                    break;
                }
            }
            else if(!demo_compatibility && midstage)
            {
                // you must press a button to continue in Doom 2
                // haleyjd: allow cast calls after arbitrary maps
                if(LevelInfo.endOfGame)
                    F_StartCast(); // cast of Doom 2 characters
                else if(finaletype == FINALE_PSX_UDOOM)
                    D_StartTitle(); // TODO: really should go back to "menu" state
                else
                    gameaction = ga_worlddone; // next level, e.g. MAP07
            }
        }
    }
}

//
// F_TextWrite
//
// This program displays the background and text at end-mission     // phares
// text time. It draws both repeatedly so that other displays,      //   |
// like the main menu, can be drawn over it dynamically and         //   V
// erased dynamically. The TEXTSPEED constant is changed into
// the Get_TextSpeed function so that the speed of writing the      //   ^
// text can be increased, and there's still time to read what's     //   |
// written.                                                         // phares
//
static void F_TextWrite()
{
    int     count;
    size_t  len;
    int     cx;
    int     cy;
    int     lumpnum;
    qstring text;

    // erase the entire screen to a tiled background
    // killough 11/98: the background-filling code was already in m_menu.c
    lumpnum = wGlobalDir.checkNumForNameNSG(LevelInfo.backDrop, lumpinfo_t::ns_flats);
    if(lumpnum > 0)
        V_DrawFSBackground(&vbscreenyscaled, lumpnum);

    // draw some of the text onto the screen
    cx = GameModeInfo->fTextPos->x;
    cy = GameModeInfo->fTextPos->y;

    text  = finaletext;
    count = (int)((finalecount - 10) / Get_TextSpeed()); // phares
    if(count < 0)
        count = 0;
    len = (size_t)count;

    if(len < text.length())
        text.truncate(len);

    V_FontWriteText(f_font, text.constPtr(), cx, cy, &subscreen43);
}

//
// Final DOOM 2 animation
// Casting by id Software.
//   in order of appearance
//

// haleyjd 07/05/03: modified to be dynamic through EDF
int         max_castorder;
castinfo_t *castorder; // Ty 03/22/98 - externalized and init moved into f_startcast()

int      castnum;
int      casttics;
state_t *caststate;
int      castrot;
bool     castdeath;
int      castframes;
int      castonmelee;
bool     castattacking;

extern gamestate_t wipegamestate;

// haleyjd 07/05/03: support old DEH names for the first
// 17 cast members, for compatibility purposes.
#define OLDCASTMAX 17
static const char *oldnames[OLDCASTMAX] = {
    "CC_ZOMBIE",  //
    "CC_SHOTGUN", //
    "CC_HEAVY",   //
    "CC_IMP",     //
    "CC_DEMON",   //
    "CC_LOST",    //
    "CC_CACO",    //
    "CC_HELL",    //
    "CC_BARON",   //
    "CC_ARACH",   //
    "CC_PAIN",    //
    "CC_REVEN",   //
    "CC_MANCU",   //
    "CC_ARCH",    //
    "CC_SPIDER",  //
    "CC_CYBER",   //
    "CC_HERO",    //
};

//
// F_StartCast
//
// haleyjd 07/05/03: rewritten for EDF support
//
static void F_StartCast()
{
    int i;

    // types are now set through EDF

    // haleyjd 04/17/09: check against max_castorder; don't trash memory.

    // if a cast name was left nullptr by EDF, it means we're going to
    // use the old DeHackEd names
    for(i = 0; i < OLDCASTMAX && i < max_castorder; i++)
    {
        if(!castorder[i].name)
            castorder[i].name = estrdup(DEH_String(oldnames[i]));
    }

    wipegamestate = GS_NOSTATE; // force a screen wipe
    castnum       = 0;
    caststate     = states[mobjinfo[castorder[castnum].type]->seestate];
    casttics      = caststate->tics;
    castrot       = 0;
    castdeath     = false;
    finalestage   = 2;
    castframes    = 0;
    castonmelee   = 0;
    castattacking = false;
    S_ChangeMusicNum(mus_evil, true);
}

//
// F_CastTicker
//
static void F_CastTicker()
{
    int st;
    int sfx;

    if(--casttics > 0)
        return; // not time to change state yet

    if(caststate->tics == -1 || caststate->nextstate == NullStateNum)
    {
        // switch from deathstate to next monster
        castnum++;
        castdeath = false;
        if(castorder[castnum].name == nullptr)
            castnum = 0;
        S_StartInterfaceSound(mobjinfo[castorder[castnum].type]->seesound);
        caststate  = states[mobjinfo[castorder[castnum].type]->seestate];
        castframes = 0;
    }
    else
    {
        // just advance to next state in animation

        // haleyjd: modified to use a field set through EDF
        // if(caststate == states[S_PLAY_ATK1])
        //   goto stopattack;    // Oh, gross hack!

        int i;
        int statenum = mobjinfo[castorder[castnum].type]->missilestate;

        if(caststate == states[statenum] && castorder[castnum].stopattack)
            goto stopattack; // not quite as hackish as it used to be

        st        = caststate->nextstate;
        caststate = states[st];
        castframes++;

        // haleyjd: new sound event method -- each actor type
        // can define up to four sound events.

        // Search for a sound matching this state.
        sfx = 0;
        for(i = 0; i < 4; ++i)
        {
            if(st == castorder[castnum].sounds[i].frame)
                sfx = castorder[castnum].sounds[i].sound;
        }

        S_StartInterfaceSound(sfx);
    }

    if(castframes == 12)
    {
        int stnum;

        // go into attack frame
        castattacking = true;
        if(castonmelee)
            caststate = states[(stnum = mobjinfo[castorder[castnum].type]->meleestate)];
        else
            caststate = states[(stnum = mobjinfo[castorder[castnum].type]->missilestate)];

        castonmelee ^= 1;

        if(caststate == states[NullStateNum])
        {
            if(castonmelee)
                caststate = states[(stnum = mobjinfo[castorder[castnum].type]->meleestate)];
            else
                caststate = states[(stnum = mobjinfo[castorder[castnum].type]->missilestate)];
        }

        // haleyjd 07/04/04: check for sounds matching the missile or
        // melee state
        if(!castorder[castnum].stopattack)
        {
            sfx = 0;
            for(int i = 0; i < 4; ++i)
            {
                if(stnum == castorder[castnum].sounds[i].frame)
                    sfx = castorder[castnum].sounds[i].sound;
            }

            S_StartInterfaceSound(sfx);
        }
    }

    if(castattacking)
    {
        if(castframes == 24 || caststate == states[mobjinfo[castorder[castnum].type]->seestate])
        {
        stopattack:
            castattacking = false;
            castframes    = 0;
            caststate     = states[mobjinfo[castorder[castnum].type]->seestate];
        }
    }

    casttics = caststate->tics;
    if(casttics == -1)
        casttics = 15;
}

//
// F_CastResponder
//
static bool F_CastResponder(const event_t *ev)
{
    if(ev->type != ev_keydown)
        return false;

    if(castdeath)
        return true; // already in dying frames

    if(ev->data1 == KEYD_LEFTARROW)
    {
        if(castrot == 7)
            castrot = 0;
        else
            ++castrot;
        return true;
    }

    if(ev->data1 == KEYD_RIGHTARROW)
    {
        if(castrot == 0)
            castrot = 7;
        else
            --castrot;
        return true;
    }

    // go into death frame
    castdeath     = true;
    caststate     = states[mobjinfo[castorder[castnum].type]->deathstate];
    casttics      = caststate->tics;
    castrot       = 0;
    castframes    = 0;
    castattacking = false;
    if(mobjinfo[castorder[castnum].type]->deathsound)
    {
        if(castorder[castnum].type == players[consoleplayer].pclass->type)
            S_StartInterfaceSound(players[consoleplayer].skin->sounds[sk_pldeth]);
        else
            S_StartInterfaceSound(mobjinfo[castorder[castnum].type]->deathsound);
    }

    return true;
}

//
// F_CastTitle
//
// haleyjd 07/28/14: If the CC_TITLE BEX string is non-empty and a title
// font is defined by EDF, we'll draw a title at the top of the cast call.
// Useful for PSX and DOOM 64 style behavior.
//
static void F_CastTitle()
{
    const char *str = DEH_String("CC_TITLE");

    if(!f_titlefont || estrempty(str))
        return;

    V_FontWriteText(f_titlefont, str, 160 - V_FontStringWidth(f_titlefont, str) / 2, GameModeInfo->castTitleY,
                    &subscreen43);
}

//
// F_CastPrint
//
// haleyjd 03/17/05: Writes the cast member name centered at the
// bottom of the screen. Rewritten to use the proper text-drawing
// methods instead of duplicating that code unnecessarily. It's
// about 200 lines shorter now.
//
static void F_CastPrint(const char *text)
{
    V_FontWriteText(f_font, text, 160 - V_FontStringWidth(f_font, text) / 2, GameModeInfo->castNameY, &subscreen43);
}

//
// F_CastDrawer
//
static void F_CastDrawer()
{
    spritedef_t   *sprdef;
    spriteframe_t *sprframe;
    int            lump, rot = 0;
    bool           flip;
    patch_t       *patch;
    byte          *translate = nullptr;
    castinfo_t    *cast      = &castorder[castnum];
    mobjinfo_t    *mi        = mobjinfo[cast->type];
    player_t      *cplayer   = &players[consoleplayer];

    // erase the entire screen to a background
    // Ty 03/30/98 bg texture extern
    V_DrawFSBackground(&vbscreenyscaled, wGlobalDir.checkNumForName(DEH_String("BGCASTCALL")));

    F_CastTitle();

    if(cast->name)
        F_CastPrint(cast->name);

    // draw the current frame in the middle of the screen
    sprdef = sprites + caststate->sprite;

    // override for alternate monster sprite?
    if(mi->altsprite != -1)
        sprdef = &sprites[mi->altsprite];

    // override for player skin?
    if(cast->type == cplayer->pclass->type)
    {
        int colormap = cplayer->colormap;

        sprdef    = &sprites[cplayer->skin->sprite];
        translate = colormap ? translationtables[colormap - 1] : nullptr;
    }

    // haleyjd 08/15/02
    if(!(sprdef->spriteframes))
        return;

    sprframe = &sprdef->spriteframes[caststate->frame & FF_FRAMEMASK];
    if(sprframe->rotate)
        rot = castrot;
    lump = sprframe->lump[rot];
    flip = !!sprframe->flip[rot];

    patch = PatchLoader::CacheNum(wGlobalDir, lump + firstspritelump, PU_CACHE);

    V_DrawPatchTranslated(160, 170, &subscreen43, patch, translate, flip);
}

//
// F_BunnyScroll
//
static void F_BunnyScroll()
{
    int        scrolled;
    patch_t   *p1;
    patch_t   *p2;
    char       name[10];
    int        stage;
    static int laststage;

    p1 = PatchLoader::CacheName(wGlobalDir, "PFUB2", PU_LEVEL);
    p2 = PatchLoader::CacheName(wGlobalDir, "PFUB1", PU_LEVEL);

    scrolled = 320 - (finalecount - 230) / 2;
    if(scrolled > 320)
        scrolled = 320;
    if(scrolled < 0)
        scrolled = 0;

    int ws_offset1, ws_offset2;

    // IOANCH: changed to fit variable widescreen assets
    int  coverage         = vbscreenyscaled.unscaledw + 320;
    int  totalwidth       = p1->width + p2->width;
    bool widescreenAssets = totalwidth > 640;
    ws_offset1            = (coverage - totalwidth) / 2;
    ws_offset2            = ws_offset1 + p1->width - 320;

    int drawx = 320 - scrolled + ws_offset2;
    if(drawx < vbscreenyscaled.unscaledw)
        V_DrawPatchGeneral(drawx, 0, &vbscreenyscaled, p2, false);
    drawx = -scrolled + ws_offset1;
    if(drawx + p1->width > 0)
        V_DrawPatchGeneral(drawx, 0, &vbscreenyscaled, p1, false);
    if(!widescreenAssets)
        D_DrawWings(); // keep visual compatibility with classic wads
    if(finalecount < 1130)
        return;
    if(finalecount < 1180)
    {
        V_DrawPatch((SCREENWIDTH - 13 * 8) / 2, (SCREENHEIGHT - 8 * 8) / 2, &subscreen43,
                    PatchLoader::CacheName(wGlobalDir, "END0", PU_CACHE));
        laststage = 0;
        return;
    }

    stage = (finalecount - 1180) / 5;
    if(stage > 6)
        stage = 6;
    if(stage > laststage)
    {
        S_StartInterfaceSound(sfx_pistol);
        laststage = stage;
    }

    snprintf(name, sizeof(name), "END%i", stage);
    V_DrawPatch((SCREENWIDTH - 13 * 8) / 2, (SCREENHEIGHT - 8 * 8) / 2, &subscreen43,
                PatchLoader::CacheName(wGlobalDir, name, PU_CACHE));
}

// haleyjd: heretic e2 ending -- sort of hackish
static void F_DrawUnderwater()
{
    int initialstage = finalestage;
    switch(finalestage)
    {
    case 1:
        C_InstaPopup(); // put away console if down

        {
            byte *palette;

            int e2end = wGlobalDir.checkNumForName("E2END");
            VPNGImage png;
            bool havePNGPal = false;
            if(e2end >= 0)
            {
                auto e2endData = static_cast<byte *>(wGlobalDir.cacheLumpNum(e2end, PU_CACHE));
                if(png.readImage(e2endData))
                {
                    palette = png.expandPalette();
                    I_SetPalette(palette);
                    havePNGPal = true;
                }
            }

            if(!havePNGPal)
            {
                palette = (byte *)wGlobalDir.cacheLumpName("E2PAL", PU_CACHE);
                I_SetPalette(palette);
            }

            V_DrawFSBackground(&vbscreenyscaled, e2end);

            finalestage = 3;
        }
        [[fallthrough]];
    case 3:
        Console.enabled = false; // let console key fall through
        paused          = 0;
        menuactive      = false;

        // Redraw to cover possible pillarbox caused by D_Display
        if(initialstage == 3)
            V_DrawFSBackground(&vbscreenyscaled, wGlobalDir.checkNumForName("E2END"));
        break;

    case 4:
        Console.enabled = true;
        V_DrawFSBackground(&vbscreenyscaled, wGlobalDir.checkNumForName("TITLE"));
        break;
    }
}

//
// Obtained from https://stackoverflow.com/a/9320349
// Thanks to Christian Ammer
//
static void F_inPlaceTranspose(byte *buffer, int width, int height)
{
    // Assume graphics was drawn transposed, so restore it
    int mn1 = width * height - 1;
    //   int m = width;
    int                 n = height;
    PODCollection<bool> visisted;
    visisted.resize(width * height);

    byte *first = buffer;
    byte *cycle = buffer;
    byte *last  = buffer + width * height;
    while(++cycle != last)
    {
        if(visisted[cycle - first])
            continue;
        int a = (int)(cycle - first);
        do
        {
            a            = a == mn1 ? mn1 : (n * a) % mn1;
            byte aux     = *(first + a);
            *(first + a) = *cycle;
            *cycle       = aux;
            visisted[a]  = true;
        }
        while((first + a) != cycle);
    }
}

//
// F_InitDemonScroller
//
// Sets up the Heretic episode 3 ending sequence.
//
static void F_InitDemonScroller()
{
    int     lnum1, lnum2;
    int     lsize1, lsize2;
    VBuffer vbuf;

    // get screens
    lnum1  = W_GetNumForName("FINAL1");
    lnum2  = W_GetNumForName("FINAL2");
    lsize1 = W_LumpLength(lnum1);
    lsize2 = W_LumpLength(lnum2);
    bool     raw1, raw2;
    patch_t *patch1 = nullptr, *patch2 = nullptr;

    int maxwidth    = 0;
    int totalheight = 0;
    int height2;

    raw1 = lsize1 == SCREENWIDTH * SCREENHEIGHT;
    if(raw1)
    {
        maxwidth     = SCREENWIDTH;
        totalheight += SCREENHEIGHT;
    }
    else
    {
        patch1       = PatchLoader::CacheNum(wGlobalDir, lnum1, PU_CACHE);
        maxwidth     = patch1->width;
        totalheight += patch1->height;
    }
    raw2 = lsize2 == SCREENWIDTH * SCREENHEIGHT;
    if(raw2)
    {
        if(maxwidth < SCREENWIDTH)
            maxwidth = SCREENWIDTH;
        totalheight += SCREENHEIGHT;
        height2      = SCREENHEIGHT;
    }
    else
    {
        patch2 = PatchLoader::CacheNum(wGlobalDir, lnum2, PU_CACHE);
        if(patch2->width > maxwidth)
            maxwidth = patch2->width;
        height2      = patch2->height;
        totalheight += height2;
    }

    DemonScreen.buffer =
        ecalloctag(byte *, maxwidth, totalheight, PU_LEVEL, reinterpret_cast<void **>(&DemonScreen.buffer));
    DemonScreen.width = maxwidth;

    // init VBuffer
    V_InitVBufferFrom(&vbuf, maxwidth, totalheight, totalheight, video.bitdepth, DemonScreen.buffer);

    if(raw2) // raw screen
    {
        ZAutoBuffer autobuf(SCREENWIDTH * SCREENHEIGHT, false);
        wGlobalDir.cacheLumpAuto(lnum2, autobuf);
        V_DrawBlock((maxwidth - SCREENWIDTH) / 2, 0, &vbuf, SCREENWIDTH, SCREENHEIGHT,
                    static_cast<const byte *>(autobuf.get()));
    }
    else
        V_DrawPatchGeneral((maxwidth - patch2->width) / 2, 0, &vbuf, patch2, false);

    if(raw1) // raw screen
    {
        ZAutoBuffer autobuf(SCREENWIDTH * SCREENHEIGHT, false);
        wGlobalDir.cacheLumpAuto(lnum1, autobuf);
        V_DrawBlock((maxwidth - SCREENWIDTH) / 2, height2, &vbuf, SCREENWIDTH, SCREENHEIGHT,
                    static_cast<const byte *>(autobuf.get()));
    }
    else
        V_DrawPatchGeneral((maxwidth - patch1->width) / 2, height2, &vbuf, patch1, false);

    F_inPlaceTranspose(DemonScreen.buffer, totalheight, maxwidth);

    V_FreeVBuffer(&vbuf);
}

//
// F_DemonScroll
//
// haleyjd: Heretic episode 3 demon scroller
//
static void F_DemonScroll()
{
    static int yval       = 0;
    static int nextscroll = 0;

    // show first screen for a while
    int origin = (vbscreenyscaled.unscaledw - DemonScreen.width) / 2;
    if(finalecount < 70)
    {
        V_DrawBlock(origin, 0, &vbscreenyscaled, DemonScreen.width, SCREENHEIGHT,
                    DemonScreen.buffer + DemonScreen.width * SCREENHEIGHT);
        nextscroll = finalecount;
        yval       = 0;
        return;
    }

    if(yval < DemonScreen.width * SCREENHEIGHT)
    {
        // scroll up one line at a time until only the top screen
        // shows
        V_DrawBlock(origin, 0, &vbscreenyscaled, DemonScreen.width, SCREENHEIGHT,
                    DemonScreen.buffer + DemonScreen.width * SCREENHEIGHT - yval);

        if(finalecount >= nextscroll)
        {
            yval       += DemonScreen.width; // move up one line
            nextscroll  = finalecount + 3;   // don't scroll too fast
        }
    }
    else
    {
        // finished scrolling
        V_DrawBlock(origin, 0, &vbscreenyscaled, DemonScreen.width, SCREENHEIGHT, DemonScreen.buffer);
    }
}

//
// F_FinaleEndDrawer
//
// haleyjd 05/26/06: new combined routine which determines what final screen
// to show based on LevelInfo.finaleType.
//
static void F_FinaleEndDrawer()
{
    // haleyjd 05/18/09: handle shareware once up here
    bool sw = ((GameModeInfo->flags & GIF_SHAREWARE) == GIF_SHAREWARE);

    switch(finaletype)
    {
    case FINALE_DOOM_CREDITS: //
        V_DrawPatchFS(&vbscreenyscaled, PatchLoader::CacheName(wGlobalDir, sw ? "HELP2" : "CREDIT", PU_CACHE));
        break;
    case FINALE_DOOM_DEIMOS: //
        V_DrawPatchFS(&vbscreenyscaled, PatchLoader::CacheName(wGlobalDir, "VICTORY2", PU_CACHE));
        break;
    case FINALE_DOOM_BUNNY: //
        F_BunnyScroll();
        break;
    case FINALE_DOOM_MARINE: //
        V_DrawPatchFS(&vbscreenyscaled, PatchLoader::CacheName(wGlobalDir, "ENDPIC", PU_CACHE));
        break;
    case FINALE_HTIC_CREDITS: //
        V_DrawFSBackground(&vbscreenyscaled, wGlobalDir.checkNumForName(sw ? "ORDER" : "CREDIT"));
        break;
    case FINALE_HTIC_WATER: //
        F_DrawUnderwater();
        break;
    case FINALE_HTIC_DEMON: //
        F_DemonScroll();
        break;
    case FINALE_END_PIC: //
        V_DrawPatchFS(&vbscreenyscaled, PatchLoader::CacheName(wGlobalDir, LevelInfo.endPic, PU_CACHE));
        break;
    default: // ?
        break;
    }
}

//
// F_Drawer
//
// Main finale drawing routine.
// Either runs a text mode finale, draws the DOOM II cast, or calls
// F_FinaleEndDrawer above.
//
void F_Drawer()
{
    switch(finalestage)
    {
    case 2: //
        F_CastDrawer();
        break;
    case 0: //
        F_TextWrite();
        break;
    default: //
        F_FinaleEndDrawer();
        break;
    }
}

//----------------------------------------------------------------------------
//
// $Log: f_finale.c,v $
// Revision 1.16  1998/05/10  23:39:25  killough
// Restore v1.9 demo sync on text intermission screens
//
// Revision 1.15  1998/05/04  21:34:30  thldrmn
// commenting and reformatting
//
// Revision 1.14  1998/05/03  23:25:05  killough
// Fix #includes at the top, nothing else
//
// Revision 1.13  1998/04/19  01:17:18  killough
// Tidy up last fix's code
//
// Revision 1.12  1998/04/17  15:14:10  killough
// Fix showstopper flat bug
//
// Revision 1.11  1998/03/31  16:19:25  killough
// Fix minor merge glitch
//
// Revision 1.10  1998/03/31  11:41:21  jim
// Fix merge glitch in f_finale.c
//
// Revision 1.9  1998/03/31  00:37:56  jim
// Ty's finale.c fixes
//
// Revision 1.8  1998/03/28  17:51:33  killough
// Allow use/fire to accelerate teletype messages
//
// Revision 1.7  1998/02/05  12:15:06  phares
// cleaned up comments
//
// Revision 1.6  1998/02/02  13:43:30  killough
// Relax endgame message speed to demo_compatibility
//
// Revision 1.5  1998/01/31  01:47:39  phares
// Removed textspeed and textwait externs
//
// Revision 1.4  1998/01/30  18:48:18  phares
// Changed textspeed and textwait to functions
//
// Revision 1.3  1998/01/30  16:08:56  phares
// Faster end-mission text display
//
// Revision 1.2  1998/01/26  19:23:14  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------
