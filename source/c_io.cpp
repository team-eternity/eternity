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
//--------------------------------------------------------------------------
//
// Console I/O
//
// Basic routines: outputting text to the console, main console functions:
//                 drawer, responder, ticker, init
//
// By Simon Howard
//
// NETCODE_FIXME -- CONSOLE_FIXME: Major changes needed in this module!
//
//-----------------------------------------------------------------------------

#include "z_zone.h"

#include "d_io.h" // SoM 3/14/2002: MSCV++
#include "c_io.h"
#include "c_runcmd.h"
#include "c_net.h"

#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "g_game.h"
#include "hu_stuff.h"
#include "hu_over.h"
#include "i_system.h"
#include "i_video.h"
#include "m_compare.h"
#include "r_main.h"
#include "v_video.h"
#include "v_font.h"
#include "doomstat.h"
#include "w_wad.h"
#include "s_sound.h"
#include "d_gi.h"
#include "g_bind.h"
#include "e_fonts.h"
#include "m_qstr.h"
#include "v_block.h"
#include "v_misc.h"
#include "v_patchfmt.h"
#include "z_auto.h"

#define MESSAGES 512
// keep the last 32 typed commands
#define HISTORY 32

extern const char *shiftxform;
static void Egg();

// the messages (what you see in the console window)
static char  msgtext[MESSAGES*LINELENGTH];
static char *messages[MESSAGES];
static int   message_pos = 0;   // position in the history (last line in window)
static int   message_last = 0;  // the last message

// the command history(what you type in)
static char history[HISTORY][LINELENGTH];
static int history_last = 0;
static int history_current = 0;

static const char *inputprompt = FC_HI "$" FC_NORMAL;
// gee what is this for? :)
static const char *altprompt = FC_HI "#" FC_NORMAL;
int c_height=100;     // the height of the console
int c_speed=10;       // pixels/tic it moves
static qstring inputtext;
static char *input_point;      // left-most point you see of the command line

// for scrolling command line
static int pgup_down=0, pgdn_down=0;

// haleyjd 09/07/03: true logging capability
static FILE *console_log = nullptr;

// SoM: Use the new VBuffer system
static VBuffer cback;
static bool cbackneedfree = false;

vfont_t *c_font;
char *c_fontname;

/////////////////////////////////////////////////////////////////////////
//
// Main Console functions
//
// ticker, responder, drawer, init etc.
//

void C_InitBackdrop()
{
   const char *lumpname;
   int lumpnum;
   bool darken = true;
   fixed_t alpha = FRACUNIT/2;

   lumpname = GameModeInfo->consoleBack;

   // 07/02/08: don't make INTERPIC quite as dark
   if(!strcasecmp(lumpname, "INTERPIC"))
      alpha = FRACUNIT/4;
   
   // allow for custom console background graphic
   if(W_CheckNumForName("CONSOLE") >= 0)
   {
      ZAutoBuffer  patch;
      byte        *data;

      wGlobalDir.cacheLumpAuto("CONSOLE", patch);
      data = patch.getAs<byte *>();

      if(data && PatchLoader::VerifyAndFormat(data, patch.getSize()))
      {
         lumpname = "CONSOLE";
         darken = false; // I assume it is already suitable for use.
      }
   }
   
   if(cbackneedfree)
   {
      V_FreeVBuffer(&cback);
   }
   else
      cbackneedfree = true;

   V_InitVBuffer(&cback, video.width, video.height, video.bitdepth);
   V_SetScaling(&cback, SCREENWIDTH, SCREENHEIGHT);
   
   if((lumpnum = W_CheckNumForName(lumpname)) < 0)
      return;
   
   // haleyjd 03/30/08: support linear fullscreen graphics
   V_DrawFSBackground(&cback, lumpnum);
   if(darken)
   {
      V_ColorBlockTL(&cback, GameModeInfo->blackIndex,
                     0, 0, video.width, video.height, alpha);
   }
}

// input_point is the leftmost point of the inputtext which
// we see. This function is called every time the inputtext
// changes to decide where input_point should be.

