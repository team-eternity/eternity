// Emacs style mode select -*- Objective-C -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2014 Ioan Chera
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
// About panel controller
//
//----------------------------------------------------------------------------

#import <Cocoa/Cocoa.h>

typedef enum ELAboutContent_tag
{
	ELAboutAuthors,
	ELAboutLicense,
} ELAboutContent;

@interface ELAboutController : NSWindowController
{
	IBOutlet NSTextField*	m_versionLabel;
	IBOutlet NSTextView*	m_contentView;
	IBOutlet NSButton*	m_switchButton;
	
	ELAboutContent			m_contentType;
}

-(IBAction)viewLicenseClicked:(id)sender;
@end
