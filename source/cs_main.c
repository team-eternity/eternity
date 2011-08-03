// Emacs style mode select -*- C -*- vim:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2011 Charles Gunyon
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
//----------------------------------------------------------------------------
//
// DESCRIPTION:
//   Client/Server routines for both clients and servers.
//
//----------------------------------------------------------------------------

#include <stdio.h>
#include <string.h>

#include "acs_intr.h"
#include "am_map.h"
#include "c_io.h"
#include "c_net.h"
#include "c_runcmd.h"
#include "d_gi.h"
#include "d_main.h"
#include "d_player.h"
#include "d_ticcmd.h"
#include "doomdef.h"
#include "doomdata.h"
#include "doomstat.h"
#include "e_ttypes.h"
#include "g_dmflag.h"
#include "g_game.h"
#include "hu_frags.h"
#include "hu_stuff.h"
#include "i_system.h"
#include "info.h"
#include "m_argv.h"
#include "m_fixed.h"
#include "m_hash.h"
#include "m_qstr.h"
#include "p_chase.h"
#include "p_inter.h"
#include "p_map.h"
#include "p_mobj.h"
#include "p_portal.h"
#include "p_pspr.h"
#include "p_saveg.h"
#include "p_skin.h"
#include "p_user.h"
#include "r_main.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_video.h"
#include "w_wad.h"

#include <json/json.h>
#include <curl/curl.h>

#include "cs_team.h"
#include "cs_config.h"
#include "cs_hud.h"
#include "cs_main.h"
#include "cs_master.h"
#include "cs_position.h"
#include "cs_ctf.h"
#include "cs_demo.h"
#include "cs_wad.h"
#include "cl_cmd.h"
#include "cl_main.h"
#include "cl_pred.h"
#include "cl_spec.h"
#include "sv_main.h"
#include "sv_spec.h"
#include "sv_queue.h"

ENetHost *net_host;
ENetPeer *net_peer;
ENetAddress *server_address;
client_t *clients;
enet_uint32 start_time;
unsigned int cs_shooting_player = 0;

char *disconnection_strings[dr_max_reasons] = {
   "",
   "Server is full",
   "Invalid message received",
   "Latency limit exceeded",
   "Command flood"
};

char *network_message_names[nm_max_messages] = {
   "game state",
   "sync",
   "map started",
   "map completed",
   "auth result",
   "client init",
   "player command",
   "client status",
   "player spawned",
   "player info updated",
   "player weapon state",
   "player removed",
   "player touched special",
   "server message",
   "player message",
   "puff spawned",
   "blood spawned",
   "actor spawned",
   "actor position",
   "actor target",
   "actor tracer",
   "actor state",
   "actor attribute",
   "actor damaged",
   "actor killed",
   "actor removed",
   "line activated",
   "monster active",
   "monster awakened",
   "missile spawned",
   "missile exploded",
   "cube spawned",
   "special spawned",
   "special status",
   "special removed",
   "sector position",
   "tic finished",
};

unsigned int respawn_protection_time;
unsigned int death_time_limit;
unsigned int death_time_expired_action;
unsigned int friendly_damage_percentage;

extern boolean d_fastrefresh;
extern int weapon_preferences[2][NUMWEAPONS+1];
extern char *weapon_str[NUMWEAPONS];

void Handler_spectate(void);
void Handler_spectate_next(void);
void Handler_spectate_prev(void);

static void build_game_state(nm_gamestate_t *message)
{
   unsigned int i;

   message->message_type = nm_gamestate;
   message->map_number = cs_current_map_number;
   message->rngseed = rngseed;
   memcpy(message->flags, cs_flags, sizeof(flag_t) * team_color_max);
   memcpy(message->team_scores, team_scores, sizeof(int32_t) * team_color_max);
   for(i = 0; i < MAXPLAYERS; i++)
   {
      message->playeringame[i] = playeringame[i];
   }
   memcpy(&message->settings, cs_settings, sizeof(clientserver_settings_t));
}

// [CG] Pretty much stolen from the save game routines.
static size_t build_save_buffer(void)
{
   // [CG] This is a global from g_game.c that lets the save game buffer size
   //      checker reallocate the buffer properly.  Likewise, savebuffer and
   //      save_p are globals as well.
   savegamesize = 32768;
   save_p = savebuffer = malloc(savegamesize);

   P_NumberObjects(); // turn pointers into numbers
   P_ArchivePlayers();
   P_ArchiveWorld();
   P_ArchivePolyObjects(); // haleyjd 03/27/06
   P_ArchiveThinkers();
   P_ArchiveSpecials();
   P_ArchiveRNG();   // killough 1/18/98: save RNG information
   P_ArchiveMap();   // killough 1/22/98: save automap information
   P_ArchiveScripts();   // sf: archive scripts
   P_ArchiveSoundSequences();
   P_ArchiveButtons();
   P_DeNumberObjects(); // clear those numbers (that once were pointers).

   return (save_p - savebuffer);
}

static void indent_next_line(FILE *file, unsigned int indent_level)
{
   unsigned int i;

   fputc('\n', file);
   for(i = 0; i < indent_level; i++)
   {
      fputs("  ", file);
   }
}

// [CG] JSON-C has a to_file function (whatever it's called) but it does no
//      formatting.  In some cases this is fine or even preferred, but it's
//      unsuitable for situations where the user is ever expected to view or
//      edit the data.  For these situations, I wrote this naive pretty-printer
//      for JSON data.  There are a couple bugs, but it does output valid JSON,
//      and 40 minutes is all I'm willing to spend on this right now :) .
void CS_PrintJSONToFile(const char *filename, const char *json_data)
{
   char c;
   size_t i;
   unsigned int indent_level = 0;
   FILE *file = fopen(filename, "wb");

   if(file == NULL)
   {
      I_Error("CS_LoadConfig: Error writing configuration out to file.\n");
   }

   for(i = 0; i < strlen(json_data); i++)
   {
      c = *(json_data + i);
      if(c == '{' || c == '[')
      {
         fputc(c, file);
         indent_next_line(file, ++indent_level);
      }
      else if(c == '}' || c == ']')
      {
         indent_next_line(file, --indent_level);
         fputc(' ', file);
         fputc(c, file);
      }
      else if(c == ',')
      {
         fputc(',', file);
         indent_next_line(file, indent_level);
      }
      else
      {
         fputc(c, file);
      }
   }

   fclose(file);
}

boolean CS_CheckURI(char *uri)
{
   if((strncmp(uri, "http", 4)   == 0) || // [CG] Also works for "https".
      (strncmp(uri, "file", 4)   == 0) ||
      (strncmp(uri, "ftp", 4)    == 0) ||
      (strncmp(uri, "gopher", 6) == 0) ||
      (strncmp(uri, "scp", 4)    == 0) ||
      (strncmp(uri, "sftp", 4)   == 0) ||
      (strncmp(uri, "tftp", 4)   == 0) ||
      (strncmp(uri, "telnet", 4) == 0) ||
      (strncmp(uri, "dict", 4)   == 0))
   {
      return true;
   }

   return false;
}

void CS_DoWorldDone(void)
{
   unsigned int i;
   client_t *client;
   server_client_t *server_client;

   idmusnum = -1;
   hub_changelevel = false;
   CS_InitNew();
   AM_clearMarks();
   ACS_RunDeferredScripts();

   for(i = 0; i < MAXPLAYERS; i++)
   {
      if(playeringame[i])
      {
         client = &clients[i];
         client->spectating = true;
         if(CS_SERVER)
         {
            server_client = &server_clients[i];
            client->join_tic = gametic;
            server_client->commands_dropped = 0;
            server_client->last_command_run_index = 0;
            M_QueueFree(&server_client->commands);
            memset(
               server_client->positions, 0, MAX_POSITIONS * sizeof(position_t)
            );
         }
         CS_SpawnPlayerCorrectly(i, true);
      }
   }

   if(CS_SERVER)
   {
      sv_should_send_new_map = true;
   }
}

