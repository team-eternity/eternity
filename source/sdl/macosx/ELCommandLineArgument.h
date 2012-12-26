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
// Command-line argument unit
//
//----------------------------------------------------------------------------

#import <Foundation/Foundation.h>

//
// ELCommandLineArgument
//
// Eternity launch command line argument unit
//
@interface ELCommandLineArgument : NSObject
{
	NSString *identifier;			// argument name (prefixed with hyphen and allowing )
	NSMutableArray *extraWords;	// extra components (such as warp level or file names)
	BOOL enabled;						// enabled flag (true by default)
}

// Make the extraWords array accessible
@property (readonly) NSMutableArray *extraWords;

// Same for the identifier
@property (readonly ) NSString *identifier;

// Allow enable modification
@property (assign) BOOL enabled;

// Initialize with an identifier and empty extra words
-(id)initWithIdentifier:(NSString *)aString;

// Get without allocking
+(ELCommandLineArgument *)argWithIdentifier:(NSString *)aString;

// Generate response argument string
-(NSString *)responseFileString:(BOOL)withQuotes;

// Generate general argument string
-(NSString *)generalArgString:(BOOL)withQuotes;

@end
