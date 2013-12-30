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
// Command-line argument unit
//
//----------------------------------------------------------------------------

#import "ELCommandLineArgument.h"

@implementation ELCommandLineArgument
@synthesize extraWords, identifier, enabled;

//
// initWithIdentifier:
//
// Initialize with an identifier and empty extra words
//
-(id)initWithIdentifier:(NSString *)aString
{
	self = [super init];
	if(self)
	{
		enabled		= YES;									// start enabled, but allow disable
		identifier	= [aString retain];					// setup the identifier
		extraWords	= [[NSMutableArray alloc] init];	// initialize the array
	}
	return self;
}

//
// dealloc
//
-(void)dealloc
{
	[extraWords release];
	[super dealloc];
}

//
// argWithIdentifier:
//
// Get without allocking
//
+(ELCommandLineArgument *)argWithIdentifier:(NSString *)aString
{
	return [[[ELCommandLineArgument alloc] initWithIdentifier:aString] autorelease];
}

//
// responseFileString:
//
// Generate response argument string
//
-(NSString *)responseFileString:(BOOL)withQuotes
{
	NSMutableString *ret = [NSMutableString stringWithString:[self identifier]];
	
	if([extraWords count] <= 0)
		return ret;
	
	[ret appendString:[extraWords objectAtIndex:0]];
	
	if(withQuotes)
	{
		
		NSRange whitespaceRange = [ret rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		
		if(whitespaceRange.location != NSNotFound)
		{
			// has white space
			[ret insertString:@"\"" atIndex:0];
			[ret appendString:@"\""];
		}
	}
	
	return ret;
}

//
// generalArgString:
//
// Generate general argument string
//
-(NSString *)generalArgString:(BOOL)withQuotes
{
	// format: identifier, followed by each extraWord component, all separated by " ". If any of the extraWord components has whitespaces, put quotes around them
	
	NSMutableString *ret;
	if(![identifier isEqualToString:@" "])
	{
		ret = [NSMutableString stringWithString:identifier];
		[ret appendString:@" "];
	}
	else
		ret = [NSMutableString string];
	NSString *component;
	NSRange whitespaceRange;
	
	for(component in extraWords)
	{
		// do your job

		whitespaceRange = [component rangeOfCharacterFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		if(withQuotes && whitespaceRange.location != NSNotFound)
		{
			[ret appendString:@"\""];
			[ret appendString:component];
			[ret appendString:@"\""];
		}
		else
			[ret appendString:component];
		[ret appendString:@" "];
	}
	
	return ret;
}

@end
