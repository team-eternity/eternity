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

#import <Cocoa/Cocoa.h>
#import "LauncherController.h"

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
	return eternityApplicationMain(argc, argv);
}

// EOF

