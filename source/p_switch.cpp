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
// Purpose: Switches, buttons. Two-state animation. Exits.
// Authors: James Haley, Stephen McGranahan, Ioan Chera, Max Waine
//

#include "z_zone.h"
#include "i_system.h"

#include "d_gi.h"
#include "d_io.h"
#include "doomstat.h"
#include "e_exdata.h"
#include "e_switch.h"
#include "ev_specials.h"
#include "g_game.h"
#include "m_collection.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "p_info.h"
#include "p_skin.h"
#include "p_spec.h"
#include "r_data.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_state.h"
#include "s_sound.h"
#include "sounds.h"
#include "w_wad.h"

// need wad iterators
#include "w_iterator.h"

// killough 2/8/98: Remove switch limit

static int *switchlist;      // killough
static int  max_numswitches; // killough
static int  numswitches;     // killough

static Collection<qstring> switchsounds;
static Collection<qstring> offswitchsounds;

button_t *buttonlist      = nullptr; // haleyjd 04/16/08: made dynamic
int       numbuttonsalloc = 0;       // haleyjd 04/16/08: number allocated

//
// Being given an EDF switch, check if we have SWITCHES entries which can be
// replaced by it, instead of adding something new
//
static bool P_replaceSwitchWithEDF(const ESwitchDef &esd)
{
    int pic = R_FindWall(esd.offpic.constPtr());
    for(int i = 0; i < numswitches; ++i)
    {
        if(switchlist[2 * i] == pic)
        {
            // Got one.
            if(!esd.onpic.empty())
                switchlist[2 * i + 1] = R_FindWall(esd.onpic.constPtr());

            // only replace if set
            if(!esd.onsound.empty())
                switchsounds[i] = esd.onsound;

            if(!esd.offsound.empty())
                offswitchsounds[i] = esd.offsound;
            else if(!esd.onsound.empty() && offswitchsounds[i].empty())
                offswitchsounds[i] = esd.onsound; // only if original had nothing
            return true;
        }
    }
    return false;
}

//
// P_InitSwitchList()
//
// Only called at game initialization in order to list the set of switches
// and buttons known to the engine. This enables their texture to change
// when activated, and in the case of buttons, change back after a timeout.
//
// This routine modified to read its data from a predefined lump or
// PWAD lump called SWITCHES rather than a static table in this module to
// allow wad designers to insert or modify switches.
//
// Lump format is an array of byte packed switchlist_t structures, terminated
// by a structure with episode == -0. The lump can be generated from a
// text source file using SWANTBLS.EXE, distributed with the BOOM utils.
// The standard list of switches and animations is contained in the example
// source text file DEFSWANI.DAT also in the BOOM util distribution.
//
// Rewritten by Lee Killough to remove limit 2/8/98
//
void P_InitSwitchList(void)
{
    int           index = 0;
    int           episode;
    switchlist_t *alphSwitchList; // jff 3/23/98 pointer to switch table
    int           lumpnum = -1;

    episode = GameModeInfo->switchEpisode;

    // jff 3/23/98 read the switch table from a predefined lump

    // haleyjd 08/29/09: run down the hash chain for SWITCHES
    WadChainIterator wci(wGlobalDir, "SWITCHES");
    for(wci.begin(); wci.current(); wci.next())
    {
        // look for a lump which is of a possibly good size
        if(wci.testLump() && (*wci)->size % sizeof(switchlist_t) == 0)
        {
            lumpnum = (*wci)->selfindex;
            break;
        }
    }

    if(lumpnum < 0)
        I_Error("P_InitSwitchList: missing SWITCHES lump\n");

    alphSwitchList = (switchlist_t *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC));

    for(int i = 0;; i++)
    {
        // Check for a deleting EDF definition
        const ESwitchDef *esd = E_SwitchForName(alphSwitchList[i].name1);
        if(esd && (esd->offpic.empty() || esd->emptyDef() || esd->episode > episode))
        {
            // skip it now, because it's too hard to retroactively delete when we
            // scan EDF below
            continue;
        }

        if(index + 1 >= max_numswitches)
        {
            max_numswitches = max_numswitches ? max_numswitches * 2 : 8;
            switchlist      = erealloc(int *, switchlist, sizeof(*switchlist) * max_numswitches);
        }
        if(SwapShort(alphSwitchList[i].episode) <= episode) // jff 5/11/98 endianess
        {
            if(!SwapShort(alphSwitchList[i].episode))
                break;
            switchlist[index++] = R_FindWall(alphSwitchList[i].name1);
            switchlist[index++] = R_FindWall(alphSwitchList[i].name2);
            switchsounds.add(qstring()); // empty means default
            offswitchsounds.add(qstring());
        }
    }

    // update now so we can scan the list while adding EDF
    numswitches = index / 2;

    // Now read the EDF/ANIMDEFS switches.
    for(const ESwitchDef *esd : eswitches)
    {
        if(esd->offpic.empty() || esd->emptyDef() || esd->episode > episode || P_replaceSwitchWithEDF(*esd))
        {
            continue;
        }

        if(index + 1 >= max_numswitches)
        {
            max_numswitches = max_numswitches ? max_numswitches * 2 : 8;
            switchlist      = erealloc(int *, switchlist, sizeof(*switchlist) * max_numswitches);
        }
        switchlist[index++] = R_FindWall(esd->offpic.constPtr());
        switchlist[index++] = R_FindWall(esd->onpic.constPtr());
        switchsounds.add(esd->onsound);
        offswitchsounds.add(esd->offsound.empty() ? esd->onsound : esd->offsound);
    }

    numswitches       = index / 2;
    switchlist[index] = -1;
    Z_ChangeTag(alphSwitchList, PU_CACHE); // jff 3/23/98 allow table to be freed
}