//
// CONSOLE_FIXME: See how Quake 2 does this, it's much better.
//
static void C_updateInputPoint()
{
   for(input_point = inputtext.getBuffer();
       V_FontStringWidth(c_font, input_point) > SCREENWIDTH-20; input_point++);
}

// initialise the console

//
// C_initMessageBuffer
//
// haleyjd 05/30/11: long overdue rewrite of the console message buffer.
// Sets messages[] to point into the msgtext buffer.
//
static void C_initMessageBuffer()
{
   for(int i = 0; i < MESSAGES; i++)
      messages[i] = &msgtext[i * LINELENGTH];
}

void C_Init()
{
   // haleyjd: initialize console qstrings
   inputtext.createSize(100);

   // haleyjd: initialize console message buffer
   C_initMessageBuffer();

   Console.enabled = true;

   if(!(c_font = E_FontForName(c_fontname)))
      I_Error("C_Init: bad EDF font name %s\n", c_fontname);
   
   // sf: stupid american spellings =)
   C_NewAlias("color", "colour %opt");

   // MaxW: Aliases for benefit of ZDoom users familiar w/ its ccmds
   C_NewAlias("stopmus", "s_stopmusic %opt");
   
   C_updateInputPoint();
   
   // haleyjd
   G_InitKeyBindings();   
}

// called every tic

void C_Ticker()
{
   Console.prev_height = Console.current_height;
   Console.showprompt = true;
   
   if(gamestate != GS_CONSOLE)
   {
      // specific to half-screen version only

      // move the console toward its target
      if(D_abs(Console.current_height - Console.current_target) >= c_speed)
      {
         Console.current_height +=
            (Console.current_target < Console.current_height) ? -c_speed : c_speed;
      }
      else
      {
         Console.current_height = Console.current_target;
      }
   }
   else
   {
      // console gamestate: no moving consoles!
      Console.current_target = Console.current_height;
   }
   
   if(consoleactive)  // no scrolling thru messages when fullscreen
   {
      // scroll based on keys down
      message_pos += pgdn_down - pgup_down;
      
      // check we're in the area of valid messages        
      if(message_pos < 0)
         message_pos = 0;
      if(message_pos > message_last)
         message_pos = message_last;
   }
   
   //
   // NETCODE_FIXME -- CONSOLE_FIXME: Buffered command crap.
   // Needs complete rewrite.
   //
   
   C_RunBuffer(c_typed);   // run the delayed typed commands
   C_RunBuffer(c_menu);
}

//
// CONSOLE_FIXME: history needs to be more efficient. Use pointers 
// instead of copying strings back and forth.
//
static void C_addToHistory(qstring *s)
{
   const char *a_prompt;
   
   // display the command in console
   // hrmm wtf does this do? I dunno.
   if(gamestate == GS_LEVEL && !strcasecmp(players[0].name, "quasar"))
      a_prompt = altprompt;
   else
      a_prompt = inputprompt;
   
   C_Printf("%s%s\n", a_prompt, s->constPtr());
   
   // check for nothing typed or just spaces
   if(s->findFirstNotOf(' ') == qstring::npos)
      return;
   
   // add it to the history
   // 6/8/99 maximum linelength to prevent segfaults
   // QSTR_FIXME: change history -> qstring
   s->copyInto(history[history_last], LINELENGTH);
   history_last++;
   
   // scroll the history if necessary
   while(history_last >= HISTORY)
   {
      int i;
      
      // haleyjd 03/02/02: this loop went one past the end of history
      // and left possible garbage in the higher end of the array
      for(i = 0; i < HISTORY - 1; i++)
      {
         strcpy(history[i], history[i+1]);
      }
      history[HISTORY - 1][0] = '\0';
      
      history_last--;
   }

   history_current = history_last;
   history[history_last][0] = '\0';
}

// respond to keyboard input/events

