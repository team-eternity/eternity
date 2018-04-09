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
//
// The heart of DOOM itself -- everything is tied together from
// here. Input, demos, netplay, level loading, exit, savegames --
// there's something pertinent to everything.
//
//-----------------------------------------------------------------------------

#include "z_zone.h"
#include "i_system.h"

// Need gamepad and timer HALs
#include "hal/i_gamepads.h"
#include "hal/i_timer.h"

#include "a_small.h"
#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_deh.h"              // Ty 3/27/98 deh declarations
#include "d_event.h"
#include "d_gi.h"
#include "d_io.h"
#include "d_main.h"
#include "d_net.h"
#include "doomstat.h"
#include "dstrings.h"
#include "e_inventory.h"
#include "e_player.h"
#include "e_states.h"
#include "e_things.h"
#include "e_weapons.h"
#include "f_finale.h"
#include "f_wipe.h"
#include "g_bind.h"
#include "g_demolog.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "in_lude.h"
#include "m_argv.h"
#include "m_collection.h"
#include "m_misc.h"
#include "m_random.h"
#include "m_shots.h"
#include "m_utils.h"
#include "metaapi.h"
#include "mn_engin.h"
#include "mn_menus.h"
#include "p_chase.h"
#include "p_hubs.h"
#include "p_info.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_maputl.h"
#include "p_saveg.h"
#include "p_setup.h"
#include "p_tick.h"
#include "p_user.h"
#include "hu_stuff.h"
#include "hu_frags.h" // haleyjd
#include "r_data.h"
#include "r_draw.h"
#include "r_main.h"
#include "r_sky.h"
#include "r_things.h" // haleyjd
#include "s_sndseq.h"
#include "s_sound.h"
#include "sounds.h"
#include "st_stuff.h"
#include "v_misc.h"
#include "version.h"
#include "w_levels.h" // haleyjd
#include "w_wad.h"

// haleyjd: new demo format stuff
static char     eedemosig[] = "ETERN";

//static size_t   savegamesize = SAVEGAMESIZE; // killough
static char    *demoname;
static bool     netdemo;
static byte    *demobuffer;   // made some static -- killough
static size_t   maxdemosize;
static byte    *demo_p;
static int16_t  consistency[MAXPLAYERS][BACKUPTICS];
static int      g_destmap;

WadDirectory *g_dir = &wGlobalDir;

gameaction_t    gameaction;
gamestate_t     gamestate;
skill_t         gameskill;
bool            respawnmonsters;
int             gameepisode;
int             gamemap;
// haleyjd: changed to an array
char            gamemapname[9] = { 0,0,0,0,0,0,0,0,0 }; 
int             paused;
bool            sendpause;     // send a pause event next tic
bool            sendsave;      // send a save event next tic
bool            usergame;      // ok to save / end game
bool            timingdemo;    // if true, exit with report on completion
bool            fastdemo;      // if true, run at full speed -- killough
bool            nodrawers;     // for comparative timing purposes
int             startgametic;
int             starttime;     // for comparative timing purposes
bool            deathmatch;    // only if started as net death
bool            netgame;       // only true if packets are broadcast
bool            playeringame[MAXPLAYERS];
player_t        players[MAXPLAYERS];
int             consoleplayer; // player taking events and displaying
int             displayplayer; // view being displayed
int             gametic;
int             levelstarttic; // gametic at level start
int             basetic;       // killough 9/29/98: for demo sync
int             totalkills, totalitems, totalsecret;    // for intermission
bool            demorecording;
bool            demoplayback;
bool            singledemo;           // quit after playing a demo from cmdline
bool            precache = true;      // if true, load all graphics at start
wbstartstruct_t wminfo;               // parms for world map / intermission
bool            haswolflevels = false;// jff 4/18/98 wolf levels present
byte            *savebuffer;
int             autorun = false;      // always running?          // phares
int             runiswalk = false;    // haleyjd 08/23/09
int             automlook = false;
int             smooth_turning = 0;   // sf
int             novert;               // haleyjd

// sf: moved sensitivity here
double          mouseSensitivity_horiz;   // has default   //  killough
double          mouseSensitivity_vert;    // has default
bool            mouseSensitivity_vanilla; // [CG] 01/20/12
int             invert_mouse = false;
int             invert_padlook = false;
int             animscreenshot = 0;       // animated screenshots
acceltype_e     mouseAccel_type = ACCELTYPE_NONE;
int             mouseAccel_threshold = 10; // [CG] 01/20/12
double          mouseAccel_value = 2.0;    // [CG] 01/20/12

//
// controls (have defaults)
//

// haleyjd: these keys are not dynamically rebindable

int key_escape = KEYD_ESCAPE;
int key_chat;
int key_help = KEYD_F1;
int key_pause;
int destination_keys[MAXPLAYERS];

// haleyjd: mousebfire is now unused -- removed
int mouseb_dblc1;  // double-clicking either of these buttons
int mouseb_dblc2;  // causes a use action, however

// haleyjd: joyb variables are obsolete -- removed

#define MAXPLMOVE      (pc->forwardmove[1])
#define TURBOTHRESHOLD (pc->oforwardmove[1])
#define SLOWTURNTICS   6
#define QUICKREVERSE   32768 // 180 degree reverse                    // phares

int  turnheld;       // for accelerative turning

bool mousearray[4];
bool *mousebuttons = &mousearray[1];    // allow [-1]

// mouse values are used once
double  mousex;
double  mousey;
int     dclicktime;
bool    dclickstate;
int     dclicks;
int     dclicktime2;
bool    dclickstate2;
int     dclicks2;

// joystick values are repeated
double joyaxes[axis_max];

int  savegameslot;
char savedescription[32];

//jff 3/24/98 declare startskill external, define defaultskill here
extern skill_t startskill;      //note 0-based
int defaultskill;               //note 1-based

// killough 2/8/98: make corpse queue variable in size
size_t bodyqueslot; 
int    bodyquesize, default_bodyquesize; // killough 2/8/98, 10/98

void *statcopy;       // for statistics driver

int keylookspeed = 5;

int cooldemo = 0;
int cooldemo_tics;      // number of tics until changing view

void G_CoolViewPoint();

static bool gameactions[NUMKEYACTIONS];

int inventoryTics;
bool usearti = true;