//
// P_FindFreeButton
//
// haleyjd 04/16/08: Made buttons dynamic.
//
static button_t *P_FindFreeButton()
{
    int i;
    int oldnumbuttons;

    // look for a free button
    for(i = 0; i < numbuttonsalloc; ++i)
    {
        if(!buttonlist[i].btimer)
            return &buttonlist[i];
    }

    // if we get here, there are no free buttons. Reallocate.
    oldnumbuttons   = numbuttonsalloc;
    numbuttonsalloc = numbuttonsalloc ? numbuttonsalloc * 2 : MAXPLAYERS * 4;
    buttonlist      = erealloc(button_t *, buttonlist, numbuttonsalloc * sizeof(button_t));

    // 05/04/08: be sure all the new buttons are initialized
    for(i = oldnumbuttons; i < numbuttonsalloc; ++i)
        memset(&buttonlist[i], 0, sizeof(button_t));

    // return the first new button
    return &buttonlist[oldnumbuttons];
}

//
// P_StartButton
//
// Start a button (retriggerable switch) counting down till it turns off.
//
// Passed the linedef the button is on, which texture on the sidedef contains
// the button, the texture number of the button, and the time the button is
// to remain active in gametics.
// No return value.
//
// haleyjd 04/16/08: rewritten to store indices instead of pointers
//
static void P_StartButton(int sidenum, line_t *line, sector_t *sector, bwhere_e w, int texture, int time, bool dopopout,
                          const char *startsound, int swindex)
{
    int       i;
    button_t *button;

    // See if button is already pressed
    for(i = 0; i < numbuttonsalloc; ++i)
    {
        if(buttonlist[i].btimer && buttonlist[i].side == sidenum)
            return;
    }

    button = P_FindFreeButton();

    button->line        = eindex(line - lines);
    button->side        = sidenum;
    button->where       = w;
    button->btexture    = texture;
    button->btimer      = time;
    button->dopopout    = dopopout;
    button->switchindex = swindex;

    // 04/19/09: rewritten to use linedef sound origin

    // switch activation sound
    S_StartSoundName(&line->soundorg, startsound);

    // haleyjd 04/16/08: and thus dies one of the last static limits.
    // I_Error("P_StartButton: no button slots left!");
}