bool C_Responder(event_t *ev)
{
   static int shiftdown;
   char ch = 0;

   // haleyjd 09/15/09: do not respond to events while the menu is up
   if(menuactive)
      return false;

   // haleyjd 05/29/06: dynamic console bindings
   int action = G_KeyResponder(ev, kac_console);
   
   if(ev->data1 == KEYD_RSHIFT)
   {
      shiftdown = (ev->type == ev_keydown);
      return consoleactive;   // eat if console active
   }

   if(action == ka_console_pageup)
   {
      pgup_down = (ev->type == ev_keydown);
      return consoleactive;
   }
   else if(action == ka_console_pagedown)
   {
      pgdn_down = (ev->type == ev_keydown);
      return consoleactive;
   }
  
   // only interested in keypresses
   if(ev->type != ev_keydown && ev->type != ev_text)
      return false;
  
   //////////////////////////////////
   // Check for special keypresses
   //
   // detect activating of console etc.
   //
   
   // activate console?
   if(action == ka_console_toggle && Console.enabled)
   {
      bool goingup = Console.current_target == c_height;

      // set console
      Console.current_target = goingup ? 0 : c_height;

      return true;
   }

   if(!consoleactive)
      return false;

   // not till it's stopped moving or stopped turning around
   if(Console.current_target < Console.current_height || Console.current_height < Console.prev_height)
      return false;

   switch(action)
   {
   ///////////////////////////////////////
   // Console active commands
   //
   // keypresses only dealt with if console active
   //

   case ka_console_tab: // tab-completion
      // set inputtext to next or previous in
      // tab-completion list depending on whether
      // shift is being held down
      inputtext = (shiftdown ? C_NextTab(inputtext) : C_PrevTab(inputtext));
      C_updateInputPoint(); // reset scrolling
      return true;
  
   case ka_console_enter: // run command
      C_addToHistory(&inputtext); // add to history
      
      if(inputtext == "r0x0rz delux0rz")
         Egg(); // shh!
      
      // run the command
      Console.cmdtype = c_typed;
      C_RunTextCmd(inputtext.constPtr());
      
      C_InitTab();            // reset tab completion
      
      inputtext.clear(); // clear inputtext now
      C_updateInputPoint();    // reset scrolling
      
      return true;

   ////////////////////////////////
   // Command history
   //  

   case ka_console_up: // previous command
      history_current =
         (history_current <= 0) ? 0 : history_current - 1;
      
      // read history from inputtext
      inputtext = history[history_current];
      
      C_InitTab();            // reset tab completion
      C_updateInputPoint();   // reset scrolling
      return true;
  
   case ka_console_down: // next command  
      history_current = (history_current >= history_last) 
         ? history_last : history_current + 1;

      // the last history is an empty string
      inputtext = ((history_current == history_last) ?
                    "" : (char *)history[history_current]);
      
      C_InitTab();            // reset tab-completion
      C_updateInputPoint();   // reset scrolling
      return true;

   /////////////////////////////////////////
   // Normal Text Input
   //
   
   case ka_console_backspace: // backspace
      inputtext.Delc();
      
      C_InitTab();            // reset tab-completion
      C_updateInputPoint();   // reset scrolling
      return true;

   default:
      break;
   }
    
   // none of these, probably just a normal character
   if(ev->type == ev_text)
      ch = ev->data1;

   // only care about valid characters
   if(ch > 31 && ch < 127)
   {
      inputtext += ch;
      
      C_InitTab();            // reset tab-completion
      C_updateInputPoint();   // reset scrolling
      return true;
   }

   if(ev->type == ev_keydown)
      return true;
   
   return false;   // dont care about this event
}


// draw the console

//
// CONSOLE_FIXME: Break messages across lines during drawing instead of
// during message insertion? It should be possible although it would 
// complicate the scrolling logic.
//

