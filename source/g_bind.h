//----------------------------------------------------------------------------
//
// Copyright(C) 2005 Simon Howard, James Haley
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
// Key Bindings
//
//----------------------------------------------------------------------------

#ifndef G_BIND_H__
#define G_BIND_H__

// haleyjd 07/03/04: key binding classes
// [CG] Renamed to key binding "category" to avoid collision with C++ keyword.
enum input_action_category_e
{
   kac_none    =  1,
   kac_player  =  2,      // player  bindings -- handled by G_BuildTiccmd
   kac_menu    =  4,      // menu    bindings -- handled by MN_Responder
   kac_map     =  8,      // map     bindings -- handled by AM_Responder
   kac_console = 16,      // console bindings -- handled by C_Responder
   kac_hud     = 32,      // hud     bindings -- handled by HU_Responder
   kac_command = 64,      // command bindings -- handled by C_RunTextCmd
   kac_max,
};

void        G_InitKeyBindings(void);
bool        G_KeyResponder(event_t *ev, int categories);
void        G_EditBinding(const char *action);
const char* G_BoundKeys(const char *action_name);
const char* G_FirstBoundKey(const char *action_name);
void        G_ClearKeyStates(void);
void        G_LoadDefaults(void);
void        G_SaveDefaults(void);
void        G_Bind_AddCommands(void);
void        G_BindDrawer(void);
bool        G_BindResponder(event_t *ev);

// action variables

extern int action_forward;
extern int action_backward;
extern int action_left;
extern int action_right;
extern int action_moveleft;
extern int action_moveright;
extern int action_lookup;
extern int action_lookdown;
extern int action_use;
extern int action_speed;
extern int action_attack;
extern int action_strafe;
extern int action_flip;
extern int action_jump;
extern int action_autorun;
extern int action_mlook;
extern int action_center;
extern int action_weapon1;
extern int action_weapon2;
extern int action_weapon3;
extern int action_weapon4;
extern int action_weapon5;
extern int action_weapon6;
extern int action_weapon7;
extern int action_weapon8;
extern int action_weapon9;
extern int action_nextweapon;
extern int action_weaponup;
extern int action_weapondown;
extern int action_frags;
extern int action_menu_help;
extern int action_menu_toggle;
extern int action_menu_setup;
extern int action_menu_up;
extern int action_menu_down;
extern int action_menu_confirm;
extern int action_menu_previous;
extern int action_menu_left;
extern int action_menu_right;
extern int action_menu_pageup;
extern int action_menu_pagedown;
extern int action_menu_contents;
extern int action_map_toggle;
extern int action_map_gobig;
extern int action_map_follow;
extern int action_map_mark;
extern int action_map_clear;
extern int action_map_grid;
extern int action_console_pageup;
extern int action_console_pagedown;
extern int action_console_toggle;
extern int action_console_tab;
extern int action_console_enter;
extern int action_console_up;
extern int action_console_down;
extern int action_console_backspace;

#endif

// EOF