//
// G_BuildTiccmd
//
// Builds a ticcmd from all of the available inputs
// or reads it from the demo buffer.
// If recording a demo, write it out
//
// NETCODE_FIXME: The ticcmd_t structure will probably need to be
// altered to better support command packing.
//
void G_BuildTiccmd(ticcmd_t *cmd)
{
   bool sendcenterview = false;
   int speed;
   int tspeed;
   int forward;
   int side;
   int newweapon;            // phares
   int look = 0; 
   int mlook = 0;
   int flyheight = 0;
   static int prevmlook = 0;
   ticcmd_t *base;
   double tmousex, tmousey;     // local mousex, mousey
   playerclass_t *pc = players[consoleplayer].pclass;
   invbarstate_t &invbarstate = players[consoleplayer].invbarstate;
   player_t &p = players[consoleplayer]; // used to pretty-up code

   base = I_BaseTiccmd();    // empty, or external driver
   memcpy(cmd, base, sizeof(*cmd));

   cmd->consistency = consistency[consoleplayer][maketic%BACKUPTICS];

   if(autorun)
      speed = !(runiswalk && gameactions[ka_speed]);
   else
      speed = gameactions[ka_speed];

   forward = side = 0;

   cmd->itemID = 0; // Nothing to see here
   if(gameactions[ka_inventory_use] && demo_version >= 401)
   {
      // FIXME: Handle noartiskip?
      if(invbarstate.inventory)
      {
         p.inv_ptr = invbarstate.inv_ptr;
         invbarstate.inventory = false;
         usearti = false;
      }
      else if(usearti)
      {
         if(E_PlayerHasVisibleInvItem(&p))
            cmd->itemID = p.inventory[p.inv_ptr].item + 1;
         usearti = false;
      }
      gameactions[ka_inventory_use] = false;
   }

   // use two stage accelerative turning on the keyboard and joystick
   if(gameactions[ka_right] || gameactions[ka_left])
      turnheld += ticdup;
   else
      turnheld = 0;

   if(turnheld < SLOWTURNTICS)
      tspeed = 2;             // slow turn
   else
      tspeed = speed;

   // turn 180 degrees in one keystroke? -- phares
   if(gameactions[ka_flip])
   {
      cmd->angleturn += (int16_t)QUICKREVERSE;
      gameactions[ka_flip] = false;
   }

   // let movement keys cancel each other out
   if(gameactions[ka_strafe])
   {
      if(gameactions[ka_right])
         side += pc->sidemove[speed];
      if(gameactions[ka_left])
         side -= pc->sidemove[speed];
      
      // analog axes: turn becomes stafe if strafe-on is held
      side += (int)(pc->sidemove[speed] * joyaxes[axis_turn]);
   }
   else
   {
      if(gameactions[ka_right])
         cmd->angleturn -= (int16_t)pc->angleturn[tspeed];
      if(gameactions[ka_left])
         cmd->angleturn += (int16_t)pc->angleturn[tspeed];

      cmd->angleturn -= (int16_t)(pc->angleturn[speed] * joyaxes[axis_turn]);
   }

   // gamepad dedicated analog strafe axis applies regardless
   side += (int)(pc->sidemove[speed] * joyaxes[axis_strafe]);

   if(gameactions[ka_forward])
      forward += pc->forwardmove[speed];
   if(gameactions[ka_backward])
      forward -= pc->forwardmove[speed];

   // analog movement axis
   forward += (int)(pc->forwardmove[speed] * joyaxes[axis_move]);

   if(gameactions[ka_moveright])
      side += pc->sidemove[speed];
   if(gameactions[ka_moveleft])
      side -= pc->sidemove[speed];
   
   if(gameactions[ka_jump])                // -- joek 12/22/07
      cmd->actions |= AC_JUMP;
   
   mlook = allowmlook && (gameactions[ka_mlook] || automlook);

   // console commands
   cmd->chatchar = C_dequeueChatChar();

   if(gameactions[ka_attack])
      cmd->buttons |= BT_ATTACK;

   if(gameactions[ka_use])
      cmd->buttons |= BT_USE;

   // only put BTN codes in here
   if(demo_version >= 401)
   {
      if(gameactions[ka_attack_alt])
         cmd->buttons |= BTN_ATTACK_ALT;

      newweapon = -1;

      if((players[consoleplayer].attackdown && !P_CheckAmmo(&players[consoleplayer]))
         || gameactions[ka_nextweapon])
      {
         weaponinfo_t *temp = E_FindBestWeapon(&players[consoleplayer]);
         if(temp == nullptr)
         {
            players[consoleplayer].attackdown = AT_NONE;
            newweapon = -1;
         }
         else
         {
            newweapon = temp->id; // phares
            cmd->slotIndex = E_FindFirstWeaponSlot(&players[consoleplayer], temp)->slotindex;
         }
      }

      for(int i = ka_weapon1; i <= ka_weapon9; i++)
      {
         if(gameactions[i])
         {
            weaponinfo_t *weapon = P_GetPlayerWeapon(&players[consoleplayer], i - ka_weapon1);
            if(weapon)
            {
               const auto slot = E_FindEntryForWeaponInSlot(&players[consoleplayer],
                                                            weapon, i - ka_weapon1);
               newweapon = weapon->id;
               cmd->slotIndex = slot->slotindex;
               break;
            }
         }
      }

      //next / prev weapon actions
      if(gameactions[ka_weaponup])
         newweapon = P_NextWeapon(&players[consoleplayer], &cmd->slotIndex);
      else if(gameactions[ka_weapondown])
         newweapon = P_PrevWeapon(&players[consoleplayer], &cmd->slotIndex);

      if(players[consoleplayer].readyweapon)
      {
         if(players[consoleplayer].readyweapon->id == newweapon)
            newweapon = -1;
      }

      cmd->weaponID = newweapon + 1;
   }
   else // Þe olde wæpon switch
   {
      // phares:
      // Toggle between the top 2 favorite weapons.
      // If not currently aiming one of these, switch to
      // the favorite. Only switch if you possess the weapon.

      // killough 3/22/98:
      //
      // Perform automatic weapons switch here rather than in p_pspr.c,
      // except in demo_compatibility mode.
      //
      // killough 3/26/98, 4/2/98: fix autoswitch when no weapons are left

      if((!demo_compatibility && (players[consoleplayer].attackdown & AT_PRIMARY) &&
          !P_CheckAmmo(&players[consoleplayer])) || gameactions[ka_nextweapon])
      {
         newweapon = P_SwitchWeaponOld(&players[consoleplayer]); // phares
      }
      else
      {                                 // phares 02/26/98: Added gamemode checks
         newweapon =
           gameactions[ka_weapon1] ? wp_fist :    // killough 5/2/98: reformatted
           gameactions[ka_weapon2] ? wp_pistol :
           gameactions[ka_weapon3] ? wp_shotgun :
           gameactions[ka_weapon4] ? wp_chaingun :
           gameactions[ka_weapon5] ? wp_missile :
           gameactions[ka_weapon6] && GameModeInfo->id != shareware ? wp_plasma :
           gameactions[ka_weapon7] && GameModeInfo->id != shareware ? wp_bfg :
           gameactions[ka_weapon8] ? wp_chainsaw :
           (!demo_compatibility && gameactions[ka_weapon9] &&  // MaxW: Adopted from PRBoom
           enable_ssg) ? wp_supershotgun :
           wp_nochange;

         // killough 3/22/98: For network and demo consistency with the
         // new weapons preferences, we must do the weapons switches here
         // instead of in p_user.c. But for old demos we must do it in
         // p_user.c according to the old rules. Therefore demo_compatibility
         // determines where the weapons switch is made.

         // killough 2/8/98:
         // Allow user to switch to fist even if they have chainsaw.
         // Switch to fist or chainsaw based on preferences.
         // Switch to shotgun or SSG based on preferences.
         //
         // killough 10/98: make SG/SSG and Fist/Chainsaw
         // weapon toggles optional
      
         if(!demo_compatibility && doom_weapon_toggles)
         {
            player_t *player = &players[consoleplayer];

            // only select chainsaw from '1' if it's owned, it's
            // not already in use, and the player prefers it or
            // the fist is already in use, or the player does not
            // have the berserker strength.

            if(newweapon==wp_fist && E_PlayerOwnsWeaponForDEHNum(player, wp_chainsaw) &&
               !E_WeaponIsCurrentDEHNum(player, wp_chainsaw) &&
               (E_WeaponIsCurrentDEHNum(player, wp_fist) ||
                !player->powers[pw_strength] ||
                P_WeaponPreferred(wp_chainsaw, wp_fist)))
            {
               newweapon = wp_chainsaw;
            }

            // Select SSG from '3' only if it's owned and the player
            // does not have a shotgun, or if the shotgun is already
            // in use, or if the SSG is not already in use and the
            // player prefers it.

            if(newweapon == wp_shotgun && enable_ssg &&
               E_PlayerOwnsWeaponForDEHNum(player, wp_supershotgun) &&
               (!E_PlayerOwnsWeaponForDEHNum(player, wp_shotgun) ||
                E_WeaponIsCurrentDEHNum(player, wp_shotgun) ||
                !(E_WeaponIsCurrentDEHNum(player, wp_supershotgun) &&
                 P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
            {
               newweapon = wp_supershotgun;
            }
         }
         // killough 2/8/98, 3/22/98 -- end of weapon selection changes
      }

      // haleyjd 03/06/09: next/prev weapon actions
      if(gameactions[ka_weaponup])
         newweapon = P_NextWeapon(&players[consoleplayer]);
      else if(gameactions[ka_weapondown])
         newweapon = P_PrevWeapon(&players[consoleplayer]);

      if(newweapon != wp_nochange)
      {
         cmd->buttons |= BT_CHANGE;
         cmd->buttons |= newweapon << BT_WEAPONSHIFT;
      }
   }

   // mouse
  
   // forward double click -- haleyjd: still allow double clicks
   if(mouseb_dblc2 >= 0 && mousebuttons[mouseb_dblc2] != dclickstate && dclicktime > 1)
   {
      dclickstate = mousebuttons[mouseb_dblc2];
      
      if(dclickstate)
         dclicks++;

      if(dclicks == 2)
      {
         cmd->buttons |= BT_USE;
         dclicks = 0;
      }
      else
         dclicktime = 0;
   }
   else if((dclicktime += ticdup) > 20)
   {
      dclicks = 0;
      dclickstate = false;
   }

   // strafe double click

   if(mouseb_dblc1 >= 0 && mousebuttons[mouseb_dblc1] != dclickstate2 && dclicktime2 > 1)
   {
      dclickstate2 = mousebuttons[mouseb_dblc1];

      if(dclickstate2)
         dclicks2++;
      
      if(dclicks2 == 2)
      {
         cmd->buttons |= BT_USE;
         dclicks2 = 0;
      }
      else
         dclicktime2 = 0;
   }
   else if((dclicktime2 += ticdup) > 20)
   {
      dclicks2 = 0;
      dclickstate2 = false;
   }

   // sf: smooth out the mouse movement
   // change to use tmousex, y   

   tmousex = mousex;
   tmousey = mousey;

   // we average the mouse movement as well
   // this is most important in smoothing movement
   if(smooth_turning)
   {
      static double oldmousex=0.0, mousex2;
      static double oldmousey=0.0, mousey2;

      mousex2 = tmousex; mousey2 = tmousey;
      tmousex = (tmousex + oldmousex) / 2.0;        // average
      oldmousex = mousex2;
      tmousey = (tmousey + oldmousey) / 2.0;        // average
      oldmousey = mousey2;
   }

   // YSHEAR_FIXME: add arrow keylook?

   if(mlook)
   {
      // YSHEAR_FIXME: provide separate mlook speed setting?
      int lookval = (int)(tmousey * 16.0 / ((double)ticdup));
      if(invert_mouse)
         look -= lookval;
      else
         look += lookval;
   }
   else
   {                // just stopped mlooking?
      // YSHEAR_FIXME: make lookspring configurable
      if(prevmlook)
         sendcenterview = true;

      // haleyjd 10/24/08: novert support
      if(!novert)
         forward += (int)tmousey;
   }
   prevmlook = mlook;

   // analog gamepad look
   look += (int)(pc->lookspeed[1] * joyaxes[axis_look] * (invert_padlook ? -1.0 : 1.0));
   
   if(gameactions[ka_lookup])
      look += pc->lookspeed[speed];
   if(gameactions[ka_lookdown])
      look -= pc->lookspeed[speed];
   if(gameactions[ka_center])
      sendcenterview = true;

   // haleyjd: special value for view centering
   if(sendcenterview)
      look = -32768;
   else
   {
      if(look > 32767)
         look = 32767;
      else if(look < -32767)
         look = -32767;
   }

   // haleyjd 06/05/12: flight
   if(gameactions[ka_flyup])
      flyheight = FLIGHT_IMPULSE_AMT;
   if(gameactions[ka_flydown])
      flyheight = -FLIGHT_IMPULSE_AMT;

   // haleyjd 05/19/13: analog fly axis
   if(flyheight == 0)
      flyheight = (int)(joyaxes[axis_fly] * FLIGHT_IMPULSE_AMT);

   if(gameactions[ka_flycenter])
   {
      flyheight = FLIGHT_CENTER;
      look = -32768;
   }


   if(gameactions[ka_strafe])
      side += (int)(tmousex * 2.0);
   else
      cmd->angleturn -= (int)(tmousex * 8.0);

   if(forward > MAXPLMOVE)
      forward = MAXPLMOVE;
   else if(forward < -MAXPLMOVE)
      forward = -MAXPLMOVE;
   
   if(side > MAXPLMOVE)
      side = MAXPLMOVE;
   else if(side < -MAXPLMOVE)
      side = -MAXPLMOVE;

   cmd->forwardmove += forward;
   cmd->sidemove += side;
   cmd->look = look;
   cmd->fly = flyheight;

   // special buttons
   if(sendpause)
   {
      sendpause = false;
      cmd->buttons = BT_SPECIAL | BTS_PAUSE;
   }

   // killough 10/6/98: suppress savegames in demos
   if(sendsave && !demoplayback)
   {
      sendsave = false;
      cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot << BTS_SAVESHIFT);
   }

   mousex = mousey = 0.0;
}

//
// G_SetGameMap
//
// sf: from gamemapname, get gamemap and gameepisode
//
void G_SetGameMap(void)
{
   gamemap = G_GetMapForName(gamemapname);
   
   if(!(GameModeInfo->flags & GIF_MAPXY))
   {
      gameepisode = gamemap / 10;
      gamemap = gamemap % 10;
   }
   else
      gameepisode = 1;
   
   if(gameepisode < 1)
      gameepisode = 1;

   // haleyjd: simplified to use gameModeInfo

   // bound to maximum episode for gamemode
   // (only start episode 1 on shareware, etc)
   if(gameepisode > GameModeInfo->numEpisodes)
      gameepisode = GameModeInfo->numEpisodes;   
   
   if(gamemap < 0)
      gamemap = 0;
   if(gamemap > 9 && !(GameModeInfo->flags & GIF_MAPXY))
      gamemap = 9;
}

//
// G_SetGameMapName
//
// haleyjd: set and normalize gamemapname
//
void G_SetGameMapName(const char *s)
{
   strncpy(gamemapname, s, 8);
   M_Strupr(gamemapname);
}

extern gamestate_t wipegamestate;
extern gamestate_t oldgamestate;

//
// G_DoLoadLevel
//
void G_DoLoadLevel()
{
   levelstarttic = gametic; // for time calculation
   
   if(!demo_compatibility && demo_version < 203)   // killough 9/29/98
      basetic = gametic;

   // haleyjd 07/28/10: Waaaay too early for this.
   //gamestate = GS_LEVEL;

   P_SetupLevel(g_dir, gamemapname, 0, gameskill);

   if(gamestate != GS_LEVEL)       // level load error
   {
      for(int i = 0; i < MAXPLAYERS; i++)
         players[i].playerstate = PST_LIVE;
      return;
   }

   HU_FragsUpdate();

   if(!netgame || demoplayback)
      consoleplayer = 0;
   
   gameaction = ga_nothing;
   displayplayer = consoleplayer;    // view the guy you are playing
   P_ResetChasecam();    // sf: because displayplayer changed
   P_ResetWalkcam();
   Z_CheckHeap();

   // clear cmd building stuff
   for(int i = 0; i < axis_max; i++)
      joyaxes[i] = 0.0;
   mousex = mousey = 0.0;
   sendpause = sendsave = false;
   paused = 0;
   memset(mousearray,  0, sizeof(mousearray));
   memset(gameactions, 0, sizeof(gameactions)); // haleyjd 05/20/05: all bindings off
   G_ClearKeyStates(); 

   // killough: make -timedemo work on multilevel demos
   // Move to end of function to minimize noise -- killough 2/22/98:
   
   //jff 4/26/98 wake up the status bar in case were coming out of a DM demo
   // killough 5/13/98: in case netdemo has consoleplayer other than green
   ST_Start();

   C_Popup();  // pop up the console
   
   // sf: if loading a hub level, restore position relative to sector
   //  for 'seamless' travel between levels
   if(hub_changelevel)
   {
      P_RestorePlayerPosition();
   }
   else
   {  // sf: no screen wipe while changing hub level
      if(wipegamestate == GS_LEVEL)
         wipegamestate = GS_NOSTATE;             // force a wipe
   }
}

//
// G_Responder
//
// Get info needed to make ticcmd_ts for the players.
//
bool G_Responder(const event_t* ev)
{
   int action;
   invbarstate_t &invbarstate = players[consoleplayer].invbarstate;

   // killough 9/29/98: reformatted
   if(gamestate == GS_LEVEL && 
      (HU_Responder(ev) ||  // chat ate the event
       ST_Responder(ev) ||  // status window ate it
       AM_Responder(ev)))
   {
      return true;
   }

   if(G_KeyResponder(ev, kac_cmd))
      return true;

   // This code is like Heretic's (to an extent). If the key is up and is the
   // inventory key (and the player isn't dead) then use the current artifact.
   if(ev->type == ev_keyup && G_KeyResponder(ev, kac_game) == ka_inventory_use
      && players[consoleplayer].playerstate != PST_DEAD)
   {
      usearti = true;
   }

   // any other key pops up menu if in demos
   //
   // killough 8/2/98: enable automap in -timedemo demos
   //
   // killough 9/29/98: make any key pop up menu regardless of
   // which kind of demo, and allow other events during playback

   if(gameaction == ga_nothing && (demoplayback || gamestate == GS_DEMOSCREEN))
   {
      // killough 9/29/98: allow user to pause demos during playback
      if (ev->type == ev_keydown && ev->data1 == key_pause)
      {
         if (paused ^= 2)
            S_PauseSound();
         else
            S_ResumeSound();
         return true;
      }

      // killough 10/98:
      // Don't pop up menu, if paused in middle
      // of demo playback, or if automap active.
      // Don't suck up keys, which may be cheats

      // haleyjd: deobfuscated into an if statement
      // fixed a bug introduced in beta 3 that possibly
      // broke the walkcam

      if(!walkcam_active) // if so, we need to go on below
      {
         if(gamestate == GS_DEMOSCREEN && !(paused & 2) && 
            !consoleactive && !automapactive &&
            (ev->type == ev_keydown || (ev->type == ev_mouse && ev->data1)))
         {
            // popup menu
            MN_StartControlPanel();
            return true;
         }
         else
         {
            return false;
         }
      }
   }

   if(gamestate == GS_FINALE && F_Responder(ev))
      return true;  // finale ate the event

   switch(ev->type)
   {
   case ev_keydown:
      if(ev->data1 == key_pause) // phares
         C_RunTextCmd("pause");
      else
      {
         action = G_KeyResponder(ev, kac_game); // haleyjd
         gameactions[action] = true;
      }

      if(gameactions[ka_autorun])
      {
         gameactions[ka_autorun] = 0;
         autorun = !autorun;
      }

      if(gameactions[ka_inventory_left])
      {
         inventoryTics = 5 * TICRATE;
         if(!invbarstate.inventory)
         {
            invbarstate.inventory = true;
            break;
         }
         E_MoveInventoryCursor(&players[consoleplayer], -1, invbarstate.inv_ptr);
         return true;
      }
      if(gameactions[ka_inventory_right])
      {
         inventoryTics = 5 * TICRATE;
         if(!invbarstate.inventory)
         {
            invbarstate.inventory = true;
            break;
         }
         E_MoveInventoryCursor(&players[consoleplayer], 1, invbarstate.inv_ptr);
         return true;
      }

      return true;    // eat key down events
      
   case ev_keyup:
      action = G_KeyResponder(ev, kac_game);   // haleyjd
      gameactions[action] = false;
      return false;   // always let key up events filter down
      
   case ev_mouse:
      mousebuttons[0] = !!(ev->data1 & 1);
      mousebuttons[1] = !!(ev->data1 & 2);
      mousebuttons[2] = !!(ev->data1 & 4);

      // SoM: this mimics the doom2 behavior better. 
      if(mouseSensitivity_vanilla)
      {
          mousex += (ev->data2 * (mouseSensitivity_horiz + 5.0) / 10.0);
          mousey += (ev->data3 * (mouseSensitivity_vert + 5.0) / 10.0);
      }
      else
      {
          // [CG] 01/20/12: raw sensitivity
          mousex += (ev->data2 * mouseSensitivity_horiz / 10.0);
          mousey += (ev->data3 * mouseSensitivity_vert / 10.0);
      }

      return true;    // eat events
      
   case ev_joystick:
      joyaxes[axisActions[ev->data1]] = ev->data2;
      return true;    // eat events
      
   default:
      break;
   }
   
   return false;
}

//
// DEMO RECORDING
//

// haleyjd 10/08/06: longtics demo support -- DOOM v1.91 writes 111 into the
// version field of its demos (because it's one more than v1.10 I guess).
#define DOOM_191_VERSION 111

static bool longtics_demo; // if true, demo playing is longtics format

static char *defdemoname;

//
// G_DemoStartMessage
//
// haleyjd 04/27/10: prints a nice startup message when playing a demo.
//
static void G_DemoStartMessage(const char *basename)
{
   if(demo_version < 200) // Vanilla demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%d\n",
               basename, demo_version / 100, demo_version % 100);
   }
   else if(demo_version >= 200 && demo_version <= 203) // Boom & MBF demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%02d%s\n",
               basename, demo_version / 100, demo_version % 100,
               demo_version >= 200 && demo_version <= 202 ? 
                  (compatibility ? "; comp=on" : "; comp=off") : "");
   }
   else // Eternity demos
   {
      C_Printf("Playing demo '%s'\n"
               FC_HI "\tVersion %d.%02d.%02d\n",
               basename, demo_version / 100, demo_version % 100, demo_subversion);
   }
}

//
// complevels
//
// haleyjd 04/10/10: compatibility matrix for properly setting comp vars based
// on the current demoversion. Derived from PrBoom.
//
struct complevel_s
{
   int fix; // first version that contained a fix
   int opt; // first version that made the fix an option
} complevels[] =
{
   { 203, 203 }, // comp_telefrag
   { 203, 203 }, // comp_dropoff
   { 201, 203 }, // comp_vile
   { 201, 203 }, // comp_pain
   { 201, 203 }, // comp_skull
   { 201, 203 }, // comp_blazing
   { 201, 203 }, // comp_doorlight
   { 201, 203 }, // comp_model
   { 201, 203 }, // comp_god
   { 203, 203 }, // comp_falloff
   { 200, 203 }, // comp_floors - FIXME
   { 201, 203 }, // comp_skymap
   { 203, 203 }, // comp_pursuit
   { 202, 203 }, // comp_doorstuck
   { 203, 203 }, // comp_staylift
   { 203, 203 }, // comp_zombie
   { 202, 203 }, // comp_stairs
   { 203, 203 }, // comp_infcheat
   { 201, 203 }, // comp_zerotags
   { 329, 329 }, // comp_terrain
   { 329, 329 }, // comp_respawnfix
   { 329, 329 }, // comp_fallingdmg
   { 329, 329 }, // comp_soul
   { 329, 329 }, // comp_theights
   { 329, 329 }, // comp_overunder
   { 329, 329 }, // comp_planeshoot
   { 335, 335 }, // comp_special
   { 337, 337 }, // comp_ninja
   { 340, 340 }, // comp_aircontrol
   { 0,   0   }
};

