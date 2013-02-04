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
//   LauncherController.m - main entry point for our Cocoa-ized SDL app
//     Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
//		Non-NIB-Code & other changes: Max Horn <max@quendi.de>
//
//  Feel free to customize this file to suit your needs
//

#import <Cocoa/Cocoa.h>

@class ELDumpConsole, ELFileViewDataSource, ELTextFieldDelegate, ELCommandLineArray;

//
// LauncherController
//
// Front-end's controller
//
@interface LauncherController : NSWindowController
{
	NSFileManager *fileMan;		// quick pointer to the file manager
	
	IBOutlet NSWindow *mainWindow;
	
	ELDumpConsole *console;
   IBOutlet ELFileViewDataSource *fileViewDataSource; //data source for pwadView
	
	IBOutlet NSMenuItem *fileClose, *fileCloseAll, *fileOpenAllRecent, *showInFinder;
	IBOutlet NSButton *saveAsGFS;
	
	IBOutlet NSTabView *tabView;
	IBOutlet NSPopUpButton *iwadPopUp, *gameTypePopUp;
	IBOutlet NSTableView *pwadView;
   
	IBOutlet NSTextField *recordDemoField;
	IBOutlet NSTextField *playDemoField;
	IBOutlet NSTextField *warpField, *skillField, *fragField, *timeField, *turboField, *dmflagField, *netField;	// heh
	IBOutlet NSTextField *otherField;
   
	IBOutlet NSButton *respawn, *fast, *nomons, *vanilla, *timedemo, *fastdemo;
	
	IBOutlet NSTextView *infoDisplay;
   
   IBOutlet ELTextFieldDelegate *textFieldDelegate;   // the notif. delegate
   
   IBOutlet NSMenuItem *docMenu;

	NSMenu *iwadPopMenu;
	
	NSAlert *noIwadAlert, *badIwadAlert, *nothingForGfsAlert, *overwriteDemoAlert, *recordDemoIsDir;

	NSArray *pwadTypes;
	NSMutableArray *pwadArray;
  	NSMutableSet *iwadSet;  // set of IWADs
   NSMutableSet *userSet;  // set of user configurations
	
	ELCommandLineArray *param;
	char *callName;
	
	NSMutableString *userPath, *basePath;
	
	NSTask *task;
	
	NSMutableArray *parmRsp, *parmIwad, *parmPwad, *parmOthers, *parmWarp, *parmSkill, *parmFlags, *parmRecord, *parmPlayDemo, *parmGameType, *parmFragLimit, *parmTimeLimit, *parmTurbo, *parmDmflags, *parmNet;
    
    BOOL dontUndo;
	
}
@property (readonly) NSMutableArray *pwadArray;

-(void)initNibData;
-(IBAction)launchGame:(id)sender;
-(IBAction)removeIwad:(id)sender;
-(IBAction)addPwad:(id)sender;
-(IBAction)addIwad:(id)sender;
-(IBAction)addAllRecentPwads:(id)sender;
-(IBAction)removeAllPwads:(id)sender;
-(IBAction)removePwad:(id)sender;
-(IBAction)chooseRecordDemo:(id)sender;
-(IBAction)clearPlayDemo:(id)sender;
-(IBAction)clearRecordDemo:(id)sender;
-(IBAction)clearNetwork:(id)sender;

-(IBAction)saveAsGFS:(id)sender;


// Kind of ugly… but safe
-(IBAction)updateParameters:(id)sender;

-(IBAction)goGetHelp:(id)sender;
-(IBAction)goGetHelpRoot:(id)sender;

-(IBAction)showUserInFinder:(id)sender;
-(IBAction)showFileInFinder:(id)sender;
-(IBAction)accessBaseFolder:(id)sender;
-(IBAction)accessOldDocs:(id)sender;

-(IBAction)showLicense:(id)sender;

-(void)taskEnded;

@end

// EOF

