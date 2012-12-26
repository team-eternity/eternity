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
// Source file for front-end interface
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

// TODO:
//
// * Backspace (delete) key for removing file entries
// * Warn user on overwriting demo at "start game" moment, not entry moment
// * Options to copy the whole parameter list to clipboard (not by selecting
//   directly)
// * Full help instead of Internet link
// * Correct menu bar
// * Some artwork background

#include <SDL.h>
#import "LauncherController.h"
#import "ELDumpConsole.h"
#import "ELFileViewDataSource.h"
#import "ELCommandLineArray.h"
#import "ELCommandLineArgument.h"
#include <sys/param.h> /* for MAXPATHLEN */
#include <unistd.h>

//
// For some reaon, Apple removed setAppleMenu from the headers in 10.4,
// but the method still is there and works. To avoid warnings, we declare
// it ourselves here.
//
//
@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end


//
// Use this flag to determine whether we use CPS (docking) or not
//
//#define		SDL_USE_CPS		1
#ifdef SDL_USE_CPS
//
// Portions of CPS.h
//
typedef struct CPSProcessSerNum
{
	UInt32		lo;
	UInt32		hi;
} CPSProcessSerNum;

extern OSErr	CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr 	CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32
                                            _arg2, UInt32 _arg3, UInt32 _arg4, 
                                            UInt32 _arg5);
extern OSErr	CPSSetFrontProcess( CPSProcessSerNum *psn);

#endif // SDL_USE_CPS

static int status;   // SDL MAINLINE EXIT STATUS
static BOOL gCalledAppMainline = FALSE;
static BOOL gSDLStarted;	// IOAN 20120616

//
// IOAN 20121225: currently disabled. just use NSApplication lol
//
#if 0
	//
	// IOAN 20120616: changed from a category to a subclass
	//
	// NSMyApplication: derived class
	//
	@interface NSMyApplication : NSApplication
	- (void)terminateSDL:(id)sender;
	@end

	@implementation NSMyApplication : NSApplication

	//
	// terminate:
	//
	// Called when application is ending. Issue SDL ending if game started.
	//
	/*
	- (void)terminate:(id)sender
	{
		if(gCalledAppMainline)
			[self terminateSDL:sender];
		else
			[super terminate:sender];
	}
	*/
	//
	// terminateSDL:
	//
	// Invoked from the Quit menu item. Equivalent to Eternity quit.
	// FIXME: make it quit instantly.
	//
	- (void)terminateSDL:(id)sender	// IOAN 20120616: changed SDL name
	{
		 // Post a SDL_QUIT event
		exit(-1);
		//
		// SDL_Event event;
		// event.type = SDL_QUIT;
		// SDL_PushEvent(&event);
		
	}
	@end
#endif

@interface LauncherController ()
-(void)loadDefaults;
-(void)saveDefaults;
-(void)doAddPwadFromURL:(NSURL *)wURL;
@end

//
// LauncherController
//
// The main class of the application, the application's delegate
//
@implementation LauncherController

@synthesize window, pwadArray;

//
// dealloc
//
// Free memory
//
-(void)dealloc
{
	[iwadSet release];
	[pwadTypes release];
	[iwadPopMenu release];
	[pwadArray release];
   [userSet release];
   
	[noIwadAlert release];
	[badIwadAlert release];
	[nothingForGfsAlert release];
	
	[parmRsp release];
	[parmIwad release];
	[parmPwad release];
	[parmOthers release];
	[parmWarp release];
	[parmSkill release];
	[parmFlags release];
	[parmRecord release];
	[parmPlayDemo release];
	[parmGameType release];
	[parmFragLimit release];
	[parmTimeLimit release];
	[parmTurbo release];
	[parmDmflags release];
	[parmNet release];
	
	[param release];
	[basePath release];
	[userPath release];
	
	[console release];
	
	[super dealloc];
}

