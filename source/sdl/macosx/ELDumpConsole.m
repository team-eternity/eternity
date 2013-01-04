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
// Dump window for console output (not yet implemented)
//
//----------------------------------------------------------------------------

#import "ELDumpConsole.h"
#import "LauncherController.h"


@implementation ELDumpConsole
//@synthesize log;
@synthesize masterOwner;

-(id)initWithWindowNibName:(NSString *)windowNibName
{
	if(self = [super initWithWindowNibName:windowNibName])
	{
//		log = [[NSMutableString alloc] init];
		
	}
	
	return self;
}

-(void)dealloc
{
//	[log release];
   
	[super dealloc];
}

extern BOOL gCalledAppMainline;
extern BOOL gSDLStarted;

- (void)taskComplete:(NSNotification *)notification
{
	NSTask *task = [notification object];
	[masterOwner taskEnded];
	int term = [task terminationStatus];
	if (term == 0)
		[pwindow orderOut:self];
	else
		[pwindow orderFront:self];
	[[NSNotificationCenter defaultCenter]
	 removeObserver:self
	 name:NSTaskDidTerminateNotification
	 object:[notification object]];
	
}

-(void)startLogging:(NSTask *)engineTask
{
//	[self showWindow:nil];
	[self loadWindow];
//	[self showWindow:self];
	[pwindow setFloatingPanel:NO];
	[pwindow orderFront:self];

//	[window setBecomesKeyOnlyIfNeeded:YES];
	[textField setFont:[NSFont fontWithName:@"Andale Mono" size:12]];
//	[log setString:@""];
//	[textField setString:log];
	
	pipe = [NSPipe pipe];
	
   outHandle = [pipe fileHandleForWriting];
   inHandle = [pipe fileHandleForReading];
	
//	NSFileHandle *testfh = [engineTask standardOutput];
	
   dup2([outHandle fileDescriptor], [[engineTask standardOutput] fileDescriptor]);
   
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(dataReady:)
												 name:NSFileHandleReadCompletionNotification
											   object:inHandle];
	
	[[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(taskComplete:) name:NSTaskDidTerminateNotification object:nil];
	
	[engineTask launch];
	[inHandle readInBackgroundAndNotify];
	
}


-(void)dataReady:(NSNotification *)notification
{
	[inHandle readInBackgroundAndNotify];
	NSData *data = [[notification userInfo]
					objectForKey:NSFileHandleNotificationDataItem];
//	NSFileHandle *handle = [notification object];
//   char line[81];
	if([data length])
	{
		NSString *string = [[NSString alloc] initWithData:data
												 encoding:NSUTF8StringEncoding];
		
		[textField setEditable:YES];
		[textField setSelectedRange:NSMakeRange([[textField string] length], 0)];
		[textField insertText:string];
		[textField setEditable:NO];
		[string release];
		[textField scrollToEndOfDocument:self];
		

//      [string getCString:line maxLength:80 encoding:NSUTF8StringEncoding];
//      NSLog(string);
	}
	else
	{
		
		[[NSNotificationCenter defaultCenter]
		 removeObserver:self
		 name:NSFileHandleReadCompletionNotification
		 object:[notification object]];
	}
}
@end

// EOF