// [CG] For debugging.
void CS_PrintTime(void)
{
   return;
   printf("%d/%d", 0, enet_time_get());
}

void CS_FormatTime(char *formatted_time, unsigned int seconds)
{
   unsigned int hours, minutes;

   hours = seconds / 3600;
   seconds = seconds % 3600;
   minutes = seconds / 60;
   seconds = seconds % 60;

   if(hours > 0)
   {
      sprintf(formatted_time, "%d:%.2d:%.2d", hours, minutes, seconds);
   }
   else
   {
      sprintf(formatted_time, "%.2d:%.2d", minutes, seconds);
   }
}

void CS_FormatTicsAsTime(char *formatted_time, unsigned int tics)
{
   CS_FormatTime(formatted_time, tics / TICRATE);
}

void CS_SetDisplayPlayer(int playernum)
{
   int old_displayplayer = displayplayer;

   displayplayer = playernum;

   if(displayplayer != old_displayplayer)
   {
     ST_Start();
     HU_Start();
     S_UpdateSounds(players[displayplayer].mo);
     P_ResetChasecam();
   }

   P_FollowCamOff();
   if(camera == &followcam)
   {
     camera = NULL;
   }

   if(players[displayplayer].health <= 0)
   {
     // use chasecam when player is dead.
     P_ChaseStart();
   }
   else
   {
      P_ChaseEnd();
   }
}

char* CS_IPToString(int ip_address)
{
   char *address = calloc(16, sizeof(char));

   sprintf(address, "%d.%d.%d.%d",
      (ip_address      ) & 0xff,
      (ip_address >>  8) & 0xff,
      (ip_address >> 16) & 0xff,
      (ip_address >> 24) & 0xff
   );

   return address;
   
}

char* CS_VersionString(void)
{
   // [CG] 20 is probably good....
   char *version_string = calloc(20, sizeof(char));

   sprintf(
      version_string,
      "%d.%d.%d",
      version / 100,
      version % 100,
      SUBVERSION
   );

   return version_string;
}

char* CS_GetSHA1HashFile(char *path)
{
   hashdata_t newhash;
   size_t bytes_read = 0;
   size_t total_bytes_read = 0;
   unsigned int chunk_size = 512;
   const unsigned char chunk[chunk_size];
   FILE *f = fopen(path, "rb");

   if(f == NULL)
   {
      I_Error(
         "CS_GetSHA1HashFile: Error opening %s: %s.\n", path, strerror(errno)
      );
   }

   M_HashInitialize(&newhash, HASH_SHA1);
   while(1)
   {
      if((bytes_read = fread((void *)chunk, sizeof(char), chunk_size, f)));
      {
         M_HashData(&newhash, (const uint8_t *)chunk, (uint32_t)bytes_read);
      }
      total_bytes_read += bytes_read;

      if(feof(f))
      {
         break;
      }

      if(ferror(f))
      {
         I_Error(
            "CS_GetSHA1HashFile: Error computing SHA-1 hash for %s: %s.\n",
            path,
            strerror(errno)
         );
      }
   }
   M_HashWrapUp(&newhash);

   if(newhash.gonebad)
   {
      I_Error(
         "CS_GetSHA1HashFile: Unknown error computing SHA-1 hash for %s.\n",
         path
      );
   }

   return M_HashDigestToStr(&newhash);
}

char* CS_GetSHA1Hash(const char *input, size_t input_size)
{
   hashdata_t newhash;

   M_HashInitialize(&newhash, HASH_SHA1);
   M_HashData(&newhash, (const uint8_t *)input, (uint32_t)input_size);
   M_HashWrapUp(&newhash);

   if(newhash.gonebad)
   {
      I_Error("Error computing SHA-1 hash.\n");
   }

   return M_HashDigestToStr(&newhash);
}

void CS_SetPlayerName(player_t *player, char *name)
{
   boolean initializing_name = false;

   if(strlen(player->name) == 0)
   {
      initializing_name = true;
   }

   if(strlen(name) >= MAX_NAME_SIZE)
   {
      if(initializing_name)
      {
         I_Error("Name size exceeds limit (%d).\n", MAX_NAME_SIZE);
      }
      else
      {
         doom_printf("Name size exceeds limit (%d).\n", MAX_NAME_SIZE);
         return;
      }
   }

   strncpy(player->name, name, MAX_NAME_SIZE);
}

void CS_InitPlayer(int playernum)
{
   memset(&players[playernum], 0, sizeof(player_t));

   players[playernum].colormap = default_colour;
   players[playernum].pclass =
      E_PlayerClassForName(GameModeInfo->defPClassName);
   players[playernum].skin = P_GetDefaultSkin(&players[playernum]);

   if(playernum == consoleplayer)
   {
      if(CS_SERVER)
      {
         CS_SetPlayerName(&players[playernum], SERVER_NAME);
      }
      else
      {
         CS_SetPlayerName(&players[playernum], default_name);
      }
   }
   else
   {
      CS_SetPlayerName(&players[playernum], "");
   }
}

void CS_InitPlayers(void)
{
   int i;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      CS_InitPlayer(i);
   }
}

void CS_ZeroClient(int clientnum)
{
   client_t *client = &clients[clientnum];
   server_client_t *sc;

   memset(client, 0, sizeof(client_t));
   client->spectating = true;

   if(CS_SERVER && server_clients != NULL)
   {
      // [CG] Clear the server client too.
      sc = &server_clients[clientnum];
      sc->connect_id = 0;
      memset(&sc->address, 0, sizeof(ENetAddress));
      sc->synchronized = false;
      sc->auth_level = cs_auth_none;
      sc->last_auth_attempt = 0;
      sc->commands_dropped = 0;
      sc->last_command_run_index = 0;
      M_QueueFree(&sc->commands);
      memset(sc->positions, 0, MAX_POSITIONS * sizeof(position_t));
      memcpy(
         sc->weapon_preferences,
         weapon_preferences[1],
         (NUMWEAPONS + 1) * sizeof(int)
      );
#if _UNLAG_DEBUG
      sc->ghost = NULL;
#endif
   }
}

void CS_ZeroClients(void)
{
   int i;
   server_client_t *server_client;

   for(i = 0; i < MAXPLAYERS; i++)
   {
      CS_ZeroClient(i);
   }

   if(CS_SERVER)
   {
      // [CG] Copy our configuration information into our own server client so
      //      it can be restored later.
      server_client = &server_clients[consoleplayer];
      server_client->options.player_bobbing = player_bobbing;
      server_client->options.doom_weapon_toggles = doom_weapon_toggles;
      server_client->options.autoaim = autoaim;
      server_client->options.weapon_speed = weapon_speed;
   }
}

size_t CS_BuildGameState(int playernum, byte **buffer)
{
   nm_gamestate_t *message;
   size_t state_size;

   // [CG] Build the generic game state stuff like flags, scores, etc.
   message = malloc(sizeof(nm_gamestate_t));
   build_game_state(message);

   // [CG] Build the save game and save its size.
   state_size = build_save_buffer();

   message->state_size = state_size;
   message->player_number = playernum;

   // [CG] Create the game state buffer, copy the message and save game into
   //      it.
   *buffer = malloc(sizeof(nm_gamestate_t) + state_size);
   memcpy(*buffer, message, sizeof(nm_gamestate_t));
   free(message);
   memcpy(*buffer + sizeof(nm_gamestate_t), savebuffer, state_size);

   // [CG] We're done with the savebuffer, free and NULL it.
   free(savebuffer);
   savebuffer = save_p = NULL;

   return sizeof(nm_gamestate_t) + state_size;
}