void C_Drawer(void)
{
   int y;
   int count;
   int real_height;
   static int oldscreenheight = 0;
   static int oldscreenwidth = 0;
   static int lastprevheight = 0;
   static int lastcurrheight = 0;
   static double lastlerp = 0.0;

   if(!consoleactive && !Console.prev_height)
      return;   // dont draw if not active

   // Check for change in screen res
   // SoM: Check width too.
   if(oldscreenheight != video.height || oldscreenwidth != video.width)
   {
      C_InitBackdrop();       // re-init to the new screen size
      oldscreenheight = video.height;
      oldscreenwidth = video.width;
   }

   // fullscreen console for fullscreen mode
   if(gamestate == GS_CONSOLE)
      Console.current_height = Console.prev_height = cback.scaled ? SCREENHEIGHT : cback.height;

   double lerp = M_FixedToDouble(R_GetLerp(true)); // don't rely on FixedMul and small integers;

   // We need to handle situations where lerp might decrease between two frames in a single tic
   if(lastcurrheight == lastprevheight)
      lastlerp = 0.0;
   else if(lastprevheight == Console.prev_height && lerp < lastlerp)
      lerp += lastlerp;

   int currentHeight = eclamp(int(round(Console.prev_height +
                                        lerp * (Console.current_height - Console.prev_height))), 1,
                              SCREENHEIGHT);

   lastprevheight = Console.prev_height;
   lastcurrheight = Console.current_height;
   lastlerp       = lerp;

   real_height = 
      cback.scaled ? cback.y2lookup[currentHeight - 1] + 1 :currentHeight;

   // draw backdrop
   // SoM: use the VBuffer
   V_BlitVBuffer(&vbscreen, 0, 0, &cback, 0, 
                 cback.height - real_height, cback.width, real_height);

   //////////////////////////////////////////////////////////////////////
   // draw text messages
   
   // offset starting point up by 8 if we are showing input prompt
   
   y = currentHeight -
         ((Console.showprompt && message_pos == message_last) ? c_font->absh : 0) - 1;

   // start at our position in the message history
   count = message_pos;
        
   while(1)
   {
      // move up one line on the screen
      // back one line in the history
      y -= c_font->absh;
      
      if(--count < 0) break;        // end of message history?
      if(y <= -c_font->absh) break; // past top of screen?
      
      // draw this line
      V_FontWriteText(c_font, messages[count], 1, y);
   }

   //////////////////////////////////
   // Draw input line
   //
  
   // input line on screen, not scrolled back in history?
   
   if(currentHeight > c_font->absh && Console.showprompt &&
      message_pos == message_last)
   {
      char tempstr[LINELENGTH];
      
      // if we are scrolled back, dont draw the input line
      if(message_pos == message_last)
      {
         const char *a_prompt;
         if(gamestate == GS_LEVEL && !strcasecmp(players[0].name, "quasar"))
            a_prompt = altprompt;
         else
            a_prompt = inputprompt;

         psnprintf(tempstr, sizeof(tempstr), 
                   "%s%s_", a_prompt, input_point);
      }
      
      V_FontWriteText(c_font, tempstr, 1, 
                      currentHeight - c_font->absh - 1);
   }
}

// updates the screen without actually waiting for d_display
// useful for functions that get input without using the gameloop
// eg. serial code

void C_Update(void)
{
   if(!nodrawers)
   {
      C_Drawer();
      I_FinishUpdate();
   }
}

//=============================================================================
//
// I/O Functions
//

// haleyjd 05/30/11: Number of messages to cut from the history buffer when
// the buffer is full.
#define MESSAGECUT 64

//
// C_ScrollUp
//
// Add a message to the console message history.
//
// haleyjd 05/30/11: message buffer made more efficient.
//
static void C_ScrollUp(void)
{
   if(message_last == message_pos)
      message_pos++;   
   message_last++;

   if(message_last >= MESSAGES) // past the end of the string
   {
      // cut off the oldest messages
      int i, j;
      char *tempptrs[MESSAGECUT];

      for(i = 0; i < MESSAGECUT; i++) // save off the first MESSAGECUT pointers
         tempptrs[i] = messages[i];

      for(i = MESSAGECUT; i < MESSAGES; i++) // shift down the message buffer
         messages[i - MESSAGECUT] = messages[i];

      for(i = MESSAGES - MESSAGECUT, j = 0; i < MESSAGES; i++, j++) // fill end
         messages[i] = tempptrs[j];
      
      message_last -= MESSAGECUT; // move the message boundary

      // haleyjd 09/04/02: set message_pos to message_last
      // to avoid problems with console flooding
      message_pos = message_last;
   }
   
   memset(messages[message_last], 0, LINELENGTH); // new line is empty
}

