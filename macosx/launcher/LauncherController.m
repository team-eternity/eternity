// Emacs style mode select -*- Objective-C -*-
//----------------------------------------------------------------------------
//
// Copyright(C) 2012 Ioan Chera
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
// Source file for front-end interface
//
//----------------------------------------------------------------------------

//
// gArgc and gArgv
//
extern NSArray *gArgArray;
extern BOOL g_hasParms;

// TODO:
//
// * Backspace (delete) key for removing file entries
// * Options to copy the whole parameter list to clipboard (not by selecting
//   directly) (???)
// * Full help instead of Internet link

#import "LauncherController.h"
#import "ELDumpConsole.h"
#import "ELFileViewDataSource.h"
#import "ELCommandLineArray.h"
#import "ELCommandLineArgument.h"
#import "ELAboutController.h"

#define SET_UNDO(a, b, c)     NSUndoManager *undom = [self getUndoFor:a]; \
                              [[undom prepareWithInvocationTarget:self] b]; \
                              [undom setActionName:c];

static NSInteger prevState = 0; // heh

static BOOL calledAppMainline = FALSE;

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
	[m_aboutController release];
	
	[iwadSet release];
	[pwadTypes release];
	[iwadPopMenu release];
	[pwadArray release];
   
	[noIwadAlert release];
	[badIwadAlert release];
	[nothingForGfsAlert release];
   [overwriteDemoAlert release];
   [recordDemoIsDir release];
	
	[param release];
	[basePath release];
	[userPath release];
	
	[console release];
	
	[task release];
	
	[super dealloc];
}

//
// createAlertBoxes
//
// Creates the various alert boxes used by this application
//

#define CREATE_ALERT_BOX(NAME, BUTTON1, BUTTON2, MESSAGE, INFORMATIVE, STYLE) \
             (NAME) = [[NSAlert alloc] init]; \
            [(NAME) addButtonWithTitle:(BUTTON1)]; \
if(BUTTON2 != nil) [(NAME) addButtonWithTitle:(BUTTON2)]; \
            [(NAME) setMessageText:    (MESSAGE)]; \
            [(NAME) setInformativeText:(INFORMATIVE)]; \
            [(NAME) setAlertStyle:     (STYLE)]


- (void)createAlertBoxes
{
   CREATE_ALERT_BOX(noIwadAlert,
                    @"Choose IWAD",
                    @"Cancel",
                    @"No IWAD file prepared",
                    @"You need to choose a main game WAD before playing Eternity.",
                    NSInformationalAlertStyle);
   
   CREATE_ALERT_BOX(badIwadAlert,
                    @"Try Another",
                    @"Cancel",
                    @"Selected file is not a valid IWAD file.",
                    @"You cannot load patch WAD (PWAD) files, or the selected file may be corrupted or invalid. You need to load a main WAD that comes shipped with the game.",
                    NSInformationalAlertStyle);
   
   CREATE_ALERT_BOX(nothingForGfsAlert,
                    @"OK",
                    nil,
                    @"There are no files to list in a GFS.",
                    @"A GFS needs to refer to an IWAD and/or to a set of add-on files.",
                    NSInformationalAlertStyle);
   
   CREATE_ALERT_BOX(overwriteDemoAlert,
                    @"Cancel Game",
                    @"Overwrite and Start",
                    @"Overwrite demo to be recorded?",
                    @"The target file in the \"Record Demo:\" field already exists and will be overwritten if you proceed. Make sure not to lose valuable data.",
                    NSCriticalAlertStyle);
   
   CREATE_ALERT_BOX(recordDemoIsDir,
                    @"Dismiss",
                    nil,
                    @"Cannot record demo.",
                    @"The target file in the \"Record Demo:\" field is a directory.",
                    NSWarningAlertStyle);
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
		iwadSet = [[NSMutableSet alloc] init];
		pwadTypes = [[NSArray alloc] initWithObjects:@"cfg", @"bex", @"deh", 
                   @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", @"pk3",
                   @"pke", @"zip", @"disk", nil];
		iwadPopMenu = [[NSMenu alloc] initWithTitle:@"Choose IWAD"];
		pwadArray = [[NSMutableArray alloc] initWithCapacity:0];
      
      [self createAlertBoxes];
      
		param = [[ELCommandLineArray alloc] init];
		
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
   NSArray *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"docs"] error:&err];
   
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
      
      item = [[[NSMenuItem alloc] initWithTitle:[fname stringByDeletingPathExtension]
                                         action:@selector(gotoDoc:)
                                  keyEquivalent:@""] autorelease];
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
   
   
   [self setupWorkingDirectory];
   
   if(g_hasParms)
   {
      // Launch directly if parameters provided
      [self executeGame:YES withArgs:gArgArray];
      // Do not reset state. Instead when the task ends, the app self-terminates.
      
//      g_hasParms = NO;
//      [gArgArray release];
//      gArgArray = nil;
   }
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
// -applicationSupportDirectory
//
// The "library/application support" directory
//
// Copied from help HTML: https://developer.apple.com/library/mac/#documentation/FileManagement/Conceptual/FileSystemProgrammingGUide/AccessingFilesandDirectories/AccessingFilesandDirectories.html#//apple_ref/doc/uid/TP40010672-CH3-SW3
//
- (NSString*)applicationSupportDirectory
{
   NSArray* possibleURLs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
	NSString* appSupportDir = nil;
	NSString* appDirectory = nil;
	
	if ([possibleURLs count] >= 1)
	{
		// Use the first directory (if multiple are returned)
		appSupportDir = [possibleURLs objectAtIndex:0];
	}
	
	// If a valid app support directory exists, add the
	// app's bundle ID to it to specify the final directory.
	if ([appSupportDir length] > 0)
	{
		NSString* appBundleID = [[NSBundle mainBundle] bundleIdentifier];
		appDirectory = [appSupportDir stringByAppendingPathComponent:appBundleID];
	}
	
	return appDirectory;
}

