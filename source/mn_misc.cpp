// Emacs style mode select -*- C++ -*- vi:sw=3 ts=3:
//----------------------------------------------------------------------------
//
// Copyright(C) 2000 James Haley
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
// Menu - misc functions
//
// Pop up alert/question messages
// Miscellaneous stuff
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"      // haleyjd: gamemode info
#include "d_main.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"
#include "m_qstr.h"
#include "m_swap.h"
#include "mn_engin.h"
#include "mn_misc.h"
#include "r_patch.h"
#include "s_sound.h"
#include "v_font.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "v_video.h"
#include "w_wad.h"

extern vfont_t *menu_font_big;

/////////////////////////////////////////////////////////////////////////
//
// Pop-up Messages
//

// haleyjd 02/24/02: bug fix -- MN_Alert and MN_Question replaced
// M_StartMessage in SMMU, but fraggle didn't put code in them to
// properly set the menuactive state based on whether or not menus 
// were active at the time of the call, leading to weird menu 
// behavior
static bool popupMenuActive;

static char  popup_message[1024];
static char *popup_message_command; // console command to run

//
// haleyjd 07/27/05: not all questions should have to run console
// commands. It's inefficient.
//
static void (*popup_callback)(void) = NULL;

enum
{
   popup_alert,
   popup_question
} popup_message_type;

extern vfont_t *menu_font;
extern vfont_t *menu_font_normal;

//
// WriteCenteredText
//
// Local routine to draw centered messages. Candidate for
// absorption into future generalized font code. Rewritten
// 02/22/04 to use qstring module. 
//
static void WriteCenteredText(char *message)
{
   static qstring qstr;
   char *rover;
   const char *buffer;
   int x, y;

   qstr.clearOrCreate(128);
   
   // rather than reallocate memory every time we draw it,
   // use one buffer and increase the size as neccesary
   // haleyjd 02/22/04: qstring handles this for us now

   y = (SCREENHEIGHT - V_FontStringHeight(menu_font_normal, popup_message)) / 2;
   qstr.clear();
   rover = message;

   while(*rover)
   {
      if(*rover == '\n')
      {
         buffer = qstr.constPtr();
         x = (SCREENWIDTH - V_FontStringWidth(menu_font_normal, buffer)) / 2;
         V_FontWriteText(menu_font_normal, buffer, x, y);         
         qstr.clear(); // clear buffer
         y += menu_font_normal->absh; // next line
      }
      else      // add next char
         qstr += *rover;

      ++rover;
   }

   // dont forget the last line.. prob. not \n terminated
   buffer = qstr.constPtr();
   x = (SCREENWIDTH - V_FontStringWidth(menu_font_normal, buffer)) / 2;
   V_FontWriteText(menu_font_normal, buffer, x, y);   
}

void MN_PopupDrawer(void)
{
   WriteCenteredText(popup_message);
}

bool MN_PopupResponder(event_t *ev)
{
   int *menuSounds = GameModeInfo->menuSounds;
   char ch;
   
   if(ev->type != ev_keydown)
      return false;

   if(ev->character)
      ch = tolower(ev->character);
   else
      ch = ev->data1;
   
   switch(popup_message_type)
   {
   case popup_alert:
      {
         // haleyjd 02/24/02: restore saved menuactive state
         menuactive = popupMenuActive;
         // kill message
         redrawborder = true; // need redraw
         current_menuwidget = NULL;
         S_StartSound(NULL, menuSounds[MN_SND_DEACTIVATE]);
      }
      break;

   case popup_question:
      if(ch == 'y')     // yes!
      {
         // run command and kill message
         // haleyjd 02/24/02: restore saved menuactive state
         menuactive = popupMenuActive;
         if(popup_callback)
         {
            popup_callback();
            popup_callback = NULL;
         }
         else
         {
            Console.cmdtype = c_menu;
            C_RunTextCmd(popup_message_command);
         }
         S_StartSound(NULL, menuSounds[MN_SND_COMMAND]);
         redrawborder = true; // need redraw
         current_menuwidget = NULL;  // kill message
      }
      if(ch == 'n' || action_menu_toggle || action_menu_previous) // no!
      {
         // kill message
         // haleyjd 02/24/02: restore saved menuactive state
         action_menu_toggle = action_menu_previous = false;
         menuactive = popupMenuActive;
         S_StartSound(NULL, menuSounds[MN_SND_DEACTIVATE]);
         redrawborder = true; // need redraw
         current_menuwidget = NULL; // kill message
      }
      break;
      
   default:
      break;
   }
   
   return true; // always eat key
}