// 
// C_AddMessage
//
// haleyjd:
// Add a message to the console.
// Replaced C_AddChar.
//
// CONSOLE_FIXME: A horrible mess, needs to be rethought entirely.
// See Quake 2's system for ideas.
//
static void C_AddMessage(const char *s)
{
   const unsigned char *c;
   unsigned char *end;
   unsigned char linecolor = GameModeInfo->colorNormal + 128;
   bool lastend = false;

   // haleyjd 09/04/02: set color to default at beginning
   if(V_FontStringWidth(c_font, messages[message_last]) > SCREENWIDTH-9 ||
      strlen(messages[message_last]) >= LINELENGTH - 1)
   {
      C_ScrollUp();
   }
   end = (unsigned char *)(messages[message_last] + strlen(messages[message_last]));
   *end++ = linecolor;
   *end = '\0';

   for(c = (const unsigned char *)s; *c; c++)
   {
      lastend = false;

      // >= 128 for colours / control chars
      if(*c == '\t' || (*c > 28 && *c < 127) || *c >= 128)
      {
         if((*c >= TEXT_COLOR_MIN && *c <= TEXT_COLOR_MAX) ||
            (*c >= TEXT_COLOR_NORMAL && *c <= TEXT_COLOR_ERROR))
            linecolor = *c;

         if(V_FontStringWidth(c_font, messages[message_last]) > SCREENWIDTH-8 ||
            strlen(messages[message_last]) >= LINELENGTH - 1)
         {
            // might possibly over-run, go onto next line
            C_ScrollUp();
            end = (unsigned char *)(messages[message_last] + strlen(messages[message_last]));
            *end++ = linecolor; // keep current color on next line
            *end = '\0';
         }
         
         end = (unsigned char *)(messages[message_last] + strlen(messages[message_last]));
         *end++ = *c;
         *end = '\0';
      }
      if(*c == '\a') // alert
      {
         S_StartInterfaceSound(GameModeInfo->c_BellSound); // 'tink'!
      }
      if(*c == '\n')
      {
         C_ScrollUp();
         end = (unsigned char *)(messages[message_last] + strlen(messages[message_last]));
         *end++ = linecolor; // keep current color on next line
         *end = '\0';
         lastend = true;
      }
   }

   if(!lastend)
      C_ScrollUp();
}

// haleyjd: this function attempts to break up formatted strings 
// into segments no more than a gamemode-dependent number of 
// characters long. It'll succeed as long as the string in question 
// doesn't contain that number of consecutive characters without a 
// space, tab, or line-break, so like, don't print stupidness 
// like that. It's a console, not a hex editor...
//
// CONSOLE_FIXME: See above, this is also a mess.
//
static void C_AdjustLineBreaks(char *str)
{
   int i, count, firstspace, lastspace, len;

   firstspace = -1;

   count = lastspace = 0;

   len = static_cast<int>(strlen(str));

   for(i = 0; i < len; ++i)
   {
      if(str[i] == ' ' || str[i] == '\t')
      {
         if(firstspace == -1)
            firstspace = i;

         lastspace = i;
      }
      
      if(str[i] == '\n')
         count = lastspace = 0;
      else
         count++;

      if(count == GameModeInfo->c_numCharsPerLine)
      {
         // 03/16/01: must add length since last space to new line
         count = i - (lastspace + 1);
         
         // replace last space with \n
         // if no last space, we're screwed
         if(lastspace)
         {
            if(lastspace == firstspace)
               firstspace = 0;
            str[lastspace] = '\n';
            lastspace = 0;
         }
      }
   }
}

static void C_AppendToLog(const char *text);