//
// tryCreateDir
//
// Helper function
//
BOOL tryCreateDir(NSString* basePath, NSString* name, NSWindow* window)
{
   NSError* err = nil;
   if(![[NSFileManager defaultManager] createDirectoryAtPath:[basePath stringByAppendingPathComponent:name] withIntermediateDirectories:YES attributes:nil error:&err])
   {
      NSAlert* alert = [NSAlert alertWithMessageText:@"Couldn't setup Eternity storage."
                                       defaultButton:@"Continue anyway"
                                     alternateButton:nil
                                         otherButton:nil
                           informativeTextWithFormat:@"Eternity failed to finish setting up its local folder in 'Library' where screenshots, configs and saves are stored. Eternity may not run correctly.\n\nMore exactly: failed to create '%@'. Description of error: %@", name, [err localizedDescription]];
      [alert setAlertStyle:NSCriticalAlertStyle];
      [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse returnCode) {}];
      return NO;
   }
   return YES;
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
   NSString *appDataPath = [self applicationSupportDirectory];
   if([appDataPath length] == 0)
   {
      NSAlert* alert = [NSAlert alertWithMessageText:@"Couldn't find Application Support folder."
                                       defaultButton:@"Continue anyway"
                                     alternateButton:nil
                                         otherButton:nil
                           informativeTextWithFormat:@"The folder where Eternity would save its saves/screenshots/etc. (i.e. 'Library/Application Support', usually hidden in Finder) could not be found or created. Eternity may not run correctly."];
      [alert setAlertStyle:NSCriticalAlertStyle];
      [alert beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse returnCode) {}];
      return;
   }
   
  	NSString *usrPath = [appDataPath stringByAppendingPathComponent:@"user"];
   
   if(!tryCreateDir(usrPath, @"doom", self.window))
      return;
   if(!tryCreateDir(usrPath, @"doom2", self.window))
      return;
   if(!tryCreateDir(usrPath, @"hacx", self.window))
      return;
   if(!tryCreateDir(usrPath, @"heretic", self.window))
      return;
   if(!tryCreateDir(usrPath, @"plutonia", self.window))
      return;
   if(!tryCreateDir(usrPath, @"shots", self.window))
      return;
   if(!tryCreateDir(usrPath, @"tnt", self.window))
      return;
   
	NSString *basPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:@"base"];
		
	[userPath release];
	userPath = [usrPath retain];
	
	[basePath release];
	basePath = [basPath retain];
	
	// Now it points to Application Support
	
	// NOTE: since Eternity may still use "." directly, I'm still using the shell chdir command.
	// TODO: copy bundled prototype user data into workingDirPath, if it doesn't exist there yet
	// FIXME: make workingDirPath a member variable.
	// COPIED from SDLMain.m
}