//
// G_SetCompatibility
//
// haleyjd 04/10/10
//
static void G_SetCompatibility(void)
{
   int i = 0;

   while(complevels[i].fix > 0)
   {
      if(demo_version < complevels[i].opt)
         comp[i] = (demo_version < complevels[i].fix);

      ++i;
   }
}

//
// NETCODE_FIXME -- DEMO_FIXME
//
// More demo-related stuff here, for playing back demos. Will need more
// version detection to detect the new non-homogeneous demo format.
// Use of G_ReadOptions also impacts the configuration, netcode, 
// console, etc. G_ReadOptions and G_WriteOptions are, as indicated in
// one of Lee's comments, designed to be able to transmit initial
// variable values during netgame arbitration. I don't know if this
// avenue should be pursued but it might be a good idea. The current
// system being used to send them at startup is garbage.
//
void G_DoPlayDemo(void)
{
   skill_t skill;
   int i, episode, map;
   int demover;
   char basename[9];
   byte *option_p = NULL;      // killough 11/98
   int lumpnum;

   memset(basename, 0, sizeof(basename));
  
   if(gameaction != ga_loadgame)      // killough 12/98: support -loadgame
      basetic = gametic;  // killough 9/29/98
      
   M_ExtractFileBase(defdemoname, basename);         // killough
   
   // haleyjd 11/09/09: check ns_demos namespace first, then ns_global
   if((lumpnum = wGlobalDir.checkNumForNameNSG(basename, lumpinfo_t::ns_demos)) < 0)
   {
      if(singledemo)
         I_Error("G_DoPlayDemo: no such demo %s\n", basename);
      else
      {
         C_Printf(FC_ERROR "G_DoPlayDemo: no such demo %s\n", basename);
         gameaction = ga_nothing;
         D_AdvanceDemo();
      }
      return;
   }

   demobuffer = demo_p = (byte *)(wGlobalDir.cacheLumpNum(lumpnum, PU_STATIC)); // killough

   // Check for empty demo lumps
   if(!demo_p)
   {
      if(singledemo)
         I_Error("G_DoPlayDemo: empty demo %s\n", basename);
      else
      {
         gameaction = ga_nothing;
         D_AdvanceDemo();
      }
      return;  // protect against zero-length lumps
   }
   
   // killough 2/22/98, 2/28/98: autodetect old demos and act accordingly.
   // Old demos turn on demo_compatibility => compatibility; new demos load
   // compatibility flag, and other flags as well, as a part of the demo.
   
   // haleyjd: this is the version for DOOM/BOOM/MBF demos; new demos 
   // test the signature and then get the new version number after it
   // if the signature matches the eedemosig array declared above.

   // killough 7/19/98: use the version id stored in demo
   demo_version = demover = *demo_p++;

   // Supported demo formats:
   // * 0.99 - 1.2:  first byte is skill level, 0-5; support is minimal.
   // * 1.4  - 1.10: 13-byte header w/version number; 1.9 supported fully.
   // * 1.11:        DOOM 1.91 longtics demo; supported fully.
   // * 2.00 - 2.02: BOOM demos; support poor for maps using BOOM features.
   // * 2.03:        MBF demos; support should be near perfect.
   // * 2.55:        These are EE demos. True version number is stored later.

   // Note that SMMU demos cannot be supported due to the fact that they
   // contain incorrect version numbers. Demo recording was also broken in
   // several versions of the port anyway.

   if(!((demover >=   0 && demover <= 4  ) || // Doom 1.2 or less
        (demover >= 104 && demover <= 111) || // Doom 1.4 - 1.9, 1.10, 1.11
        (demover >= 200 && demover <= 203) || // BOOM, MBF
        (demover == 255)))                    // Eternity
   {
      if(singledemo)
         I_Error("G_DoPlayDemo: unsupported demo format\n");
      else
      {
         C_Printf(FC_ERROR "Unsupported demo format\n");
         gameaction = ga_nothing;
         Z_ChangeTag(demobuffer, PU_CACHE);
         D_AdvanceDemo();
      }
      return;
   }
   
   int dmtype = 0;
   if(demover < 200)     // Autodetect old demos
   {
      // haleyjd 10/08/06: longtics support
      longtics_demo = (demover >= DOOM_191_VERSION);

      demo_subversion = 0; // haleyjd: always 0 for old demos

      G_SetCompatibility();

      // haleyjd 03/17/09: in old Heretic demos, some should be false
      if(GameModeInfo->type == Game_Heretic)
      {
         comp[comp_terrain]   = 0; // terrain on
         comp[comp_overunder] = 0; // 3D clipping on
      }

      // killough 3/2/98: force these variables to be 0 in demo_compatibility

      variable_friction = 0;

      weapon_recoil = 0;

      allow_pushers = 0;

      monster_infighting = 1;           // killough 7/19/98

      bfgtype = bfg_normal;                  // killough 7/19/98

      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98

      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98

      monster_friction = 0;             // killough 10/98
      help_friends = 0;                 // killough 9/9/98
      monkeys = 0;

      // haleyjd 05/23/04: autoaim is sync-critical
      default_autoaim = autoaim;
      autoaim = 1;

      default_allowmlook = allowmlook;
      allowmlook = 0;

      // for the sake of Heretic/Hexen demos only
      pitchedflight = false;

      // killough 3/6/98: rearrange to fix savegame bugs (moved fastparm,
      // respawnparm, nomonsters flags to G_LoadOptions()/G_SaveOptions())

      if(demover >= 100)         // For demos from versions >= 1.4
      {
         skill = (skill_t)(*demo_p++);
         episode = *demo_p++;
         map = *demo_p++;
         dmtype = *demo_p++;
         respawnparm = !!(*demo_p++);
         fastparm = !!(*demo_p++);
         nomonsters = !!(*demo_p++);
         consoleplayer = *demo_p++;
      }
      else
      {
         skill = (skill_t)demover;
         episode = *demo_p++;
         map = *demo_p++;
         dmtype = respawnparm = fastparm = nomonsters = false;
         consoleplayer = 0;
      }
   }
   else    // new versions of demos
   {
      // haleyjd: don't ignore the signature any more -- use it
      // demo_p += 6;               // skip signature;
      if(demo_version == 255 && !strncmp((const char *)demo_p, eedemosig, 5))
      {
         int temp;
         
         demo_p += 6; // increment past signature
         
         // reconstruct full version number and reset it
         temp  =        *demo_p++;         // byte one
         temp |= ((int)(*demo_p++)) <<  8; // byte two
         temp |= ((int)(*demo_p++)) << 16; // byte three
         temp |= ((int)(*demo_p++)) << 24; // byte four
         demo_version = demover = temp;
         
         // get subversion
         demo_subversion = *demo_p++;
      }
      else
      {
         demo_p += 6; // increment past signature
         
         // subversion is always 0 for demo versions < 329
         demo_subversion = 0;
      }
      
      compatibility = *demo_p++;       // load old compatibility flag
      skill = (skill_t)(*demo_p++);
      episode = *demo_p++;
      map = *demo_p++;
      dmtype = *demo_p++;
      consoleplayer = *demo_p++;

      // haleyjd 10/08/06: determine longtics support in new demos
      longtics_demo = (full_demo_version >= make_full_version(333, 50));

      // haleyjd 04/14/03: retrieve dmflags if appropriate version
      if(demo_version >= 331)
      {
         dmflags  = *demo_p++;
         dmflags |= ((unsigned int)(*demo_p++)) << 8;
         dmflags |= ((unsigned int)(*demo_p++)) << 16;
         dmflags |= ((unsigned int)(*demo_p++)) << 24;
      }

      // haleyjd 12/14/01: retrieve gamemapname if in appropriate
      // version
      if(full_demo_version >= make_full_version(329, 5))
      {
         int mn;
         
         for(mn = 0; mn < 8; mn++)
            gamemapname[mn] = *demo_p++;
         
         gamemapname[8] = '\0';
      }

      // killough 11/98: save option pointer for below
      if (demover >= 203)
         option_p = demo_p;

      demo_p = G_ReadOptions(demo_p);  // killough 3/1/98: Read game options
      
      if(demover == 200)        // killough 6/3/98: partially fix v2.00 demos
         demo_p += 256-GAME_OPTION_SIZE;
   }

   if(demo_compatibility)  // only 4 players can exist in old demos
   {
      for(i=0; i<4; i++)  // intentionally hard-coded 4 -- killough
         playeringame[i] = !!(*demo_p++);
      for(;i < MAXPLAYERS; i++)
         playeringame[i] = 0;
   }
   else
   {
      for(i = 0; i < MAXPLAYERS; ++i)
         playeringame[i] = !!(*demo_p++);
      demo_p += MIN_MAXPLAYERS - MAXPLAYERS;
   }

   if(playeringame[1])
      netgame = netdemo = true;

   // haleyjd 04/10/03: determine game type
   if(demo_version < 331)
   {
      // note: do NOT set default_dmflags here
      if(dmtype)
      {
         GameType = gt_dm;
         G_SetDefaultDMFlags(dmtype, false);
      }
      else
      {
         // Support -solo-net for demos previously recorded so, at vanilla
         // compatibility.
         GameType = (netgame || M_CheckParm("-solo-net") ? gt_coop : gt_single);
         G_SetDefaultDMFlags(0, false);
      }
   }
   else
   {
      // dmflags was already set above,
      // "dmtype" now holds the game type
      GameType = (gametype_t)dmtype;
   }
   deathmatch = !!dmtype; // ioanch: fix this now
   
   // don't spend a lot of time in loadlevel

   if(gameaction != ga_loadgame)      // killough 12/98: support -loadgame
   {
      // killough 2/22/98:
      // Do it anyway for timing demos, to reduce timing noise
      precache = timingdemo;
      
      // haleyjd: choose appropriate G_InitNew based on version
      if(full_demo_version >= make_full_version(329, 5))
         G_InitNew(skill, gamemapname);
      else
         G_InitNewNum(skill, episode, map);

      // killough 11/98: If OPTIONS were loaded from the wad in G_InitNew(),
      // reload any demo sync-critical ones from the demo itself, to be exactly
      // the same as during recording.
      
      if(option_p)
         G_ReadOptions(option_p);
   }

   precache = true;
   usergame = false;
   demoplayback = true;
   
   for(i=0; i<MAXPLAYERS;i++)         // killough 4/24/98
      players[i].cheats = 0;
   
   gameaction = ga_nothing;

   G_DemoStartMessage(basename);
   
   if(timingdemo)
   {
      static int first = 1;
      if(first)
      {
         starttime = i_haltimer.GetRealTime();
         startgametic = gametic;
         first = 0;
      }
   }
}

#define DEMOMARKER    0x80

//
// NETCODE_FIXME -- DEMO_FIXME
//
// In order to allow console commands and the such in demos, the demo
// reading system will require major modifications. In order to support
// old demos too, it will probably be necessary to split this function
// off into one that handles the old format and one that handles the
// new. Once demos contain things other than just ticcmds, they get
// a lot more complicated. The new demo format will also have to deal
// with ticcmd packing. It will make demos smaller, but will require
// use of the same functions needed by the netcode to pack/unpack
// ticcmds.
//

static void G_ReadDemoTiccmd(ticcmd_t *cmd)
{
   if(*demo_p == DEMOMARKER)
   {
      G_CheckDemoStatus();      // end of demo data stream
   }
   else
   {
      cmd->forwardmove = ((signed char)*demo_p++);
      cmd->sidemove    = ((signed char)*demo_p++);
      
      if(longtics_demo) // haleyjd 10/08/06: longtics support
      {
         cmd->angleturn  =  *demo_p++;
         cmd->angleturn |= (*demo_p++) << 8;
      }
      else
         cmd->angleturn = ((unsigned char)*demo_p++)<<8;
      
      cmd->buttons = (unsigned char)*demo_p++;

      // old Heretic demo?
      if(demo_version <= 4 && GameModeInfo->type == Game_Heretic)
      {
         demo_p++;
         demo_p++; // TODO/FIXME: put into cmd->fly as is mostly compatible
      }
      
      if(demo_version >= 335)
         cmd->actions = *demo_p++;
      else
         cmd->actions = 0;

      if(demo_version >= 333)
      {
         cmd->look  =  *demo_p++;
         cmd->look |= (*demo_p++) << 8;
      }
      else if(demo_version >= 329)
      {
         // haleyjd: 329 and 331 store updownangle, but we can't use
         // it any longer. Demos recorded with mlook will desync, 
         // but ones without can still play with this here.
         ++demo_p;
         cmd->look = 0;
      }
      else
         cmd->look = 0;

      if(full_demo_version >= make_full_version(340, 23))
         cmd->fly = *demo_p++;
      else
         cmd->fly = 0;

      if(demo_version >= 401)
      {
         cmd->itemID =   *demo_p++;
         cmd->itemID |= (*demo_p++) << 8;
         cmd->weaponID = *demo_p++;
         cmd->weaponID |= (*demo_p++) << 8;
         cmd->slotIndex = *demo_p++;
      }
      else
      {
         cmd->itemID = 0;
         cmd->weaponID = 0;
         cmd->slotIndex = 0;
      }

      // killough 3/26/98, 10/98: Ignore savegames in demos 
      if(demoplayback && 
         cmd->buttons & BT_SPECIAL && cmd->buttons & BTS_SAVEGAME)
      {
         cmd->buttons &= ~BTS_SAVEGAME;
         doom_printf("Game Saved (Suppressed)");
      }
   }
}

//
// G_WriteDemoTiccmd
//
// Demo limits removed -- killough
//
// NETCODE_FIXME: This will have to change as well, but maintaining
// compatibility is not necessary for demo writing, making this function
// much simpler to handle. Like reading, writing of other types of
// commands and ticcmd packing must be handled here for the new demo
// format. The way the demo buffer grows is also sensitive and will
// have to be changed, otherwise the buffer may be overflowed before
// it checks for another reallocation. zdoom changes this, so I know
// it is an issue.
//
static void G_WriteDemoTiccmd(ticcmd_t *cmd)
{
   unsigned int position = static_cast<unsigned int>(demo_p - demobuffer);
   int i = 0;
   
   demo_p[i++] = cmd->forwardmove;
   demo_p[i++] = cmd->sidemove;

   // haleyjd 10/08/06: longtics support from Choco Doom.
   // If this is a longtics demo, record in higher resolution
   if(longtics_demo)
   {
      demo_p[i++] =  cmd->angleturn & 0xff;
      demo_p[i++] = (cmd->angleturn >> 8) & 0xff;
   }
   else
      demo_p[i++] = (cmd->angleturn + 128) >> 8; 

   demo_p[i++] =  cmd->buttons;

   if(demo_version >= 335)
      demo_p[i++] =  cmd->actions;         //  -- joek 12/22/07

   if(demo_version >= 333)
   {
      demo_p[i++] =  cmd->look & 0xff;
      demo_p[i++] = (cmd->look >> 8) & 0xff;
   }

   if(full_demo_version >= make_full_version(340, 23))
      demo_p[i++] = cmd->fly;
   
   if(demo_version >= 401)
   {
      demo_p[i++] =  cmd->itemID & 0xff;
      demo_p[i++] = (cmd->itemID >> 8) & 0xff;
      demo_p[i++] = cmd->weaponID & 0xff;
      demo_p[i++] = (cmd->weaponID >> 8) & 0xff;
      demo_p[i] = cmd->slotIndex;
   }

   // NOTE: the distance is *double* that of (ticcmd_t + trailer) because on
   // Release builds, if ticcmd_t becomes larger, just using the simple value
   // would lock up the program when calling realloc!
   if(position + 2 * (sizeof(ticcmd_t) + sizeof(uint32_t)) > maxdemosize)   // killough 8/23/98
   {
      // no more space
      maxdemosize += 128*1024;   // add another 128K  -- killough
      demobuffer = erealloc(byte *, demobuffer, maxdemosize);
      demo_p = position + demobuffer;  // back on track
      // end of main demo limit changes -- killough
   }
   
   G_ReadDemoTiccmd(cmd); // make SURE it is exactly the same
}