void CS_ProcessPlayerCommand(int playernum)
{
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];
   weapontype_t newweapon;
   mapthing_t *spawn_point;

   if(player->playerstate == PST_DEAD)
   {
      return;
   }

   // haleyjd 04/03/05: new yshear code
   if(!allowmlook || ((dmflags2 & dmf_allow_freelook) == 0))
   {
      player->pitch = 0;
   }
   else if(player->cmd.look)
   {
      // test for special centerview value
      if(player->cmd.look == -32768)
      {
         player->pitch = 0;
      }
      else
      {
         player->pitch -= player->cmd.look << 16;
         // [CG] Normal lower look range is 32 degrees, but "NS" range is 56.
         if(comp[comp_mouselook])
         {
            if(player->pitch < -ANGLE_1 * 32)
            {
               player->pitch = -ANGLE_1 * 32;
            }
            else if(player->pitch > ANGLE_1 * 32)
            {
               player->pitch = ANGLE_1 * 32;
            }
         }
         else
         {
            if(player->pitch < -ANGLE_1 * 32)
            {
               player->pitch = -ANGLE_1 * 32;
            }
            else if(player->pitch > ANGLE_1 * 56)
            {
               player->pitch = ANGLE_1 * 56;
            }
         }
      }
   }

   // haleyjd: count down jump timer
   if(player->jumptime)
   {
      player->jumptime--;
   }

   // Move around.
   // Reactiontime is used to prevent movement
   //  for a bit after a teleport.

   if(player->mo->reactiontime)
   {
      player->mo->reactiontime--;
   }
   else
   {
      P_MovePlayer(player);

      // Handle actions   -- joek 12/22/07
      
      if((dmflags2 & dmf_allow_jump) != 0)
      {
         if(player->cmd.actions & AC_JUMP)
         {
            if((player->mo->z == player->mo->floorz || 
                (player->mo->intflags & MIF_ONMOBJ)) && !player->jumptime)
            {
               // PCLASS_FIXME: make jump height pclass property
               player->mo->momz += 8 * FRACUNIT;
               player->mo->intflags &= ~MIF_ONMOBJ;
               player->jumptime = 18;
            }
         }
      }
   }

   P_CalcHeight(player); // Determines view height and bobbing

   // Check for weapon change.
   
   // A special event has no other buttons.

   if(player->cmd.buttons & BT_SPECIAL)
   {
      player->cmd.buttons = 0;
   }

   if(!cl_predicting)
   {
      if(player->cmd.buttons & BT_CHANGE)
      {
         // The actual changing of the weapon is done
         //  when the weapon psprite can do it
         //  (read: not in the middle of an attack).

         newweapon = (player->cmd.buttons & BT_WEAPONMASK) >> BT_WEAPONSHIFT;
         
         // killough 3/22/98: For demo compatibility we must perform the fist
         // and SSG weapons switches here, rather than in G_BuildTiccmd(). For
         // other games which rely on user preferences, we must use the latter.

         // WEAPON_FIXME: bunch of crap.

         if(demo_compatibility)
         { 
            // compatibility mode -- required for old demos -- killough
            if(newweapon == wp_fist && player->weaponowned[wp_chainsaw] &&
               (player->readyweapon != wp_chainsaw ||
                !player->powers[pw_strength]))
            {
               newweapon = wp_chainsaw;
            }
            if(enable_ssg &&
               newweapon == wp_shotgun &&
               player->weaponowned[wp_supershotgun] &&
               player->readyweapon != wp_supershotgun)
            {
               newweapon = wp_supershotgun;
            }
         }

         // killough 2/8/98, 3/22/98 -- end of weapon selection changes

         // WEAPON_FIXME: shareware availability -> weapon property

         if(player->weaponowned[newweapon] && newweapon != player->readyweapon)
         {
            // Do not go to plasma or BFG in shareware, even if cheated.
            if((newweapon != wp_plasma && newweapon != wp_bfg)
               || (GameModeInfo->id != shareware))
            {
               player->pendingweapon = newweapon;
               if(CS_SERVER)
               {
                  SV_BroadcastPlayerScalarInfo(playernum, ci_pending_weapon);
               }
            }
         }
      }
   }

   if(player->cmd.buttons & BT_USE)
   {
      if(!player->usedown)
      {
         player->usedown = true;
         if(!client->spectating)
         {
            P_UseLines(player);
         }
         else if(CS_SERVER && SV_HandleJoinRequest(playernum))
         {
            spawn_point = CS_SpawnPlayerCorrectly(playernum, false);
            SV_BroadcastPlayerSpawned(spawn_point, playernum);
         }
      }
   }
   else
   {
      player->usedown = false;
   }

   if(!cl_predicting)
   {
      // cycle psprites
      P_MovePsprites(player);
   }
}

void CS_ApplyCommandButtons(ticcmd_t *cmd)
{
   int newweapon;

   if((!demo_compatibility && players[consoleplayer].attackdown &&
       !P_CheckAmmo(&players[consoleplayer])) || action_nextweapon)
   {
      newweapon = P_SwitchWeapon(&players[consoleplayer]); // phares
   }
   else
   {                                 // phares 02/26/98: Added gamemode checks
      newweapon =
        action_weapon1 ? wp_fist :    // killough 5/2/98: reformatted
        action_weapon2 ? wp_pistol :
        action_weapon3 ? wp_shotgun :
        action_weapon4 ? wp_chaingun :
        action_weapon5 ? wp_missile :
        action_weapon6 && GameModeInfo->id != shareware ? wp_plasma :
        action_weapon7 && GameModeInfo->id != shareware ? wp_bfg :
        action_weapon8 ? wp_chainsaw :
        action_weapon9 && enable_ssg ? wp_supershotgun :
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
         const player_t *player = &players[consoleplayer];

         // only select chainsaw from '1' if it's owned, it's
         // not already in use, and the player prefers it or
         // the fist is already in use, or the player does not
         // have the berserker strength.

         if(newweapon==wp_fist && player->weaponowned[wp_chainsaw] &&
            player->readyweapon!=wp_chainsaw &&
            (player->readyweapon==wp_fist ||
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
            player->weaponowned[wp_supershotgun] &&
            (!player->weaponowned[wp_shotgun] ||
             player->readyweapon == wp_shotgun ||
             (player->readyweapon != wp_supershotgun &&
              P_WeaponPreferred(wp_supershotgun, wp_shotgun))))
         {
            newweapon = wp_supershotgun;
         }
      }
      // killough 2/8/98, 3/22/98 -- end of weapon selection changes
   }

   // haleyjd 03/06/09: next/prev weapon actions
   if(action_weaponup)
      newweapon = P_NextWeapon(&players[consoleplayer]);
   else if(action_weapondown)
      newweapon = P_PrevWeapon(&players[consoleplayer]);
}