//
// application:openFile:
//
// Handle Finder interface file opening (double click, right click menu, drag-drop icon)
// IOAN 20121203: removed long winded SDL comment
//
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	 if (g_hasParms || calledAppMainline)
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
	// [self setupWorkingDirectory];

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
	if(!calledAppMainline)
		[self updateParameters:self];
	
	// Save GUI configurations
	[self saveDefaults];	// FIXME: get user configurations into the preferences, 
                        // don't read them from bundle
	if(!calledAppMainline)
		[NSApp terminate:[self window]];

}

//
// chooseIwadAlertDidEnd:returnCode:contextInfo:
//
-(void)chooseIwadAlertDidEnd:(NSAlert *)alert returnCode:(NSInteger)returnCode contextInfo:(void *)contextInfo
{
	if(returnCode == NSAlertFirstButtonReturn)
	{
		[[alert window] orderOut:self];
		[self addIwad:self];
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
// Access the console
//
-(IBAction)showConsole:(id)sender
{
   [console makeVisible];
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
	calledAppMainline = FALSE;

	[[self window] orderFront:nil];
	
	[[param argumentWithIdentifier:@"-base"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-user"] setEnabled:NO];
   
   // Clear the demo record field
   [recordDemoField setStringValue:@""];
   [self updateParameters:self];
   
   if(g_hasParms)
      [NSApp terminate:self];
}

//
// executeGame:withArgs:
//
// Run the game given the args and arch
//
- (void)executeGame:(BOOL)x64flag withArgs:(NSArray *)deploy
{
   //
	// Start console
   //
   
	[[self window] orderOut:self];
	
	// IOAN 20130103: use Neil's PrBoom-Mac Launcher code
	[task release];
	task = [[NSTask alloc] init];
   NSString *enginePath = [[NSBundle mainBundle] pathForResource:@"eternity" ofType:nil];
   if(!enginePath)
   {
      NSBeep();   // Unexpected error not to have an EE executable, at any rate
      // Beep of death
      return;
   }
	
   __block char *env;
   NSMutableDictionary *environment = [[[NSMutableDictionary alloc] initWithCapacity:2] autorelease];
   [environment setDictionary:@{@"ETERNITYUSER":userPath, @"ETERNITYBASE":basePath}];
   
   
   void (^AddEnv)(const char *) = ^(const char *envname)
   {
      env = getenv(envname);
      if(env)
         [environment setObject:[NSString stringWithCString:env encoding:NSUTF8StringEncoding] forKey:[NSString stringWithCString:envname encoding:NSUTF8StringEncoding]];
   };
   
   AddEnv("DOOMWADDIR");
   AddEnv("DOOMWADPATH");
   
   [task setEnvironment:environment];
   [task setLaunchPath:enginePath];
   [task setArguments:deploy];
   
   calledAppMainline = TRUE;
	
	[console startLogging:task];
	
   //	[console showInstantLog];
   // We're done, thank you for playing
   //   if(status == 0)   // only exit if it's all ok
   //    exit(status);
   
}

//
// doLaunchGame
//
// Everything is set, start the game.
//
-(void)doLaunchGameAs64Bit:(BOOL)x64flag
{
	[self updateParameters:self];	// Update parameters.
   // FIXME: do it in real-time
	
	// Add -base and user here

   [self executeGame:x64flag withArgs:[param deployArray]];
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
      [self doLaunchGameAs64Bit:*(BOOL *)contextInfo];
   }
}

//
// launchGame:
//
// Start the game
//
-(IBAction)launchGame:(id)sender
{
   static BOOL x64 = YES;
   if (sender == runAs32BitMenuItem)
      x64 = NO;
   else
      x64 = YES;
   
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
   if([[NSFileManager defaultManager] fileExistsAtPath:[recordDemoField stringValue] isDirectory:&isDir])
   {
      if(isDir)
      {
         [recordDemoIsDir beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:NULL contextInfo:NULL];
         return;
      }
      else
      {
         [overwriteDemoAlert beginSheetModalForWindow:[self window] modalDelegate:self didEndSelector:@selector(overwriteDemoDidEnd:returnCode:contextInfo:) contextInfo:&x64];
         
         return;
      }
   }
   
   [self doLaunchGameAs64Bit:x64];
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
		      
      [iwadPopUp insertItemWithTitle:[[NSFileManager defaultManager] displayNameAtPath:iwadPath] atIndex:ind];
      
      SET_UNDO(iwadPopUp, doRemoveIwadAtIndex:ind, @"Add/Remove Game WAD")
      
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
      
      SET_UNDO(iwadPopUp, doAddIwadFromURL:iwadURL atIndex:ind, @"Add/Remove Game WAD")
      
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
	[panel setCanChooseDirectories:true];
   [panel setMessage:@"Choose add-on file"];
   [panel setAllowedFileTypes:pwadTypes];
   [panel beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger result) {
      if(result == NSFileHandlingPanelCancelButton)
      {
         return;
      }

      // Look thru the array to locate the PWAD and put that on.

      [self doAddPwadsFromURLs:[panel URLs] atIndexes:nil];
   }];
}

//
// showFileInFinder:
//
-(IBAction)showFileInFinder:(id)sender
{
	[[NSWorkspace sharedWorkspace] openFile:
    [[pwadArray objectAtIndex:[pwadView selectedRow] ] path]
                           withApplication:@"Finder"];
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
   [panel setAllowedFileTypes:pwadTypes];
   [panel beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger result) {
      if(result == NSFileHandlingPanelCancelButton)
      {
         return;
      }

      // Look thru the array to locate the IWAD and put that on.
      NSURL *openCandidate;

      for(openCandidate in [panel URLs])
      {
         [self doAddIwadFromURL:openCandidate];
         
      }
   }];
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
      anIndexSet = [NSIndexSet
                    indexSetWithIndexesInRange:NSMakeRange([pwadArray count],
                                                           [wURLArray count])];
   // nothing designated, so add at end.
   
   SET_UNDO(pwadView, doRemovePwadsAtIndexes:anIndexSet, @"Add/Remove Files")
   
   [pwadArray insertObjects:wURLArray atIndexes:anIndexSet];
   
   NSURL *URL;
   for (URL in wURLArray)
   {
      [[NSDocumentController sharedDocumentController]
       noteNewRecentDocumentURL:URL];
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
   SET_UNDO(pwadView, doAddPwadsFromURLs:undoRemoveURLs atIndexes:set, @"Add/Remove Files")
   
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
   [panel beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger result) {
      if(result == NSFileHandlingPanelCancelButton)
      {
         return;
      }
      [self setRecordDemo:[[panel URL] path]];
   }];
}

//
// resetRecordDemo
//
-(void)resetRecordDemo
{
   NSString *strval = [recordDemoField stringValue];
   SET_UNDO(recordDemoField, setRecordDemo:strval, @"Set Demo Record")
   
   [recordDemoField setStringValue:@""];
	
	[self updateParameters:self];
}

//
// setRecordDemo:
//
-(void)setRecordDemo:(NSString *)path
{
   SET_UNDO(recordDemoField, resetRecordDemo, @"Set Demo Record")
   
   [recordDemoField setStringValue:path];
	
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
   [panel setAllowedFileTypes:types];
   [panel beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger result) {
      if(result == NSFileHandlingPanelCancelButton)
      {
         return;
      }
      [playDemoField setStringValue:[[panel URL] relativePath]];

      [self updateParameters:self];
   }];
}

