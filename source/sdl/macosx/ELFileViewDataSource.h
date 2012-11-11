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
// Data source object for the file table view
//
//----------------------------------------------------------------------------

#import <Foundation/Foundation.h>

//
// ELFileViewDataSource
//
// Data source for the file view table
//
@interface ELFileViewDataSource : NSObject
{
   NSMutableArray *array;   // array for the data
   IBOutlet id owner;
}
@property (assign) NSMutableArray *array;

@end

// EOF