// widget for popup message alternate draw
menuwidget_t popup_widget = {MN_PopupDrawer, MN_PopupResponder};

//
// MN_Alert
//
// alert message
// -- just press enter
//
void MN_Alert(const char *message, ...)
{
   va_list args;
   
   // haleyjd 02/24/02: bug fix for menuactive state
   popupMenuActive = menuactive;
   
   MN_ActivateMenu();
   
   // hook in widget so message will be displayed
   current_menuwidget = &popup_widget;
   popup_message_type = popup_alert;
   
   va_start(args, message);
   pvsnprintf(popup_message, sizeof(popup_message), message, args);
   va_end(args);
}

//
// MN_Question
//
// question message
// console command will be run if user responds with 'y'
//
void MN_Question(const char *message, char *command)
{
   // haleyjd 02/24/02: bug fix for menuactive state
   popupMenuActive = menuactive;
   
   MN_ActivateMenu();
   
   // hook in widget so message will be displayed
   current_menuwidget = &popup_widget;
   
   strncpy(popup_message, message, 1024);
   popup_message_type = popup_question;
   popup_message_command = command;
}

//
// MN_QuestionFunc
//
// haleyjd: I restored the ability for questions to simply execute
// native functions because calling C_RunTextCmd from inside the program
// to execute code otherwise immediately accessible is actually kinda
// stupid when you stop to think about it.
//
void MN_QuestionFunc(const char *message, void (*handler)(void))
{
   popupMenuActive = menuactive;
   
   MN_ActivateMenu();
   
   // hook in widget so message will be displayed
   current_menuwidget = &popup_widget;
   
   strncpy(popup_message, message, 1024);
   popup_message_type = popup_question;
   popup_callback = handler;
}

//////////////////////////////////////////////////////////////////////////
//
// Credits Screens
//

void MN_DrawCredits(void);

typedef struct helpscreen_s
{
   int lumpnum;
   void (*Drawer)(); // alternate drawer
} helpscreen_t;

static helpscreen_t helpscreens[120];  // 100 + credit/built-in help screens
static int num_helpscreens;
static int viewing_helpscreen;     // currently viewing help screen
extern bool inhelpscreens; // indicates we are in or just left a help screen

static void AddHelpScreen(const char *screenname)
{
   int lumpnum;
   
   if((lumpnum = W_CheckNumForName(screenname)) != -1)
   {
      helpscreens[num_helpscreens].Drawer = NULL;   // no drawer
      helpscreens[num_helpscreens++].lumpnum = lumpnum;
   }  
}

// build help screens differently according to whether displaying
// help or displaying credits

static void MN_FindCreditScreens(void)
{
   num_helpscreens = 0;  // reset
   
   // add dynamic smmu credits screen
   
   helpscreens[num_helpscreens++].Drawer = MN_DrawCredits;
   
   // other help screens

   // haleyjd: do / do not want certain screens for different
   // game modes, even though its not necessary to weed them out
   // if they're not in the wad

   if(GameModeInfo->type == Game_Heretic)
      AddHelpScreen(DEH_String("ORDER"));
   else
      AddHelpScreen(DEH_String("HELP2"));
   
   AddHelpScreen(DEH_String("CREDIT"));
}

static void MN_FindHelpScreens(void)
{
   int custom;
   
   num_helpscreens = 0;
   
   // add custom menus first
   
   for(custom = 0; custom < 100; ++custom)
   {
      char tempstr[10];

      sprintf(tempstr, "HELP%.02i", custom);
      AddHelpScreen(tempstr);
   }
   
   // now the default original doom ones
   
   // sf: keep original help screens until key bindings rewritten
   // and i can restore the dynamic help screens

   if(GameModeInfo->type == Game_Heretic)
      AddHelpScreen(DEH_String("ORDER"));
   else
      AddHelpScreen("HELP");
   
   AddHelpScreen("HELP1");
   
   // promote the registered version at every availability
   // haleyjd: HELP2 is a help screen in heretic too
   
   AddHelpScreen(DEH_String("HELP2")); 
}

#define NUMCATS 5