//
// P_ClearButtons
//
// haleyjd 10/16/05: isolated from P_SpawnSpecials
//
void P_ClearButtons()
{
    if(numbuttonsalloc)
        memset(buttonlist, 0, numbuttonsalloc * sizeof(button_t));
}

//
// P_RunButtons
//
// Check buttons and change texture on timeout.
// haleyjd 10/16/05: moved here and turned into its own function.
// haleyjd 04/16/08: rewritten to use indices instead of pointers
//
void P_RunButtons()
{
    int       i;
    button_t *button;

    for(i = 0; i < numbuttonsalloc; ++i)
    {
        button = &buttonlist[i];

        if(button->btimer)
        {
            button->btimer--;
            if(!button->btimer)
            {
                // haleyjd 04/16/08: these are even used for one-use switches now,
                // so that every switch press can have its own sound origin. Only
                // change texture back and make pop-out sound if this is a reusable
                // switch.
                if(button->dopopout)
                {
                    line_t *line = &lines[button->line]; // 04/19/09: line soundorgs

                    switch(button->where)
                    {
                    case top:    sides[button->side].toptexture = button->btexture; break;
                    case middle: sides[button->side].midtexture = button->btexture; break;
                    case bottom: sides[button->side].bottomtexture = button->btexture; break;
                    }

                    int         idx   = button->switchindex;
                    const char *sound = (idx % 2 ? switchsounds : offswitchsounds)[idx / 2].constPtr();
                    if(!*sound)
                        sound = "EE_SwitchOn";
                    S_StartSoundName(&line->soundorg, sound);
                }

                // clear out the button
                memset(button, 0, sizeof(button_t));
            }
        }
    }
}

//
// P_ChangeSwitchTexture()
//
// Function that changes switch wall texture on activation.
//
// Passed the line which the switch is on, whether it's retriggerable,
// and the side it was activated on (haleyjd 03/13/05).
// If not retriggerable, this function clears the line special to insure that
//
// No return value
//
// haleyjd 04/16/08: rewritten for button_t changes.
//
void P_ChangeSwitchTexture(line_t *line, int useAgain, int side)
{
    int         texTop;
    int         texMid;
    int         texBot;
    int         i;
    const char *defsound; // haleyjd
    const char *sound;
    int         sidenum;
    sector_t   *sector;

    if(!useAgain)
        line->special = 0;

    sidenum = line->sidenum[side];

    // haleyjd: cannot start a button on a non-existent side
    if(sidenum == -1)
        return;

    // haleyjd 04/16/08: get proper sector pointer
    sector = side ? line->backsector : line->frontsector;

    if(!sector) // ???; really should not happen.
        return;

    texTop = sides[sidenum].toptexture;
    texMid = sides[sidenum].midtexture;
    texBot = sides[sidenum].bottomtexture;

    defsound = "EE_SwitchOn"; // haleyjd

    // EXIT SWITCH?
    if(EV_CompositeActionFlags(EV_ActionForSpecial(line->special)) & EV_ISMAPPEDEXIT)
    {
        defsound = "EE_SwitchEx"; // haleyjd
    }

    for(i = 0; i < numswitches * 2; ++i)
    {
        // haleyjd 07/02/09: we do not match "-" as a switch texture
        if(switchlist[i] == 0)
            continue;

        // Check if the switch has a dedicated sound ("none" can silence it)
        sound = (i % 2 ? offswitchsounds : switchsounds)[i / 2].constPtr();
        if(!*sound)
            sound = defsound;

        if(switchlist[i] == texTop) // if an upper texture
        {
            sides[sidenum].toptexture = switchlist[i ^ 1]; // chg texture
            R_CacheTexture(switchlist[i ^ 1]);
            R_CacheIfSkyTexture(switchlist[i], switchlist[i ^ 1]); // Sky transfers are only off of top textures

            P_StartButton(sidenum, line, sector, top, switchlist[i], BUTTONTIME, !!useAgain, sound, i); // start timer

            return;
        }
        else if(switchlist[i] == texMid) // if a normal texture
        {
            sides[sidenum].midtexture = switchlist[i ^ 1]; // chg texture
            R_CacheTexture(switchlist[i ^ 1]);

            P_StartButton(sidenum, line, sector, middle, switchlist[i], BUTTONTIME, !!useAgain, sound,
                          i); // start timer

            return;
        }
        else if(switchlist[i] == texBot) // if a lower texture
        {
            sides[sidenum].bottomtexture = switchlist[i ^ 1]; // chg texture
            R_CacheTexture(switchlist[i ^ 1]);

            P_StartButton(sidenum, line, sector, bottom, switchlist[i], BUTTONTIME, !!useAgain, sound,
                          i); // start timer

            return;
        }
    }
}