//
// C_Printf
//
// Write some text 'printf' style to the console.
// The main function for I/O.
//
// CONSOLE_FIXME: As the two above, this needs to be adjusted to
// work better with any new console message buffering system that
// is designed.
//
void C_Printf(E_FORMAT_STRING(const char *s), ...)
{
   char tempstr[1024];
   va_list args;

   // haleyjd: sanity check
   if(!s)
      return;
   
   va_start(args, s);
   pvsnprintf(tempstr, sizeof(tempstr), s, args);
   va_end(args);

   // haleyjd: write this message to the log if one is open
   C_AppendToLog(tempstr); 
   
   C_AdjustLineBreaks(tempstr); // haleyjd
   
   C_AddMessage(tempstr);
}

// haleyjd 01/24/03: got rid of C_WriteText, added real C_Puts from
// prboom

void C_Puts(const char *s)
{
   C_Printf("%s\n", s);
}

void C_Separator(void)
{
   C_Puts("\x1D\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E"
          "\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E"
          "\x1E\x1E\x1E\x1F");
}

//
// C_StripColorChars
//
// haleyjd 09/07/03: Abstracted out of the below function, this
// code copies from src to dest, skipping any characters greater
// than 128 in value. This prevents console logs and dumps from
// having a bunch of meaningless extended ASCII in them.
//
static void C_StripColorChars(const unsigned char *src, 
                              unsigned char *dest, 
                              int len)
{
   int srcidx = 0, destidx = 0;

   for(; srcidx < len; srcidx++)
   {
      if(src[srcidx] < 128)
      {
         dest[destidx] = src[srcidx];
         ++destidx;
      }
   }
}

// 
// C_DumpMessages
//
// haleyjd 03/01/02: now you can dump the console to file :)
//
void C_DumpMessages(qstring *filename)
{
   int i, len;
   FILE *outfile;
   unsigned char tmpmessage[LINELENGTH];

   memset(tmpmessage, 0, LINELENGTH);

   if(!(outfile = fopen(filename->constPtr(), "a+")))
   {
      C_Printf(FC_ERROR "Could not append console buffer to file %s\n", 
               filename->constPtr());
      return;
   }

   for(i = 0; i < message_last; i++)
   {
      // strip color codes from strings
      memset(tmpmessage, 0, LINELENGTH);
      len = static_cast<int>(strlen(messages[i]));

      C_StripColorChars((unsigned char *)messages[i], tmpmessage, len);

      fprintf(outfile, "%s\n", tmpmessage);
   }

   fclose(outfile);

   C_Printf("Console buffer appended to file %s\n", filename->constPtr());
}

//
// C_OpenConsoleLog
//
// haleyjd 09/07/03: true console logging
//
void C_OpenConsoleLog(qstring *filename)
{
   // don't do anything if a log is already open
   if(console_log)
      return;

   // open file in append mode
   if(!(console_log = fopen(filename->constPtr(), "a+")))
      C_Printf(FC_ERROR "Couldn't open file %s for console logging\n", filename->constPtr());
   else
      C_Printf("Opened file %s for console logging\n", filename->constPtr());
}

//
// C_CloseConsoleLog
//
// haleyjd 09/07/03: true console logging
//
void C_CloseConsoleLog(void)
{
   if(console_log)
      fclose(console_log);

   console_log = nullptr;
}

//
// C_AppendToLog
//
// haleyjd 09/07/03: true console logging
//
static void C_AppendToLog(const char *text)
{
   // only if console logging is enabled
   if(console_log)
   {
      int len;
      unsigned char tmpmessage[1024];
      const unsigned char *src = (const unsigned char *)text;

      memset(tmpmessage, 0, 1024);
      len = static_cast<int>(strlen(text));

      C_StripColorChars(src, tmpmessage, len);

      fprintf(console_log, "%s", tmpmessage);
      fflush(console_log);
   }
}

///////////////////////////////////////////////////////////////////
//
// Console activation
//

// put smmu into console mode

