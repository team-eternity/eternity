// Emacs style mode select -*- C++ -*-
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
@synthesize log, textView;

-(id)initWithWindowNibName:(NSString *)windowNibName
{
	if([super initWithWindowNibName:windowNibName])
	{
		log = [[NSMutableString alloc] init];
		outHandle = [NSFileHandle fileHandleWithStandardOutput];
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
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(dataReady:)
												 name:NSFileHandleReadCompletionNotification
											   object:outHandle];
	
	[outHandle readInBackgroundAndNotify];
}


-(void)dataReady:(NSNotification *)notification
{
	NSData *data = [[notification userInfo]
					objectForKey:NSFileHandleNotificationDataItem];
	NSFileHandle *handle = [notification object];
	if([data length])
	{
		NSString *string = [[NSString alloc] initWithData:data
												 encoding:NSUTF8StringEncoding];
		[log appendString:string];
		[string release];
		
		attrStr = [[NSAttributedString alloc] initWithString:log];
		
		[[textView textStorage] beginEditing];
		[[textView textStorage] setAttributedString:attrStr];
		[[textView textStorage] endEditing];
		
		[attrStr release];
		
		[textView scrollRangeToVisible:NSMakeRange([[textView string] length], 0)];
		
		[handle readInBackgroundAndNotify];
	}
	else
	{
		
		[[NSNotificationCenter defaultCenter]
		 removeObserver:self
		 name:NSFileHandleReadCompletionNotification
		 object:[notification object]];
	}
}

-(void)showInstantLog
{
	[outHandle seekToFileOffset:0];
	
	NSData *data = [outHandle readDataToEndOfFile];
	
	NSString *string = [[NSString alloc] initWithData:data
											 encoding:NSUTF8StringEncoding];
	
	attrStr = [[NSAttributedString alloc] initWithString:string];
	
	[string release];
	
	[[textView textStorage] beginEditing];
	[[textView textStorage] setAttributedString:attrStr];
	[[textView textStorage] endEditing];
	
	[attrStr release];
	
	[textView scrollRangeToVisible:NSMakeRange([[textView string] length], 0)];
}
@end

// EOF