//
// init
//
// Initialize members
//
-(id)init
{
	if([super init])
	{
		fileMan = [NSFileManager defaultManager];
		iwadSet = [[NSMutableSet alloc] initWithCapacity:0];
		pwadTypes = [[NSArray alloc] initWithObjects:@"cfg", @"bex", @"deh", 
                   @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", @"pk3",
                   @"pke", @"zip", nil];
		iwadPopMenu = [[NSMenu alloc] initWithTitle:@"Choose IWAD"];
		pwadArray = [[NSMutableArray alloc] initWithCapacity:0];
      userSet = [[NSMutableSet alloc] initWithCapacity:0];
		
		noIwadAlert = [[NSAlert alloc] init];
		[noIwadAlert addButtonWithTitle:@"Choose IWAD"];
		[noIwadAlert addButtonWithTitle:@"Cancel"];
		[noIwadAlert setMessageText:@"No IWAD file prepared."];
		[noIwadAlert setInformativeText:
       @"You need to choose a main game WAD before playing Eternity."];
		[noIwadAlert setAlertStyle:NSInformationalAlertStyle];
		
		badIwadAlert = [[NSAlert alloc] init];
		[badIwadAlert addButtonWithTitle:@"Try Another"];
		[badIwadAlert addButtonWithTitle:@"Cancel"];
		[badIwadAlert setMessageText:@"Selected file is not a valid IWAD file."];
		[badIwadAlert setInformativeText:
       @"You cannot load patch WAD (PWAD) files," 
		 " or the selected file may be corrupted or invalid. You need to load a "
       "main WAD that comes shipped with the game."];
		[badIwadAlert setAlertStyle:NSInformationalAlertStyle];
		
		nothingForGfsAlert = [[NSAlert alloc] init];
		[nothingForGfsAlert addButtonWithTitle:@"OK"];
		[nothingForGfsAlert setMessageText:
       @"There are no files to list in a GFS."];
		[nothingForGfsAlert setInformativeText:
       @"A GFS needs to refer to an IWAD and/or to a set of add-on files."];
		[nothingForGfsAlert setAlertStyle:NSInformationalAlertStyle];
		
		parmRsp = [[NSMutableArray alloc] initWithCapacity:0];
		parmIwad = [[NSMutableArray alloc] initWithCapacity:0];
		parmPwad = [[NSMutableArray alloc] initWithCapacity:0];
		parmOthers = [[NSMutableArray alloc] initWithCapacity:0];
		parmWarp = [[NSMutableArray alloc] initWithCapacity:0];
		parmSkill = [[NSMutableArray alloc] initWithCapacity:0];
		parmFlags = [[NSMutableArray alloc] initWithCapacity:0];
		parmRecord = [[NSMutableArray alloc] initWithCapacity:0];
		parmPlayDemo = [[NSMutableArray alloc] initWithCapacity:0];
		parmGameType = [[NSMutableArray alloc] initWithCapacity:0];
		parmFragLimit = [[NSMutableArray alloc] initWithCapacity:0];
		parmTimeLimit = [[NSMutableArray alloc] initWithCapacity:0];
		parmTurbo = [[NSMutableArray alloc] initWithCapacity:0];
		parmDmflags = [[NSMutableArray alloc] initWithCapacity:0];
		parmNet = [[NSMutableArray alloc] initWithCapacity:0];
		
		param = [[ELCommandLineArray alloc] init];
		basePath = [[NSMutableString alloc] init];
		userPath = [[NSMutableString alloc] init];
		
		console = [[ELDumpConsole alloc] initWithWindowNibName:@"DumpConsole"];
	}
	return self;
}

//
// initNibData:argVector:
//
// Added after Nib got loaded. Generic initialization that wouldn't go in init
//
-(void)initNibData:(int)argc argVector:(char **)argv
{
	// TODO: depending on argc and argv, either jump directly to launchGame (giving it correct arguments), or initialize param with no elements and continue on waiting user input. For now, assume empty entries
	
	// Set the pop-up button
	[iwadPopUp setMenu:iwadPopMenu];


	// set the parameter array
	callName = argv[0];	// Argument 0: file name
	// [param setFromStringArray:parmArray];
	
   // TODO: Allow file list view to accept Finder files
   // [pwadView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
	
   // Set array for data source
   [fileViewDataSource setArray:pwadArray];
   
	[self loadDefaults];
}

//
// -applicationDataDirectory
//
// The "library/application support" directory
//
// Copied from help HTML: https://developer.apple.com/library/mac/#documentation/FileManagement/Conceptual/FileSystemProgrammingGUide/AccessingFilesandDirectories/AccessingFilesandDirectories.html#//apple_ref/doc/uid/TP40010672-CH3-SW3
//
- (NSURL*)applicationDataDirectory
{
	NSArray* possibleURLs = [fileMan URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask];
	NSURL* appSupportDir = nil;
	NSURL* appDirectory = nil;
	
	if ([possibleURLs count] >= 1)
	{
		// Use the first directory (if multiple are returned)
		appSupportDir = [possibleURLs objectAtIndex:0];
	}
	
	// If a valid app support directory exists, add the
	// app's bundle ID to it to specify the final directory.
	if (appSupportDir)
	{
		NSString* appBundleID = [[NSBundle mainBundle] bundleIdentifier];
		appDirectory = [appSupportDir URLByAppendingPathComponent:appBundleID];
	}
	
	return appDirectory;
}

//
// setupWorkingDirectory:
//
// Set the working directory to the .app's directory
// (IOAN 20121104: don't use parent directory as in original)
//
// IOAN 20121203: no longer relevant if launched from Finder or not
//
- (void) setupWorkingDirectory
{
	NSString *appDataPath = [[self applicationDataDirectory] path];
	if(![fileMan fileExistsAtPath:appDataPath])
	{
		[fileMan createDirectoryAtPath:appDataPath withIntermediateDirectories:YES attributes:nil error:nil];
	}
	NSString *usrPath = [appDataPath stringByAppendingPathComponent:@"user"];
	NSString *basPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"base"];
	NSString *internalUserPath = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"user"];
	
	NSError *err = nil;
	if(![fileMan fileExistsAtPath:usrPath])
	{
		[fileMan copyItemAtPath:internalUserPath toPath:usrPath error:&err];
	}
	
	[userPath setString:usrPath];
	[basePath setString:basPath];
	
	// Now it points to Application Support
	
	// NOTE: since Eternity may still use "." directly, I'm still using the shell chdir command.
	// TODO: copy bundled prototype user data into workingDirPath, if it doesn't exist there yet
	// FIXME: make workingDirPath a member variable.
	
	chdir([appDataPath cStringUsingEncoding:NSUTF8StringEncoding]);
}

