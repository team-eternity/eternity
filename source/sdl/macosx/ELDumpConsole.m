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


@implementation ELDumpConsole
@synthesize log;

-(id)initWithWindowNibName:(NSString *)windowNibName
{
	if([super initWithWindowNibName:windowNibName])
	{
		log = [[NSMutableString alloc] init];
      
		
	}
	
	return self;
}

-(void)dealloc
{
	[log release];
   
	[super dealloc];
}

-(void)startLogging
{

	[[self window] makeKeyAndOrderFront:self];
	
   pipe = [NSPipe pipe];
   outHandle = [pipe fileHandleForWriting];
   inHandle = [pipe fileHandleForReading];
  
   dup2([outHandle fileDescriptor], fileno(stdout));
   
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(dataReady:)
												 name:NSFileHandleReadCompletionNotification
											   object:inHandle];
	
	[inHandle readInBackgroundAndNotify];
}


-(void)dataReady:(NSNotification *)notification
{
   [inHandle readInBackgroundAndNotify];
	NSData *data = [[notification userInfo]
					objectForKey:NSFileHandleNotificationDataItem];
//   char line[81];
	if([data length])
	{
		NSString *string = [[NSString alloc] initWithData:data
												 encoding:NSUTF8StringEncoding];
		[log appendString:string];
		[string release];
		[textField setStringValue:log];
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