//
// clearPlayDemo:
//
-(IBAction)clearPlayDemo:(id)sender
{
   [self toggleBlankValue:playDemoField toValue:@""];
   [self toggleBlankState3:demotype toState:0];
}

//
// toggleBlankValue:toValue:
//
-(void)toggleBlankValue:(NSTextField *)field toValue:(NSString *)value
{
   if([[field stringValue] isEqualToString:value])
      return;
   
   SET_UNDO(field, toggleBlankValue:field toValue:[field stringValue], @"Change Value")
   
   [field setStringValue:value];
   
   [self updateParameters:self];
}

//
// toggleBlankState:toState:
//
-(void)toggleBlankState:(NSButton *)field toState:(NSCellStateValue)state
{
   if([field state] == state)
      return;
   SET_UNDO(field, toggleBlankState:field toState:[field state], @"Change Value")
   
   [field setState:state];
   
   [self updateParameters:self];
}

//
// toggleBlankState2:toState:
//
-(void)toggleBlankState2:(NSPopUpButton *)field toState:(NSInteger)state
{
   if([field indexOfSelectedItem] == state)
      return;
   SET_UNDO(field, toggleBlankState2:field toState:[field indexOfSelectedItem], @"Change Value")
   
   [field selectItemAtIndex:state];
   
   [self updateParameters:self];
}