void C_SetConsole(void)
{
   gamestate = GS_CONSOLE;
   gameaction = ga_nothing;
   Console.current_height = SCREENHEIGHT;
   Console.current_target = SCREENHEIGHT;
   
   S_StopMusic();                  // stop music if any
   S_StopSounds(true);             // and sounds
   G_StopDemo();                   // stop demo playing
}

// make the console go up

void C_Popup(void)
{
   Console.current_target = 0;
}

// make the console disappear

void C_InstaPopup(void)
{
   // haleyjd 10/20/08: no popup in GS_CONSOLE gamestate!
   if(gamestate != GS_CONSOLE)
      Console.current_target = Console.current_height = 0;
}

#if 0
//
// Small native functions
//

//
// sm_c_print
//
// Implements ConsolePrint(const string[], ...)
//
// Prints preformatted messages to the console
//
static cell AMX_NATIVE_CALL sm_c_print(AMX *amx, cell *params)
{
   int i, j;
   int err;
   int len;
   int totallen = 0;
   cell *cstr;
   char **msgs;
   char *msg;

   int numparams = (int)(params[0] / sizeof(cell));

   // create a string table
   msgs = (char **)(Z_Calloc(numparams, sizeof(char *), PU_STATIC, nullptr));

   for(i = 1; i <= numparams; i++)
   {      
      // translate reference parameter to physical address
      if((err = amx_GetAddr(amx, params[i], &cstr)) != AMX_ERR_NONE)
      {
         amx_RaiseError(amx, err);

         // clean up allocations before exit
         for(j = 0; j < numparams; j++)
         {
            if(msgs[j])
               Z_Free(msgs[j]);
         }
         Z_Free(msgs);

         return 0;
      }

      // get length of string
      amx_StrLen(cstr, &len);

      msgs[i-1] = (char *)(Z_Malloc(len + 1, PU_STATIC, nullptr));

      // convert from small string to C string
      amx_GetString(msgs[i-1], cstr, 0);
   }

   // calculate total strlen
   for(i = 0; i < numparams; i++)
      totallen += (int)strlen(msgs[i]);
  
   // create complete message
   msg = (char *)(Z_Calloc(1, totallen + 1, PU_STATIC, nullptr));

   for(i = 0; i < numparams; i++)
      strcat(msg, msgs[i]);

   C_Printf(msg);

   // free all allocations (fun!)
   Z_Free(msg);                    // complete message
   for(i = 0; i < numparams; i++)  // individual strings
      Z_Free(msgs[i]);   
   Z_Free(msgs);                   // string table

   return 0;
}

//
// sm_consolehr
//
// Implements ConsoleHR()
//
// Prints a <hr> style separator bar to the console --
// this is possible through ConsolePrint as well, but doesn't
// look pretty on the user end.
//
static cell AMX_NATIVE_CALL sm_consolehr(AMX *amx, cell *params)
{
   C_Separator();

   return 0;
}

static cell AMX_NATIVE_CALL sm_consolebeep(AMX *amx, cell *params)
{
   C_Printf("\a");

   return 0;
}

AMX_NATIVE_INFO cons_io_Natives[] =
{
   { "_ConsolePrint", sm_c_print },
   { "_ConsoleHR",    sm_consolehr },
   { "_ConsoleBeep",  sm_consolebeep },
   { nullptr, nullptr }
};
#endif

//
// Egg
//
// haleyjd: ??? ;)
//
static void Egg()
{   
   int x, y;
   byte *egg = static_cast<byte *>(wGlobalDir.cacheLumpName("SFRAGGLE", PU_CACHE));

   for(x = 0; x < video.width; ++x)
   {
      for(y = 0; y < video.height; ++y)
      {
         byte *s = egg + ((y % 44 * 42) + (x % 42));
         if(*s)
            cback.data[x * video.height + y] = *s;
      }
   }

   V_FontWriteText(hud_overfont, 
                   FC_BROWN "my hair looks much too\n"
                   "dark in this pic.\n"
                   "oh well, have fun!\n-- fraggle", 160, 168, &cback);
}
// EOF