static const char *cat_strs[NUMCATS] =
{
   FC_HI "Programming:",
   FC_HI "Based On:",
   FC_HI "Graphics:",
   FC_HI "Start Map:",
   FC_HI "Special Thanks:",
};

static const char *val_strs[NUMCATS] =
{
   "James Haley\nStephen McGranahan\n",
   
   FC_HI "SMMU" FC_NORMAL " by Simon Howard\n"
   FC_HI "MBF " FC_NORMAL " by Lee Killough\n"
   FC_HI "BOOM" FC_NORMAL " by Team TNT\n",

   "Sven Ruthner",
   "Derek MacDonald",
   "Joe Kennedy\nJulian Aubourg\nJoel Murdoch\nAnders Astrand\nSargeBaldy\n",
};

void MN_DrawCredits(void)
{
   static int cat_width = -1, val_width = -1, line_x;
   int i, y;
   const char *str;

   if(cat_width == -1)
   {
      // determine widest category string
      int w;

      for(i = 0; i < NUMCATS; ++i)
      {
         w = V_FontStringWidth(menu_font_normal, cat_strs[i]);

         if(w > cat_width)
            cat_width = w;
      }

      // determine widest value string
      for(i = 0; i < NUMCATS; ++i)
      {
         w = V_FontStringWidth(menu_font_normal, val_strs[i]);

         if(w > val_width)
            val_width = w;
      }

      // determine line position
      line_x = (SCREENWIDTH - (cat_width + val_width + 16)) >> 1;
   }

   inhelpscreens = true;

   // sf: altered for SMMU
   // haleyjd: altered for Eternity :)
   
   V_DrawDistortedBackground(GameModeInfo->creditBackground, &vbscreen);

   y = GameModeInfo->creditY;
   str = FC_ABSCENTER FC_HI "The Eternity Engine";
   V_FontWriteTextShadowed(menu_font_big, str, 0, y);
   y += V_FontStringHeight(menu_font_big, str) + GameModeInfo->creditTitleStep;

   // draw info categories
   for(i = 0; i < NUMCATS; ++i)
   {
      V_FontWriteText(menu_font_normal, cat_strs[i], 
                      line_x + (cat_width - 
                                V_FontStringWidth(menu_font_normal, 
                                                  cat_strs[i])), 
                                                  y);

      V_FontWriteText(menu_font_normal, val_strs[i], line_x + cat_width + 16, y);

      y += V_FontStringHeight(menu_font_normal, val_strs[i]);
   }

   V_FontWriteText(menu_font_normal, 
                   FC_ABSCENTER "Copyright 2011 Team Eternity et al.\n"
                   "http://doomworld.com/eternity/", 0, y);
}

void MN_HelpDrawer(void)
{
   if(helpscreens[viewing_helpscreen].Drawer)
   {
      helpscreens[viewing_helpscreen].Drawer();   // call drawer
   }
   else
   {
      // haleyjd: support raw lumps
      int lumpnum = helpscreens[viewing_helpscreen].lumpnum;

      // haleyjd 05/18/09: use smart background drawer
      V_DrawFSBackground(&vbscreen, lumpnum);
   }
}

// haleyjd 05/29/06: record state of menu activation
static bool help_prev_menuactive;

bool MN_HelpResponder(event_t *ev)
{
   int *menuSounds = GameModeInfo->menuSounds;
   
   if(ev->type != ev_keydown) return false;
   
   if(action_menu_previous)
   {
      action_menu_previous = false;

      // go to previous screen
      // haleyjd: only make sound if we really went back
      viewing_helpscreen--;
      if(viewing_helpscreen < 0)
      {
         // cancel
         goto cancel;
      }
      else
         S_StartSound(NULL, menuSounds[MN_SND_PREVIOUS]);
   }

   if(action_menu_confirm)
   {
      action_menu_confirm = false;

      // go to next helpscreen
      viewing_helpscreen++;
      if(viewing_helpscreen >= num_helpscreens)
      {
         // cancel
         goto cancel;
      }
      else
         S_StartSound(NULL, menuSounds[MN_SND_COMMAND]);
   }

   if(action_menu_toggle)
   {
      action_menu_toggle = false;

      // cancel helpscreen
cancel:
      redrawborder = true; // need redraw
      current_menuwidget = NULL;
      // haleyjd 05/29/06: maintain previous menu activation state
      if(!help_prev_menuactive)
         menuactive = false;
      S_StartSound(NULL, menuSounds[MN_SND_DEACTIVATE]);
   }

   // always eatkey
   return false;
}