void CS_PlayerThink(int playernum)
{
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];

   if(!playeringame[playernum])
   {
      return;
   }

   // killough 2/8/98, 3/21/98:
   // (this code is necessary despite questions raised elsewhere in a comment)
   if(player->cheats & CF_NOCLIP)
   {
      player->mo->flags |= MF_NOCLIP;
   }
   else
   {
      player->mo->flags &= ~MF_NOCLIP;
   }

   // chain saw run forward
   if(player->mo->flags & MF_JUSTATTACKED)
   {
      player->cmd.angleturn = 0;
      player->cmd.forwardmove = 0xc800/512;
      player->cmd.sidemove = 0;
      player->mo->flags &= ~MF_JUSTATTACKED;
   }

   if(player->playerstate == PST_DEAD)
   {
      P_DeathThink(player);
      if(CS_SERVER)
      {
         SV_RunPlayerCommands(playernum);
         P_MobjThinker(player->mo);
      }
      else if(CS_CLIENT)
      {
         player->cmd.forwardmove = 0;
         player->cmd.sidemove = 0;
         player->cmd.look = 0;
         player->cmd.angleturn = 0;
         if(player->cmd.buttons & BT_USE)
         {
            player->cmd.buttons = BT_USE;
         }
         else
         {
            player->cmd.buttons = 0;
         }
         player->cmd.actions = 0;
         P_MobjThinker(player->mo);
      }
      return;
   }

   // [CG] Begin command processing.  See the netcode discussion in notes for
   //      more information.

   if(playernum == consoleplayer)
   {
      CS_ProcessPlayerCommand(playernum);
   }
   else if(CS_SERVER && !SV_RunPlayerCommands(playernum))
   {
      // [CG] The client was disconnected for some reason, so quit thinking
      //      on it.
      return;
   }

   // haleyjd: are we falling? might need to scream :->
   // [CG] Spectators don't scream.
   if(!client->spectating)
   {
      if(!comp[comp_fallingdmg] && demo_version >= 329)
      {  
         if(player->mo->momz >= 0)
         {
            player->mo->intflags &= ~MIF_SCREAMED;
         }

         if(player->mo->momz <= -35*FRACUNIT && 
            player->mo->momz >= -40*FRACUNIT &&
            !(player->mo->intflags & MIF_SCREAMED))
         {
            player->mo->intflags |= MIF_SCREAMED;
            S_StartSound(player->mo, GameModeInfo->playerSounds[sk_plfall]);
         }
      }
   }

   // Determine if there's anything about the sector you're in that's
   // going to affect you, like painful floors.

   if(P_SectorIsSpecial(player->mo->subsector->sector))
   {
      P_PlayerInSpecialSector(player);
   }

   // haleyjd 08/23/05: terrain-based effects
   P_PlayerOnSpecialFlat(player);

   // haleyjd: Heretic current specials
   P_HereticCurrent(player);

   // Strength counts up to diminish fade.
   if(player->powers[pw_strength])
   {
      player->powers[pw_strength]++;
   }

   // killough 1/98: Make idbeholdx toggle:
   if(player->powers[pw_invulnerability] > 0) // killough
   {
      player->powers[pw_invulnerability]--;
   }

   if(player->powers[pw_invisibility] > 0)
   {
      if(!--player->powers[pw_invisibility] )
      {
         player->mo->flags &= ~MF_SHADOW;
      }
   }

   if(player->powers[pw_infrared] > 0) // killough
   {
      player->powers[pw_infrared]--;
   }

   if(player->powers[pw_ironfeet] > 0) // killough
   {
      player->powers[pw_ironfeet]--;
   }

   if(player->powers[pw_ghost] > 0) // haleyjd
   {
      if(!--player->powers[pw_ghost])
      {
         player->mo->flags3 &= ~MF3_GHOST;
      }
   }

   if(player->powers[pw_totalinvis] > 0) // haleyjd
   {
      player->mo->flags2 &= ~MF2_DONTDRAW; // flash
      player->powers[pw_totalinvis]--;  
      player->mo->flags2 |=               
         player->powers[pw_totalinvis] &&
         (player->powers[pw_totalinvis] > 4*32 ||
          player->powers[pw_totalinvis] & 8)
          ? MF2_DONTDRAW : 0;
   }

   if(player->damagecount)
   {
      player->damagecount--;
   }

   if(player->bonuscount)
   {
      player->bonuscount--;
   }

   // Handling colormaps.
   // killough 3/20/98: reformat to terse C syntax
   // sf: removed MBF beta stuff
   player->fixedcolormap = 
    (player->powers[pw_invulnerability] > 4*32 ||    
     player->powers[pw_invulnerability] & 8) ? INVERSECOLORMAP :
    (player->powers[pw_infrared] > 4*32 || player->powers[pw_infrared] & 8);

   // haleyjd 01/21/07: clear earthquake flag before running quake thinkers
   //                   later
   player->quake = 0;

   P_MobjThinker(player->mo);
}

void CS_PlayerTicker(int playernum)
{
   if(playeringame[playernum])
   {
      if(CS_SERVER)
      {
         SV_LoadClientOptions(playernum);
         CS_PlayerThink(playernum);
         SV_RestoreServerOptions();
      }
      else if(playernum == consoleplayer &&
              (cl_enable_prediction || clients[playernum].spectating))
      {
         CL_Predict(cl_current_world_index, cl_current_world_index + 1, false);
      }
      else if(CS_DEMO)
      {
         CS_PlayerThink(playernum);
      }
      else
      {
         client_t *client = &clients[playernum];
         player_t *player = &players[playernum];
         mobj_t   *actor  = player->mo;

         if(client->spectating)
         {
            // [CG] Don't think on spectators clientside.
            return;
         }

         // [CG] Here we do some things clientside that are inconsequential to
         //      the game world, but are necessary so that all players (local
         //      or otherwise) "act" correctly.

         // [CG] Move the player's weapon sprite.
         P_MovePsprites(player);

         // [CG] These shouldn't wait on server messages because they're
         //      annoying when they linger.
         if(player->damagecount)
         {
            player->damagecount--;
         }
         if(player->bonuscount)
         {
            player->bonuscount--;
         }

         // [CG] Make sounds when a player hits the floor.
         if(client->floor_status)
         {
            if(actor->health > 0)
            {
               if(!comp[comp_fallingdmg])
               {
                  // haleyjd: new features -- feet sound for normal hits,
                  //          grunt for harder, falling damage for worse
                  if(actor->momz < -23 * FRACUNIT)
                  {
                     if(!actor->player->powers[pw_invulnerability] &&
                        !(actor->player->cheats & CF_GODMODE))
                     {
                        // [CG] TODO: Figure out the if statement so it can be
                        //            removed.
                        // P_FallingDamage(actor->player);
                     }
                     else
                     {
                        S_StartSound(
                           actor, GameModeInfo->playerSounds[sk_oof]
                        );
                     }
                  }
                  else if(actor->momz < -12 * FRACUNIT)
                  {
                     S_StartSound(actor, GameModeInfo->playerSounds[sk_oof]);
                  }
                  else if((client->floor_status == cs_fs_hit_on_thing) ||
                          !E_GetThingFloorType(actor)->liquid)
                  {
                     S_StartSound(
                        actor, GameModeInfo->playerSounds[sk_plfeet]
                     );
                  }
               }
               else if((client->floor_status == cs_fs_hit_on_thing) ||
                       !E_GetThingFloorType(actor)->liquid)
               {
                  S_StartSound(actor, GameModeInfo->playerSounds[sk_oof]);
               }
            }
            client->floor_status = cs_fs_none;
         }
      }
   }
}

boolean CS_WeaponPreferred(int playernum, weapontype_t weapon_one,
                                          weapontype_t weapon_two)
{
   unsigned int p1, p2;
   server_client_t *server_client;

   weapon_one++;
   weapon_two++;

   if(CS_SERVER)
   {
      server_client = &server_clients[playernum];

      for(p1 = 0; p1 < NUMWEAPONS; p1++)
      {
         if(server_client->weapon_preferences[p1] == weapon_one)
         {
            break;
         }
      }

      for(p2 = 0; p2 < NUMWEAPONS; p2++)
      {
         if(server_client->weapon_preferences[p2] == weapon_two)
         {
            break;
         }
      }
   }
   else
   {
      for(p1 = 0; p1 < NUMWEAPONS; p1++)
      {
         if(weapon_preferences[0][p1] == weapon_one)
         {
            break;
         }
      }

      for(p2 = 0; p2 < NUMWEAPONS; p2++)
      {
         if(weapon_preferences[0][p2] == weapon_two)
         {
            break;
         }
      }
   }

   if(p1 < p2)
   {
      return true;
   }
   return false;
}

