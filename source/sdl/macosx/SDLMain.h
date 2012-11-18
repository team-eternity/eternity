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
// Header file for front-end interface
//
//----------------------------------------------------------------------------

//
// FILE BASED ON:
//
//   SDLMain.m - main entry point for our Cocoa-ized SDL app
//     Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
//		Non-NIB-Code & other changes: Max Horn <max@quendi.de>
//
//  Feel free to customize this file to suit your needs
//


#ifndef SDLMAIN_H_
#define SDLMAIN_H_

#import <Cocoa/Cocoa.h>

@class ELDumpConsole, ELFileViewDataSource, ELTextFieldDelegate;

//
// SDLMain
//
// Front-end's controller
//
@interface SDLMain : NSObject
{
	IBOutlet NSWindow *window;
	NSFileManager *fileMan;
	ELDumpConsole *console;
   IBOutlet ELFileViewDataSource *fileViewDataSource; //data source for pwadView
	
	IBOutlet NSMenuItem *fileClose, *fileCloseAll, *fileOpenAllRecent;
	IBOutlet NSButton *saveAsGFS;
	
	IBOutlet NSTabView *tabView;
	IBOutlet NSPopUpButton *iwadPopUp, *gameTypePopUp;
   IBOutlet NSPopUpButton *userPopUp;  // user (player) configuration list
	IBOutlet NSTableView *pwadView;
   
	IBOutlet NSTextField *recordDemoField, *playDemoField;
	IBOutlet NSTextField *warpField, *skillField, *fragField, *timeField,
      *turboField, *dmflagField, *netField;	// heh
	IBOutlet NSTextField *otherField;
   
	IBOutlet NSButton *respawn, *fast, *nomons, *vanilla, *timedemo, *fastdemo;
	IBOutlet NSTextField *infoDisplay;
   
   IBOutlet ELTextFieldDelegate *textFieldDelegate;   // the notif. delegate
	
//	IBOutlet NSButton *launchButton;
	
	NSMenu *iwadPopMenu;
	
	NSAlert *noIwadAlert, *badIwadAlert, *nothingForGfsAlert;
	
   NSString *eeUserExt;
	NSArray *pwadTypes;
	NSMutableArray *pwadArray, *param;
  	NSMutableSet *iwadSet;  // set of IWADs
   NSMutableSet *userSet;  // set of user configurations
	
	NSMutableArray *parmRsp, *parmIwad, *parmPwad, *parmOthers, *parmWarp,
      *parmSkill, *parmFlags, *parmRecord, *parmPlayDemo,
		*parmGameType, *parmFragLimit, *parmTimeLimit, *parmTurbo, *parmDmflags,
      *parmNet;
}
@property (assign) IBOutlet NSWindow *window;
@property (readonly) NSMutableArray *pwadArray;

-(id)init;
-(void)dealloc;
-(void)initNibData;
-(void)setupTextFieldNotification;
-(IBAction)launchGame:(id)sender;
-(void)doAddUserFromURL:(NSURL *)wURL;
-(void)addUserEnded:(NSOpenPanel *)panel returnCode:(int)code
        contextInfo:(void *)info;
-(IBAction)addUser:(id)sender;
-(IBAction)removeUser:(id)sender;
-(void)doAddIwadFromURL:(NSURL *)wURL;
-(void)chooseIwadAlertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode
                 contextInfo:(void *)contextInfo;
-(IBAction)removeIwad:(id)sender;
-(IBAction)addPwad:(id)sender;
-(IBAction)addIwad:(id)sender;
-(IBAction)addAllRecentPwads:(id)sender;
-(void)doAddPwadFromURL:(NSURL *)wURL;
-(void)addPwadEnded:(NSOpenPanel *)panel returnCode:(int)code contextInfo:(void
                                                                        *)info;
-(IBAction)removeAllPwads:(id)sender;
-(IBAction)removePwad:(id)sender;
-(IBAction)chooseRecordDemo:(id)sender;
-(void)chooseRecordDidEnd:(NSSavePanel *)sheet returnCode:(int)returnCode
              contextInfo:(void *)contextInfo;
-(IBAction)choosePlayDemo:(id)sender;
-(void)choosePlayDidEnd:(NSOpenPanel *)panel returnCode:(int)code
            contextInfo:(void *)info;
-(IBAction)clearPlayDemo:(id)sender;
-(IBAction)clearRecordDemo:(id)sender;
-(IBAction)clearNetwork:(id)sender;

-(IBAction)saveAsGFS:(id)sender;


// Kind of ugly... but safe
-(void)updateParmIwad:(id)sender; // called by pop-up button command
-(void)updateParmPwad:(id)sender;	// called by table addition/removal
-(IBAction)updateParmRsp:(id)sender;
-(IBAction)updateParmOthers:(id)sender;	// called by text field
-(IBAction)updateParmWarp:(id)sender;	// called by text field
-(IBAction)updateParmSkill:(id)sender;	// called by text field
-(IBAction)updateParmFlags:(id)sender;	// called by any check box
-(IBAction)updateParmRecord:(id)sender;	// called by text field
-(IBAction)updateParmPlayDemo:(id)sender;	// called by text field, check boxes
-(IBAction)updateParmGameType:(id)sender;	// called by pop-up menu command
-(IBAction)updateParmFragLimit:(id)sender;
-(IBAction)updateParmTimeLimit:(id)sender;
-(IBAction)updateParmTurbo:(id)sender;
-(IBAction)updateParmDmflags:(id)sender;
-(IBAction)updateParmNet:(id)sender;
-(IBAction)updateParameters:(id)sender;

-(IBAction)goGetHelp:(id)sender;

-(void)saveDefaults;
-(void)loadDefaults;

@end

#endif /* SDLMAIN_H_ */

// EOF