static bool secretexit;

// haleyjd: true if a script called exitsecret()
bool scriptSecret = false; 

void G_ExitLevel(int destmap)
{
   // double tabs to be easily visible against deaths
   G_DemoLog("%d\tExit normal\t\t", gametic);
   G_DemoLogStats();
   G_DemoLog("\n");
   G_DemoLogSetExited(true);
   g_destmap  = destmap;
   secretexit = scriptSecret = false;
   gameaction = ga_completed;
}

//
// G_SecretExitLevel
//
// Here's for the german edition.
// IF NO WOLF3D LEVELS, NO SECRET EXIT!
// (unless it's a script secret exit)
//
void G_SecretExitLevel(int destmap)
{
   G_DemoLog("%d\tExit secret\t\t", gametic);
   G_DemoLogStats();
   G_DemoLog("\n");
   G_DemoLogSetExited(true);
   secretexit = !(GameModeInfo->flags & GIF_WOLFHACK) || haswolflevels || scriptSecret;
   g_destmap  = destmap;
   gameaction = ga_completed;
}

//
// G_PlayerFinishLevel
//
// Called when a player completes a level.
//
static void G_PlayerFinishLevel(int player)
{
   player_t *p = &players[player];

   // INVENTORY_TODO: convert powers to inventory
   memset(p->powers, 0, sizeof p->powers);
   p->mo->flags  &= ~MF_SHADOW;             // cancel invisibility
   p->mo->flags2 &= ~MF2_DONTDRAW;          // haleyjd: cancel total invis.
   p->mo->flags4 &= ~MF4_TOTALINVISIBLE; 
   p->mo->flags3 &= ~MF3_GHOST;             // haleyjd: cancel ghost

   E_InventoryEndHub(p);   // haleyjd: strip inventory

   p->extralight    = 0;   // cancel gun flashes
   p->fixedcolormap = 0;   // cancel ir gogles
   p->damagecount   = 0;   // no palette changes
   p->bonuscount    = 0;
}

//
// G_SetNextMap
//
// haleyjd 07/03/09: Replaces gamemode-dependent exit determination
// functions with interpretation of a rule set held in GameModeInfo.
//
static void G_SetNextMap()
{
   exitrule_t *exitrule = GameModeInfo->exitRules;
   exitrule_t *theRule = NULL;

   // find a rule
   for(; exitrule->gameepisode != -2; exitrule++)
   {
      if((exitrule->gameepisode == -1 || exitrule->gameepisode == gameepisode) &&
         (exitrule->gamemap == -1 || exitrule->gamemap == gamemap) &&
         exitrule->isSecret == secretexit)
      {
         theRule = exitrule;
         break;
      }
   }

   if(theRule)
      wminfo.next = theRule->destmap;
   else if(!secretexit)
      wminfo.next = gamemap;
}

//
// G_doFinale
//
// Test whether or not a level should have a finale done for it.
//
static bool G_doFinale()
{
   if(LevelInfo.killFinale) // "kill finale" flag disables unconditionally
      return false;

   // determine which finale will be run
   if(secretexit)
   {
      if(LevelInfo.finaleNormalOnly) // normal exit only?
         return false;

      // at least one of the texts must be defined; secret will be preferred,
      // but intertext will be used if it is undefined
      if(!LevelInfo.interTextSecret && !LevelInfo.interText)
         return false;
   }  
   else
   {
      if(LevelInfo.finaleSecretOnly) // secret exit only?
         return false;

      // only normal text is used here
      if(!LevelInfo.interText)
         return false;
   }

   return true;
}

//
// Kind of next level: the secret or the overt one.
//
enum levelkind_t
{
   lk_overt,
   lk_secret
};

//
// Gets the name of the next level, either from map-info or explicit next
//
static const char *G_getNextLevelName(levelkind_t kind, int map)
{
   const char *nextName = kind == lk_secret ? LevelInfo.nextSecret :
   LevelInfo.nextLevel;
   if(!wminfo.nextexplicit && nextName && *nextName)
      return nextName;
   return G_GetNameForMap(gameepisode, map);
}

//
// Setups the MapInfo/LevelInfo fields of wminfo
//
static void G_setupMapInfoWMInfo(levelkind_t kind)
{
   const intermapinfo_t &next =
   IN_GetMapInfo(G_getNextLevelName(kind, wminfo.next + 1));

   wminfo.li_lastlevelname = LevelInfo.interLevelName;  // just reference it
   wminfo.li_nextlevelname = next.levelname;

   wminfo.li_lastlevelpic = LevelInfo.levelPic;
   wminfo.li_nextlevelpic = next.levelpic;

   const intermapinfo_t &last = IN_GetMapInfo(gamemapname);

   // NOTE: just for exit-pic, do NOT use LevelInfo.interPic! We need to tell if
   // it was set explicitly in map-info by the author, and intermapinfo_t is
   // certain to be populated directly from XL_ metatables.
   wminfo.li_lastexitpic = last.exitpic;
   wminfo.li_nextenterpic = next.enterpic;
}

//
// G_DoCompleted
//
// Called upon level completion. Figures out what map is next and
// starts the intermission.
//
static void G_DoCompleted()
{
   gameaction = ga_nothing;
   
   for(int i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
         G_PlayerFinishLevel(i);        // take away cards and stuff
   }

   // clear hubs now
   P_ClearHubs();
   
   if(automapactive)
      AM_Stop();

   if(LevelInfo.finaleEarly && G_doFinale())
   {
      gameaction = ga_victory;
      return;
   }

   if(!(GameModeInfo->flags & GIF_MAPXY)) // kilough 2/7/98
   {
      if(gamemap == 9)
      {
         for(int i = 0; i < MAXPLAYERS; i++)
            players[i].didsecret = true;
      }
   }

   wminfo.gotosecret = secretexit; // haleyjd
   wminfo.didsecret = players[consoleplayer].didsecret;
   wminfo.epsd = gameepisode - 1;
   wminfo.last = gamemap - 1;

   // set the next gamemap
   G_SetNextMap();

   // haleyjd: override with MapInfo values
   if(!secretexit)
   {
      if(*LevelInfo.nextLevel) // only for normal exit
      {
         wminfo.next = G_GetMapForName(LevelInfo.nextLevel);
         if(!(GameModeInfo->flags & GIF_MAPXY))
            wminfo.next = wminfo.next % 10;
         wminfo.next--;
      }
   }
   else
   {
      if(*LevelInfo.nextSecret) // only for secret exit
      {
         wminfo.next = G_GetMapForName(LevelInfo.nextSecret);
         if(!(GameModeInfo->flags & GIF_MAPXY))
            wminfo.next %= 10;
         wminfo.next--;
      }
   }

   // haleyjd: possibly override with g_destmap value
   if(g_destmap)
   {
      wminfo.next = g_destmap;
      wminfo.nextexplicit = true;
      if(!(GameModeInfo->flags & GIF_MAPXY))
      {
         if(wminfo.next < 1)
            wminfo.next = 1;
         else if(wminfo.next > 9)
            wminfo.next = 9;
      }
      wminfo.next--;
      g_destmap = 0;
   }
   else
      wminfo.nextexplicit = false;

   wminfo.maxkills  = totalkills;
   wminfo.maxitems  = totalitems;
   wminfo.maxsecret = totalsecret;
   wminfo.maxfrags  = 0;

   wminfo.partime = LevelInfo.partime; // haleyjd 07/22/04

   wminfo.pnum = consoleplayer;

   for(int i = 0; i < MAXPLAYERS; i++)
   {
      wminfo.plyr[i].in      = playeringame[i];
      wminfo.plyr[i].skills  = players[i].killcount;
      wminfo.plyr[i].sitems  = players[i].itemcount;
      wminfo.plyr[i].ssecret = players[i].secretcount;
      wminfo.plyr[i].stime   = leveltime;
      memcpy(wminfo.plyr[i].frags, players[i].frags, sizeof(wminfo.plyr[i].frags));
   }
  
   gamestate = GS_INTERMISSION;
   automapactive = false;
   
   if(statcopy)
      memcpy(statcopy, &wminfo, sizeof(wminfo));

   G_setupMapInfoWMInfo(secretexit ? lk_secret : lk_overt);
   
   IN_Start(&wminfo);
}

static void G_DoWorldDone()
{
   idmusnum = -1; //jff 3/17/98 allow new level's music to be loaded
   gamestate = GS_LOADING;
   gamemap = wminfo.next+1;

   // haleyjd: handle heretic hidden levels via missioninfo samelevel rules
   if(!wminfo.nextexplicit && GameModeInfo->missionInfo->sameLevels)
   {
      samelevel_t *sameLevel = GameModeInfo->missionInfo->sameLevels;
      while(sameLevel->episode != -1)
      {
         if(gameepisode == sameLevel->episode && gamemap == sameLevel->map)
         {
            --gamemap; // return to same level by default
            break;
         }
         ++sameLevel;
      }
   }
   
   // haleyjd: customizable secret exits
   if(secretexit)
      G_SetGameMapName(G_getNextLevelName(lk_secret, gamemap));
   else
   {
      // haleyjd 12/14/01: don't use nextlevel for secret exits here either!
      G_SetGameMapName(G_getNextLevelName(lk_overt, gamemap));
   }

   // haleyjd 10/24/10: if in Master Levels mode, see if the next map exists
   // in the wad directory, and if so, use it. Otherwise, return to the Master
   // Levels selection menu.
   if(inmanageddir)
   {
      wadlevel_t *level = W_FindLevelInDir(g_dir, gamemapname);

      if(!level && inmanageddir == MD_MASTERLEVELS)
      {
         gameaction = ga_nothing;
         inmanageddir = MD_NONE;
         W_DoMasterLevels(false, gameskill);
         return;
      }
   }
   
   hub_changelevel = false;
   G_DoLoadLevel();
   gameaction = ga_nothing;
}

//
// G_ForceFinale
//
// haleyjd 07/01/09: Used by the Hexen Teleport_EndGame special.
// If the map does not have a finale sequence defined, it will be given
// a default sequence.
//
void G_ForceFinale()
{
   // in DOOM 2, we want a cast call
   if(GameModeInfo->flags & GIF_SETENDOFGAME)
      LevelInfo.endOfGame = true;   
   
   if(LevelInfo.finaleType == FINALE_TEXT) // modify finale type?
      LevelInfo.finaleType = GameModeInfo->teleEndGameFinaleType;

   // no text defined? make up something.
   if(!LevelInfo.interText)
      LevelInfo.interText = "You have won.";

   // set other variables for consistency
   LevelInfo.killFinale       = false;
   LevelInfo.finaleNormalOnly = false;
   LevelInfo.finaleSecretOnly = false;

   gameaction = ga_victory;
}

static char *savename;

//
// killough 5/15/98: add forced loadgames, which allow user to override checks
//

bool forced_loadgame = false;
bool command_loadgame = false;

void G_ForcedLoadGame(void)
{
   gameaction = ga_loadgame;
   forced_loadgame = true;
}

// killough 3/16/98: add slot info
// killough 5/15/98: add command-line

void G_LoadGame(const char *name, int slot, bool command)
{
   if(savename)
      efree(savename);
   savename = estrdup(name);
   savegameslot = slot;
   gameaction = ga_loadgame;
   forced_loadgame = false;
   command_loadgame = command;
   hub_changelevel = false;
}

// killough 5/15/98:
// Consistency Error when attempting to load savegame.

static void G_LoadGameErr(char *msg)
{
   Z_Free(savebuffer);           // Free the savegame buffer
   MN_ForcedLoadGame(msg);       // Print message asking for 'Y' to force
   if(command_loadgame)          // If this was a command-line -loadgame
   {
      D_StartTitle();            // Start the title screen
      gamestate = GS_DEMOSCREEN; // And set the game state accordingly
   }
}

//
// G_SaveGame
// Called by the menu task.
// Description is a 24 byte text string
//

void G_SaveGame(int slot, const char *description)
{
   savegameslot = slot;
   strcpy(savedescription, description);
   sendsave = true;
   hub_changelevel = false;
}

/*
//
// CheckSaveGame
//
// Lee Killough 1/22/98
// Check for overrun and realloc if necessary
//
void CheckSaveGame(size_t size)
{
   size_t pos = save_p - savebuffer;
   size += 1024;  // breathing room
   
   if(pos + size > savegamesize)
   {
      // haleyjd: deobfuscated
      savegamesize += (size + 1023) & ~1023;
      savebuffer = erealloc(byte *, savebuffer, savegamesize);
      save_p = savebuffer + pos;
   }
}
*/

// killough 3/22/98: form savegame name in one location
// (previously code was scattered around in multiple places)

void G_SaveGameName(char *name, size_t len, int slot)
{
   // Ty 05/04/98 - use savegamename variable (see d_deh.c)
   // killough 12/98: add .7 to truncate savegamename

   psnprintf(name, len, "%s/%.7s%d.dsg", 
             basesavegame, savegamename, slot);
}

//
// G_Signature
//
// killough 12/98:
// This function returns a signature for the current wad.
// It is used to distinguish between wads, for the purposes
// of savegame compatibility warnings, and options lookups.
//
// killough 12/98: use faster algorithm which has less IO
//
uint64_t G_Signature(const WadDirectory *dir)
{
   uint64_t s = 0;
   int lump, i;
   
   // sf: use gamemapname now, not gameepisode and gamemap
   lump = dir->checkNumForName(gamemapname);
   
   if(lump != -1 && (i = lump + 10) < dir->getNumLumps())
   {
      do
      {
         s = s * 2 + dir->lumpLength(i);
      }
      while(--i > lump);
   }
   
   return s;
}

// sf: split into two functions
// haleyjd 11/27/10: moved all savegame writing to p_saveg.cpp

static void G_DoSaveGame(void)
{
   char *name = NULL;
   size_t len = M_StringAlloca(&name, 2, 26, basesavegame, savegamename);
   
   G_SaveGameName(name, len, savegameslot);
   
   P_SaveCurrentLevel(name, savedescription);
   
   gameaction = ga_nothing;
   savedescription[0] = 0;
}

WadDirectory *d_dir;

static void G_DoLoadGame(void)
{
   gameaction = ga_nothing;
   P_LoadGame(savename);
}