void CS_HandleSpectateKey(event_t *ev)
{
   boolean spectating = clients[consoleplayer].spectating;

   if(CS_CLIENT && !spectating && ev->type == ev_keydown)
   {
      Handler_spectate();
   }
}

void CS_HandleSpectatePrevKey(event_t *ev)
{
   if(clients[consoleplayer].spectating && ev->type == ev_keydown)
   {
      Handler_spectate_prev();
   }
}

void CS_HandleSpectateNextKey(event_t *ev)
{
   if(clients[consoleplayer].spectating && ev->type == ev_keydown)
   {
      Handler_spectate_next();
   }
}

void CS_HandleFlushPacketBufferKey(event_t *ev)
{
   if(CS_CLIENT && net_peer != NULL && ev->type == ev_keydown)
   {
      cl_flush_packet_buffer = true;
   }
}

void CS_HandleUpdatePlayerInfoMessage(nm_playerinfoupdated_t *message)
{
   char *buffer;
   mapthing_t *spawn_point;
   server_client_t *server_client;
   boolean respawn_player = false;
   int playernum = message->player_number;
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];

   if(CS_SERVER)
   {
      server_client = &server_clients[playernum];
   }

   playeringame[playernum] = true;

   // [CG] Now dealing with client string info.
   if(message->info_type == ci_name ||
      message->info_type == ci_skin ||
      message->info_type == ci_class)
   {
      if(message->string_size > MAX_STRING_SIZE)
      {
         doom_printf(
            "CS_HUPIM: string size exceeds limit (%d)\n", MAX_STRING_SIZE
         );
         return;
      }
      buffer = ((char *)message) + sizeof(nm_playerinfoupdated_t);
      if(message->info_type == ci_name)
      {
         if(strlen(buffer) >= MAX_NAME_SIZE)
         {
            doom_printf("Name update message exceeds maximum name length.\n");
            return;
         }

         if(!strlen(buffer))
         {
            if(CS_SERVER)
            {
               SV_SendMessage(playernum, "Cannot blank your name.\n");
            }
            return;
         }

         if(playernum != consoleplayer &&
            strncmp(player->name, buffer, MAX_NAME_SIZE) == 0)
         {
            // [CG] If the name is unchanged, don't do anything.
            return;
         }

         if(strlen(player->name) > 0)
         {
            // [CG] Only print out a notice if the original name wasn't blank.
            if(playernum == consoleplayer)
            {
               doom_printf("You are now known as %s.", buffer);
            }
            else
            {
               doom_printf("%s is now known as %s.", player->name, buffer);
            }
         }
         else if(playernum == consoleplayer)
         {
            doom_printf("Connected as %s.", buffer);
         }
         else
         {
            doom_printf("%s connected.", buffer);
         }
         CS_SetPlayerName(player, buffer);
      }
      else if(message->info_type == ci_skin)
      {
         CS_SetSkin(buffer, playernum);
      }
      else if(message->info_type == ci_class)
      {
         // [CG] I'm not even sure you're supposed to be able to do this,
         //      but let's give it a try.
         player->pclass = E_PlayerClassForName(buffer);
      }
      if(CS_SERVER)
      {
         SV_BroadcastPlayerStringInfo(playernum, message->info_type);
      }
      return;
   }

   // [CG] Now dealing with client array info.
   if(message->info_type == ci_frags         ||
      message->info_type == ci_power_enabled ||
      message->info_type == ci_owns_card     ||
      message->info_type == ci_owns_weapon   ||
      message->info_type == ci_ammo_amount   ||
      message->info_type == ci_max_ammo      ||
      message->info_type == ci_pwo)
   {
      if(CS_CLIENT && message->info_type == ci_frags)
      {
         player->frags[message->array_index] = message->int_value;
         HU_FragsUpdate();
      }
      else if(CS_CLIENT && message->info_type == ci_power_enabled)
      {
         player->powers[message->array_index] = message->int_value;
      }
      else if(CS_CLIENT && message->info_type == ci_owns_card)
      {
         player->cards[message->array_index] = message->boolean_value;
      }
      else if(CS_CLIENT && message->info_type == ci_owns_weapon)
      {
         player->weaponowned[message->array_index] = message->boolean_value;
      }
      else if(CS_CLIENT && message->info_type == ci_ammo_amount)
      {
         player->ammo[message->array_index] = message->int_value;
      }
      else if(CS_CLIENT && message->info_type == ci_max_ammo)
      {
         player->maxammo[message->array_index] = message->int_value;
      }
      else if(CS_SERVER && message->info_type == ci_pwo)
      {
         SV_SetWeaponPreference(
            playernum, message->array_index, message->int_value
         );
      }
      return;
   }

   // [CG] Now dealing with scalars.
   if(message->info_type == ci_team)
   {
      if(client->team == message->int_value)
      {
         // [CG] Don't do anything if the value hasn't actually changed.
         return;
      }

      if(!CS_TEAMS_ENABLED)
      {
         // [CG] If this isn't a team game, then we don't care about client
         //      teams.
         return;
      }

      if(message->int_value < team_color_none ||
         message->int_value > team_color_max)
      {
         if(CS_SERVER)
         {
            SV_SendMessage(
               playernum, "Invalid team %d.\n", message->int_value
            );
         }
         else if(CS_CLIENT)
         {
            // [CG] TODO: Really if we somehow get an invalid team from the
            //            server it means something else is broken, so this
            //            should probably be handled with more than just an
            //            error message.
            doom_printf("Invalid team %d.\n", message->int_value);
         }
         return;
      }

      if(CS_SERVER && message->int_value == team_color_none)
      {
         // [CG] In a team game, you can't switch to "no team at all", so
         //      default to the red team here.
         message->int_value = team_color_red;
      }

      client->team = message->int_value;
      if(client->queue_level == ql_none)
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now watching on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now waiting on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }
      }
      else if(client->queue_level == ql_waiting)
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now waiting on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now waiting on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }
      }
      else
      {
         if(playernum == consoleplayer)
         {
            doom_printf(
               "You are now on the %s team.",
               team_color_names[client->team]
            );
         }
         else
         {
            doom_printf(
               "%s is now on the %s team.",
               player->name,
               team_color_names[client->team]
            );
         }
         if(CS_SERVER)
         {
            respawn_player = true;
         }
      }
   }
   else if(message->info_type == ci_spectating)
   {
      if(client->spectating == message->boolean_value)
      {
         // [CG] Don't do anything if the value hasn't actually changed.
         return;
      }

      if(!message->boolean_value)
      {
         // [CG] The case where the player is attempting to join the game is
         //      handled in CS_ProcessPlayerCommand serverside and
         //      CL_HandlePlayerSpawned clientside.  Receiving a false update
         //      for ci_spectating means the sending client is bugged somehow.
         if(CS_SERVER)
         {
            SV_SendMessage(playernum, "Invalid join request.\n");
         }
         doom_printf(
            "Received invalid spectating value from %d.\n", playernum
         );
         return;
      }

      // [CG] At this point, the player is attempting to spectate.
      if(CS_SERVER)
      {
         client->spectating = true;
         player->frags[playernum]++; // [CG] Spectating costs a frag.
         SV_BroadcastPlayerArrayInfo(playernum, ci_frags, playernum);
         // printf("Spectating costs a frag: %d.\n", player->frags[playernum]);
         HU_FragsUpdate();
         SV_PutPlayerAtQueueEnd(playernum);
         respawn_player = true;
      }
      else
      {
         if(playernum == consoleplayer)
         {
            doom_printf("You are now spectating.");
            CS_UpdateQueueMessage();
         }
         else
         {
            doom_printf("%s is now spectating.\n", player->name);
         }
      }
   }
   else if(message->info_type == ci_colormap)
   {
      player->colormap = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_ready_weapon)
   {
      if(playernum != consoleplayer)
      {
         // [CG] We always know what our own ready and pending weapons are.
         player->readyweapon = message->int_value;
      }
   }
   else if(CS_CLIENT && message->info_type == ci_pending_weapon)
   {
      if(playernum != consoleplayer)
      {
         // [CG] We always know what our own ready and pending weapons are.
         player->pendingweapon = message->int_value;
      }
   }
   else if(CS_CLIENT && message->info_type == ci_kill_count)
   {
      player->killcount = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_item_count)
   {
      player->itemcount = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_secret_count)
   {
      player->secretcount = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_cheats)
   {
      player->cheats = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_health)
   {
      player->health = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_armor_points)
   {
      player->armorpoints = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_armor_type)
   {
      player->armortype = message->int_value;
   }
   else if(CS_CLIENT && message->info_type == ci_owns_backpack)
   {
      player->backpack = message->boolean_value;
   }
   else if(CS_CLIENT && message->info_type == ci_did_secret)
   {
      player->didsecret = message->boolean_value;
   }
   else if(CS_CLIENT && message->info_type == ci_queue_level)
   {
      client->queue_level = message->int_value;
      if(playernum == consoleplayer)
      {
         CS_UpdateQueueMessage();
      }
   }
   else if(CS_CLIENT && message->info_type == ci_queue_position)
   {
      client->queue_position = message->int_value;
      if(playernum == consoleplayer)
      {
         CS_UpdateQueueMessage();
      }
   }
   else if(message->info_type == ci_wsop)
   {
      if(CS_SERVER)
      {
         server_client->weapon_switch_on_pickup = message->int_value;
      }
   }
   else if(message->info_type == ci_asop)
   {
      if(CS_SERVER)
      {
         server_client->ammo_switch_on_pickup = message->int_value;
      }
   }
   else if(message->info_type == ci_bobbing)
   {
      if(CS_SERVER)
      {
         server_client->options.player_bobbing = message->boolean_value;
      }
   }
   else if(message->info_type == ci_weapon_toggle)
   {
      if(CS_SERVER)
      {
         server_client->options.doom_weapon_toggles = message->boolean_value;
      }
   }
   else if(message->info_type == ci_autoaim)
   {
      if(CS_SERVER)
      {
         server_client->options.autoaim = message->boolean_value;
      }
   }
   else if(message->info_type == ci_weapon_speed)
   {
      if(CS_SERVER)
      {
         server_client->options.weapon_speed = message->int_value;
      }
   }
   else
   {
      doom_printf(
         "CS_HUPIM: received unknown info type %d\n", message->info_type
      );
      return;
   }
   if(CS_SERVER)
   {
      CS_PrintTime();
      printf(".\n");
      SV_BroadcastPlayerScalarInfo(playernum, message->info_type);
      if(respawn_player)
      {
         spawn_point = CS_SpawnPlayerCorrectly(playernum, true);
         SV_BroadcastPlayerSpawned(spawn_point, playernum);
      }
   }
}

