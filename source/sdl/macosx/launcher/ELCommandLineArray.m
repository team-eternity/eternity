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
// Command-line argument array
//
//----------------------------------------------------------------------------

#import "ELCommandLineArgument.h"
#import "ELCommandLineArray.h"

@implementation ELCommandLineArray
@synthesize array;

-(id)init
{
	self = [super init];
	if(self)
	{
		array = [[NSMutableArray alloc] init];
	}
	return self;
}

-(void)dealloc
{
	[array release];
	[super dealloc];
}

//
// hasArgument
//
// See if it has argument, without creating it
//
- (ELCommandLineArgument *)hasArgument:(NSString *)aString
{
	if([aString isEqualToString:@""])	// don't allow empty string
		return nil;
	
	ELCommandLineArgument *arg;
	
	for(arg in array)
	{
		// Be strict, be case sensitive (easier code mind you)
		if([[arg identifier] isEqualToString:aString])
		{
			return arg;	// the result will be expandable thanks to access to array
		}
	}
	
	return nil;
}

//
// argumentWithIdentifier:
//
// Return argument which has aString identifier, or nil if not found
//
- (ELCommandLineArgument *)argumentWithIdentifier:(NSString *)aString
{
	// Warning: assume all components are command line arguments
	ELCommandLineArgument *arg;
	arg = [self hasArgument:aString];
	if(arg)
		return arg;
	
	// not found: create the argument here
	arg = [ELCommandLineArgument argWithIdentifier:aString];
	[array addObject:arg];
	return arg;
}

//
// extraWordsForArgument:
//
// Set argument value
//
- (NSMutableArray *)extraWordsForArgument:(NSString *)argName
{
	ELCommandLineArgument *arg = [self argumentWithIdentifier:argName];
	return [arg extraWords];
}

//
// countWords
//
// Count the words
//
- (NSUInteger)countWords
{
	NSUInteger ret = 0;
	ELCommandLineArgument *arg;
	NSString *argid;
	
	for(arg in array)
	{
		if(![arg enabled])
			continue;
		argid = [arg identifier];
		if(![argid isEqualToString:@" "] && ![argid isEqualToString:@"@"])
			++ret;
		
		ret += [[arg extraWords] count];
	}
	
	return ret;
}

//
// deployArray
//
// Generate the argv array
//
- (NSArray *)deployArray
{
	// two special cases: params with identifiers "@" and " "
	ELCommandLineArgument *arg;
	NSString *str;
	NSMutableArray *ret = [NSMutableArray array];
	
	arg = [self hasArgument:@"@"];
	if([arg enabled])
	{
		// responses
		[ret addObject:[arg responseFileString:NO]];
	}
	// now look through all.
	
	for(arg in array)
	{
		if(![arg enabled] || [[arg identifier] isEqualToString:@"@"])
			continue;	// skip if that kind
		
		// the rest are valid
		if(![[arg identifier] isEqualToString:@" "])
			[ret addObject:[arg identifier]];
		
		for(str in [arg extraWords])
			[ret addObject:str];
	}
	
	return ret;
}

//
// addArgument:
//
// Add argument
//
- (void)addArgument:(ELCommandLineArgument *)arg
{
	ELCommandLineArgument *curArg;
	curArg = [self hasArgument:[arg identifier]];
	if(curArg)
	{
		if([curArg enabled])
			[[curArg extraWords ] setArray:[arg extraWords]];
		else
		{
			[curArg setEnabled:YES];
			[[curArg extraWords ] setArray:[arg extraWords]];
		}
	}
	else
		[array addObject:arg];
}

//
// miscHasWord:
//
// Check if the " " parameter has the given word
//
- (BOOL)miscHasWord:(NSString *)wordName
{
	ELCommandLineArgument *arg;
	arg = [self hasArgument:@" "];
	if ([arg enabled])
	{
		NSString *wrd;
		
		for(wrd in [arg extraWords])
		{
			if([wrd caseInsensitiveCompare:wordName] == NSOrderedSame)
				return YES;
		}
	}
	return NO;
}

@end
