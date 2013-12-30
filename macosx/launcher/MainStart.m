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

int           gArgc;   // Argument count
const char  **gArgv;   // Argument values

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
   NSMutableArray *argArray = [[NSMutableArray alloc] initWithCapacity:(NSUInteger)argc];
   for(int i = 0; i < argc; ++i)
      [argArray addObject:[NSString stringWithCString:argv[i] encoding:NSUTF8StringEncoding]];
   
   if ([argArray count] >= 2 && [[[argArray objectAtIndex:1] substringToIndex:4] isEqualToString:@"-psn"])
      [argArray removeObjectAtIndex:1];   // remove Finder gimmick

   [argArray removeObjectAtIndex:0];   // remove executable name
   gArgArray = [argArray retain];
   [argArray release];

	return eternityApplicationMain(argc, argv);
}

// EOF