void CS_DisconnectPeer(ENetPeer *peer, enet_uint32 reason)
{
   if(peer == NULL)
   {
      // [CG] Peer's already disconnected, so don't worry about this stuff.
      return;
   }
   enet_peer_disconnect(peer, reason);
   enet_peer_reset(peer);
}

void CS_RemovePlayer(int playernum)
{
   mobj_t *flash;
   player_t *player = &players[playernum];

   CS_DropFlag(playernum);

   if(gamestate == GS_LEVEL && player->mo != NULL)
   {
      flash = P_SpawnMobj(
         player->mo->x,
         player->mo->y,
         player->mo->z + GameModeInfo->teleFogHeight,
         GameModeInfo->teleFogType
      );
      flash->momx = player->mo->momx;
      flash->momy = player->mo->momy;

      if((!clients[playernum].spectating) && drawparticles)
      {
         flash->flags2 |= MF2_DONTDRAW;
         P_DisconnectEffect(player->mo);
      }
      P_RemoveMobj(player->mo);
      player->mo = NULL;
   }

   CS_ZeroClient(playernum);
   playeringame[playernum] = false;
   CS_InitPlayer(playernum);
}

// [CG] A c/s version of P_SetSkin from p_skin.c.
void CS_SetSkin(const char *skin_name, int playernum)
{
   skin_t *skin;
   player_t *player = &players[playernum];

   if(!playeringame[playernum])
   {
      return;
   }

   skin = P_SkinForName(skin_name);
   if(skin == NULL)
   {
      // [CG] FIXME: this should be better all around
      return;
   }
   player->skin = skin;

   if(gamestate == GS_LEVEL)
   {
      player->mo->skin = skin;
      if(player->mo->state &&
         player->mo->state->sprite == player->mo->info->defsprite)
      {
         player->mo->sprite = skin->sprite;
      }
   }

   if(CS_CLIENT && playernum == consoleplayer)
   {
      default_skin = skin->skinname;
   }
}

size_t CS_BuildPlayerStringInfoPacket(nm_playerinfoupdated_t **update_message,
                                      int playernum, client_info_t info_type)
{
   char *buffer;
   char *player_info;
   size_t info_size;
   player_t *player = &players[playernum];

   if(info_type == ci_name)
   {
      player_info = player->name;
   }
   else if(info_type == ci_skin)
   {
      player_info = player->skin->skinname;
   }
   else if(info_type == ci_class)
   {
      player_info = player->pclass->mnemonic;
   }
   else
   {
      I_Error(
         "CS_GetPlayerStringInfo: Unsupported info type %d\n",
         info_type
      );
   }

   info_size = strlen(player_info) + 1;

   buffer = calloc(1, sizeof(nm_playerinfoupdated_t) + info_size);

   *update_message = (nm_playerinfoupdated_t *)buffer;
   (*update_message)->string_size = info_size;
   (*update_message)->message_type = nm_playerinfoupdated;
   (*update_message)->player_number = playernum;
   (*update_message)->info_type = info_type;
   (*update_message)->array_index = 0;

   memcpy(buffer + sizeof(nm_playerinfoupdated_t), player_info, info_size);

   return sizeof(nm_playerinfoupdated_t) + info_size;
}

void CS_BuildPlayerArrayInfoPacket(nm_playerinfoupdated_t *update_message,
                                   int playernum, client_info_t info_type,
                                   int array_index)
{
   player_t *player = &players[playernum];

   update_message->message_type = nm_playerinfoupdated;
   update_message->player_number = playernum;
   update_message->info_type = info_type;
   update_message->array_index = array_index;

   if(info_type == ci_frags)
   {
      update_message->int_value =  player->frags[array_index];
   }
   else if(info_type == ci_owns_card)
   {
      update_message->boolean_value = player->cards[array_index];
   }
   else if(info_type == ci_owns_weapon)
   {
      update_message->boolean_value = player->killcount;
   }
   else if(info_type == ci_power_enabled)
   {
      update_message->int_value = player->powers[array_index];
   }
   else if(info_type == ci_ammo_amount)
   {
      update_message->int_value = player->ammo[array_index];
   }
   else if(info_type == ci_max_ammo)
   {
      update_message->int_value = player->maxammo[array_index];
   }
   else if(info_type == ci_pwo)
   {
      update_message->int_value = weapon_preferences[0][array_index];
   }
   else
   {
      I_Error(
         "CS_GetPlayerArrayInfo: Unsupported info type %d\n",
         info_type
      );
   }
}

