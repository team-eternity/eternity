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


// TODO:
//
// * Backspace (delete) key for removing file entries
// * Warn user on overwriting demo at "start game" moment, not entry moment
// * Options to copy the whole parameter list to clipboard (not by selecting
//   directly)
// * Full help instead of Internet link
// * Correct menu bar
// * Some artwork background

#import "LauncherController.h"
#import "ELDumpConsole.h"
#import "ELFileViewDataSource.h"
#import "ELCommandLineArray.h"
#import "ELCommandLineArgument.h"


static BOOL gCalledAppMainline = FALSE;
static BOOL gSDLStarted;	// IOAN 20120616

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

@synthesize pwadArray;

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
   [overwriteDemoAlert release];
   [recordDemoIsDir release];
	
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
	
	[task release];
	
	[super dealloc];
}

//
// init
//
// Initialize members
//
-(id)init
{
	if(self = [super init])
	{
      dontUndo = FALSE;
      
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
      
      overwriteDemoAlert = [[NSAlert alloc] init];
      [overwriteDemoAlert addButtonWithTitle:@"Cancel Game"];
      [overwriteDemoAlert addButtonWithTitle:@"Overwrite and Start"];
      [overwriteDemoAlert setMessageText:@"Overwrite demo to be recorded?"];
      [overwriteDemoAlert setInformativeText:@"The target file in the \"Record Demo:\" field already exists and will be overwritten if you proceed. Make sure not to lose valuable data."];
      [overwriteDemoAlert setAlertStyle:NSCriticalAlertStyle];
      
      recordDemoIsDir = [[NSAlert alloc] init];
		[recordDemoIsDir addButtonWithTitle:@"Dismiss"];
		[recordDemoIsDir setMessageText:
       @"Cannot record demo."];
		[recordDemoIsDir setInformativeText:
       @"The target file in the \"Record Demo:\" field is a directory."];
		[recordDemoIsDir setAlertStyle:NSWarningAlertStyle];
		
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
		
		task = nil;
		
		console = [[ELDumpConsole alloc] initWithWindowNibName:@"DumpConsole"];

	}
	return self;
}

//
// gotoDoc:
//
// go to specified documentation
//
-(void)gotoDoc:(id)sender
{
   [[NSWorkspace sharedWorkspace] openFile:[sender representedObject]];
}

//
// makeDocumentMenu
//
-(void)makeDocumentMenu
{
   NSError *err = nil;
   NSArray *contents = [fileMan contentsOfDirectoryAtPath:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"docs"] error:&err];
   
   if(err)
   {
      [NSAlert alertWithError:err];
      return;
   }
   
   NSString *fname, *fullname;
   
   for(fname in contents)
   {
      fullname = [[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"docs"] stringByAppendingPathComponent:fname];
      NSMenuItem *item;
      
      item = [[NSMenuItem alloc] initWithTitle:[fname stringByDeletingPathExtension] action:@selector(gotoDoc:) keyEquivalent:@""];
      [item setRepresentedObject:fullname];
      
      [[docMenu submenu] addItem:item];
   }
}

//
// initNibData
//
// Added after Nib got loaded. Generic initialization that wouldn't go in init
//
-(void)initNibData
{
	
	// Set the pop-up button
	[iwadPopUp setMenu:iwadPopMenu];


	
	
   // TODO: Allow file list view to accept Finder files
   // [pwadView registerForDraggedTypes:[NSArray arrayWithObjects:NSFilenamesPboardType, nil]];
	
   // Set array for data source
   [fileViewDataSource setArray:pwadArray];

	[console setMasterOwner:self];
   
	[self loadDefaults];
   
   // Add documents to list
   [self makeDocumentMenu];
}

//
// getUndoFor:
//
// Gets undo manager for an object. But returns nil if it has to be suppressed
//
-(NSUndoManager *)getUndoFor:(id)obj
{
   if(self->dontUndo)
      return nil;
   return [obj undoManager];
}

