// Emacs style mode select -*- C++ -*-
//----------------------------------------------------------------------------
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
// Menu - misc functions
//
// Pop up alert/question messages
// Miscellaneous stuff
//
// By Simon Howard
//
//----------------------------------------------------------------------------

#include "z_zone.h"

#include "c_io.h"
#include "c_runcmd.h"
#include "d_dehtbl.h"
#include "d_event.h"
#include "d_gi.h"      // haleyjd: gamemode info
#include "d_main.h"
#include "doomstat.h"
#include "e_fonts.h"
#include "g_bind.h"
#include "hal/i_timer.h"
#include "m_compare.h"
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

static char        popup_message[1024];
static const char *popup_message_command; // console command to run

static void MN_PopupDrawer();
static bool MN_PopupResponder(event_t *ev, int action);

// widget for popup message alternate draw
menuwidget_t popup_widget = { MN_PopupDrawer, MN_PopupResponder };

//
// haleyjd 07/27/05: not all questions should have to run console
// commands. It's inefficient.
//
static void (*popup_callback)(void) = nullptr;

static enum
{
   popup_alert,
   popup_question
} popup_message_type;

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
   int w, h;

   qstr.clearOrCreate(128);
   
   // rather than reallocate memory every time we draw it,
   // use one buffer and increase the size as neccesary
   // haleyjd 02/22/04: qstring handles this for us now

   w = V_FontStringWidth(menu_font_normal, popup_message);
   h = V_FontStringHeight(menu_font_normal, popup_message);

   x = (SCREENWIDTH  - w) / 2;
   y = (SCREENHEIGHT - h) / 2;
   qstr.clear();
   rover = message;

   if(popup_widget.prev) // up over another widget?
      V_DrawBox(x - 8, y - 8, w + 16, h + 16);

   while(*rover)
   {
      if(*rover == '\n')
      {
         buffer = qstr.constPtr();
         x = (SCREENWIDTH - V_FontStringWidth(menu_font_normal, buffer)) / 2;
         V_FontWriteText(menu_font_normal, buffer, x, y, &subscreen43);         
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
   V_FontWriteText(menu_font_normal, buffer, x, y, &subscreen43);   
}

static void MN_PopupDrawer(void)
{
   // haleyjd 08/31/12: If we popped up over another widget, draw it.
   if(popup_widget.prev && popup_widget.prev->drawer)
      popup_widget.prev->drawer();
   
   WriteCenteredText(popup_message);
}

static bool MN_PopupResponder(event_t *ev, int action)
{
   int *menuSounds = GameModeInfo->menuSounds;
   char ch;
   
   if(ev->type != ev_keydown && ev->type != ev_text && ev->type != ev_quit)
      return false;

   if(ev->type == ev_text)
      ch = ectype::toLower(ev->data1);
   else if (ev->type == ev_quit && menuactive &&
            popup_message_type == popup_question &&
            !strcmp(popup_message_command, "quit"))
      ch = 'y';
   else if(!ectype::isPrint(ev->data1))
      ch = ev->data1;
   else
      return false;
   
   switch(popup_message_type)
   {
   case popup_alert:
      {
         // haleyjd 02/24/02: restore saved menuactive state
         menuactive = popupMenuActive;
         // kill message
         MN_PopWidget(consumeText_e::NO);
         S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
      }
      break;

   case popup_question:
      if(ch == 'y' || action == ka_menu_confirm) // yes!
      {
         // run command and kill message
         // haleyjd 02/24/02: restore saved menuactive state
         menuactive = popupMenuActive;
         if(popup_callback)
         {
            popup_callback();
         }
         else
         {
            Console.cmdtype = c_menu;
            C_RunTextCmd(popup_message_command);
         }
         S_StartInterfaceSound(menuSounds[MN_SND_COMMAND]);
         MN_PopWidget(consumeText_e::NO);  // kill message
      }
      if(ch == 'n' || action == ka_menu_toggle || action == ka_menu_previous) // no!
      {
         // kill message
         // haleyjd 02/24/02: restore saved menuactive state
         menuactive = popupMenuActive;
         S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
         MN_PopWidget(consumeText_e::NO); // kill message
      }
      break;
      
   default:
      break;
   }
   
   return true; // always eat key
}

//
// MN_Alert
//
// alert message
// -- just press enter
//
void MN_Alert(E_FORMAT_STRING(const char *message), ...)
{
   va_list args;
   
   // haleyjd 02/24/02: bug fix for menuactive state
   popupMenuActive = menuactive;
   
   MN_ActivateMenu();
   
   // hook in widget so message will be displayed
   MN_PushWidget(&popup_widget);
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
void MN_Question(const char *message, const char *command)
{
   // haleyjd 02/24/02: bug fix for menuactive state
   popupMenuActive = menuactive;
   
   MN_ActivateMenu();
   
   // hook in widget so message will be displayed
   MN_PushWidget(&popup_widget);

   // If a widget we popped up over is fullscreen, so are we, since
   // we'll call down to its drawer. Otherwise, not.
   popup_widget.fullscreen = (popup_widget.prev && popup_widget.prev->fullscreen);

   strncpy(popup_message, message, 1024);
   popup_message_type = popup_question;
   popup_message_command = command;
   popup_callback = nullptr;
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
   MN_PushWidget(&popup_widget);

   // If a widget we popped up over is fullscreen, so are we, since
   // we'll call down to its drawer. Otherwise, not.
   popup_widget.fullscreen = (popup_widget.prev && popup_widget.prev->fullscreen);
   
   strncpy(popup_message, message, 1024);
   popup_message_type = popup_question;
   popup_message_command = nullptr;
   popup_callback = handler;
}

//////////////////////////////////////////////////////////////////////////
//
// Credits Screens
//

void MN_DrawCredits(void);

struct helpscreen_t
{
   int lumpnum;
   void (*Drawer)(); // alternate drawer
};

static helpscreen_t helpscreens[120];  // 100 + credit/built-in help screens
static int num_helpscreens;
static int viewing_helpscreen;     // currently viewing help screen
extern bool inhelpscreens; // indicates we are in or just left a help screen

static void AddHelpScreen(const char *screenname)
{
   int lumpnum;
   
   if((lumpnum = W_CheckNumForName(screenname)) != -1)
   {
      helpscreens[num_helpscreens].Drawer = nullptr;   // no drawer
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

      snprintf(tempstr, sizeof(tempstr), "HELP%.02i", custom);
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

enum
{
   CAT_PROGRAMMING,
   CAT_BASEDON,
   CAT_GRAPHICS,
   CAT_SPECIALTHANKS,
   NUMCATS
};

static const char *cat_strs[NUMCATS] =
{
   FC_ABSCENTER FC_HI "Programming",
   FC_ABSCENTER FC_HI "Based On",
   FC_ABSCENTER FC_HI "Graphics",
   FC_ABSCENTER FC_HI "Special Thanks",
};

struct val_str_t
{
   const char *l; // String in the left column
   const char *m; // String in the middle (if no string is on either side)
   const char *r; // String in the right column

   bool isNull() const { return l == nullptr && m == nullptr && r == nullptr; }
};

static const val_str_t val_programmers[] =
{
   { "James Haley", nullptr,     "David Hill"                  },
   { "Ioan Chera",  nullptr,     "Max Waine"                   },
   { nullptr,       FC_ABSCENTER "Stephen McGranahan", nullptr },
   { nullptr, nullptr, nullptr }
};

static const val_str_t val_basedon[] =
{
   { nullptr, FC_ABSCENTER FC_HI "SMMU" FC_NORMAL " by Simon Howard", nullptr },
   { nullptr, nullptr, nullptr }
};

static const val_str_t val_graphics[] = {
   { "Sarah Mancuso", nullptr, "Sven Ruthner" },
   { nullptr, nullptr, nullptr }
};

static const val_str_t val_thanks[] =
{
   { "Joe Kennedy",  nullptr, "Julian Aubourg"   },
   { "Joel Murdoch", nullptr, "Anders Astrand"   },
   { "SargeBaldy",   nullptr, "Kaitlyn Anne Fox" },
   { nullptr, nullptr, nullptr }
};

static const val_str_t *val_strs[NUMCATS] =
{
   val_programmers,
   val_basedon,
   val_graphics,
   val_thanks
};

extern bool help_prev_menuactive;

void MN_DrawCredits()
{
   static int startTic = 0;
   static int lastTic  = 0;
   static int yOffset  = 0;

   static int line_x = -1;
   int i, y;
   const char *str;

   // Reset scoll if a full second has passed or we opened the menu
   if(gametic - lastTic > TICRATE || (help_prev_menuactive && gametic == mn_lastSelectTic + 1))
   {
      startTic = gametic;
      yOffset  = 0;
   }
   lastTic = gametic;

   if(line_x == -1)
      line_x = SCREENWIDTH >> 1;

   inhelpscreens = true;

   // sf: altered for SMMU
   // haleyjd: altered for Eternity :)
   
   V_DrawDistortedBackground(GameModeInfo->creditBackground, &vbscreen);

   y = GameModeInfo->creditY - yOffset;
   str = FC_ABSCENTER FC_HI "The Eternity Engine";
   V_FontWriteTextShadowed(menu_font_big, str, 0, y, &subscreen43);
   y += V_FontStringHeight(menu_font_big, str) + GameModeInfo->creditTitleStep;

   // draw info categories
   for(i = 0; i < NUMCATS; ++i)
   {
      V_FontWriteText(menu_font_normal, cat_strs[i], 0, y, &subscreen43);

      y += V_FontStringHeight(menu_font_normal, cat_strs[i]);

      // Iterate through the val_str_t's for the current category.
      // TODO: Figure out if there's a way to process all the val_str_t's at once for a single
      // category. ATM it iterates through so it can correctly align the left strings.
      for(int j = 0; !val_strs[i][j].isNull(); j++)
      {
         // Write left if need be
         if(val_strs[i][j].l != nullptr)
         {
            int valStrLWidth = V_FontStringWidth(menu_font_normal, val_strs[i][j].l);
            V_FontWriteText(menu_font_normal, val_strs[i][j].l, line_x - valStrLWidth - 8,
                            y, &subscreen43);
         }

         // Write right if need be
         if(val_strs[i][j].r != nullptr)
            V_FontWriteText(menu_font_normal, val_strs[i][j].r, line_x + 8, y, &subscreen43);

         // Write middle if need be
         if(val_strs[i][j].m != nullptr)
            V_FontWriteText(menu_font_normal, val_strs[i][j].m, 0,  y, &subscreen43);

         y += V_FontStringHeight(menu_font_normal, "");
       }

      y += V_FontStringHeight(menu_font_normal, "");
   }

   // MaxW: I'm going to hell for this. Automatically update copyright year.
   static const char *const copyright_text = []() {
      static char temp[] = FC_ABSCENTER "Copyright YEAR Team Eternity et al.";
      memcpy(temp + 11, &__DATE__[7], 4); // Overwrite YEAR in temp.
      return temp;
   }();

   V_FontWriteText(menu_font_normal, copyright_text, 0, y, &subscreen43);
   y += V_FontStringHeight(menu_font_normal, "");


   // Scroll the credits
   constexpr int   WAIT_TICS              = TICRATE * 2;
   constexpr float PIXELS_PER_GAMETIC     = 0.5f;

   static const int maximumDesiredY = SCREENHEIGHT - GameModeInfo->creditY;
   static const int maximumY        = y;
   static const int maximumScroll   = maximumY - maximumDesiredY;
   static const int transitionTics  = int(maximumScroll / PIXELS_PER_GAMETIC);
   static const int loopTics        = WAIT_TICS + transitionTics + WAIT_TICS + transitionTics;
   if(maximumY > SCREENHEIGHT)
   {
      // Goes _/-\ then loops
      const int sequenceTic = (gametic - startTic) % loopTics;
      if(sequenceTic < WAIT_TICS)
         yOffset = 0;
      else if(sequenceTic < WAIT_TICS + transitionTics)
         yOffset = int((sequenceTic - WAIT_TICS) * PIXELS_PER_GAMETIC);
      else if(sequenceTic < WAIT_TICS + transitionTics + WAIT_TICS)
         yOffset = maximumScroll;
      else
         yOffset = int((loopTics - sequenceTic) * PIXELS_PER_GAMETIC);

      yOffset = emin(maximumScroll, yOffset);
   }
}

extern menuwidget_t helpscreen_widget; // actually just below...

static void MN_HelpDrawer()
{
   if(helpscreens[viewing_helpscreen].Drawer)
   {
      helpscreens[viewing_helpscreen].Drawer();   // call drawer
   }
   else
   {
      // haleyjd: support raw lumps
      int lumpnum = helpscreens[viewing_helpscreen].lumpnum;

      // if the screen is larger than 4:3, we need to draw pillars ourselves,
      // as D_Display won't be able to know if this widget is fullscreen or not
      // in time to do it as usual.
      D_DrawPillars();

      // haleyjd 05/18/09: use smart background drawer
      V_DrawFSBackground(&subscreen43, lumpnum);
   }
}

// haleyjd 05/29/06: record state of menu activation
bool help_prev_menuactive;

static bool MN_HelpResponder(event_t *ev, int action)
{
   int *menuSounds = GameModeInfo->menuSounds;
   
   if(ev->type != ev_keydown) return false;
   
   if(action == ka_menu_previous)
   {
      // go to previous screen
      // haleyjd: only make sound if we really went back
      viewing_helpscreen--;
      if(viewing_helpscreen < 0)
      {
         // cancel
         goto cancel;
      }
      else
         S_StartInterfaceSound(menuSounds[MN_SND_PREVIOUS]);
   }

   if(action == ka_menu_confirm)
   {
      // go to next helpscreen
      viewing_helpscreen++;
      if(viewing_helpscreen >= num_helpscreens)
      {
         // cancel
         goto cancel;
      }
      else
         S_StartInterfaceSound(menuSounds[MN_SND_COMMAND]);
   }

   if(action == ka_menu_toggle)
   {
      // cancel helpscreen
cancel:
      MN_PopWidget();
      // haleyjd 05/29/06: maintain previous menu activation state
      if(!help_prev_menuactive)
         menuactive = false;
      S_StartInterfaceSound(menuSounds[MN_SND_DEACTIVATE]);
   }

   // always eatkey
   return false;
}

menuwidget_t helpscreen_widget = {MN_HelpDrawer, MN_HelpResponder, nullptr, true};

CONSOLE_COMMAND(help, 0)
{
   // haleyjd 05/29/06: record state of menu activation
   help_prev_menuactive = menuactive;

   MN_ActivateMenu();
   MN_FindHelpScreens();        // search for help screens
   
   // hook in widget to display menu
   MN_PushWidget(&helpscreen_widget);
   
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
   MN_PushWidget(&helpscreen_widget);
   
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

template <mapColorType_e mapColorType>
static void MN_mapColorDrawer()
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
   
   V_DrawPatch(x, y, &subscreen43, patch);
   
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
   V_DrawBlock(x, y, &subscreen43, BLOCK_SIZE, BLOCK_SIZE, block);

   if constexpr(mapColorType == mapColorType_e::optional)
   {
      if(!selected_colour)
         V_DrawPatch(x+1, y+1, &subscreen43, PatchLoader::CacheName(wGlobalDir, "M_PALNO", PU_CACHE));
   }
}

static bool MN_MapColourResponder(event_t *ev, int action)
{
   if(action == ka_menu_left)
      selected_colour--;

   if(action == ka_menu_right)
      selected_colour++;
   
   if(action == ka_menu_up)
      selected_colour -= 16;
   
   if(action == ka_menu_down)
      selected_colour += 16;
   
   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      // cancel colour selection
      MN_PopWidget();
      return true;
   }

   if(action == ka_menu_confirm)
   {
      static char tempstr[128];
      snprintf(tempstr, sizeof(tempstr), "%i", selected_colour);
     
      // run command
      C_RunCommand(colour_command, tempstr);
      
      // kill selector
      MN_PopWidget();
      return true;
   }

   if(selected_colour < 0) 
      selected_colour = 0;
   if(selected_colour > 255) 
      selected_colour = 255;
   
   return true; // always eat key
}

template <mapColorType_e mapColorType>
menuwidget_t colour_widget =
{
   MN_mapColorDrawer<mapColorType>,
   MN_MapColourResponder,
   nullptr,
   true
};

void MN_SelectColor(const char *variable_name, const mapColorType_e color_type)
{
   if(color_type == mapColorType_e::mandatory)
      MN_PushWidget(&colour_widget<mapColorType_e::mandatory>);
   else
      MN_PushWidget(&colour_widget<mapColorType_e::optional>);

   colour_command = C_GetCmdForName(variable_name);
   selected_colour = *(int *)colour_command->variable->variable;
}

//=============================================================================
//
// Font Test Widget
//

static vfont_t *testfont; // font to test
static qstring  teststr;  // string for test

static void MN_fontTestDrawer()
{
   int totalHeight = SCREENHEIGHT - testfont->absh * 2;
   int itemHeight  = totalHeight / (CR_BUILTIN + 1);   
   int x = 160 - V_FontStringWidth(testfont, teststr.constPtr())/2;
   int y = testfont->absh;

   V_DrawBackground(mn_background_flat, &vbscreen);
   
   for(int i = 0; i <= CR_BUILTIN; i++)
   {
      V_FontWriteTextColored(testfont, teststr.constPtr(), i, x, y);
      y += itemHeight;
   }
}

static bool MN_fontTestResponder(event_t *ev, int action)
{
   if(action == ka_menu_toggle || action == ka_menu_previous)
   {
      // exit widget
      MN_PopWidget();
      teststr.freeBuffer();
   }

   return true;
}

static menuwidget_t fonttest_widget = 
{
   MN_fontTestDrawer, 
   MN_fontTestResponder, 
   nullptr,
   true
};

CONSOLE_COMMAND(mn_testfont, 0)
{
   vfont_t *font;
   const char *fontName;

   if(Console.argc < 1)
   {
      C_Puts(FC_ERROR "Usage: mn_testfont fontname [message]");
      return;
   }
   
   fontName = Console.argv[0]->constPtr();
   if(!(font = E_FontForName(fontName)))
   {
      C_Printf(FC_ERROR "Unknown font %s\n", fontName);
      return;
   }

   if(Console.argc >= 2)
      teststr = *Console.argv[1];
   else
      teststr = "ABCDEFGHIJKL";

   testfont = font;   
   MN_PushWidget(&fonttest_widget);
}

// EOF