void CS_BuildPlayerScalarInfoPacket(nm_playerinfoupdated_t *update_message,
                                    int playernum, client_info_t info_type)
{
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];
   server_client_t *server_client;

   if(CS_SERVER)
   {
      server_client = &server_clients[playernum];
   }

   update_message->message_type = nm_playerinfoupdated;
   update_message->player_number = playernum;
   update_message->info_type = info_type;
   update_message->array_index = 0;

   if(info_type == ci_team)
   {
      update_message->int_value = client->team;
   }
   else if(info_type == ci_spectating)
   {
      update_message->boolean_value = client->spectating;
   }
   else if(info_type == ci_kill_count)
   {
      update_message->int_value = player->killcount;
   }
   else if(info_type == ci_item_count)
   {
      update_message->int_value = player->itemcount;
   }
   else if(info_type == ci_secret_count)
   {
      update_message->int_value = player->secretcount;
   }
   else if(info_type == ci_colormap)
   {
      update_message->int_value = player->colormap;
   }
   else if(info_type == ci_cheats)
   {
      update_message->int_value = player->cheats;
   }
   else if(info_type == ci_health)
   {
      update_message->int_value = player->health;
   }
   else if(info_type == ci_armor_points)
   {
      update_message->int_value = player->armorpoints;
   }
   else if(info_type == ci_armor_type)
   {
      update_message->int_value = player->armortype;
   }
   else if(info_type == ci_ready_weapon)
   {
      update_message->int_value = player->readyweapon;
   }
   else if(info_type == ci_pending_weapon)
   {
      update_message->int_value = player->pendingweapon;
   }
   else if(info_type == ci_owns_backpack)  // backpack
   {
      update_message->boolean_value = player->backpack;
   }
   else if(info_type == ci_did_secret)    // didsecret
   {
      update_message->boolean_value = player->didsecret;
   }
   else if(info_type == ci_queue_level)
   {
      update_message->int_value = client->queue_level;
   }
   else if(info_type == ci_queue_position)
   {
      update_message->int_value = client->queue_position;
   }
   else if(info_type == ci_wsop)
   {
      update_message->int_value = GET_WSOP(playernum);
   }
   else if(info_type == ci_asop)
   {
      update_message->int_value = GET_ASOP(playernum);
   }
   else if(info_type == ci_bobbing)
   {
      if(CS_CLIENT)
      {
         update_message->boolean_value = player_bobbing;
      }
      else if(CS_SERVER)
      {
         update_message->boolean_value = server_client->options.player_bobbing;
      }
   }
   else if(info_type == ci_weapon_toggle)
   {
      if(CS_CLIENT)
      {
         update_message->boolean_value = doom_weapon_toggles;
      }
      else if(CS_SERVER)
      {
         update_message->boolean_value =
            server_client->options.doom_weapon_toggles;
      }
   }
   else if(info_type == ci_autoaim)
   {
      if(CS_CLIENT)
      {
         update_message->boolean_value = autoaim;
      }
      else if(CS_SERVER)
      {
         update_message->boolean_value = server_client->options.autoaim;
      }
   }
   else if(info_type == ci_weapon_speed)
   {
      if(CS_CLIENT)
      {
         update_message->int_value = weapon_speed;
      }
      else if(CS_SERVER)
      {
         update_message->int_value = server_client->options.weapon_speed;
      }
   }
   else
   {
      I_Error(
         "CS_GetPlayerScalarInfo: Unsupported info type %d\n",
         info_type
      );
   }
}

void CS_SetSpectator(int playernum, boolean spectating)
{
   client_t *client = &clients[playernum];
   player_t *player = &players[playernum];

   if(spectating)
   {
      player->weaponowned[wp_pistol] = false;
      player->ammo[am_clip] = 0;
      player->readyweapon = player->pendingweapon = wp_fist;

      player->mo->flags &= ~MF_SOLID;
      player->mo->flags &= ~MF_SHOOTABLE;
      player->mo->flags &= ~MF_PICKUP;
      player->mo->flags |=  MF_NOCLIP;
      player->mo->flags |=  MF_SLIDE;

      player->mo->flags2 &= ~MF2_FOOTCLIP;
      player->mo->flags2 |=  MF2_NOCROSS;
      player->mo->flags2 |=  MF2_DONTDRAW;
      player->mo->flags2 |=  MF2_INVULNERABLE;
      player->mo->flags2 |=  MF2_NOSPLASH;

      player->mo->flags3 &= ~MF3_TELESTOMP;
      player->mo->flags3 &= ~MF3_WINDTHRUST;
      player->cheats |= CF_NOCLIP;
   }
   else
   {
      player->weaponowned[wp_pistol] = true;
      player->ammo[am_clip] = initial_bullets;
      player->readyweapon = wp_pistol;
      player->mo->flags &= ~MF_NOCLIP;
      player->cheats &= ~CF_NOCLIP;
   }

   client->spectating = spectating;
}

void CS_SpawnPlayer(int playernum, fixed_t x, fixed_t y, fixed_t z,
                    angle_t angle, boolean as_spectator)
{
   int i;
   mobj_t *fog;
   player_t *player = &players[playernum];
   client_t *client = &clients[playernum];

   client->death_time = 0;

   playeringame[playernum] = true;

   if(player->mo != NULL)
   {
      P_RemoveMobj(player->mo);
      player->mo = NULL;
   }

   G_PlayerReborn(playernum);

   // [CG] Stop perpetually displaying the scoreboard in c/s mode once we
   //      respawn.
   if(!CS_HEADLESS)
   {
      action_frags = 0;
   }

   if(!as_spectator)
   {
      // [CG] Spectators create no telefog and make no sounds when they spawn.
      fog = G_SpawnFog(x, y, angle);
      S_StartSound(fog, GameModeInfo->teleSound);

      // [CG] Only set the player's colormap if they're not spectating,
      //      otherwise we don't care.
      if(CS_TEAMS_ENABLED && client->team != team_color_none)
      {
         player->colormap = team_colormaps[client->team];
      }
   }

   player->mo = P_SpawnMobj(x, y, z, player->pclass->type);
   if(as_spectator)
   {
      CS_ReleaseActorNetID(player->mo);
   }

   player->mo->colour = player->colormap;
   player->mo->angle = R_WadToAngle(angle);
   player->mo->player = player;
   player->mo->health = player->health;

   if(!player->skin)
   {
      I_Error("CS_SpawnPlayer: player skin undefined!\n");
   }

   player->mo->skin = player->skin;
   player->mo->sprite = player->skin->sprite;

   player->playerstate = PST_LIVE;
   player->refire = 0;
   player->damagecount = 0;
   player->bonuscount = 0;
   player->extralight = 0;
   player->fixedcolormap = 0;
   player->viewheight = VIEWHEIGHT;
   player->viewz = player->mo->z + VIEWHEIGHT;

   player->momx = player->momy = 0;

   P_SetupPsprites(player);

   if(GameType == gt_dm)
   {
      for(i = 0; i < NUMCARDS; i++)
      {
         player->cards[i] = true;
      }
   }
   if(clientside && playernum == consoleplayer)
   {
      ST_Start(); // wake up the status bar
      // [CG] I commented this out because it causes server messages sent right
      //      before a respawn to not be displayed on the HUD.  It tested OK,
      //      so I think this doesn't cause problems.
      // HU_Start(); // wake up the HUD
   }
   if(clientside && playernum == displayplayer)
   {
      P_ResetChasecam();
   }

   CS_SetSpectator(playernum, as_spectator);
}