//
// application:openFile:
//
// Handle Finder interface file opening (double click, right click menu, drag-drop icon)
// IOAN 20121203: removed long winded SDL comment
//
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	if (gCalledAppMainline)
		return NO;	// ignore this document, it's too late within the game

	[self doAddPwadFromURL:[NSURL fileURLWithPath:filename]];
	[self updateParameters:self];

   return YES;
}

//
// applicationDidFinishLaunching:
//
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	// Set the working directory to the .app's parent directory
	[self setupWorkingDirectory];

	// IOAN 20121104: no need to fix the menu
	// IOAN 20121203: no longer relevant if launched from Finder or not

	return;
}

///////////////////////////////////////////////////////////////////////////////
//// GRAPHIC USER INTERFACE SECTION
///////////////////////////////////////////////////////////////////////////////

//
// windowWillClose:
//
// Close this application
//
- (void)windowWillClose:(NSNotification *)notification
{
	if(!gSDLStarted)
		[self updateParameters:self];
	
	// Save GUI configurations
	[self saveDefaults];	// FIXME: get user configurations into the preferences, 
                        // don't read them from bundle
	if(!gSDLStarted)
	{
		[NSApp terminate:window];
	}
}

//
// chooseIwadAlertDidEnd:returnCode:contextInfo:
//
-(void)chooseIwadAlertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	if(returnCode == NSAlertFirstButtonReturn)
	{
		[[alert window] orderOut:self];
		[self addPwad:self];
	}
}

//
// launchGame:
//
// Start the damned game
//
-(IBAction)launchGame:(id)sender
{
	if([iwadPopUp numberOfItems] < 1)
	{
		// No IWAD selected: look for a GFS anyway
		NSURL *wURL;
		for(wURL in pwadArray)
			if ([[[wURL path] pathExtension] caseInsensitiveCompare:@"gfs"] 
             == NSOrderedSame) 
         {
				goto iwadMightBe;	// Found a GFS. It might contain an IWAD, 
                              // so go ahead.
			}
		
		// Prompt the user to select an IWAD file
		[noIwadAlert beginSheetModalForWindow:[self window] modalDelegate:self 
      didEndSelector:@selector(chooseIwadAlertDidEnd:returnCode:contextInfo:) 
                                contextInfo:nil];
		
		return;
	}
iwadMightBe:
	
	[self updateParameters:sender];	// Update parameters. 
                                    // FIXME: do it in real-time
	
	// Add -base and user here
	ELCommandLineArgument *argBase, *argUser;
	argBase = [ELCommandLineArgument argWithIdentifier:@"-base"];
	[[argBase extraWords] addObject:basePath];
	argUser = [ELCommandLineArgument argWithIdentifier:@"-user"];
	[[argUser extraWords] addObject:userPath];
	[param addArgument:argBase];
	[param addArgument:argUser];
	
	int gArgc;
	char **gArgv;

	gArgc = [param countWords] + 1;	// call name + arguments
	NSArray *deploy = [param deployArray];
	NSString *compon;
	
	gArgv = (char **)SDL_malloc((gArgc + 1) * sizeof(char *));
	gArgv[gArgc] = NULL;
	gArgv[0] = callName;
	
	NSUInteger i = 1, len;
	for(compon in deploy)
	{
		len = [compon length] + 1;
		gArgv[i] = (char*)SDL_malloc(len*sizeof(char));

		strcpy(gArgv[i], [compon UTF8String]);
		++i;
	}
	
	// Start console
//	[console startLogging];
	
	gCalledAppMainline = TRUE;
	gSDLStarted = YES;	// IOAN 20120616
	[window close];
	
    status = SDL_main (gArgc, gArgv);
	
//	[console showInstantLog];
// We're done, thank you for playing
   if(status == 0)   // only exit if it's all ok
    exit(status);
}

//
// doAddIwadFromURL:
//
-(void)doAddIwadFromURL:(NSURL *)wURL
{
	NSInteger ind;
	if(![iwadSet containsObject:wURL])
	{
		[iwadSet addObject:wURL];
		
		NSString *iwadString = [[wURL path] stringByAbbreviatingWithTildeInPath];
		
		[iwadPopUp addItemWithTitle:iwadString];
		ind = [iwadPopUp numberOfItems] - 1;
		[[[iwadPopUp menu] itemAtIndex:ind] setToolTip:iwadString];
		[[[iwadPopUp menu] itemAtIndex:ind] setRepresentedObject:wURL];
		[[[iwadPopUp menu] itemAtIndex:ind] setImage:[[NSWorkspace sharedWorkspace] iconForFile:iwadString]];
		[[[iwadPopUp menu] itemAtIndex:ind] setAction:@selector(updateParameters:)];
		[[[iwadPopUp menu] itemAtIndex:ind] setTarget:self];
		
		[iwadPopUp selectItemAtIndex:ind];
	}
}

//
// removeIwad:
//
-(IBAction)removeIwad:(id)sender
{
	if([iwadPopUp numberOfItems] > 0)
	{
		NSURL *iwadURL = [[iwadPopUp selectedItem] representedObject];
		
		[iwadPopUp removeItemAtIndex:[iwadPopUp indexOfSelectedItem]];
		[iwadSet removeObject:iwadURL];
		
		[self updateParameters:[iwadPopUp selectedItem]];
	}
}

