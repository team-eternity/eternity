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

#import <Foundation/Foundation.h>

@class ELCommandLineArgument;

//
// ELCommandLineArray
//
// Modified mutable array that deals with command-line arguments
//
@interface ELCommandLineArray : NSObject
{
	NSMutableArray *array;
}
@property (readonly) NSMutableArray *array;

// Return argument which has aString identifier, or nil if not found
- (ELCommandLineArgument *)argumentWithIdentifier:(NSString *)aString;

// Set argument value
- (NSMutableArray *)extraWordsForArgument:(NSString *)argName;

// See if it has argument, without creating it
- (ELCommandLineArgument *)hasArgument:(NSString *)argName;

// Count the words
- (NSUInteger)countWords;

// Generate the argv array
- (NSArray *)deployArray;

// Add argument
- (void)addArgument:(ELCommandLineArgument *)arg;

// Check if the " " parameter has the given word
- (BOOL)miscHasWord:(NSString *)wordName;



@end