//
// -applicationDataDirectory
//
// The "library/application support" directory
//
// Copied from help HTML: https://developer.apple.com/library/mac/#documentation/FileManagement/Conceptual/FileSystemProgrammingGUide/AccessingFilesandDirectories/AccessingFilesandDirectories.html#//apple_ref/doc/uid/TP40010672-CH3-SW3
//
#if 0 // NOT USED: requires OS X 10.6 or later
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
#endif // 0

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
   NSString *appDataPath = [[@"~/Library/Application Support" stringByExpandingTildeInPath] stringByAppendingPathComponent:[[NSBundle mainBundle] bundleIdentifier]];
  	NSString *usrPath = [appDataPath stringByAppendingPathComponent:@"user"];
   
	[fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"doom"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"doom2"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"hacx"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"heretic"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"plutonia"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"shots"] withIntermediateDirectories:YES attributes:nil error:nil];
   [fileMan createDirectoryAtPath:[usrPath stringByAppendingPathComponent:@"tnt"] withIntermediateDirectories:YES attributes:nil error:nil];

	NSString *basPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"base"];
		
	[userPath setString:usrPath];
	[basePath setString:basPath];
	
	// Now it points to Application Support
	
	// NOTE: since Eternity may still use "." directly, I'm still using the shell chdir command.
	// TODO: copy bundled prototype user data into workingDirPath, if it doesn't exist there yet
	// FIXME: make workingDirPath a member variable.
	
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
	if([notification object] != mainWindow)
		return;
	if(!gSDLStarted)
		[self updateParameters:self];
	
	// Save GUI configurations
	[self saveDefaults];	// FIXME: get user configurations into the preferences, 
                        // don't read them from bundle
	if(!gSDLStarted)
	{
		[NSApp terminate:[self window]];
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
// showUserInFinder:
//
-(IBAction)showUserInFinder:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:userPath withApplication:@"Finder"];
}

//
// accessBaseFolder:
//
-(IBAction)accessBaseFolder:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:basePath withApplication:@"Finder"];
}

//
// accessOldDocs:
//
-(IBAction)accessOldDocs:(id)sender
{
   [[NSWorkspace sharedWorkspace] openFile:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"docs"] withApplication:@"Finder"];
}

//
// taskEnded
//
// Game ended, do clean-up
//
-(void)taskEnded
{
	// remove last four parameters
	gCalledAppMainline = FALSE;
	gSDLStarted = NO;	// IOAN 20120616
	[[self window] orderFront:nil];
	
	[[param argumentWithIdentifier:@"-base"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-user"] setEnabled:NO];
   
   // Clear the demo record field
   [recordDemoField setStringValue:@""];
   [self updateParameters:self];
}

//
// doLaunchGame
//
// Everything is set, start the game.
//
-(void)doLaunchGame:(id)sender
{
	[self updateParameters:sender];	// Update parameters.
   // FIXME: do it in real-time
	
	// Add -base and user here
	ELCommandLineArgument *argBase, *argUser;
	if(![param miscHasWord:@"-base"])
	{
		argBase = [ELCommandLineArgument argWithIdentifier:@"-base"];
		[[argBase extraWords] addObject:basePath];
		[param addArgument:argBase];
	}
	if(![param miscHasWord:@"-user"])
	{
		argUser = [ELCommandLineArgument argWithIdentifier:@"-user"];
		[[argUser extraWords] addObject:userPath];
		[param addArgument:argUser];
	}
   
	NSArray *deploy = [param deployArray];
	// Start console
	
	
	gCalledAppMainline = TRUE;
	gSDLStarted = YES;	// IOAN 20120616
	[[self window] orderOut:self];
	
	// IOAN 20130103: use Neil's PrBoom-Mac Launcher code
	[task release];
	task = [[NSTask alloc] init];
	NSBundle *engineBundle = [NSBundle bundleWithPath:[[NSBundle mainBundle] pathForResource:@"Eternity.app" ofType:nil]];
	
	[task setLaunchPath:[engineBundle executablePath]];
	[task setArguments:deploy];
	
	[console startLogging:task];
	
   //	[console showInstantLog];
   // We're done, thank you for playing
   //   if(status == 0)   // only exit if it's all ok
   //    exit(status);
}

//
// overwriteDemoDidEnd:returnCode:contextInfo:
//
// User has chosen to overwrite demo
//
-(void)overwriteDemoDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
   if(returnCode == NSAlertSecondButtonReturn)
	{
      [self doLaunchGame:self];
   }
}

//
// launchGame:
//
// Start the game
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
		[noIwadAlert beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:@selector(chooseIwadAlertDidEnd:returnCode:contextInfo:) contextInfo:nil];
		
		return;
	}
iwadMightBe:
   ;
   // Check if wad exists at path and query
   BOOL isDir;
   if([fileMan fileExistsAtPath:[recordDemoField stringValue] isDirectory:&isDir])
   {
      if(isDir)
      {
         [recordDemoIsDir beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:NULL contextInfo:NULL];
         return;
      }
      else
      {
         [overwriteDemoAlert beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:@selector(overwriteDemoDidEnd:returnCode:contextInfo:) contextInfo:nil];
         
         return;
      }
   }
   
   [self doLaunchGame:sender];
}

//
// iwadPopUpShowDifferentPaths
//
// Show the path differences if file names are equal
//
-(void)iwadPopUpShowDifferentPaths
{
	// I have to scan all set components
	NSURL *url;
	NSString *iwadPath;
	
	// There will be a list of files with identical last names
	// Each component will contain a last name and the indices in the list view
	
	for(url in iwadSet)
	{
		// We've established that non-path URL can't enter the list
		iwadPath = [url path];
		
	}
}

