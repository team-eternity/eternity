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

#import "ELAboutController.h"

static NSString* s_versionString;
static NSString* s_authorsContent;
static NSString* s_copyingContent;

@implementation ELAboutController

+(void)initialize
{
	NSObject* obj = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleShortVersionString"];
	
	if([obj isKindOfClass:[NSString class]])
	{
		s_versionString = [[NSString stringWithFormat:@"Version %@", (NSString*)obj] retain];
	}
	else
	{
		s_versionString = @"";	// just empty it
	}
	
	NSString* path = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"AUTHORS"];
	s_authorsContent = [[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL] retain];
	path = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"COPYING"];
	s_copyingContent = [[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:NULL] retain];
	
}

- (id)initWithWindow:(NSWindow *)window
{
    self = [super initWithWindow:window];
    if (self)
	 {
		 m_contentType = ELAboutAuthors;
    }
    return self;
}

///
/// Called on NIB initialization
///
- (void)windowDidLoad
{
    [super windowDidLoad];
	
	// Setup the version text field
   
	[m_versionLabel setStringValue:s_versionString];
	
	[m_contentView setFont:[NSFont fontWithName:@"Monaco" size:10]];
	[m_contentView setString:s_authorsContent];
}

-(IBAction)viewLicenseClicked:(id)sender
{
	switch (m_contentType)
	{
		case ELAboutAuthors:
			m_contentType = ELAboutLicense;
			[m_contentView setString:s_copyingContent];
			[m_switchButton setTitle:@"View Authors"];

			break;
		case ELAboutLicense:
			m_contentType = ELAboutAuthors;
			[m_contentView setString:s_authorsContent];
			[m_switchButton setTitle:@"View License"];
			break;
			
		default:
			break;
	}
}

@end
