// Emacs style mode select -*- Objective-C -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Ioan Chera
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
//
// Starting point for the OS X build
//
//----------------------------------------------------------------------------

#include <SDL.h>
#import <Cocoa/Cocoa.h>
#import "LauncherController.h"

// Static global variables to be passed to LaunchController
static int    gArgc;				// argument count
static char  **gArgv;			// argument contents

//
// Setup the name swap
//
#ifdef main
#undef main
#endif

//
// eternityApplicationMain
//
// Custom NSApplicationMain called from main below
//
static void eternityApplicationMain(int argc, char **argv)
{
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
	LauncherController *launchController;
	
	[NSApplication sharedApplication];
	
	// Initialize the controller
	launchController = [[LauncherController alloc] init];
	
	// Set the controller as delegate
	[NSApp setDelegate:launchController];
	
	// Setup the NIB
	[NSBundle loadNibNamed:@"MainMenu" owner:launchController];
	
	// Do the UI initialization on the controller
	[launchController initNibData:argc argVector:argv];
	
	// Run the app
	[NSApp run];
	
	// End (probably never reached?)
	[launchController release];
	[pool release];
}

//
// main
//
// Main entry point to executable - should *not* be SDL_main!
//
int main(int argc, char **argv)
{
	if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 )
	{
		gArgv = (char **) SDL_malloc(sizeof (char *) * 2);
		gArgv[0] = argv[0];
		gArgv[1] = NULL;
		gArgc = 1;
	}
	else
	{
		int i;
		gArgc = argc;
		gArgv = (char **) SDL_malloc(sizeof (char *) * (argc+1));
		for (i = 0; i <= argc; i++)
		{
			gArgv[i] = argv[i];
		}
	}
	
	eternityApplicationMain(gArgc, gArgv);
	
	return 0;
}

// EOF