//
// P_UseSpecialLine
//
// Called when a thing uses (pushes) a special line.
// Only the front sides of lines are usable.
// Dispatches to the appropriate linedef function handler.
//
// Passed the thing using the line, the line being used, and the side used
// Returns true if a thinker was created
//
bool P_UseSpecialLine(Mobj *thing, line_t *line, int side)
{
    return EV_ActivateSpecialLineWithSpac(line, side, thing, nullptr, SPAC_USE, false);
}

//----------------------------------------------------------------------------
//
// $Log: p_switch.c,v $
// Revision 1.25  1998/06/01  14:48:19  jim
// Fix switch use from back side
//
// Revision 1.24  1998/05/11  14:04:46  jim
// Fix endianess of speed field in SWITCH predefined lump
//
// Revision 1.23  1998/05/07  21:30:40  jim
// formatted/documented p_switch
//
// Revision 1.22  1998/05/03  23:17:44  killough
// Fix #includes at the top, nothing else
//
// Revision 1.21  1998/03/24  22:10:32  jim
// Fixed switch won't light problem in UDOOM
//
// Revision 1.20  1998/03/23  18:39:45  jim
// Switch and animation tables now lumps
//
// Revision 1.19  1998/03/16  15:47:18  killough
// Merge Jim's linedef changes
//
// Revision 1.18  1998/03/15  14:39:58  jim
// added pure texture change linedefs & generalized sector types
//
// Revision 1.17  1998/03/14  17:19:15  jim
// Added instant toggle floor type
//
// Revision 1.16  1998/03/12  21:54:23  jim
// Freed up 12 linedefs for use as vectors
//
// Revision 1.15  1998/03/05  16:59:22  jim
// Fixed inability of monsters/barrels to use new teleports
//
// Revision 1.14  1998/03/02  15:33:10  jim
// fixed errors in numeric model sector search and 0 tag trigger defeats
//
// Revision 1.13  1998/02/28  01:25:06  jim
// Fixed error in 0 tag trigger fix
//
// Revision 1.11  1998/02/23  00:42:17  jim
// Implemented elevators
//
// Revision 1.10  1998/02/13  03:28:49  jim
// Fixed W1,G1 linedefs clearing untriggered special, cosmetic changes
//
// Revision 1.9  1998/02/09  03:11:03  killough
// Remove limit on switches
//
// Revision 1.8  1998/02/08  05:35:54  jim
// Added generalized linedef types
//
// Revision 1.6  1998/02/02  13:39:27  killough
// Program beautification
//
// Revision 1.5  1998/01/30  14:44:20  jim
// Added gun exits, right scrolling walls and ceiling mover specials
//
// Revision 1.2  1998/01/26  19:24:28  phares
// First rev with no ^Ms
//
// Revision 1.1.1.1  1998/01/19  14:03:01  rand
// Lee's Jan 19 sources
//
//
//----------------------------------------------------------------------------