//
// addPwad:
//
-(IBAction)addPwad:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setAllowsMultipleSelection:true];
	[panel setCanChooseFiles:true];
	[panel setCanChooseDirectories:false];
	[panel beginSheetForDirectory:nil file:nil types:pwadTypes modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(addPwadEnded:returnCode:contextInfo:) contextInfo:nil];
}

//
// addIwad:
//
-(IBAction)addIwad:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setAllowsMultipleSelection:true];
	[panel setCanChooseFiles:true];
	[panel setCanChooseDirectories:false];
	[panel beginSheetForDirectory:nil file:nil types:pwadTypes modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(addIwadEnded:returnCode:contextInfo:) contextInfo:nil];
}

//
// addAllRecentPwads:
//
-(IBAction)addAllRecentPwads:(id)sender
{
	NSURL *wURL;
	
	for(wURL in [[NSDocumentController sharedDocumentController] recentDocumentURLs])
   {
      [self doAddPwadFromURL:wURL];
      [self updateParameters:self];
   }
}

//
// validateMenuItem:
//
-(BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	if(menuItem == fileClose || menuItem == fileCloseAll)
		return [pwadArray count] > 0;
	if(menuItem == fileOpenAllRecent)
		return [[[NSDocumentController sharedDocumentController] recentDocumentURLs] count] > 0;
	
	return YES;
}

//
// doAddPwadFromURL:
//
-(void)doAddPwadFromURL:(NSURL *)wURL
{
	[pwadArray addObject:wURL];
	[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:wURL];
	
	if(pwadView)
		[pwadView reloadData];
}
	
//
// addPwadEnded:returnCode:contextInfo:
//
-(void)addPwadEnded:(NSOpenPanel *)panel returnCode:(int)code contextInfo:(void *)info
{
	if(code == NSCancelButton)
	{
		return;
	}
	
	// Look thru the array to locate the PWAD and put that on.
	NSURL *openCandidate;
	
	for(openCandidate in [panel URLs])
	{
      [self doAddPwadFromURL:openCandidate];
      [self updateParameters:self];
	}
}

//
// addIwadEnded:returnCode:contextInfo:
//
-(void)addIwadEnded:(NSOpenPanel *)panel returnCode:(int)code contextInfo:(void *)info
{
	if(code == NSCancelButton)
	{
		return;
	}
	
	// Look thru the array to locate the IWAD and put that on.
	NSURL *openCandidate;
	
	for(openCandidate in [panel URLs])
	{
      [self doAddIwadFromURL:openCandidate];
      [self updateParameters:self];
	}
}

//
// removeAllPwads:
//
-(IBAction)removeAllPwads:(id)sender
{
	[pwadArray removeAllObjects];
	[self updateParameters:sender];
	[pwadView reloadData];
}

//
// removePwad:
//
-(IBAction)removePwad:(id)sender
{
	NSIndexSet *set = [pwadView selectedRowIndexes];
	
	if([set count] == 0)
	{
		if([pwadArray count] > 0)
		{
			
			[pwadArray removeObjectAtIndex:0];
			[self updateParameters:sender];
			[pwadView reloadData];
			[pwadView selectRowIndexes:[NSIndexSet indexSetWithIndex:0] byExtendingSelection:NO];
			
			[pwadView scrollRowToVisible:0];
		}
	}
	else
	{
		
		[pwadArray removeObjectsAtIndexes:set];
		[self updateParameters:sender];
		
		[pwadView reloadData];
	
		NSInteger dest;
	
		if([[pwadView selectedRowIndexes] count] < 1)
			dest = [pwadView numberOfRows] - 1;
		else // if ([[pwadView selectedRowIndexes] count] >= 1)
			dest = [pwadView selectedRow] - [[pwadView selectedRowIndexes] count] + 1;
	
		[pwadView selectRowIndexes:[NSIndexSet indexSetWithIndex:dest] byExtendingSelection:NO];
		[pwadView scrollRowToVisible:dest];
	}
}

