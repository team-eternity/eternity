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
// Dump window for console output 
//
//----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>


@interface ELDumpConsole : NSWindowController
{
	IBOutlet NSTextView *textField;			// the text display
	IBOutlet NSPanel *pwindow;					// the console panel
	IBOutlet NSView *errorMessage;			// the error message icon + label
   NSPipe *pipe;									// i/o pipe with eternity engine
	NSFileHandle *inHandle;						// the pipe file handle
	id masterOwner;								// LauncherController
	NSMutableString *outputMessageString;	// what gets received
	IBOutlet NSTextField *errorLabel;		// text label with error message
}

@property (assign) id masterOwner;

-(id)initWithWindowNibName:(NSString *)windowNibName;
-(void)startLogging:(NSTask *)engineTask;
-(void)makeVisible;

@end

// EOF

