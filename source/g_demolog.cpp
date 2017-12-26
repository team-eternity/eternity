//
// The Eternity Engine
// Copyright (C) 2017 James Haley, Ioan Chera, et al.
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
// Purpose: Isolated demo logging functions
// Authors: Ioan Chera
//

#include "d_main.h"
#include "doomstat.h"
#include "g_demolog.h"
#include "m_argv.h"

FILE *demoLogFile;

static bool demoLogLevelExited;

static void G_demoLogAtExit()
{
   if(demoLogFile && !demoLogLevelExited)
      fprintf(demoLogFile, "Quit without exiting level\n");
}

//
// Initialize the demo log file
//
void G_DemoLogInit(const char *path)
{
   // open it for appending
   demoLogFile = fopen(path, "at");
   if(!demoLogFile)
   {
      usermsg("G_DemoLogInit: failed opening '%s'\n", path);
      return;
   }
   // write date and time
   char buffer[81];
   time_t now = time(nullptr);
   struct tm *info = localtime(&now);
   strftime(buffer, 81, "%Y-%m-%d %H:%M:%S %Z", info);
   fprintf(demoLogFile, "%s\n", buffer);
   // write arguments into it
   for(int i = 1; i < myargc; ++i)
      fprintf(demoLogFile, "%s ", myargv[i]);
   fprintf(demoLogFile, "\n");
   atexit(G_demoLogAtExit);
}

//
// Write a message to the -demolog file
//
void G_DemoLog(const char *format, ...)
{
   if(!demoLogFile)
      return;
   va_list ap;
   va_start(ap, format);
   vfprintf(demoLogFile, format, ap);
   va_end(ap);
}

//
// Logs the current stats (useful to tell if a death was deliberate because
// the user considered the level finished anyway)
//
void G_DemoLogStats()
{
   int allKills = 0, allItems = 0, allSecret = 0;
   for(int i = 0; i < MAXPLAYERS; ++i)
   {
      allKills += players[i].killcount;
      allItems += players[i].itemcount;
      allSecret += players[i].secretcount;
   }
   G_DemoLog("(k: %g%%, i: %g%%, s: %g%%)",
             totalkills ? floor(100. * allKills / totalkills) : 0,
             totalitems ? floor(100. * allItems / totalitems) : 0,
             totalsecret ? floor(100. * allSecret / totalsecret) : 0);
}

//
// Sets the flag
//
void G_DemoLogSetExited(bool value)
{
   demoLogLevelExited = value;
}

//
// True if demo logging is enabled.
//
bool G_DemoLogEnabled()
{
   return demoLogFile != nullptr;
}

// EOF

