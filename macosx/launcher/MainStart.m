// Emacs style mode select -*- Objective-C -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Ioan Chera
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//
// Starting point for the OS X build
//
//----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>
#import "LauncherController.h"

BOOL     g_hasParms;
NSArray *gArgArray;

//
// eternityApplicationMain
//
// Custom NSApplicationMain called from main below
//
static int eternityApplicationMain(int argc, const char **argv)
{
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
	LauncherController *launchController;
	
	[NSApplication sharedApplication];
	
	// Initialize the controller
	launchController = [[LauncherController alloc] init];
	
	// Set the controller as delegate
	[NSApp setDelegate:launchController];
	
	// Setup the NIB
	[NSBundle loadNibNamed:@"Launcher" owner:launchController];
	
	// Do the UI initialization on the controller
	[launchController initNibData];
	
	// Run the app
	[NSApp run];
	
	// End (probably never reached?)
	[launchController release];
	[pool release];
	
	return 0;
}

//
// main
//
// Main entry point to executable - should *not* be SDL_main!
//
int main(int argc, const char **argv)
{
   // Copy the arguments into a global variable
   // This is passed if we are launched by double-clicking
   NSMutableArray *argArray = [[[NSMutableArray alloc] initWithCapacity:(NSUInteger)argc] autorelease];
   for(int i = 1; i < argc; ++i)
   {
      if(strlen(argv[i]) == 0 || strncmp(argv[i], "-psn", 4) == 0)
         continue;
      if(argv[i][0] == '-')   // Found a parameter!
         g_hasParms = YES;
      [argArray addObject:[NSString stringWithCString:argv[i] encoding:NSUTF8StringEncoding]];
   }
   
   gArgArray = [argArray retain];

	return eternityApplicationMain(argc, argv);
}

// EOF