mapthing_t* CS_SpawnPlayerCorrectly(int playernum, boolean as_spectator)
{
   mapthing_t *spawn_point;

   if(as_spectator)
   {
      spawn_point = &playerstarts[0];
   }
   else
   {
      // [CG] In order for clipping routines to work properly, the player can't
      //      be a spectator when we search for a spawn point.
      CS_SetSpectator(playernum, false);
      if(GameType == gt_dm)
      {
         if(CS_TEAMS_ENABLED)
         {
            spawn_point = SV_GetTeamSpawnPoint(playernum);
         }
         else
         {
            spawn_point = SV_GetDeathMatchSpawnPoint(playernum);
         }
      }
      else
      {
         spawn_point = SV_GetCoopSpawnPoint(playernum);
      }
      CS_SetSpectator(playernum, true);
   }
   CS_SpawnPlayer(
      playernum,
      spawn_point->x << FRACBITS,
      spawn_point->y << FRACBITS,
      ONFLOORZ,
      spawn_point->angle,
      as_spectator
   );
   return spawn_point;
}

char* CS_ExtractMessage(char *data, size_t data_length)
{
   size_t message_length;
   char *message = NULL;
   nm_playermessage_t *player_message = (nm_playermessage_t *)data;

   if(player_message->length > 256)
   {
      printf("player_message->length > 256.\n");
      return NULL;
   }
   else if(data_length > sizeof(nm_playermessage_t) + 256)
   {
      printf("data_length > sizeof(nm_playermessage_t) + 256.\n");
      return NULL;
   }

   // [CG] So we avoid falling prey to malicious messages that don't have an
   //      ending '\0', we create a string 1 character larger than what the
   //      message should be and zero it.  If strlen is (length - 1), then the
   //      message was malicious and we return NULL.
   //      - length is size of packet data - size of playermessage.
   //      - strlen will be length - 1 (if correct).
   //      - buffer size will be length + 1
   data_length = data_length - sizeof(nm_playermessage_t);
   message = calloc(data_length + 1, sizeof(char));
   memcpy(message, data + sizeof(nm_playermessage_t), data_length);
   message_length = strlen(message);

   // [CG] For debugging.
   // printf(
   //    "CS_ExtractMessage: %d, %d, %d, %d, %d.\n",
   //    packet->dataLength,
   //    sizeof(nm_playermessage_t),
   //    player_message->length,
   //    data_length,
   //    strlen(message)
   // );

   if(message_length >= data_length)
   {
      // [CG] The string didn't end in a '\0', instead strlen went all the way
      //      to the end of our zero'd buffer.  Consequently this message is
      //      either malformed, malicious, or both; don't use it.

      // [CG] For debugging.
      // printf("message_length >= data_length.\n");
      return NULL;
   }

   if(message_length != (data_length - 1))
   {
      // [CG] The message's reported length was incorrect, so this message is
      //      either malformed, malicious, or both; don't use it.

      // [CG] For debugging.
      // printf("message_length != (data_length - 1).\n");
      return NULL;
   }

   // [CG] Message is cool, go ahead and return it.

   // [CG] For debugging.
   // printf("CS_ExtractMessage: Message was cool.\n");
   return message;
}

void CS_FlushConnection(void)
{
   if(net_host != NULL)
   {
      enet_host_flush(net_host);
   }
}

void CS_ReadFromNetwork(void)
{
   ENetEvent event;
   char *address;
   int playernum = 0;

   while(enet_host_service(net_host, &event, 1) > 0)
   {
      if(CS_SERVER)
      {
         playernum = SV_GetPlayerNumberFromPeer(event.peer);
      }
      switch(event.type)
      {
         case ENET_EVENT_TYPE_CONNECT:
            if(CS_CLIENT)
            {
               // [CG] Clients should already have received their connect event
               //      from the server before this loop is ever run, so ignore
               //      further events of this type.
               doom_printf(
                  "Somehow received a connection from server %d:%d\n",
                  event.peer->address.host,
                  event.peer->address.port
               );
            }
            else if(CS_SERVER)
            {
               address = CS_IPToString(event.peer->address.host);
               if(playernum == 0)
               {
                  doom_printf(
                     "Adding a new client: %s:%d\n",
                     address,
                     event.peer->address.port
                  );
                  if(SV_HandleClientConnection(event.peer) == 0)
                  {
                     // [CG] The server couldn't find a slot for the new
                     //      client, so disconnect it.
                     enet_peer_disconnect(event.peer, 1);
                  }
               }
               else
               {
                  doom_printf(
                     "Received a spurious connection from: %s:%d\n",
                     address,
                     event.peer->address.port
                  );
               }
               free(address);
            }
            break;
         case ENET_EVENT_TYPE_RECEIVE:
            // [CG] Save all received network messages in the demo if
            //      recording.
            if(cs_demo_recording)
            {
               if(!CS_WriteNetworkMessageToDemo(
                     event.packet->data, event.packet->dataLength, playernum))
               {
                  doom_printf(
                     "Demo error, recording aborted: %s\n",
                     CS_GetDemoErrorMessage()
                  );
                  CS_StopDemo();
               }
            }

            if(CS_CLIENT)
            {
               CL_HandleMessage(
                  (char *)event.packet->data,
                  event.packet->dataLength
               );
            }
            else if(CS_SERVER)
            {
               if(playernum == 0)
               {
                  doom_printf(
                     "Received a message from an unknown client, ignoring.\n"
                  );
                  return;
               }
               SV_HandleMessage(
                  (char *)event.packet->data,
                  event.packet->dataLength,
                  playernum
               );
            }
            event.peer->data = NULL;
            enet_packet_destroy(event.packet);
            break;
         case ENET_EVENT_TYPE_DISCONNECT:
            if(CS_CLIENT)
            {
               printf("Server Disconnected.\n");
               CL_Disconnect();
            }
            else if(CS_SERVER)
            {
               playernum = SV_GetClientNumberFromAddress(&event.peer->address);
               if(playernum &&
                  playernum < MAX_CLIENTS &&
                  playeringame[playernum])
               {
                  SV_DisconnectPlayer(playernum, dr_no_reason);
               }
               else
               {
                  address = CS_IPToString(event.peer->address.host);
                  doom_printf(
                     "Received disconnect from unknown player %s:%d.\n",
                     address,
                     event.peer->address.port
                  );
                  free(address);
               }
            }
            event.peer->data = NULL;
            break;
         case ENET_EVENT_TYPE_NONE:
         default:
            doom_printf("Received unknown event type: %d\n", event.type);
            break;
      }
   }
}

void CS_TryRunTics(void)
{
   if(d_fastrefresh)
   {
      I_Error(
         "CS_TryRunTics: d_fastrefresh is currently unsupported in c/s "
         "Eternity, exiting.\n"
      );
   }

   if(CS_CLIENTDEMO)
   {
      CL_RunDemoTics();
   }
   else if(CS_SERVERDEMO)
   {
      SV_RunDemoTics();
   }
   else if(CS_CLIENT)
   {
      CL_TryRunTics();
   }
   else
   {
      SV_TryRunTics();
   }
}

VARIABLE_STRING(cs_demo_folder_path, NULL, 1024);
CONSOLE_VARIABLE(demo_folder_path, cs_demo_folder_path, cf_netonly) {}

void CS_AddCommands(void)
{
   C_AddCommand(demo_folder_path);
}