//
// G_CameraTicker
//
// haleyjd 01/10/2010: SMMU was calling camera tickers too early, so I turned
// it into a separate function to call from below.
//
static void G_CameraTicker(void)
{
   // run special cameras
   if((walkcam_active = (camera == &walkcamera)))
      P_WalkTicker();
   else if((chasecam_active = (camera == &chasecam)))
      P_ChaseTicker();
   else if(camera == &followcam)
   {
      if(!P_FollowCamTicker())
         cooldemo_tics = 0; // force refresh
   }

   // cooldemo countdown   
   if(demoplayback && cooldemo)
   {
      // force refresh on death (or rebirth in follow mode) of displayed player
      if(players[displayplayer].health <= 0 ||
         (cooldemo == 2 && camera != &followcam))
         cooldemo_tics = 0;

      if(cooldemo_tics)
         cooldemo_tics--;
      else
         G_CoolViewPoint();
   }
}

//
// G_Ticker
//
// Make ticcmd_ts for the players.
//
void G_Ticker()
{
   int i;

   // do player reborns if needed
   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i] && players[i].playerstate == PST_REBORN)
         G_DoReborn(i);
   }

   // do things to change the game state
   while (gameaction != ga_nothing)
   {
      switch (gameaction)
      {
      case ga_loadlevel:
         G_DoLoadLevel();
         break;
      case ga_newgame:
         G_DoNewGame();
         break;
      case ga_loadgame:
         G_DoLoadGame();
         break;
      case ga_savegame:
         G_DoSaveGame();
         break;
      case ga_playdemo:
         G_DoPlayDemo();
         break;
      case ga_completed:
         G_DoCompleted();
         break;
      case ga_victory:
         F_StartFinale(secretexit);
         break;
      case ga_worlddone:
         G_DoWorldDone();
         break;
      case ga_screenshot:
         M_ScreenShot();
         gameaction = ga_nothing;
         break;
      default:  // killough 9/29/98
         gameaction = ga_nothing;
         break;
      }
   }

   if(animscreenshot)    // animated screen shots
   {
      if(gametic % 16 == 0)
      {
         animscreenshot--;
         M_ScreenShot();
      }
   }

   // killough 10/6/98: allow games to be saved during demo
   // playback, by the playback user (not by demo itself)
   
   if (demoplayback && sendsave)
   {
      sendsave = false;
      G_DoSaveGame();
   }

   // killough 9/29/98: Skip some commands while pausing during demo
   // playback, or while menu is active.
   //
   // We increment basetic and skip processing if a demo being played
   // back is paused or if the menu is active while a non-net game is
   // being played, to maintain sync while allowing pauses.
   //
   // P_Ticker() does not stop netgames if a menu is activated, so
   // we do not need to stop if a menu is pulled up during netgames.

   if((paused & 2) || (!demoplayback && (menuactive || consoleactive) && !netgame))
   {
      basetic++;  // For revenant tracers and RNG -- we must maintain sync
   }
   else
   {
      // get commands, check consistency, and build new consistancy check
      int buf = (gametic / ticdup) % BACKUPTICS;
      
      for(i=0; i<MAXPLAYERS; i++)
      {
         if(playeringame[i])
         {
            ticcmd_t *cmd = &players[i].cmd;
            playerclass_t *pc = players[i].pclass;
            
            memcpy(cmd, &netcmds[i][buf], sizeof *cmd);
            
            if(demoplayback)
               G_ReadDemoTiccmd(cmd);
            
            if(demorecording)
               G_WriteDemoTiccmd(cmd);
            
            // check for turbo cheats
            // killough 2/14/98, 2/20/98 -- only warn in netgames and demos
            
            if((netgame || demoplayback) && 
               cmd->forwardmove > TURBOTHRESHOLD &&
               !(gametic&31) && ((gametic>>5)&3) == i)
            {
               doom_printf("%s is turbo!", players[i].name); // killough 9/29/98
            }
            
            if(netgame && !netdemo && !(gametic % ticdup))
            {
               if(gametic > BACKUPTICS && 
                  consistency[i][buf] != cmd->consistency)
               {
                  D_QuitNetGame();
                  C_Printf(FC_ERROR "consistency failure");
                  C_Printf(FC_ERROR "(%i should be %i)",
                              cmd->consistency, consistency[i][buf]);
               }
               
               // sf: include y as well as x
               if(players[i].mo)
                  consistency[i][buf] = (int16_t)(players[i].mo->x + players[i].mo->y);
               else
                  consistency[i][buf] = 0; // killough 2/14/98
            }
         }
      }
      
      // check for special buttons
      for(i = 0; i < MAXPLAYERS; i++)
      {
         if(playeringame[i] && 
            players[i].cmd.buttons & BT_SPECIAL)
         {
            // killough 9/29/98: allow multiple special buttons
            if(players[i].cmd.buttons & BTS_PAUSE)
            {
               if((paused ^= 1))
               {
                  I_PauseHaptics(true);
                  S_PauseSound();
               }
               else
               {
                  I_PauseHaptics(false);
                  S_ResumeSound();
               }
            }
            
            if(players[i].cmd.buttons & BTS_SAVEGAME)
            {
               if(!savedescription[0])
                  strcpy(savedescription, "NET GAME");
               savegameslot =
                  (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
               gameaction = ga_savegame;
            }
         }
      }
   }

   // turn inventory off after a certain amount of time
   invbarstate_t &invbarstate = players[consoleplayer].invbarstate;
   if(invbarstate.inventory && !(--inventoryTics))
   {
      players[consoleplayer].inv_ptr = invbarstate.inv_ptr;
      invbarstate.inventory = false;
   }

   // do main actions
   
   // killough 9/29/98: split up switch statement
   // into pauseable and unpauseable parts.
   
   // call other tickers
   C_NetTicker();        // sf: console network commands
   if(inwipe)
      Wipe_Ticker();
 
   if(gamestate == GS_LEVEL)
   {
      P_Ticker();
      G_CameraTicker(); // haleyjd: move cameras
      ST_Ticker(); 
      AM_Ticker(); 
      HU_Ticker();
   }
   else if(!(paused & 2)) // haleyjd: refactored
   {
      switch(gamestate)
      {
      case GS_INTERMISSION:
         IN_Ticker();
         break;
      case GS_FINALE:
         F_Ticker();
         break;
      case GS_DEMOSCREEN:
         D_PageTicker();
         break;
      default:
         break;
      }
   }
}

//
// PLAYER STRUCTURE FUNCTIONS
// also see P_SpawnPlayer in P_Things
//

//
// G_PlayerReborn
//
// Called after a player dies
// almost everything is cleared and initialized
//
void G_PlayerReborn(int player)
{
   player_t *p;
   int frags[MAXPLAYERS];
   int totalfrags;
   int killcount;
   int itemcount;
   int secretcount;
   int cheats;
   int playercolour;
   char playername[20];
   skin_t *playerskin;
   playerclass_t *playerclass;
   inventory_t inventory;
   inventoryindex_t inv_ptr;

   p = &players[player];

   memcpy(frags, p->frags, sizeof frags);
   strncpy(playername, p->name, 20);

   killcount    = p->killcount;
   itemcount    = p->itemcount;
   secretcount  = p->secretcount;
   cheats       = p->cheats;     // killough 3/10/98,3/21/98: preserve cheats across idclev
   playercolour = p->colormap;
   totalfrags   = p->totalfrags;
   playerskin   = p->skin;
   playerclass  = p->pclass;     // haleyjd: playerclass
   inventory    = p->inventory;  // haleyjd: inventory
   inv_ptr      = p->inv_ptr;

   delete p->weaponctrs;
  
   memset(p, 0, sizeof(*p));

   memcpy(p->frags, frags, sizeof(p->frags));
   strcpy(p->name, playername);

   p->weaponctrs = new WeaponCounterTree();
   
   p->killcount   = killcount;
   p->itemcount   = itemcount;
   p->secretcount = secretcount;
   p->cheats      = cheats;
   p->colormap    = playercolour;
   p->totalfrags  = totalfrags;
   p->skin        = playerskin;
   p->pclass      = playerclass;              // haleyjd: playerclass
   p->inventory   = inventory;                // haleyjd: inventory
   p->inv_ptr     = inv_ptr;
   p->playerstate = PST_LIVE;
   p->health      = p->pclass->initialhealth; // Ty 03/12/98 - use dehacked values
   p->quake       = 0;                        // haleyjd 01/21/07

   p->usedown = true; // don't do anything immediately

   // MaxW: 2018/01/03: Adapt for new attackdown
   p->attackdown = demo_version >= 401 ? AT_ALL : AT_PRIMARY;

   // clear inventory unless otherwise indicated
   if(!(dmflags & DM_KEEPITEMS))
      E_ClearInventory(p);
   
   // haleyjd 08/05/13: give reborn inventory
   for(unsigned int i = 0; i < playerclass->numrebornitems; i++)
   {
      // ignore this item due to cancellation by, ie., DeHackEd?
      if(playerclass->rebornitems[i].flags & RBIF_IGNORE)
         continue;

      const char   *name   = playerclass->rebornitems[i].itemname;
      int           amount = playerclass->rebornitems[i].amount;
      itemeffect_t *effect = E_ItemEffectForName(name);

      // only if have none, in the case that DM_KEEPITEMS is specified
      if(!E_GetItemOwnedAmount(p, effect))
         E_GiveInventoryItem(p, effect, amount);
   }

   if(!(p->readyweapon = E_FindBestWeapon(p)))
      p->readyweapon = E_WeaponForID(UnknownWeaponInfo);
   else
      p->readyweaponslot = E_FindFirstWeaponSlot(p, p->readyweapon);
}

void P_SpawnPlayer(mapthing_t *mthing);

// haleyjd 08/19/13: externalized player corpse queue from G_CheckSpot; 
// made into a PODCollection for encapsulation and safety.
static PODCollection<Mobj *> bodyque;

//
// G_queuePlayerCorpse
//
// haleyjd 08/19/13: enqueue a player corpse; externalized from G_CheckSpot.
// * When bodyquesize is negative, player corpses are never removed.
// * When bodyquesize is zero, player corpses get removed immediately.
// * When bodyquesize is positive, that number of corpses are allowed to exist,
//   and are removed from the queue FIFO when it fills up.
//
static void G_queuePlayerCorpse(Mobj *mo)
{
   if(bodyquesize > 0)
   {
      size_t queuesize = static_cast<size_t>(bodyquesize);
      size_t index     = bodyqueslot % queuesize;

      if(bodyque.getLength() < queuesize)
         bodyque.resize(queuesize);
      
      if(bodyque[index] != NULL)
      {
         bodyque[index]->intflags &= ~MIF_PLYRCORPSE;
         bodyque[index]->remove();
      }
      
      mo->intflags |= MIF_PLYRCORPSE;
      bodyque[index] = mo;
      bodyqueslot = (bodyqueslot + 1) % queuesize;
   }
   else if(!bodyquesize)
      mo->remove();   
}

//
// G_DeQueuePlayerCorpse
//
// haleyjd 08/19/13: If an Mobj in the bodyque is removed elsewhere, a dangling
// reference would remain in the array. Call this to cleanse the queue.
//
void G_DeQueuePlayerCorpse(const Mobj *mo)
{
   for(auto itr = bodyque.begin(); itr != bodyque.end(); itr++)
   {
      if(mo == *itr)
         *itr = NULL;
   }
}

//
// G_ClearPlayerCorpseQueue
//
// haleyjd 08/19/13: For safety, we clear the queue fully now between levels.
//
void G_ClearPlayerCorpseQueue()
{
   bodyque.zero();
   bodyqueslot = 0;
}

//
// G_CheckSpot
//
// Returns false if the player cannot be respawned
// at the given mapthing_t spot
// because something is occupying it
//
static bool G_CheckSpot(int playernum, mapthing_t *mthing, Mobj **fog)
{
   fixed_t      x, y;
   subsector_t *ss;
   unsigned     an;
   Mobj        *mo;
   int          i;
   fixed_t      mtcos, mtsin;

   if(!players[playernum].mo)
   {
      // first spawn of level, before corpses
      for(i = 0; i < playernum; i++)
      {
         // ioanch 20151218: fixed mapthing coordinates
         if(players[i].mo->x == mthing->x &&
            players[i].mo->y == mthing->y)
            return false;
      }
      return true;
   }

   // ioanch 20151218: fixed mapthing coordinates
   x = mthing->x;
   y = mthing->y;
   
   // killough 4/2/98: fix bug where P_CheckPosition() uses a non-solid
   // corpse to detect collisions with other players in DM starts
   //
   // Old code:
   // if (!P_CheckPosition (players[playernum].mo, x, y))
   //    return false;

   players[playernum].mo->flags |=  MF_SOLID;
   i = P_CheckPosition(players[playernum].mo, x, y);
   players[playernum].mo->flags &= ~MF_SOLID;
   if(!i)
      return false;

   // flush an old corpse if needed
   // killough 2/8/98: make corpse queue have an adjustable limit
   // killough 8/1/98: Fix bugs causing strange crashes
   G_queuePlayerCorpse(players[playernum].mo);

   // spawn a teleport fog
   ss = R_PointInSubsector(x,y);

   // haleyjd: There was a weird bug with this statement:
   //
   // an = (ANG45 * (mthing->angle/45)) >> ANGLETOFINESHIFT;
   //
   // Even though this code stores the result into an unsigned variable, most
   // compilers seem to ignore that fact in the optimizer and use the resulting
   // value directly in a lea instruction. This causes the signed mapthing_t
   // angle value to generate an out-of-bounds access into the fine trig
   // lookups. In vanilla, this accesses the finetangent table and other parts
   // of the finesine table, and the result is what I call the "ninja spawn,"
   // which is missing the fog and sound, as it spawns somewhere out in the
   // far reaches of the void.

   if(!comp[comp_ninja])
   {
      an = ANG45 * (angle_t)(mthing->angle / 45);
      mtcos = finecosine[an >> ANGLETOFINESHIFT];
      mtsin = finesine[an >> ANGLETOFINESHIFT];
   }
   else
   {
      // emulate out-of-bounds access to finecosine / finesine tables
      angle_t mtangle = (angle_t)(mthing->angle / 45);
      
      an = ANG45 * mtangle;

      switch(mtangle)
      {
      case 4: // 180 degrees (0x80000000 >> 19 == -4096)
         mtcos = finetangent[2048];
         mtsin = finetangent[0];
         break;
      case 5: // 225 degrees (0xA0000000 >> 19 == -3072)
         mtcos = finetangent[3072];
         mtsin = finetangent[1024];
         break;
      case 6: // 270 degrees (0xC0000000 >> 19 == -2048)
         mtcos = finesine[0];
         mtsin = finetangent[2048];
         break;
      case 7: // 315 degrees (0xE0000000 >> 19 == -1024)
         mtcos = finesine[1024];
         mtsin = finetangent[3072];
         break;
      default: // everything else works properly
         mtcos = finecosine[an >> ANGLETOFINESHIFT];
         mtsin = finesine[an >> ANGLETOFINESHIFT];
         break;
      }
   }

   mo = P_SpawnMobj(x + 20 * mtcos, 
                    y + 20 * mtsin,
                    ss->sector->floorheight + 
                       GameModeInfo->teleFogHeight, 
                    E_SafeThingName(GameModeInfo->teleFogType));

   // haleyjd: There was a hack here trying to avoid playing the sound on the
   // "first frame"; but if this is done, then you miss your own spawn sound
   // quite often, due to the fact your sound origin hasn't been moved yet.
   // So instead, I'll return the fog in *fog and play the sound at the caller.
   if(fog)
      *fog = mo;

   return true;
}

