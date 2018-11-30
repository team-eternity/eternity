//----------------------------------------------------------------------------
//
// Copyright(C) 2005 Simon Howard, James Haley, et al.
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
// Key Bindings
//
//----------------------------------------------------------------------------

#ifndef G_BIND_H__
#define G_BIND_H__

struct event_t;
class  qstring;

void G_InitKeyBindings();
int  G_KeyResponder(const event_t *ev, int bclass, bool *allreleased = nullptr);

void G_ClearKeyStates();

typedef void (*binding_handler)(event_t *ev);

void G_EditBinding(const char *action);
void G_BoundKeys(const char *action, qstring &outstr);
const char *G_FirstBoundKey(const char *action);

// default file loading

void G_LoadDefaults();
void G_SaveDefaults();

void G_Bind_AddCommands();

// haleyjd 07/03/04: key binding classes
enum keyactionclass
{
   kac_game,            // game bindings -- handled by G_BuildTiccmd
   kac_menu,            // menu bindings -- handled by MN_Responder
   kac_map,             // map  bindings -- handled by AM_Responder
   kac_console,         // con. bindings -- handled by C_Repsonder
   kac_hud,             // hud  bindings -- handled by HU_Responder
   kac_cmd,             // command
   NUMKEYACTIONCLASSES
};

// key actions
enum keyaction_e
{
   ka_nothing,
   ka_forward,
   ka_backward,
   ka_left,      
   ka_right,     
   ka_moveleft,  
   ka_moveright, 
   ka_use,       
   ka_strafe,    
   ka_attack,    
   ka_attack_alt,
   ka_flip,
   ka_speed,
   ka_jump,
   ka_autorun,
   ka_mlook,     
   ka_lookup,    
   ka_lookdown,  
   ka_center,
   ka_flyup,
   ka_flydown,
   ka_flycenter,
   ka_weapon1,   
   ka_weapon2,   
   ka_weapon3,   
   ka_weapon4,   
   ka_weapon5,   
   ka_weapon6,   
   ka_weapon7,   
   ka_weapon8,   
   ka_weapon9,   
   ka_nextweapon,
   ka_weaponup,
   ka_weapondown,
   ka_frags,   
   ka_menu_toggle,   
   ka_menu_help,     
   ka_menu_setup,    
   ka_menu_up,       
   ka_menu_down,     
   ka_menu_confirm,  
   ka_menu_previous, 
   ka_menu_left,     
   ka_menu_right,    
   ka_menu_pageup,   
   ka_menu_pagedown,
   ka_menu_contents,
   ka_map_right,   
   ka_map_left,    
   ka_map_up,      
   ka_map_down,    
   ka_map_zoomin,  
   ka_map_zoomout, 
   ka_map_toggle,  
   ka_map_gobig,   
   ka_map_follow,  
   ka_map_mark,    
   ka_map_clear,   
   ka_map_grid,
   ka_console_pageup,
   ka_console_pagedown,
   ka_console_toggle,
   ka_console_tab,
   ka_console_enter,
   ka_console_up,
   ka_console_down,
   ka_console_backspace,
   ka_inventory_left,
   ka_inventory_right,
   ka_inventory_use,
   ka_inventory_drop,
   ka_join_demo,
   NUMKEYACTIONS
};

// Possible action types for analog axis input
enum axisaction_e
{
   axis_none,       // input is ignored
   axis_move,       // forward/backward movement
   axis_strafe,     // strafe left/right
   axis_turn,       // turn left/right
   axis_look,       // look up/down
   axis_fly,        // fly up/down
   axis_max         // keep this last
};

// Axis bindings
extern int axisActions[];

// Axis orientations
extern int axisOrientation[];

bool G_ExecuteGamepadProfile(const char *name);

#endif

// EOF