menuwidget_t helpscreen_widget = {MN_HelpDrawer, MN_HelpResponder, NULL, true};

CONSOLE_COMMAND(help, 0)
{
   // haleyjd 05/29/06: record state of menu activation
   help_prev_menuactive = menuactive;

   MN_ActivateMenu();
   MN_FindHelpScreens();        // search for help screens
   
   // hook in widget to display menu
   current_menuwidget = &helpscreen_widget;
   
   // start on first screen
   viewing_helpscreen = 0;
}

CONSOLE_COMMAND(credits, 0)
{
   // haleyjd 05/29/06: record state of menu activation
   help_prev_menuactive = menuactive;

   MN_ActivateMenu();
   MN_FindCreditScreens();        // search for help screens
   
   // hook in widget to display menu
   current_menuwidget = &helpscreen_widget;
   
   // start on first screen
   viewing_helpscreen = 0;
}

///////////////////////////////////////////////////////////////////////////
//
// Automap Colour selection
//

// selection of automap colours for menu.

command_t *colour_command;
int selected_colour;

#define HIGHLIGHT_COLOUR (GameModeInfo->whiteIndex)

void MN_MapColourDrawer(void)
{
   patch_t *patch;
   int x, y;
   int u, v;
   byte block[BLOCK_SIZE*BLOCK_SIZE];

   // draw the menu in the background
   
   MN_DrawMenu(current_menu);

   // draw colours table
   
   patch = PatchLoader::CacheName(wGlobalDir, "M_COLORS", PU_CACHE);
   
   x = (SCREENWIDTH  - patch->width ) / 2;
   y = (SCREENHEIGHT - patch->height) / 2;
   
   V_DrawPatch(x, y, &vbscreen, patch);
   
   x += 4 + 8 * (selected_colour % 16);
   y += 4 + 8 * (selected_colour / 16);

   // build block
   
   // border
   memset(block, HIGHLIGHT_COLOUR, BLOCK_SIZE*BLOCK_SIZE);
  
   // draw colour inside
   for(u = 1; u < BLOCK_SIZE - 1; ++u)
      for(v = 1; v < BLOCK_SIZE - 1; ++v)
         block[v*BLOCK_SIZE + u] = selected_colour;
  
   // draw block
   V_DrawBlock(x, y, &vbscreen, BLOCK_SIZE, BLOCK_SIZE, block);

   if(!selected_colour)
   {
      V_DrawPatch(x+1, y+1, &vbscreen, 
                  PatchLoader::CacheName(wGlobalDir, "M_PALNO", PU_CACHE));
   }
}

bool MN_MapColourResponder(event_t *ev)
{
   if(action_menu_left)
   {
      action_menu_left = false;
      selected_colour--;
   }

   if(action_menu_right)
   {
      action_menu_right = false;
      selected_colour++;
   }
   
   if(action_menu_up)
   {
      action_menu_up = false;
      selected_colour -= 16;
   }
   
   if(action_menu_down)
   {
      action_menu_down = false;
      selected_colour += 16;
   }
   
   if(action_menu_toggle || action_menu_previous)
   {
      // cancel colour selection
      action_menu_toggle = action_menu_previous = false;
      current_menuwidget = NULL;
      return true;
   }

   if(action_menu_confirm)
   {
      static char tempstr[128];
      sprintf(tempstr, "%i", selected_colour);
     
      // run command
      C_RunCommand(colour_command, tempstr);
      
      // kill selector
      action_menu_confirm = false;
      current_menuwidget = NULL;
      return true;
   }

   if(selected_colour < 0) 
      selected_colour = 0;
   if(selected_colour > 255) 
      selected_colour = 255;
   
   return true; // always eat key
}

menuwidget_t colour_widget = 
{
   MN_MapColourDrawer, 
   MN_MapColourResponder,
   NULL,
   true
};

void MN_SelectColour(const char *variable_name)
{
   current_menuwidget = &colour_widget;
   colour_command = C_GetCmdForName(variable_name);
   selected_colour = *(int *)colour_command->variable->variable;
}


void MN_AddMiscCommands(void)
{
   C_AddCommand(credits);
   C_AddCommand(help);
}

// EOF