//
// G_ClosestDMSpot
//
// haleyjd 02/16/10: finds the closest deathmatch start to a given
// location. Returns -1 if a spot wasn't found.
//
// Will not return the spot marked in "notspot" if notspot is >= 0.
//
int G_ClosestDMSpot(fixed_t x, fixed_t y, int notspot)
{
   int j, numspots = int(deathmatch_p - deathmatchstarts);
   int closestspot = -1;
   fixed_t closestdist = 32767*FRACUNIT;

   if(numspots <= 0)
      return -1;

   for(j = 0; j < numspots; ++j)
   {
      // ioanch 20151218: fixed point mapthing coordinates
      fixed_t dist = P_AproxDistance(x - deathmatchstarts[j].x,
                                     y - deathmatchstarts[j].y);
      
      if(dist < closestdist && j != notspot)
      {
         closestdist = dist;
         closestspot = j;
      }
   }

   return closestspot;
}

extern const char *level_error;

//
// G_DeathMatchSpawnPlayer
//
// Spawns a player at one of the random death match spots
// called at level load and each death
//
void G_DeathMatchSpawnPlayer(int playernum)
{
   int j, selections = int(deathmatch_p - deathmatchstarts);
   Mobj *fog = NULL;
   
   if(selections < MAXPLAYERS)
   {
      static char errormsg[64];
      psnprintf(errormsg, sizeof(errormsg), 
                "Only %d deathmatch spots, %d required", 
                selections, MAXPLAYERS);
      level_error = errormsg;
      return;
   }

   for(j = 0; j < 20; j++)
   {
      int i = P_Random(pr_dmspawn) % selections;
      
      if(G_CheckSpot(playernum, &deathmatchstarts[i], &fog))
      {
         deathmatchstarts[i].type = playernum + 1;
         P_SpawnPlayer(&deathmatchstarts[i]);
         if(fog)
            S_StartSound(fog, GameModeInfo->teleSound);
         return;
      }
   }

   // no good spot, so the player will probably get stuck
   P_SpawnPlayer(&playerstarts[playernum]);
}

//
// G_DoReborn
//
void G_DoReborn(int playernum)
{
   hub_changelevel = false;

   if(GameType == gt_single) // haleyjd 04/10/03
   {
      // reload level from scratch
      // sf: use P_HubReborn, so that if in a hub, we restart from the
      // last time we entered this level
      // normal levels are unaffected
      P_HubReborn();
   }
   else
   {                               // respawn at the start
      int i;
      Mobj *fog = NULL;
      
      // first disassociate the corpse
      players[playernum].mo->player = NULL;

      // spawn at random spot if in deathmatch
      if(GameType == gt_dm)
      {
         G_DeathMatchSpawnPlayer(playernum);
         
         // haleyjd: G_DeathMatchSpawnPlayer may set level_error
         if(level_error)
         {
            C_Printf(FC_ERROR "G_DeathMatchSpawnPlayer: %s\a\n", level_error);
            C_SetConsole();
         }

         return;
      }

      if(G_CheckSpot(playernum, &playerstarts[playernum], &fog))
      {
         P_SpawnPlayer(&playerstarts[playernum]);
         if(fog)
            S_StartSound(fog, GameModeInfo->teleSound);
         return;
      }

      // try to spawn at one of the other players spots
      for(i = 0; i < MAXPLAYERS; i++)
      {
         fog = NULL;

         if(G_CheckSpot(playernum, &playerstarts[i], &fog))
         {
            playerstarts[i].type = playernum + 1; // fake as other player
            P_SpawnPlayer(&playerstarts[i]);
            if(fog)
               S_StartSound(fog, GameModeInfo->teleSound);
            playerstarts[i].type = i+1;   // restore
            return;
         }
         // he's going to be inside something.  Too bad.
      }
      P_SpawnPlayer(&playerstarts[playernum]);
   }
}

void G_ScreenShot()
{
   gameaction = ga_screenshot;
}

//
// G_WorldDone
//
void G_WorldDone()
{
   gameaction = ga_worlddone;

   // haleyjd 10/24/10: if in Master Levels mode, just return from here now.
   // The choice of whether to go to another level or show the Master Levels
   // menu is taken care of in G_DoWorldDone.
   if(inmanageddir == MD_MASTERLEVELS)
      return;

   if(secretexit)
      players[consoleplayer].didsecret = true;

   if(G_doFinale())
      F_StartFinale(secretexit);
}

static skill_t d_skill;
static int     d_episode;
static int     d_map;
static char    d_mapname[10];

int G_GetMapForName(const char *name)
{
   char normName[9];
   int map;

   strncpy(normName, name, 9);

   M_Strupr(normName);
   
   if(GameModeInfo->flags & GIF_MAPXY)
   {
      map = isMAPxy(normName) ? 
         10 * (normName[3]-'0') + (normName[4]-'0') : 0;
      return map;
   }
   else
   {
      int episode;
      if(isExMy(normName))
      {
         episode = normName[1] - '0';
         map = normName[3] - '0';
      }
      else
      {
         episode = 1;
         map = 0;
      }
      return (episode*10) + map;
   }
}

char *G_GetNameForMap(int episode, int map)
{
   static char levelname[9];

   memset(levelname, 0, 9);

   if(GameModeInfo->flags & GIF_MAPXY)
   {
      sprintf(levelname, "MAP%02d", map);
   }
   else
   {
      sprintf(levelname, "E%01dM%01d", episode, map);
   }

   return levelname;
}

void G_DeferedInitNewNum(skill_t skill, int episode, int map)
{
   G_DeferedInitNew(skill, G_GetNameForMap(episode, map) );
}

void G_DeferedInitNew(skill_t skill, const char *levelname)
{
   strncpy(d_mapname, levelname, 8);
   d_map = G_GetMapForName(levelname);
   
   if(!(GameModeInfo->flags & GIF_MAPXY))
   {
      d_episode = d_map / 10;
      d_map = d_map % 10;
   }
   else
      d_episode = 1;
   
   d_skill = skill;

   // managed directory defaults to null
   d_dir = nullptr;
   inmanageddir = MD_NONE;
   
   gameaction = ga_newgame;
}

//
// G_DeferedInitNewFromDir
//
// haleyjd 06/16/10: Calls G_DeferedInitNew and sets d_dir to the provided wad
// directory, for use when loading the level.
//
void G_DeferedInitNewFromDir(skill_t skill, const char *levelname, WadDirectory *dir)
{
   G_DeferedInitNew(skill, levelname);
   d_dir = dir;
}

// killough 7/19/98: Marine's best friend :)
static int G_GetHelpers()
{
   int j = M_CheckParm ("-dog");
   
   if(!j)
      j = M_CheckParm ("-dogs");

   return j ? ((j+1 < myargc) ? atoi(myargv[j+1]) : 1) : default_dogs;
}

// killough 3/1/98: function to reload all the default parameter
// settings before a new game begins

void G_ReloadDefaults()
{
   // killough 3/1/98: Initialize options based on config file
   // (allows functions above to load different values for demos
   // and savegames without messing up defaults).
   
   weapon_recoil = default_weapon_recoil;    // weapon recoil
   
   player_bobbing = default_player_bobbing;  // haleyjd: readded
   
   variable_friction = allow_pushers = true;
   
   monsters_remember = default_monsters_remember;   // remember former enemies
   
   monster_infighting = default_monster_infighting; // killough 7/19/98
   
   // dogs = netgame ? 0 : G_GetHelpers();             // killough 7/19/98
   if(GameType == gt_single) // haleyjd 04/10/03
      dogs = G_GetHelpers();
   else
      dogs = 0;
   
   dog_jumping = default_dog_jumping;
   
   distfriend = default_distfriend;                 // killough 8/8/98
   
   monster_backing = default_monster_backing;     // killough 9/8/98
   
   monster_avoid_hazards = default_monster_avoid_hazards; // killough 9/9/98
   
   monster_friction = default_monster_friction;     // killough 10/98
   
   help_friends = default_help_friends;             // killough 9/9/98
   
   autoaim = default_autoaim;
   
   allowmlook = default_allowmlook;
   
   monkeys = default_monkeys;
   
   bfgtype = default_bfgtype;               // killough 7/19/98
   
   // jff 1/24/98 reset play mode to command line spec'd version
   // killough 3/1/98: moved to here
   respawnparm = clrespawnparm;
   fastparm = clfastparm;
   nomonsters = clnomonsters;
   
   //jff 3/24/98 set startskill from defaultskill in config file, unless
   // it has already been set by a -skill parameter
   if(startskill == sk_none)
      startskill = (skill_t)(defaultskill - 1);
   
   demoplayback = false;
   singledemo = false; // haleyjd: restore from MBF
   netdemo = false;
   
   // killough 2/21/98:
   memset(playeringame+1, 0, sizeof(*playeringame)*(MAXPLAYERS-1));
   
   consoleplayer = 0;
   
   compatibility = false;     // killough 10/98: replaced by comp[] vector
   memcpy(comp, default_comp, sizeof comp);
   
   vanilla_mode = false;
   demo_version = version;       // killough 7/19/98: use this version's id
   demo_subversion = subversion; // haleyjd 06/17/01
   
   // killough 3/31/98, 4/5/98: demo sync insurance
   demo_insurance = default_demo_insurance == 1;

   // haleyjd 06/07/12: pitchedflight has default
   pitchedflight = default_pitchedflight;
   
   G_ScrambleRand();
}

//
// G_ScrambleRand
//
// killough 3/26/98: shuffle random seed
// sf: seperate function
//
void G_ScrambleRand()
{
   // SoM 3/13/2002: New SMMU code actually compiles in VC++
   // sf: simpler
   rngseed = (unsigned int) time(NULL);
}

void G_DoNewGame()
{
   //G_StopDemo();
   G_ReloadDefaults();            // killough 3/1/98
   P_ClearHubs();                 // sf: clear hubs when starting new game
   
   netgame  = false;              // killough 3/29/98
   GameType = DefaultGameType;    // haleyjd  4/10/03
   dmflags  = default_dmflags;    // haleyjd  4/15/03
   basetic  = gametic;            // killough 9/29/98
   
   G_InitNew(d_skill, d_mapname);
   gameaction = ga_nothing;
}

// haleyjd 07/13/03: -fast projectile information list

class MetaSpeedSet : public MetaObject
{
   DECLARE_RTTI_TYPE(MetaSpeedSet, MetaObject)

protected:
   int mobjType;            // the type this speedset is for
   int normalSpeed;         // the normal speed of this thing type
   int fastSpeed;           // -fast speed

public:
   // Default constructor
   MetaSpeedSet() 
      : MetaObject(), mobjType(-1), normalSpeed(0), fastSpeed(0)
   {
   }

   // Parameterized constructor
   MetaSpeedSet(int pMobjType, int pNormalSpeed, int pFastSpeed) 
      : MetaObject("speedset"), mobjType(pMobjType), 
        normalSpeed(pNormalSpeed), fastSpeed(pFastSpeed)
   {
   }

   // Copy constructor
   MetaSpeedSet(const MetaSpeedSet &other) : MetaObject(other) 
   {
      this->mobjType    = other.mobjType;
      this->normalSpeed = other.normalSpeed;
      this->fastSpeed   = other.fastSpeed;
   }

   // Virtual Methods
   virtual MetaObject *clone() const override { return new MetaSpeedSet(*this); }
   virtual const char *toString() const override
   {
      static char buf[128];
      int ns = normalSpeed;
      int fs = fastSpeed;

      if(ns >= FRACUNIT)
         ns >>= FRACBITS;
      if(fs >= FRACUNIT)
         fs >>= FRACBITS;

      psnprintf(buf, sizeof(buf), "type: %d, normal: %d, fast: %d", 
                mobjType, ns, fs);

      return buf;
   }

   // Accessors
   int getMobjType()    const { return mobjType;    }
   int getNormalSpeed() const { return normalSpeed; }
   int getFastSpeed()   const { return fastSpeed;   }

   void setNormalSpeed(int i)   { normalSpeed = i; }
   void setFastSpeed(int i)     { fastSpeed   = i; }
   
   void setSpeeds(int normal, int fast) { normalSpeed = normal; fastSpeed = fast; }
};

IMPLEMENT_RTTI_TYPE(MetaSpeedSet)

// Speedset key cache for fast lookups
static MetaKeyIndex speedsetKey("speedset");

void G_SpeedSetAddThing(int thingtype, int nspeed, int fspeed)
{
   MetaSpeedSet *mss;
   MetaTable    *meta = mobjinfo[thingtype]->meta;

   if((mss = meta->getObjectKeyAndTypeEx<MetaSpeedSet>(speedsetKey)))
      mss->setSpeeds(nspeed, fspeed);
   else
      meta->addObject(new MetaSpeedSet(thingtype, nspeed, fspeed));
}

//
// G_SetFastParms
//
// killough 4/10/98: New function to fix bug which caused Doom
// lockups when idclev was used in conjunction with -fast.
//
void G_SetFastParms(int fast_pending)
{
   static int fast = 0;            // remembers fast state
   MetaSpeedSet *mss;
   
   if(fast != fast_pending)       // only change if necessary
   {
      if((fast = fast_pending))
      {
         for(int i = 0; i < NUMSTATES; i++)
         {
            if(states[i]->flags & STATEF_SKILL5FAST)
            {
               // killough 4/10/98
               // don't change 1->0 since it causes cycles
               if(states[i]->tics != 1 || demo_compatibility)
                  states[i]->tics >>= 1;
            }
         }

         for(int i = 0; i < NUMMOBJTYPES; i++)
         {
            MetaTable *meta = mobjinfo[i]->meta;
            if((mss = meta->getObjectKeyAndTypeEx<MetaSpeedSet>(speedsetKey)))
               mobjinfo[i]->speed = mss->getFastSpeed();
         }
      }
      else
      {
         for(int i = 0; i < NUMSTATES; i++)
         {
            if(states[i]->flags & STATEF_SKILL5FAST)
               states[i]->tics <<= 1;
         }

         for(int i = 0; i < NUMMOBJTYPES; i++)
         {
            MetaTable *meta = mobjinfo[i]->meta;
            if((mss = meta->getObjectKeyAndTypeEx<MetaSpeedSet>(speedsetKey)))
               mobjinfo[i]->speed = mss->getNormalSpeed();
         }
      }
   }
}

//
// G_InitNewNum
//
void G_InitNewNum(skill_t skill, int episode, int map)
{
   G_InitNew(skill, G_GetNameForMap(episode, map) );
}