//
// toggleBlankState3:toState:
//
-(void)toggleBlankState3:(NSMatrix *)field toState:(NSInteger)state
{
   if([field selectedRow] == state)
      return;
   SET_UNDO(field, toggleBlankState3:field toState:[field selectedRow], @"Change Value")
   
   [field selectCellAtRow:state column:0];
   
   [self updateParameters:self];
}

//
// clearRecordDemo:
//
// Clear the record demo and warp inputs
//
-(IBAction)clearRecordDemo:(id)sender
{
   [self toggleBlankValue:recordDemoField toValue:@""];
   [self toggleBlankValue:warpField toValue:@""];
   [self toggleBlankValue:skillField toValue:@""];
   
   [self toggleBlankState:respawn toState:NSOffState];
   [self toggleBlankState:fast toState:NSOffState];
   [self toggleBlankState:nomons toState:NSOffState];
   [self toggleBlankState:vanilla toState:NSOffState];

}

//
// clearNetwork:
//
// Clear the network inputs
//
-(IBAction)clearNetwork:(id)sender
{
   [self toggleBlankValue:fragField toValue:@""];
   [self toggleBlankValue:timeField toValue:@""];
   [self toggleBlankValue:turboField toValue:@""];
   [self toggleBlankValue:dmflagField toValue:@""];
   [self toggleBlankValue:netField toValue:@""];
   
   [self toggleBlankState2:gameTypePopUp toState:0];
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
   [panel beginSheetModalForWindow:[NSApp mainWindow] completionHandler:^(NSInteger result) {
      if(result != NSFileHandlingPanelCancelButton)
         [self saveAsGFSDidEnd:panel];
   }];
}