//
// doAddIwadFromURL:atIndex:
//
// Tries to add IWAD as specified by URL to the pop-up button list
// Returns YES if successful or already there (URL valid for path, RFC 1808)
// Returns NO if given URL is invalid
//
-(BOOL)doAddIwadFromURL:(NSURL *)wURL atIndex:(NSInteger)ind
{
   if(ind < 0 || ind > [iwadPopUp numberOfItems])
      return NO;
   
	NSMenuItem *last;
	if(![iwadSet containsObject:wURL])
	{
		
		NSString *iwadPath = [wURL path];
		if(iwadPath == nil)
			return NO;	// URL not RFC 1808
		
		[iwadSet addObject:wURL];
		      
      [iwadPopUp insertItemWithTitle:[fileMan displayNameAtPath:iwadPath] atIndex:ind];
      
      NSUndoManager *undom = [self getUndoFor:iwadPopUp];
      [[undom prepareWithInvocationTarget:self] doRemoveIwadAtIndex:ind];
      [undom setActionName:@"Add/Remove Game WAD"];


		last = [[[iwadPopUp menu] itemArray] objectAtIndex:ind];
		[last setRepresentedObject:wURL];
		[last setImage:[[NSWorkspace sharedWorkspace] iconForFile:iwadPath]];
		
		[last setAction:@selector(updateParameters:)];
		[last setTarget:self];
		
		// NOTE: it's a very rare case to choose between two different IWADs with the same name. In any case…
		// FIXME: implement path difference specifier in parentheses
		
		[iwadPopUp selectItem:last];
      [self updateParameters:self];
	} 
	// FIXME: select the existing path component
	// FIXME: each set component should point to a menu item
	return YES;
}

//
// doAddIwadFromURL:
//
-(BOOL)doAddIwadFromURL:(NSURL *)wURL
{
   return [self doAddIwadFromURL:wURL atIndex:[iwadPopUp numberOfItems]];
}

//
// doRemoveIwadAtIndex:
//
-(void)doRemoveIwadAtIndex:(NSInteger)ind
{
   if(ind >= 0 && [iwadPopUp numberOfItems] > ind)
	{
		NSURL *iwadURL = [[iwadPopUp itemAtIndex:ind] representedObject];
      
      NSUndoManager *undom = [self getUndoFor:iwadPopUp];
      [[undom prepareWithInvocationTarget:self] doAddIwadFromURL:iwadURL atIndex:ind];
      [undom setActionName:@"Add/Remove Game WAD"];
		
		[iwadPopUp removeItemAtIndex:ind];
		[iwadSet removeObject:iwadURL];
		
		[self updateParameters:[iwadPopUp selectedItem]];
	}
   else
      NSBeep();
}

//
// removeIwad:
//
-(IBAction)removeIwad:(id)sender
{
   [self doRemoveIwadAtIndex:[iwadPopUp indexOfSelectedItem]];
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
   [panel setMessage:@"Choose add-on file"];
	[panel beginSheetForDirectory:nil file:nil types:pwadTypes modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(addPwadEnded:returnCode:contextInfo:) contextInfo:nil];
}

//
// showFileInFinder:
//
-(IBAction)showFileInFinder:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:[[pwadArray objectAtIndex:[pwadView selectedRow] ] path] withApplication:@"Finder"];
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
   [panel setMessage:@"Choose main game WAD"];
	[panel beginSheetForDirectory:nil file:nil types:pwadTypes modalForWindow:[NSApp mainWindow] modalDelegate:self didEndSelector:@selector(addIwadEnded:returnCode:contextInfo:) contextInfo:nil];
}

//
// addAllRecentPwads:
//
-(IBAction)addAllRecentPwads:(id)sender
{
   [self doAddPwadsFromURLs:[[NSDocumentController sharedDocumentController] recentDocumentURLs] atIndexes:nil];
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
	if(menuItem == fileCloseIWAD)
      return [iwadPopUp numberOfItems] > 0;
   
	return YES;
}