//
// G_InitNew
//
// Can be called by the startup code or the menu task,
// consoleplayer, displayplayer, playeringame[] should be set.
//
void G_InitNew(skill_t skill, const char *name)
{
   int i;

   // ACS_FIXME: For now, call ACS_NewGame from here. This may not suffice once
   // hubs are working, but then, pretty much all the level transfer code needs
   // to be rewritten for that to work smoothly anyways.
   ACS_NewGame();

   if(paused)
   {
      paused = 0;
      S_ResumeSound();
   }

   hub_changelevel = false;  // sf
   
   if(skill > sk_nightmare)
      skill = sk_nightmare;

   G_SetFastParms(fastparm || skill == sk_nightmare);  // killough 4/10/98

   M_ClearRandom();
   
   respawnmonsters = 
      (GameModeInfo->flags & GIF_SKILL5RESPAWN && skill == sk_nightmare) 
      || respawnparm;

   // force players to be initialized upon first level load
   for(i = 0; i < MAXPLAYERS; i++)
      players[i].playerstate = PST_REBORN;

   usergame = true;                // will be set false if a demo
   paused = 0;

   if(demoplayback)
   {
      netgame = false;
      displayplayer = consoleplayer = 0;
      P_ResetChasecam();      // sf: displayplayer changed
   }

   demoplayback = false;
   
   //G_StopDemo();
   
   automapactive = false;
   gameskill = skill;

   G_SetGameMapName(name);

   G_SetGameMap();  // sf
  
   if(demo_version >= 203)
      M_LoadOptions();     // killough 11/98: read OPTIONS lump from wad
  
   //G_StopDemo();

   // haleyjd 06/16/04: set g_dir to d_dir if it is valid, or else restore it
   // to the default value.
   g_dir = d_dir ? d_dir : (inmanageddir = MD_NONE, &wGlobalDir);
   d_dir = NULL;
   
   G_DoLoadLevel();
}

//
// G_RecordDemo
//
// NETCODE_FIXME -- DEMO_FIXME: See the comment above where demos
// are read. Some of the same issues may apply here.
//
void G_RecordDemo(const char *name)
{
   int i;
   
   demo_insurance = (default_demo_insurance != 0); // killough 12/98
   
   usergame = false;

   if(demoname)
      efree(demoname);
   demoname = emalloc(char *, strlen(name) + 8);

   M_AddDefaultExtension(strcpy(demoname, name), ".lmp");  // 1/18/98 killough
   
   i = M_CheckParm("-maxdemo");
   
   if(i && i<myargc-1)
      maxdemosize = atoi(myargv[i+1]) * 1024;
   
   if(maxdemosize < 0x20000)  // killough
      maxdemosize = 0x20000;
   
   demobuffer = emalloc(byte *, maxdemosize); // killough
   
   demorecording = true;
}

// These functions are used to read and write game-specific options in demos
// and savegames so that demo sync is preserved and savegame restoration is
// complete. Not all options (for example "compatibility"), however, should
// be loaded and saved here. It is extremely important to use the same
// positions as before for the variables, so if one becomes obsolete, the
// byte(s) should still be skipped over or padded with 0's.
// Lee Killough 3/1/98

// NETCODE_FIXME -- DEMO_FIXME -- SAVEGAME_FIXME: G_ReadOptions/G_WriteOptions
// These functions are going to be very important. The way they work may
// need to be altered for the new demo format too, although this must be
// done carefully so as to preserve demo compatibility with previous
// versions.

byte *G_WriteOptions(byte *demoptr)
{
   byte *target = demoptr + GAME_OPTION_SIZE;
   
   *demoptr++ = monsters_remember;  // part of monster AI -- byte 1
   
   *demoptr++ = variable_friction;  // ice & mud -- byte 2
   
   *demoptr++ = weapon_recoil;      // weapon recoil -- byte 3
   
   *demoptr++ = allow_pushers;      // PUSH Things -- byte 4
   
   *demoptr++ = 0;                  // ??? unused -- byte 5
   
   *demoptr++ = player_bobbing;     // whether player bobs or not -- byte 6
   
   // killough 3/6/98: add parameters to savegame, move around some in demos
   *demoptr++ = respawnparm; // byte 7
   *demoptr++ = fastparm;    // byte 8
   *demoptr++ = nomonsters;  // byte 9
   
   *demoptr++ = demo_insurance;        // killough 3/31/98 -- byte 10
   
   // killough 3/26/98: Added rngseed. 3/31/98: moved here
   *demoptr++ = (byte)((rngseed >> 24) & 0xff); // byte 11
   *demoptr++ = (byte)((rngseed >> 16) & 0xff); // byte 12
   *demoptr++ = (byte)((rngseed >>  8) & 0xff); // byte 13
   *demoptr++ = (byte)( rngseed        & 0xff); // byte 14
   
   // Options new to v2.03 begin here
   *demoptr++ = monster_infighting;        // killough 7/19/98 -- byte 15
   
   *demoptr++ = dogs;                      // killough 7/19/98 -- byte 16
   
   *demoptr++ = bfgtype;                   // killough 7/19/98 -- byte 17
   
   *demoptr++ = 0;                         // unused - (beta mode) -- byte 18
   
   *demoptr++ = (distfriend >> 8) & 0xff;  // killough 8/8/98 -- byte 19  
   *demoptr++ =  distfriend       & 0xff;  // killough 8/8/98 -- byte 20
   
   *demoptr++ = monster_backing;           // killough 9/8/98 -- byte 21
   
   *demoptr++ = monster_avoid_hazards;     // killough 9/9/98 -- byte 22
   
   *demoptr++ = monster_friction;          // killough 10/98  -- byte 23
   
   *demoptr++ = help_friends;              // killough 9/9/98 -- byte 24
   
   *demoptr++ = dog_jumping;               // byte 25
   
   *demoptr++ = monkeys;                   // byte 26
   
   // killough 10/98: a compatibility vector now
   for(int i = 0; i < COMP_TOTAL; i++)
      *demoptr++ = comp[i] != 0;           // bytes 27 - 58 : comp   
   
   // haleyjd 05/23/04: autoaim is sync critical
   *demoptr++ = autoaim;                   // byte 59

   // haleyjd 04/06/05: allowmlook is sync critical
   *demoptr++ = allowmlook;                // byte 60

   // haleyjd 06/07/12: pitchedflight
   *demoptr++ = pitchedflight;             // byte 61
   
   // CURRENT BYTES LEFT: 3

   //----------------
   // Padding at end
   //----------------
   while(demoptr < target)
      *demoptr++ = 0;
   
   if(demoptr != target)
      I_Error("G_WriteOptions: GAME_OPTION_SIZE is too small\n");
   
   return target;
}

// Same, but read instead of write

byte *G_ReadOptions(byte *demoptr)
{
   byte *target = demoptr + GAME_OPTION_SIZE;

   monsters_remember = *demoptr++;

   variable_friction = *demoptr;  // ice & mud
   demoptr++;

   weapon_recoil = *demoptr;      // weapon recoil
   demoptr++;

   allow_pushers = *demoptr;      // PUSH Things
   demoptr++;

   demoptr++;

   // haleyjd: restored bobbing to proper sync critical status
   player_bobbing = *demoptr;     // whether player bobs or not
   demoptr++;

   // killough 3/6/98: add parameters to savegame, move from demo
   respawnparm = !!(*demoptr++);
   fastparm    = !!(*demoptr++);
   nomonsters  = !!(*demoptr++);

   demo_insurance = *demoptr++;              // killough 3/31/98

   // killough 3/26/98: Added rngseed to demos; 3/31/98: moved here

   rngseed  = *demoptr++ & 0xff;
   rngseed <<= 8;
   rngseed += *demoptr++ & 0xff;
   rngseed <<= 8;
   rngseed += *demoptr++ & 0xff;
   rngseed <<= 8;
   rngseed += *demoptr++ & 0xff;

   // Options new to v2.03
   if(demo_version >= 203)
   {
      monster_infighting = *demoptr++;   // killough 7/19/98
      
      dogs = *demoptr++;                 // killough 7/19/98
      
      bfgtype = (bfg_t)(*demoptr++);     // killough 7/19/98
      demoptr++;                         // sf: where beta was
      
      distfriend = *demoptr++ << 8;      // killough 8/8/98
      distfriend+= *demoptr++;
      
      monster_backing = *demoptr++;      // killough 9/8/98
      
      monster_avoid_hazards = *demoptr++; // killough 9/9/98
      
      monster_friction = *demoptr++;     // killough 10/98
      
      help_friends = *demoptr++;         // killough 9/9/98
      
      dog_jumping = *demoptr++;          // killough 10/98
      
      monkeys = *demoptr++;
      
      {   // killough 10/98: a compatibility vector now
         int i;
         for(i = 0; i < COMP_TOTAL; ++i)
            comp[i] = *demoptr++;
      }

      G_SetCompatibility();
     
      // Options new to v2.04, etc.

      // haleyjd 05/23/04: autoaim is sync-critical
      if(full_demo_version >= make_full_version(331, 8))
         autoaim = *demoptr++;

      if(demo_version >= 333)
      {
         // haleyjd 04/06/05: allowmlook is sync-critical
         allowmlook = *demoptr++; 
      }

      if(full_demo_version >= make_full_version(340, 23))
      {
         // haleyjd 06/07/12: pitchedflight
         pitchedflight = (*demoptr ? true : false); 
         // Remember: ADD INCREMENT :)
      }
   }
   else  // defaults for versions <= 2.02
   {
      int i;  // killough 10/98: a compatibility vector now
      for(i = 0; i <= comp_zerotags; ++i)
         comp[i] = compatibility;

      G_SetCompatibility();
      
      monster_infighting = 1;           // killough 7/19/98
      
      monster_backing = 0;              // killough 9/8/98
      
      monster_avoid_hazards = 0;        // killough 9/9/98
      
      monster_friction = 0;             // killough 10/98
      
      help_friends = 0;                 // killough 9/9/98
      
      bfgtype = bfg_normal;             // killough 7/19/98
      
      dogs = 0;                         // killough 7/19/98
      dog_jumping = 0;                  // killough 10/98
      monkeys = 0;
      
      default_autoaim = autoaim; // FIXME: err?
      autoaim = 1;

      default_allowmlook = allowmlook; // FIXME: err??
      allowmlook = 0;

      pitchedflight = false;
   }
  
   return target;
}

//
// G_SetOldDemoOptions
//
// haleyjd 02/21/10: Configure everything to run for an old demo.
//
void G_SetOldDemoOptions()
{
   int i;

   vanilla_mode = true;

   // support -longtics when recording vanilla format demos
   longtics_demo = (M_CheckParm("-longtics") != 0);

   // set demo version appropriately
   if(longtics_demo)
      demo_version = DOOM_191_VERSION; // v1.91 (unofficial patch)
   else
      demo_version = 109;              // v1.9

   demo_subversion = 0;

   compatibility = 1;

   for(i = 0; i < COMP_TOTAL; ++i)
      comp[i] = 1;
      
   monsters_remember     = 0;
   variable_friction     = 0;
   weapon_recoil         = 0;
   allow_pushers         = 0;
   player_bobbing        = 1;
   demo_insurance        = 0;
   monster_infighting    = 1;
   monster_backing       = 0;
   monster_avoid_hazards = 0;
   monster_friction      = 0;
   help_friends          = 0;
   bfgtype               = bfg_normal;
   dogs                  = 0;
   dog_jumping           = 0;
   monkeys               = 0;
   default_autoaim       = autoaim;
   autoaim               = 1;
   default_allowmlook    = allowmlook;
   allowmlook            = 0;
   pitchedflight         = false; // haleyjd 06/07/12
}

//
// G_BeginRecordingOld
//
// haleyjd 02/21/10: Support recording of vanilla demos.
//
static void G_BeginRecordingOld()
{
   int i;

   // haleyjd 01/16/11: set again to ensure consistency
   G_SetOldDemoOptions();

   demo_p = demobuffer;

   *demo_p++ = demo_version;    
   *demo_p++ = gameskill;
   *demo_p++ = gameepisode;
   *demo_p++ = gamemap;
   if(GameType == gt_dm)
   {
      if(dmflags & DM_ITEMRESPAWN)
         *demo_p++ = 2;
      else
         *demo_p++ = 1;
   }
   else
      *demo_p++ = 0;
   *demo_p++ = respawnparm;
   *demo_p++ = fastparm;
   *demo_p++ = nomonsters;
   *demo_p++ = consoleplayer;

   for(i = 0; i < MAXPLAYERS; i++)
      *demo_p++ = playeringame[i];
}

/*
  haleyjd 06/17/01: new demo format changes

  1. The old version field is now always written as 255
  2. The signature has been changed to the null-term'd string ETERN
  3. The version and new subversion are written immediately following
     the signature
  4. cmd->updownangle is now recorded and read back appropriately

  12/14/01:
  5. gamemapname is recorded and will be used on loading demos

  Note that the demo-reading code still handles the "sacred" formats
  for DOOM, BOOM and MBF, so purists don't need to have heart attacks.
  However, only new Eternity-format demos can be written, and these
  will not be compatible with other engines.
*/
// NETCODE_FIXME -- DEMO_FIXME: Yet more demo writing.

void G_BeginRecording()
{
   int i;

   // haleyjd 02/21/10: -vanilla will record v1.9-format demos
   if(M_CheckParm("-vanilla"))
   {
      G_BeginRecordingOld();
      return;
   }
   
   demo_p = demobuffer;

   longtics_demo = true;
   
   //*demo_p++ = version;
   // haleyjd 06/17/01: always write 255 for Eternity-format demos,
   // since VERSION is now > 255 -- version setting is now handled
   // immediately after the new signature below.
   *demo_p++ = 255;
   
   // signature -- haleyjd: updated to use new Eternity signature
   *demo_p++ = eedemosig[0]; //0x1d;
   *demo_p++ = eedemosig[1]; //'M';
   *demo_p++ = eedemosig[2]; //'B';
   *demo_p++ = eedemosig[3]; //'F';
   *demo_p++ = eedemosig[4]; //0xe6;
   *demo_p++ = '\0';
   
   // haleyjd: write appropriate version and subversion numbers
   // write the WHOLE version number :P
   *demo_p++ =  version & 255;
   *demo_p++ = (version >> 8 ) & 255;
   *demo_p++ = (version >> 16) & 255;
   *demo_p++ = (version >> 24) & 255;
   
   *demo_p++ = subversion; // always ranges from 0 to 255
   
   // killough 2/22/98: save compatibility flag in new demos
   *demo_p++ = compatibility;       // killough 2/22/98
   
   vanilla_mode = false;
   demo_version = version;       // killough 7/19/98: use this version's id
   demo_subversion = subversion; // haleyjd 06/17/01
   
   *demo_p++ = gameskill;
   *demo_p++ = gameepisode;
   *demo_p++ = gamemap;
   *demo_p++ = GameType; // haleyjd 04/10/03
   *demo_p++ = consoleplayer;

   // haleyjd 04/14/03: save dmflags
   *demo_p++ = (unsigned char)(dmflags & 255);
   *demo_p++ = (unsigned char)((dmflags >> 8 ) & 255);
   *demo_p++ = (unsigned char)((dmflags >> 16) & 255);
   *demo_p++ = (unsigned char)((dmflags >> 24) & 255);
   
   // haleyjd 12/14/01: write gamemapname in new demos -- this will
   // enable demos to be recorded for arbitrarily-named levels
   for(i = 0; i < 8; i++)
      *demo_p++ = gamemapname[i];
   
   demo_p = G_WriteOptions(demo_p); // killough 3/1/98: Save game options
   
   for(i = 0; i < MAXPLAYERS; i++)
      *demo_p++ = playeringame[i];
   
   // killough 2/28/98:
   // We always store at least MIN_MAXPLAYERS bytes in demo, to
   // support enhancements later w/o losing demo compatibility
   
   for(; i < MIN_MAXPLAYERS; i++)
      *demo_p++ = 0;
}

