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

#import "ELDumpConsole.h"
#import "LauncherController.h"

//
// NSPanel (ConsolePanel)
//
// Category of NSPanel that merely composites a few panel-setting instructions
//
@interface NSPanel (ConsolePanel)
// Sets it as an infopanel
-(void)setAsPanel:(BOOL)flag;
@end

@implementation NSPanel (ConsolePanel)

//
// setAsPanel:
//
// Sets it as an infopanel
//
-(void)setAsPanel:(BOOL)flag
{
	[self setFloatingPanel:flag];
	[self setHidesOnDeactivate:flag];
}

@end

//
// ELDumpConsole
//
// Console that displays Eternity Engine's standard output and error
//
@implementation ELDumpConsole

@synthesize masterOwner;

//
// initWithWindowNibName:
//
// Standard initializator
//
-(id)initWithWindowNibName:(NSString *)windowNibName
{
	if((self = [super initWithWindowNibName:windowNibName]))
	{
		outputMessageString = nil;
	}
	
	return self;
}

//
// dealloc
//
-(void)dealloc
{
	[outputMessageString release];
	[super dealloc];
}

//
// taskComplete:
//
// Called when Eternity Engine ends
//
- (void)taskComplete:(NSNotification *)notification
{
	NSTask *task = [notification object];
	[masterOwner taskEnded];
	int term = [task terminationStatus];
	if (term == 0)
		[pwindow orderOut:self];
	else
	{
		[errorMessage setHidden:NO];
		[pwindow orderFront:self];
		[pwindow setAsPanel:YES];
	}
	[[NSNotificationCenter defaultCenter] removeObserver:self name:NSTaskDidTerminateNotification object:[notification object]];
}

//
// startLogging:
//
// Runs the specified task and sets up the console to accept its output
//
-(void)startLogging:(NSTask *)engineTask
{
	// load the window
	[self window];
	
	[errorMessage setHidden:YES];
	[pwindow orderFront:self];
	[pwindow setAsPanel:NO];
	
	[pwindow center];

	[textField setFont:[NSFont fontWithName:@"Andale Mono" size:12]];
   [textField setTextColor:[NSColor whiteColor]];
   [textField setBackgroundColor:[NSColor blackColor]];
	
	pipe = [NSPipe pipe];
	
	inHandle = [pipe fileHandleForReading];
	[engineTask setStandardOutput:pipe];
	[engineTask setStandardError:pipe];
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(dataReady:) name:NSFileHandleReadCompletionNotification object:inHandle];
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(taskComplete:) name:NSTaskDidTerminateNotification object:nil];
	
	[engineTask launch];
	[inHandle readInBackgroundAndNotify];
	
}

//
// dataReady:
//
// Message came
//
-(void)dataReady:(NSNotification *)notification
{

	NSData *data = [[notification userInfo]
					objectForKey:NSFileHandleNotificationDataItem];

	if([data length])
	{
		if(!outputMessageString)
			outputMessageString = [[NSMutableString alloc] init];
		
		[outputMessageString setString:[NSString stringWithCString:[data bytes] encoding:NSUTF8StringEncoding]];
		
		[textField setEditable:YES];
		[textField setSelectedRange:NSMakeRange([[textField string] length], 0)];
		[textField insertText:outputMessageString];
		[textField setEditable:NO];
		[textField scrollToEndOfDocument:self];
		
		[inHandle readInBackgroundAndNotify];
	}
	else
	{
		
		[[NSNotificationCenter defaultCenter] removeObserver:self name:NSFileHandleReadCompletionNotification object:[notification object]];
	}
}

//
// Makes the console visible, from a user command
//
-(void)makeVisible
{
   [self window]; // make sure the window is loaded
   [pwindow orderFront:self];
   [pwindow setAsPanel:YES];
}

@end

// EOF