//
// chooseRecordDemo:
//
-(IBAction)chooseRecordDemo:(id)sender
{
	NSSavePanel *panel = [NSSavePanel savePanel];
	NSArray *types = [NSArray arrayWithObject:@"lmp"];
	[panel setAllowedFileTypes:types];
	[panel beginSheetForDirectory:nil file:nil modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(chooseRecordDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

//
// chooseRecordDidEnd:returnCode:contextInfo:
//
-(void)chooseRecordDidEnd:(NSSavePanel *)panel returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	if(returnCode == NSCancelButton)
	{
		return;
	}
	[recordDemoField setStringValue:[[panel URL] path]];
	
	[self updateParameters:self];
	
}

//
// choosePlayDemo:
//
-(IBAction)choosePlayDemo:(id)sender
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	[panel setAllowsMultipleSelection:false];
	[panel setCanChooseFiles:true];
	[panel setCanChooseDirectories:false];
	NSArray *types = [NSArray arrayWithObject:@"lmp"];
	[panel beginSheetForDirectory:nil file:nil types:types modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(choosePlayDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

//
// choosePlayDidEnd:returnCode:contextInfo:
//
// Write into the text field the file name
//
-(void)choosePlayDidEnd:(NSOpenPanel *)panel returnCode:(int)code contextInfo:(void *)info
{
	if(code == NSCancelButton)
	{
		return;
	}
	[playDemoField setStringValue:[[panel URL] relativePath]];
	
	[self updateParameters:self];
	
}

//
// clearPlayDemo:
//
-(IBAction)clearPlayDemo:(id)sender
{
	[playDemoField setStringValue:@""];
	[fastdemo setState:NSOffState];
	[timedemo setState:NSOffState];
	
	[self updateParameters:sender];
}

//
// clearRecordDemo:
//
// Clear the record demo and warp inputs
//
-(IBAction)clearRecordDemo:(id)sender
{
	[recordDemoField setStringValue:@""];
	[warpField setStringValue:@""];
	[skillField setStringValue:@""];
	
	[respawn setState:NSOffState];
	[fast setState:NSOffState];
	[nomons setState:NSOffState];
	[vanilla setState:NSOffState];
	
	[self updateParameters:sender];

}

//
// clearNetwork:
//
// Clear the network inputs
//
-(IBAction)clearNetwork:(id)sender
{
	[gameTypePopUp selectItemAtIndex:0];
	
	[fragField setStringValue:@""];
	[timeField setStringValue:@""];
	[turboField setStringValue:@""];
	[dmflagField setStringValue:@""];
	[netField setStringValue:@""];
	
	[self updateParameters:sender];
}

//
// saveAsGFS:
//
// Start dialog box
//
-(IBAction)saveAsGFS:(id)sender
{
	
	if([iwadPopUp numberOfItems] == 0 && [pwadArray count] == 0)
	{
		[nothingForGfsAlert beginSheetModalForWindow:[self window] 
                                     modalDelegate:self 
							   didEndSelector:NULL contextInfo:nil];
		return;
	}
	
	NSSavePanel *panel = [NSSavePanel savePanel];
	NSArray *types = [NSArray arrayWithObject:@"gfs"];
	[panel setAllowedFileTypes:types];
	[panel beginSheetForDirectory:nil file:nil modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(saveAsGFSDidEnd:returnCode:contextInfo:) contextInfo:nil];
}

//
// saveAsGFSDidEnd:returnCode:contextInfo:
//
// Save parameters into a GFS
//
-(void)saveAsGFSDidEnd:(NSSavePanel *)panel returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
	if(returnCode == NSCancelButton)
	{
		return;
	}
	
	NSMutableString *gfsOut = [[NSMutableString alloc] init];
	
	if([iwadPopUp numberOfItems] > 0)
	{
		[gfsOut appendString:@"iwad = \""];
		[gfsOut appendString:[[[iwadPopUp selectedItem] representedObject] 
                            path]];
		[gfsOut appendString:@"\"\n"];
	}
	
	if([pwadArray count] > 0)
	{
		NSInteger pathPosition = 0;
		NSURL *wURL = nil;
		NSString *path, *root, *oldroot;
		NSMutableArray *pathArray = [[NSMutableArray alloc] initWithCapacity:0];

		for(pathPosition = 0;; pathPosition++)
		{
			oldroot = nil;
         root = nil;
			for(wURL in pwadArray)
			{
				path = [wURL path];
				root = [[path pathComponents] objectAtIndex:pathPosition];
				
				if(pathPosition + 1 >= [[path pathComponents] count] || (oldroot && [root caseInsensitiveCompare:oldroot]
               != NSOrderedSame))
				{
					goto foundOut;
				}
				oldroot = root;
			}
         if(root!=nil)
            [pathArray addObject:root];
		}
	foundOut:
		path = [NSString pathWithComponents:pathArray];
		
		{
			[gfsOut appendString:@"basepath = \""];
			[gfsOut appendString:path];
			[gfsOut appendString:@"\"\n"];
		}
		
		NSUInteger i;
		NSString *extension;
		for(wURL in pwadArray)
		{
			root = [wURL path];
			extension = [root pathExtension];
			
			[pathArray setArray:[root pathComponents]];
			for(i = 0; i < pathPosition; i++)
				[pathArray removeObjectAtIndex:0];
			
			path = [NSString pathWithComponents:pathArray];
			
			if([extension caseInsensitiveCompare:@"bex"] == NSOrderedSame ||
					[extension caseInsensitiveCompare:@"deh"] == NSOrderedSame)
			{
				[gfsOut appendString:@"dehfile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
			else if([extension caseInsensitiveCompare:@"edf"] == NSOrderedSame)
			{
				[gfsOut appendString:@"edffile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
			else if([extension caseInsensitiveCompare:@"csc"] == NSOrderedSame)
			{
				[gfsOut appendString:@"cscfile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
			else // any other extension defaults to wadfile
			{
				[gfsOut appendString:@"wadfile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
		}

		[pathArray release];
	}
	[gfsOut writeToURL:[panel URL] atomically:YES encoding:NSUTF8StringEncoding 
                error:NULL];
	[gfsOut release];
}

//
// updateParmRsp:
//
-(void)updateParmRsp:(id)sender
{
	// scan for values starting with @ and remove them
	// if found: set something
	// if not found: delete it
	
	// This is more complex, so divide it further
	// pwadTypes = [NSArray arrayWithObjects:@"cfg", @"bex", @"deh", @"edf", 
   // @"csc", @"wad", @"gfs", @"rsp", @"lmp", nil];
	
	NSURL *url;
	NSString *extension, *path;
	for(url in pwadArray)
	{
		path = [url path];
		extension = [path pathExtension];
		// -config
		if([extension caseInsensitiveCompare:@"rsp"] == NSOrderedSame)
		{
			NSString *rstr = [NSString stringWithFormat:@"%s%s", "@", [path UTF8String]];
			[[param argumentWithIdentifier:@"@"] setEnabled:YES];
			[[[param argumentWithIdentifier:@"@"] extraWords] removeAllObjects];
			[[[param argumentWithIdentifier:@"@"] extraWords] addObject:rstr];

			return;
		}
	}
	// not found
	[[param argumentWithIdentifier:@"@"] setEnabled:NO];
}

//
// updateParmIwad:
//
-(void)updateParmIwad:(id)sender
{
	if([iwadPopUp numberOfItems] <= 0)
	{
		[[param argumentWithIdentifier:@"-iwad"] setEnabled:NO];
	}
	else
	{
		[[param argumentWithIdentifier:@"-iwad"] setEnabled:YES];
		[[[param argumentWithIdentifier:@"-iwad"] extraWords] removeAllObjects];
		[[[param argumentWithIdentifier:@"-iwad"] extraWords] addObject:[[[iwadPopUp selectedItem] representedObject] path]];
	}
}

//
// updateParmPwad:
//
-(void)updateParmPwad:(id)sender
{
	[parmPwad removeAllObjects];
	// This is more complex, so divide it further
   // pwadTypes = [[NSArray alloc] initWithObjects:@"cfg", @"bex", @"deh", 
   //             @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", @"pk3",
   ///             @"pke", @"zip", nil];
	
	[[param argumentWithIdentifier:@"-config"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-deh"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-edf"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-exec"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-file"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-gfs"] setEnabled:NO];
	
	NSDictionary *specialParams = [NSDictionary dictionaryWithObjectsAndKeys:@"-config", @"cfg", @"-deh", @"bex", @"-deh", @"deh", @"-edf", @"edf", @"-exec", @"csc", @"-gfs", @"gfs", nil];
	
	NSURL *url;
	NSString *exttest, *parmtest, *extension, *path;
	BOOL found;
	for(url in pwadArray)
	{
		path = [url path];
		extension = [path pathExtension];
		// -config
		
		found = NO;
		for(exttest in specialParams)
		{
			if([extension caseInsensitiveCompare:exttest] == NSOrderedSame)
			{
				parmtest = [specialParams objectForKey:exttest];
				if(![[param argumentWithIdentifier:parmtest] enabled])
				{
					[[param argumentWithIdentifier:parmtest] setEnabled:YES];
					[[[param argumentWithIdentifier:parmtest] extraWords] removeAllObjects];
				}
				[[[param argumentWithIdentifier:parmtest] extraWords] addObject:path];
				found = YES;
			}
		}
		
		if(!found)
		{
			if(![[param argumentWithIdentifier:@"-file"] enabled])
			{
				[[param argumentWithIdentifier:@"-file"] setEnabled:YES];
				[[[param argumentWithIdentifier:@"-file"] extraWords] removeAllObjects];
			}
			[[[param argumentWithIdentifier:@"-file"] extraWords] addObject:path];
		}
	}
}

//
// updateParmOthers:
//
// How do: there's one parameter kind labelled simply " "
// Read the other 
//
-(IBAction)updateParmOthers:(id)sender
{
	NSUInteger i, j;
	NSRange rng;
	NSMutableString *aggr = [NSMutableString string], *comp2 = [NSMutableString string];
	BOOL quittest;
	
	[[param argumentWithIdentifier:@" "] setEnabled:NO];
	
	if([[otherField stringValue] length] <= 0)
		return;
	
	NSMutableArray *othersString = [NSMutableArray arrayWithArray:[[otherField stringValue] componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	
	NSString *token;
	for(i = 0; i < [othersString count]; ++i)
	{
		token = [othersString objectAtIndex:i];
		if([token length] > 0 && [token characterAtIndex:0] == '\"')
		{
			[aggr appendString:[[othersString objectAtIndex:i] substringFromIndex:1]];
			j = i;
			rng.location = j;
			for(++i; i < [othersString count]; ++i)
			{
				quittest = NO;
				[comp2 setString:[othersString objectAtIndex:i]];
				if([comp2 length] > 0 && [comp2 characterAtIndex:[comp2 length] - 1] == '\"')
				{
					[comp2 deleteCharactersInRange:NSMakeRange([comp2 length] - 1, 1)];
					quittest = YES;
				}
				[aggr appendString:@" "];
				[aggr appendString:comp2];
				if(quittest || i + 1 == [othersString count])
					break;
			}
			rng.length = i - j + 1;
			[othersString removeObjectsInRange:rng];
			[othersString insertObject:aggr atIndex:j];
			i = j;
		}
	}
	
	// put othersString into its own category
	[[param argumentWithIdentifier:@" "] setEnabled:YES];
	[[[param argumentWithIdentifier:@" "] extraWords] setArray:othersString];
}

//
// updateParmDiscrete:fromControl:
//
// Updates parameter as input from a string text field as separate (discrete) words
//
-(void)updateParmDiscrete:(NSString *)identifier fromControl:(NSControl *)stringControl
{
	if([[stringControl stringValue] length] > 0)
	{
		[[param argumentWithIdentifier:identifier] setEnabled:YES];
		[[[param argumentWithIdentifier:identifier] extraWords] setArray:[[stringControl stringValue] componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
	else
		[[param argumentWithIdentifier:identifier] setEnabled:NO];
}

//
// updateParmFlags:
//
-(IBAction)updateParmFlags:(id)sender
{
	[[param argumentWithIdentifier:@"-respawn"] setEnabled:[respawn state]];
	[[param argumentWithIdentifier:@"-fast"] setEnabled:[fast state]];
	[[param argumentWithIdentifier:@"-nomonsters"] setEnabled:[nomons state]];
	[[param argumentWithIdentifier:@"-vanilla"] setEnabled:[vanilla state]];
}

//
// updateParmRecord:
//
-(IBAction)updateParmRecord:(id)sender
{
	[parmRecord removeAllObjects];
	if([[recordDemoField stringValue] length] > 0)
	{
		[[param argumentWithIdentifier:@"-record"] setEnabled:YES];
		[[[param argumentWithIdentifier:@"-record"] extraWords] setArray:[NSArray arrayWithObject:[recordDemoField stringValue]]];
	}
	else
		[[param argumentWithIdentifier:@"-record"] setEnabled:NO];
}

//
// updateParmPlayDemo:
//
-(IBAction)updateParmPlayDemo:(id)sender
{
	if(sender == timedemo && [timedemo state] == NSOnState)
		[fastdemo setState:NSOffState];
	else if(sender == fastdemo && [fastdemo state] == NSOnState)
		[timedemo setState:NSOffState];
	
	[[param argumentWithIdentifier:@"-timedemo"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-fastdemo"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-playdemo"] setEnabled:NO];
	if([[playDemoField stringValue] length] > 0)
	{
		NSString *name;
		if([timedemo state] == NSOnState)
			name = @"-timedemo";
		else if([fastdemo state] == NSOnState)
			name = @"-fastdemo";
		else
			name = @"-playdemo";
		[[param argumentWithIdentifier:name] setEnabled:YES];
		[[[param argumentWithIdentifier:name] extraWords] setArray:[NSArray arrayWithObject:[playDemoField stringValue]]];
	}
}

//
// updateParmGameType:
//
-(IBAction)updateParmGameType:(id)sender
{
	[[param argumentWithIdentifier:@"-deathmatch"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-altdeath"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-trideath"] setEnabled:NO];
	
	switch([gameTypePopUp indexOfSelectedItem])
	{
		case 1:
			[[param argumentWithIdentifier:@"-deathmatch"] setEnabled:YES];
			break;
		case 2:
			[[param argumentWithIdentifier:@"-altdeath"] setEnabled:YES];
			break;
		case 3:
			[[param argumentWithIdentifier:@"-trideath"] setEnabled:YES];
			break;
	}
}
		 
//
// updateParameters:
//
// Called to set the argument array from all input sources
//
-(IBAction)updateParameters:(id)sender
{
	[self updateParmRsp:sender];
	[self updateParmIwad:sender];
	[self updateParmPwad:sender];
	[self updateParmDiscrete:@"-skill" fromControl:skillField];
	[self updateParmDiscrete:@"-warp" fromControl:warpField];
	[self updateParmFlags:sender];
	[self updateParmRecord:sender];
	[self updateParmPlayDemo:sender];
	[self updateParmGameType:sender];
	[self updateParmDiscrete:@"-frags" fromControl:fragField];
	[self updateParmDiscrete:@"-timer" fromControl:timeField];
	[self updateParmDiscrete:@"-turbo" fromControl:turboField];
	[self updateParmDiscrete:@"-dmflags" fromControl:dmflagField];
	[self updateParmDiscrete:@"-net" fromControl:netField];
	[self updateParmOthers:sender];
	
	
	NSMutableString *infotext = [NSMutableString string];
	
	// two special cases: params with identifiers "@" and " "
	ELCommandLineArgument *arg;
	
	arg = [param hasArgument:@"@"];
	if([arg enabled])
	{
		// responses
		[infotext appendString:[arg responseFileString:YES]];
	}
	// now look through all.
	
	for(arg in [param array])
	{
		if(![arg enabled] || [[arg identifier] isEqualToString:@"@"])
			continue;	// skip if that kind
		
		// the rest are valid
		[infotext appendString:@" "];
		[infotext appendString:[arg generalArgString:YES]];
		
	}
	
	[infoDisplay setStringValue:infotext];
	
}

//
// saveDefaults
//
// Save the user configuration.
//
-(void)saveDefaults
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	// Let's go.
	[defaults setBool:YES forKey:@"Saved."];
   
	//
	// ^saveURLsFromCollection
	//
   // Block to save URLs from a given collection with compatible path text
	//
   void (^saveURLsFromCollection)(id, NSString *) = ^(id aURLCollection, NSString *aString)
   {
      NSURL *URL;
      NSMutableArray *archArr = [NSMutableArray array];
      for(URL in aURLCollection)
         [archArr addObject:[URL path]];
      [defaults setObject:archArr forKey:aString];
   };
	
	// IWAD list
   saveURLsFromCollection(iwadSet, @"iwadSet");
   
   // Index of selected item
	[defaults setInteger:[iwadPopUp indexOfSelectedItem] forKey:@"iwadPopUpIndex"];
   
	// PWAD array
   saveURLsFromCollection(pwadArray, @"pwadArray");
      
   // User set
   saveURLsFromCollection(userSet, @"userSet");
   
	// Other parameters
	[defaults setObject:[otherField stringValue] forKey:@"otherField"];
	
	// Warp
	[defaults setObject:[warpField stringValue] forKey:@"warpField"];
	
	// Skill
	[defaults setObject:[skillField stringValue] forKey:@"skillField"];
	
	// respawn
	[defaults setBool:[respawn state] == NSOnState ? YES : NO forKey:@"respawn"];
	
	// fast
	[defaults setBool:[fast state] == NSOnState ? YES : NO forKey:@"fast"];
	
	// nomons
	[defaults setBool:[nomons state] == NSOnState ? YES : NO forKey:@"nomons"];
	
	// vanilla
	[defaults setBool:[vanilla state] == NSOnState ? YES : NO forKey:@"vanilla"];
	
	// record: don't save it
	// playdemo
	[defaults setObject:[playDemoField stringValue] forKey:@"playdemo"];
	
	// timedemo, fastdemo
	[defaults setBool:[timedemo state] == NSOnState ? YES : NO forKey:@"timedemo"];
	[defaults setBool:[fastdemo state] == NSOnState ? YES : NO forKey:@"fastdemo"];
	
	// gametype
	[defaults setInteger:[gameTypePopUp indexOfSelectedItem] forKey:@"gameTypePopUpIndex"];
	[defaults setObject:[fragField stringValue] forKey:@"fragField"];
	[defaults setObject:[timeField stringValue] forKey:@"timeField"];
	[defaults setObject:[turboField stringValue] forKey:@"turboField"];
	[defaults setObject:[dmflagField stringValue] forKey:@"dmflagField"];
	[defaults setObject:[netField stringValue] forKey:@"netField"];
	// tab position
	[defaults setObject:[[tabView selectedTabViewItem] identifier] forKey:@"tabView"];
	
	[defaults synchronize];
}

//
// loadDefaults
//
// Load user configuration from the commons
//
-(void)loadDefaults
{
	NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
	
	if([defaults boolForKey:@"Saved."])
	{
		// Let's go.
      
		//
		// ^loadURLsFromKey
		//
      // block to load URLs from a list
		//
      void (^loadURLsFromKey)(NSString *, SEL) = ^(NSString *aKey, SEL aMessage)
      {
         NSArray *archArr;
         NSString *archStr;
         
         archArr = [defaults stringArrayForKey:aKey];
         for(archStr in archArr)
         {
            [self performSelector:aMessage
                       withObject:[NSURL URLWithString:archStr]];
         }
      };
		
		// IWAD set
      loadURLsFromKey(@"iwadSet", @selector(doAddIwadFromURL:));
		[iwadPopUp selectItemAtIndex:[defaults integerForKey:@"iwadPopUpIndex"]];

      // PWAD array
      loadURLsFromKey(@"pwadArray", @selector(doAddPwadFromURL:));
		[pwadView reloadData];
			 
		// Other parameters
		[otherField setStringValue:[defaults objectForKey:@"otherField"]];
		
		// Warp
		[warpField setStringValue:[defaults objectForKey:@"warpField"]];
		
		// Skill
		[skillField setStringValue:[defaults objectForKey:@"skillField"]];
		
		// respawn
		[respawn setState:[defaults boolForKey:@"respawn"] ? NSOnState : NSOffState];
		
		// fast
		[fast setState:[defaults boolForKey:@"fast"] ? NSOnState : NSOffState];
		
		// nomons
		[nomons setState:[defaults boolForKey:@"nomons"] ? NSOnState : NSOffState];
		
		// vanilla
		[vanilla setState:[defaults boolForKey:@"vanilla"] ? NSOnState : NSOffState];
		
		// record: don't save it
		
		// playdemo
		[playDemoField setStringValue:[defaults objectForKey:@"playdemo"]];
		
		// timedemo, fastdemo
		[timedemo setState:[defaults boolForKey:@"timedemo"] ? NSOnState : NSOffState];
		[fastdemo setState:[defaults boolForKey:@"fastdemo"] ? NSOnState : NSOffState];
		
		// gametype
		[gameTypePopUp selectItemAtIndex:[defaults integerForKey:@"gameTypePopUpIndex"]];
		
		[fragField setStringValue:[defaults objectForKey:@"fragField"]];
		[timeField setStringValue:[defaults objectForKey:@"timeField"]];
		[turboField setStringValue:[defaults objectForKey:@"turboField"]];
		[dmflagField setStringValue:[defaults objectForKey:@"dmflagField"]];
		[netField setStringValue:[defaults objectForKey:@"netField"]];
		
		// tabView
		[tabView selectTabViewItemWithIdentifier:[defaults objectForKey:@"tabView"]];
		
		[self updateParameters:self];
	}
}

//
// goGetHelp:
//
// Go to the parameters article in the You Fail It Eternity wiki. I might decide to turn it into an offline help
//
-(IBAction)goGetHelp:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://eternity.youfailit.net/index.php?title=List_of_command_line_parameters"]];
}

@end	// end LauncherController class definition



// EOF