//
// G_DeferedPlayDemo
//
void G_DeferedPlayDemo(const char *name)
{
   // haleyjd: removed SMMU cruft in attempt to fix
   if(defdemoname)
      efree(defdemoname);
   defdemoname = estrdup(name);
   gameaction = ga_playdemo;
}

//
// G_TimeDemo - sf
//
void G_TimeDemo(const char *name, bool showmenu)
{
   // haleyjd 10/19/01: hoo boy this had problems
   // It was using a variable called "name" from who knows where --
   // neither I nor Visual C++ could find a variable called name
   // that was in scope for this function -- now name is a
   // parameter, not s. I've also made some other adjustments.

   if(wGlobalDir.checkNumForNameNSG(name, lumpinfo_t::ns_demos) == -1)
   {
      C_Printf("%s: demo not found\n", name);
      return;
   }
   
   //G_StopDemo();         // stop any previous demos
   
   if(defdemoname)
      efree(defdemoname);
   defdemoname = estrdup(name);
   gameaction = ga_playdemo;
   singledemo = true;      // sf: moved from reloaddefaults
   
   singletics = true;
   timingdemo = true;      // show stats after quit   
}

//
// G_CheckDemoStatus
//
// Called after a death or level completion to allow demos to be cleaned up
// Returns true if a new demo loop action will take place
//
bool G_CheckDemoStatus()
{
   if(demorecording)
   {
      demorecording = false;
      if(demo_p)
      {
         *demo_p++ = DEMOMARKER;

         if(!M_WriteFile(demoname, demobuffer, demo_p - demobuffer))
         {
            // killough 11/98
            I_Error("Error recording demo %s: %s\n", demoname,
               errno ? strerror(errno) : "(Unknown Error)");
         }
      }
      
      efree(demobuffer);
      demobuffer = NULL;  // killough
      if(demo_p)
         I_ExitWithMessage("Demo %s recorded\n", demoname);
      else
      {
         I_ExitWithMessage("Demo %s not recorded: exited prematurely\n", 
            demoname);
      }
      return false;  // killough
   }

   if(timingdemo)
   {
      int endtime = i_haltimer.GetRealTime();

      // killough -- added fps information and made it work for longer demos:
      unsigned int realtics = endtime - starttime;
      I_Error("Timed %u gametics in %u realtics = %-.1f frames per second\n",
              (unsigned int)(gametic), realtics,
              (unsigned int)(gametic) * (double) TICRATE / realtics);
   }              

   if(demoplayback)
   {
      bool wassingledemo = singledemo; // haleyjd 01/08/12: must remember this

      // haleyjd 01/08/11: refactored so that stopping netdemos doesn't cause
      // access violations by leaving the game in "netgame" mode.
      Z_ChangeTag(demobuffer, PU_CACHE);
      G_ReloadDefaults();    // killough 3/1/98
      netgame = false;       // killough 3/29/98

      if(wassingledemo)
         C_SetConsole();
      else
         D_AdvanceDemo();

      return !wassingledemo;
   }

   return false;
}

void G_StopDemo()
{
   extern bool advancedemo;
   
   if(!demorecording && !demoplayback)
      return;
   
   G_CheckDemoStatus();
   advancedemo = false;
   if(gamestate != GS_CONSOLE)
      C_SetConsole();
}

// killough 1/22/98: this is a "Doom printf" for messages. I've gotten
// tired of using players->message=... and so I've added this doom_printf.
//
// killough 3/6/98: Made limit static to allow z_zone functions to call
// this function, without calling realloc(), which seems to cause problems.

// sf: changed to run console command instead

#define MAX_MESSAGE_SIZE 1024

void doom_printf(const char *s, ...)
{
   static char msg[MAX_MESSAGE_SIZE];
   va_list v;
   
   va_start(v, s);
   pvsnprintf(msg, sizeof(msg), s, v); // print message in buffer
   va_end(v);
   
   C_Puts(msg);  // set new message
   HU_PlayerMsg(msg);
}

//
// player_printf
//
// sf: printf to a particular player only
// to make up for the loss of player->msg = ...
//
void player_printf(const player_t *player, const char *s, ...)
{
   static char msg[MAX_MESSAGE_SIZE];
   va_list v;
   
   va_start(v, s);
   pvsnprintf(msg, sizeof(msg), s, v); // print message in buffer
   va_end(v);
   
   if(player == &players[consoleplayer])
   {
      C_Puts(msg);  // set new message
      HU_PlayerMsg(msg);
   }
}

extern camera_t intercam;

//
// G_CoolViewPoint
//
// Change to new viewpoint
//
void G_CoolViewPoint()
{
   int viewtype;
   int old_displayplayer = displayplayer;

   if(cooldemo == 2) // always followcam?
      viewtype = 2;
   else
      viewtype = M_Random() % 3;
   
   // pick the next player
   do
   { 
      displayplayer++; 
      if(displayplayer == MAXPLAYERS) 
         displayplayer = 0;
   } while(!playeringame[displayplayer]);
  
   if(displayplayer != old_displayplayer)
   {
      ST_Start();
      HU_Start();
      S_UpdateSounds(players[displayplayer].mo);
      P_ResetChasecam();      // reset the chasecam
   }

   if(players[displayplayer].health <= 0)
      viewtype = 1; // use chasecam when player is dead
  
   // turn off the chasecam?
   if(chasecam_active && viewtype != 1)
   {
      chasecam_active = false;
      P_ChaseEnd();
   }

   // turn off followcam
   P_FollowCamOff();
   if(camera == &followcam)
      camera = NULL;
  
   if(viewtype == 1)  // view from the chasecam
   {
      chasecam_active = true;
      P_ChaseStart();
   }
   else if(viewtype == 2) // follow camera view
   {
      fixed_t x, y;
      Mobj *spot = players[displayplayer].mo;

      P_LocateFollowCam(spot, x, y);
      P_SetFollowCam(x, y, spot);
      
      camera = &followcam;
   }
  
   // pick a random number of seconds until changing the viewpoint
   cooldemo_tics = (6 + M_Random() % 4) * TICRATE;
}

//==============================================================================

//
// Counts the total kills, items, secrets or whatever
//
static int G_totalPlayerParam(int player_t::*tally)
{
   int score = 0;
   for(int i = 0; i < MAXPLAYERS; ++i)
   {
      if(!playeringame[i])
         return score;
      score += players[i].*tally;
   }
   return score;
}

// Named this way to prevent confusion with similarly named variables
int G_TotalKilledMonsters()
{
   return G_totalPlayerParam(&player_t::killcount);
}
int G_TotalFoundItems()
{
   return G_totalPlayerParam(&player_t::itemcount);
}
int G_TotalFoundSecrets()
{
   return G_totalPlayerParam(&player_t::secretcount);
}

#if 0

//
// Small native functions
//


static cell AMX_NATIVE_CALL sm_exitlevel(AMX *amx, cell *params)
{
   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   G_ExitLevel();
   return 0;
}

static cell AMX_NATIVE_CALL sm_exitsecret(AMX *amx, cell *params)
{
   if(gamestate != GS_LEVEL)
   {
      amx_RaiseError(amx, SC_ERR_GAMEMODE | SC_ERR_MASK);
      return -1;
   }

   scriptSecret = true;
   G_SecretExitLevel();

   return 0;
}

//
// sm_startgame
//
// Implements StartGame(skill, levelname)
//
static cell AMX_NATIVE_CALL sm_startgame(AMX *amx, cell *params)
{
   int err;
   skill_t skill;
   char *levelname;

   // note: user view of skill is 1 - 5, internal view is 0 - 4
   skill = (skill_t)(params[1] - 1);

   // get level name
   if((err = SM_GetSmallString(amx, &levelname, params[2])) != AMX_ERR_NONE)
   {
      amx_RaiseError(amx, err);
      return 0;
   }

   G_DeferedInitNew(skill, levelname);

   Z_Free(levelname);

   return 0;
}

static cell AMX_NATIVE_CALL sm_gameskill(AMX *amx, cell *params)
{
   return gameskill + 1;
}

static cell AMX_NATIVE_CALL sm_gametype(AMX *amx, cell *params)
{
   return (cell)GameType;
}

AMX_NATIVE_INFO game_Natives[] =
{
   { "_ExitLevel",  sm_exitlevel },
   { "_ExitSecret", sm_exitsecret },
   { "_StartGame",  sm_startgame },
   { "_GameSkill",  sm_gameskill },
   { "_GameType",   sm_gametype },
   { NULL, NULL }
};

#endif

//----------------------------------------------------------------------------
//
// $Log: g_game.c,v $
// Revision 1.59  1998/06/03  20:23:10  killough
// fix v2.00 demos
//
// Revision 1.58  1998/05/16  09:16:57  killough
// Make loadgame checksum friendlier
//
// Revision 1.57  1998/05/15  00:32:28  killough
// Remove unnecessary crash hack
//
// Revision 1.56  1998/05/13  22:59:23  killough
// Restore Doom bug compatibility for demos, fix multiplayer status bar
//
// Revision 1.55  1998/05/12  12:46:16  phares
// Removed OVER_UNDER code
//
// Revision 1.54  1998/05/07  00:48:51  killough
// Avoid displaying uncalled for precision
//
// Revision 1.53  1998/05/06  15:32:24  jim
// document g_game.c, change externals
//
// Revision 1.52  1998/05/05  16:29:06  phares
// Removed RECOIL and OPT_BOBBING defines
//
// Revision 1.51  1998/05/04  22:00:33  thldrmn
// savegamename globalization
//
// Revision 1.50  1998/05/03  22:15:19  killough
// beautification, decls & headers, net consistency fix
//
// Revision 1.49  1998/04/27  17:30:12  jim
// Fix DM demo/newgame status, remove IDK (again)
//
// Revision 1.48  1998/04/25  12:03:44  jim
// Fix secret level fix
//
// Revision 1.46  1998/04/24  12:09:01  killough
// Clear player cheats before demo starts
//
// Revision 1.45  1998/04/19  19:24:19  jim
// Improved IWAD search
//
// Revision 1.44  1998/04/16  16:17:09  jim
// Fixed disappearing marks after new level
//
// Revision 1.43  1998/04/14  10:55:13  phares
// Recoil, Bobbing, Monsters Remember changes in Setup now take effect immediately
//
// Revision 1.42  1998/04/13  21:36:12  phares
// Cemented ESC and F1 in place
//
// Revision 1.41  1998/04/13  10:40:58  stan
// Now synch up all items identified by Lee Killough as essential to
// game synch (including bobbing, recoil, rngseed).  Commented out
// code in g_game.c so rndseed is always set even in netgame.
//
// Revision 1.40  1998/04/13  00:39:29  jim
// Fix automap marks carrying over thru levels
//
// Revision 1.39  1998/04/10  06:33:00  killough
// Fix -fast parameter bugs
//
// Revision 1.38  1998/04/06  04:51:32  killough
// Allow demo_insurance=2
//
// Revision 1.37  1998/04/05  00:50:48  phares
// Joystick support, Main Menu re-ordering
//
// Revision 1.36  1998/04/02  16:15:24  killough
// Fix weapons switch
//
// Revision 1.35  1998/04/02  04:04:27  killough
// Fix DM respawn sticking problem
//
// Revision 1.34  1998/04/02  00:47:19  killough
// Fix net consistency errors
//
// Revision 1.33  1998/03/31  10:36:41  killough
// Fix crash caused by last change, add new RNG options
//
// Revision 1.32  1998/03/28  19:15:48  killough
// fix DM spawn bug (Stan's fix)
//
// Revision 1.31  1998/03/28  17:55:06  killough
// Fix weapons switch bug, improve RNG while maintaining sync
//
// Revision 1.30  1998/03/28  15:49:47  jim
// Fixed merge glitches in d_main.c and g_game.c
//
// Revision 1.29  1998/03/28  05:32:00  jim
// Text enabling changes for DEH
//
// Revision 1.28  1998/03/27  21:27:00  jim
// Fixed sky bug for Ultimate DOOM
//
// Revision 1.27  1998/03/27  16:11:43  stan
// (SG) Commented out lines in G_ReloadDefaults that reset netgame and
//      deathmatch to zero.
//
// Revision 1.26  1998/03/25  22:51:25  phares
// Fixed headsecnode bug trashing memory
//
// Revision 1.25  1998/03/24  15:59:17  jim
// Added default_skill parameter to config file
//
// Revision 1.24  1998/03/23  15:23:39  phares
// Changed pushers to linedef control
//
// Revision 1.23  1998/03/23  03:14:27  killough
// Fix savegame checksum, net/demo consistency w.r.t. weapon switch
//
// Revision 1.22  1998/03/20  00:29:39  phares
// Changed friction to linedef control
//
// Revision 1.21  1998/03/18  16:16:47  jim
// Fix to idmusnum handling
//
// Revision 1.20  1998/03/17  20:44:14  jim
// fixed idmus non-restore, space bug
//
// Revision 1.19  1998/03/16  12:29:14  killough
// Add savegame checksum test
//
// Revision 1.18  1998/03/14  17:17:24  jim
// Fixes to deh
//
// Revision 1.17  1998/03/11  17:48:01  phares
// New cheats, clean help code, friction fix
//
// Revision 1.16  1998/03/09  18:29:17  phares
// Created separately bound automap and menu keys
//
// Revision 1.15  1998/03/09  07:09:20  killough
// Avoid realloc() in doom_printf(), fix savegame -nomonsters bug
//
// Revision 1.14  1998/03/02  11:27:45  killough
// Forward and backward demo sync compatibility
//
// Revision 1.13  1998/02/27  08:09:22  phares
// Added gamemode checks to weapon selection
//
// Revision 1.12  1998/02/24  08:45:35  phares
// Pushers, recoil, new friction, and over/under work
//
// Revision 1.11  1998/02/23  04:19:35  killough
// Fix Internal and v1.9 Demo sync problems
//
// Revision 1.10  1998/02/20  22:50:51  killough
// Fix doom_printf for multiplayer games
//
// Revision 1.9  1998/02/20  06:15:08  killough
// Turn turbo messages on in demo playbacks
//
// Revision 1.8  1998/02/17  05:53:41  killough
// Suppress "green is turbo" in non-net games
// Remove dependence on RNG for net consistency (intereferes with RNG)
// Use new RNG calling method, with keys assigned to blocks
// Friendlier savegame version difference message (instead of nothing)
// Remove futile attempt to make Boom v1.9-savegame-compatibile
//
// Revision 1.7  1998/02/15  02:47:41  phares
// User-defined keys
//
// Revision 1.6  1998/02/09  02:57:08  killough
// Make player corpse limit user-configurable
// Fix ExM8 level endings
// Stop 'q' from ending demo recordings
//
// Revision 1.5  1998/02/02  13:44:45  killough
// Fix doom_printf and CheckSaveGame realloc bugs
//
// Revision 1.4  1998/01/26  19:23:18  phares
// First rev with no ^Ms
//
// Revision 1.3  1998/01/24  21:03:07  jim
// Fixed disappearence of nomonsters, respawn, or fast mode after demo play or IDCLEV
//
// Revision 1.1.1.1  1998/01/19  14:02:54  rand
// Lee's Jan 19 sources
//
//----------------------------------------------------------------------------