//
// doAddPwadsFromURLs:atIndexes:
//
// Adds multiple files, from an array made of NSURL objects.
//
-(void)doAddPwadsFromURLs:(NSArray *)wURLArray atIndexes:(NSIndexSet *)anIndexSet
{
   // if it's null, assign it
   if(anIndexSet == nil)
      anIndexSet = [NSIndexSet indexSetWithIndex:[pwadArray count]];   // nothing designated, so add at end.
   
   NSUndoManager *undom = [self getUndoFor:pwadView];
   [undom registerUndoWithTarget:self selector:@selector(doRemovePwadsAtIndexes:) object:anIndexSet];
   [undom setActionName:@"Add/Remove Files"];
   
   [pwadArray insertObjects:wURLArray atIndexes:anIndexSet];
   
   NSURL *URL;
   for (URL in wURLArray)
   {
      [[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:URL];
   }
   if(pwadView)
      [pwadView reloadData];
   
   // Needed here.
   [self updateParameters:self];
}

//
// doAddPwadFromURL:
//
-(void)doAddPwadFromURL:(NSURL *)wURL
{
   [self doAddPwadsFromURLs:[NSArray arrayWithObject:wURL] atIndexes:nil];
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
   
   [self doAddPwadsFromURLs:[panel URLs] atIndexes:nil];
	
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

	}
}

//
// doRemovePwadsAtIndexes:
//
// Removes pwads at designated indexes
//
-(void)doRemovePwadsAtIndexes:(NSIndexSet *)anIndexSet
{
   if([pwadArray count] <= 0) // empty array, do nothing
   {
      NSBeep();
      return;
   }
   
   if([anIndexSet count] > 0 && [anIndexSet lastIndex] >= [pwadArray count]) // out of bounds, do nothing
   {
      NSBeep();
      return;
   }
   
   //
   // Take the set.
   //
   NSIndexSet *set;
   if([anIndexSet count] <= 0)                     // Nothing specified (empty set), so take {0}
      set = [NSIndexSet indexSetWithIndex:0];
   else                                            // Specified in argument
      set = anIndexSet;

   // Set the objects to be deleted
   NSArray *undoRemoveURLs = [pwadArray objectsAtIndexes:set];
   
   //
   // Do the deletion, update parameters, update view
   //
   [pwadArray removeObjectsAtIndexes:set];
   [pwadView reloadData];
   [self updateParameters:self];
   
   //
   // Register undo
   //
   NSUndoManager *undom = [self getUndoFor:pwadView];
   [[undom prepareWithInvocationTarget:self] doAddPwadsFromURLs:undoRemoveURLs atIndexes:set];
   [undom setActionName:@"Add/Remove Files"];
   
   //
   // Update selection
   //
   if([anIndexSet count] <= 0)   // Deleted when nothing was specified
   {
      [pwadView selectRowIndexes:set byExtendingSelection:NO];
      [pwadView scrollRowToVisible:0];
   }
   else                          // Deleted when something was selected
   {
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
// removeAllPwads:
//
-(IBAction)removeAllPwads:(id)sender
{
   [self doRemovePwadsAtIndexes:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, [pwadArray count])]];
}

//
// removePwad:
//
-(IBAction)removePwad:(id)sender
{
   [self doRemovePwadsAtIndexes:[pwadView selectedRowIndexes]];
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
   
   // Now, replace the files with the GFS
   [self removeAllPwads:self];
   [self doAddPwadFromURL:[panel URL]];
   [self updateParameters:self];
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

		[infotext appendString:[arg generalArgString:YES]];
		[infotext appendString:@"\n"];		
	}
	
	[infoDisplay setString:infotext];
	
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
         [archArr addObject:[URL relativeString]];
      [defaults setObject:archArr forKey:aString];
   };
	
	// IWAD list
   saveURLsFromCollection(iwadSet, @"iwadSet_v2");
   
   // Index of selected item
	[defaults setInteger:[iwadPopUp indexOfSelectedItem] forKey:@"iwadPopUpIndex"];
   
	// PWAD array
   saveURLsFromCollection(pwadArray, @"pwadArray_v2");
   
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
   
   self->dontUndo = TRUE;
	
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
            [self performSelector:aMessage withObject:[NSURL URLWithString:archStr]];
	//			NSString *sss;
         }
      };
		
		// IWAD set
      loadURLsFromKey(@"iwadSet_v2", @selector(doAddIwadFromURL:));
		[iwadPopUp selectItemAtIndex:[defaults integerForKey:@"iwadPopUpIndex"]];

      // PWAD array
      loadURLsFromKey(@"pwadArray_v2", @selector(doAddPwadFromURL:));
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
   
   self->dontUndo = FALSE;
}

//
// goGetHelp:
//
// Go to the parameters article in the Eternity wiki. I might decide to turn it into an offline help
//
-(IBAction)goGetHelp:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://eternity.youfailit.net/index.php?title=List_of_command_line_parameters"]];
}

//
// goGetHelp:
//
// Go to the rot article in the Eternity wiki. I might decide to turn it into an offline help
//
-(IBAction)goGetHelpRoot:(id)sender
{
	[[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://eternity.youfailit.net/"]];
}

//
// showLicense
//
-(IBAction)showLicense:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"COPYING.pdf"] withApplication:@"Preview"];
}

@end	// end LauncherController class definition



// EOF