//
// saveAsGFSDidEnd:returnCode:contextInfo:
//
// Save parameters into a GFS
//
-(void)saveAsGFSDidEnd:(NSSavePanel *)panel
{
	NSMutableString *gfsOut = [[[NSMutableString alloc] init] autorelease];
	
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
		NSMutableArray *pathArray = [[[NSMutableArray alloc] initWithCapacity:0] autorelease];

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
         else if([extension caseInsensitiveCompare:@"disk"] == NSOrderedSame
                 || [extension caseInsensitiveCompare:@"gfs"] == NSOrderedSame
                 || [extension caseInsensitiveCompare:@"rsp"] == NSOrderedSame)
         {
            // Do nothing in case of some extensions (which cannot be -file)
            // FIXME: refactor this. 
            ;
         }
			else
			{
             // any other extension defaults to wadfile
				[gfsOut appendString:@"wadfile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
		}
	}
	[gfsOut writeToURL:[panel URL] atomically:YES encoding:NSUTF8StringEncoding
                error:NULL];
   
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
// Updates the command-line parameter for the IWAD. If a .disk image is set,
// don't load it here but in updateParmPwad: (which deals with the other files)
//
-(void)updateParmIwad:(id)sender
{
	if([iwadPopUp numberOfItems] <= 0
      || [[[[[iwadPopUp selectedItem] representedObject] path] pathExtension]
          caseInsensitiveCompare:@"disk"] == NSOrderedSame)
	{
      // Either nothing selected, or a .disk image is loaded - so load it from
      // updateParmPwad: below
		[[param argumentWithIdentifier:@"-iwad"] setEnabled:NO];
	}
	else
	{
		[[param argumentWithIdentifier:@"-iwad"] setEnabled:YES];
		[[[param argumentWithIdentifier:@"-iwad"] extraWords] removeAllObjects];
		[[[param argumentWithIdentifier:@"-iwad"] extraWords]
       addObject:[[[iwadPopUp selectedItem] representedObject] path]];
	}
}

//
// addIwadDiskParm
//
// Needs to be called before adding .disk from table pwadView
//
-(void)addIwadDiskParm
{
   if([iwadPopUp numberOfItems] <= 0
      || [[[[[iwadPopUp selectedItem] representedObject] path] pathExtension]
          caseInsensitiveCompare:@"disk"] != NSOrderedSame)
	{
      // Either nothing selected, or a .disk image is loaded - so load it from
      // updateParmPwad: below
		[[param argumentWithIdentifier:@"-disk"] setEnabled:NO];
	}
	else
	{
		[[param argumentWithIdentifier:@"-disk"] setEnabled:YES];
		[[[param argumentWithIdentifier:@"-disk"] extraWords] removeAllObjects];
		[[[param argumentWithIdentifier:@"-disk"] extraWords]
       addObject:[[[iwadPopUp selectedItem] representedObject] path]];
	}
}

//
// updateParmPwad:
//
// Updates the command-line parameters for various add-on files (not just PWADs)
//
-(void)updateParmPwad:(id)sender
{
	
	[[param argumentWithIdentifier:@"-config"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-deh"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-edf"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-exec"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-gfs"] setEnabled:NO];
 	[[param argumentWithIdentifier:@"-file"] setEnabled:NO];
   // Take care of disk from IWAD first...
   [self addIwadDiskParm];
   
	NSDictionary *specialParams = [NSDictionary dictionaryWithObjectsAndKeys:
                                  @"-config", @"cfg",
                                  @"-deh", @"bex",
                                  @"-deh", @"deh",
                                  @"-edf", @"edf",
                                  @"-exec", @"csc",
                                  @"-gfs", @"gfs",
                                  @"-disk", @"disk", nil];
	
	NSURL *url;
	NSString *exttest, *parmtest, *extension, *path;
	BOOL found;
	for(url in pwadArray)
	{
		path = [url path];
		extension = [path pathExtension];
		
		found = NO;
      
      // Look for any extension that shouldn't be treated as a mission pack
      // (-file)
		for(exttest in specialParams)
		{
			if([extension caseInsensitiveCompare:exttest] == NSOrderedSame)
			{
				parmtest = [specialParams objectForKey:exttest];
				if(![[param argumentWithIdentifier:parmtest] enabled])
				{
					[[param argumentWithIdentifier:parmtest] setEnabled:YES];
					[[[param argumentWithIdentifier:parmtest] extraWords]
                removeAllObjects];
				}
				[[[param argumentWithIdentifier:parmtest] extraWords]
             addObject:path];
				found = YES;
            
            // if found an extension, don't bother looking for others
            break;
			}
		}
		
      // No special extension found, fallback to -file (may or may not work)
		if(!found)
		{
			if(![[param argumentWithIdentifier:@"-file"] enabled])
			{
				[[param argumentWithIdentifier:@"-file"] setEnabled:YES];
				[[[param argumentWithIdentifier:@"-file"] extraWords]
             removeAllObjects];
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
	
	
	[[param argumentWithIdentifier:@"-timedemo"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-fastdemo"] setEnabled:NO];
	[[param argumentWithIdentifier:@"-playdemo"] setEnabled:NO];
	if([[playDemoField stringValue] length] > 0)
	{
		NSString *name;
      switch([demotype selectedRow])
      {
         default:
            name = @"-playdemo";
            break;
         case 1:
            name = @"-timedemo";
            break;
         case 2:
            name = @"-fastdemo";
            break;
      }
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
// doMakeCheckboxUndo:
//
-(void)doMakeCheckboxUndo:(NSCellStateValue)state toSender:(NSButton *)sender
{
   SET_UNDO(sender, doMakeCheckboxUndo:(state == NSOnState ? NSOffState : NSOnState) toSender:sender, @"Toggle Value")
   
   [sender setState:state];
   
   [self updateParameters:sender];
}

//
// makeCheckboxUndo:
//
-(IBAction)makeCheckboxUndo:(id)sender
{
   [self doMakeCheckboxUndo:[sender state] toSender:sender];
}

//
// doMakeRadioUndo:
//
-(void)doMakeRadioUndo:(NSInteger)state toSender:(NSMatrix *)sender
{
   SET_UNDO(sender, doMakeRadioUndo:prevState toSender:sender, @"Change Value")
   
   [sender selectCellAtRow:state column:0];
   
   prevState = state;
   
   [self updateParameters:sender];
}

//
// makeRadioUndo:
//
-(IBAction)makeRadioUndo:(id)sender
{
   [self doMakeRadioUndo:[sender selectedRow] toSender:sender];
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
	[infoDisplay scrollToEndOfDocument:sender];
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
   [defaults setInteger:[demotype selectedRow] forKey:@"demotype"];
	
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
      [demotype selectCellAtRow:(prevState = [defaults integerForKey:@"demotype"]) column:0];
		
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

///
/// Displays a custom about panel
///
-(IBAction)showAboutPanel:(id)sender
{
	if(!m_aboutController)
		m_aboutController = [[ELAboutController alloc] initWithWindowNibName:@"About"];
	[m_aboutController showWindow:nil];
	
}

@end	// end LauncherController class definition

// EOF


