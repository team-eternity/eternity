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
// Data source object for the file table view
//
//----------------------------------------------------------------------------

#import "ELFileViewDataSource.h"
#import "LauncherController.h"

//
// ELFileViewDataSource
//
// Data source for the file view table
//
@implementation ELFileViewDataSource
@synthesize array;

//
// numberOfRowsInTableView:
//
// Get size of array for table maintenance
//
-(NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	NSInteger count=0;
	if (array)
		count=[array count];
	return count;
}

//
// tableView:objectValueForTableColumn:row:
//
// Read from pwadarray and put into table
//
-(id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
	return [array objectAtIndex:rowIndex];
}

//
// tableView:setObjectValue:forTableColumn:row:
//
// Allow renaming -file entries
//
-(void)tableView:(NSTableView *)aTableView setObjectValue:(id)anObject forTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
   // set value to pwadArray element (currently only offline files are loaded
   NSURL *URLCandidate = [NSURL fileURLWithPath:[anObject stringByExpandingTildeInPath]];
   
   [array replaceObjectAtIndex:rowIndex withObject:URLCandidate];
   [mOwner updateParameters:aTableView];
}

@end

// EOF

