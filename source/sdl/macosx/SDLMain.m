/*   SDLMain.m - main entry point for our Cocoa-ized SDL app
       Initial Version: Darrell Walisser <dwaliss1@purdue.edu>
       Non-NIB-Code & other changes: Max Horn <max@quendi.de>

    Feel free to customize this file to suit your needs
*/
/*
Copyright (C) 2012  Ioan Chera

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 
 */

#include <SDL.h>
#import "SDLMain.h"
#import "ELDumpConsole.h"
#include <sys/param.h> /* for MAXPATHLEN */
#include <unistd.h>

/* For some reaon, Apple removed setAppleMenu from the headers in 10.4,
 but the method still is there and works. To avoid warnings, we declare
 it ourselves here. */
@interface NSApplication(SDL_Missing_Methods)
- (void)setAppleMenu:(NSMenu *)menu;
@end

/* Use this flag to determine whether we use SDLMain.nib or not */
#define		SDL_USE_NIB_FILE	1

/* Use this flag to determine whether we use CPS (docking) or not */
#define		SDL_USE_CPS		1
#ifdef SDL_USE_CPS
/* Portions of CPS.h */
typedef struct CPSProcessSerNum
{
	UInt32		lo;
	UInt32		hi;
} CPSProcessSerNum;

extern OSErr	CPSGetCurrentProcess( CPSProcessSerNum *psn);
extern OSErr 	CPSEnableForegroundOperation( CPSProcessSerNum *psn, UInt32 _arg2, UInt32 _arg3, UInt32 _arg4, UInt32 _arg5);
extern OSErr	CPSSetFrontProcess( CPSProcessSerNum *psn);

#endif /* SDL_USE_CPS */

static int    gArgc;
static char  **gArgv;
static BOOL   gFinderLaunch;
static BOOL   gCalledAppMainline = FALSE;
static BOOL gSDLStarted;	// IOAN 16.06.2012

static NSString *getApplicationName(void)
{
    const NSDictionary *dict;
    NSString *appName = 0;

    /* Determine the application name */
    dict = (const NSDictionary *)CFBundleGetInfoDictionary(CFBundleGetMainBundle());
    if (dict)
        appName = [dict objectForKey: @"CFBundleName"];
    
    if (![appName length])
        appName = [[NSProcessInfo processInfo] processName];

    return appName;
}

#if SDL_USE_NIB_FILE
/* A helper category for NSString */
@interface NSString (ReplaceSubString)
- (NSString *)stringByReplacingRange:(NSRange)aRange with:(NSString *)aString;
@end
#endif

// IOAN 16.06.2012: changed from a category to a subclass
@interface NSMyApplication : NSApplication
- (void)terminateSDL:(id)sender;
@end

@implementation NSMyApplication : NSApplication
- (void)terminate:(id)sender
{
	if(gSDLStarted)
		[self terminateSDL:sender];
	else
		[super terminate:sender];
}
/* Invoked from the Quit menu item */
- (void)terminateSDL:(id)sender	// IOAN 16.06.2012: changed SDL name
{
    /* Post a SDL_QUIT event */
    SDL_Event event;
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}
@end

/* The main class of the application, the application's delegate */
@implementation SDLMain

@synthesize window;

-(void)dealloc
{
	[iwadSet release];
	[pwadTypes release];
	[iwadPopMenu release];
	[pwadArray release];
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
	
	[console release];
	
	[super dealloc];
}

-(id)init
{
	if([super init])
	{
		fileMan = [NSFileManager defaultManager];
		iwadSet = [[NSMutableSet alloc] initWithCapacity:0];
		pwadTypes = [[NSArray alloc] initWithObjects:@"cfg", @"bex", @"deh", @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", nil];
		iwadPopMenu = [[NSMenu alloc] initWithTitle:@"Choose IWAD"];
		pwadArray = [[NSMutableArray alloc] initWithCapacity:0];
		
		noIwadAlert = [[NSAlert alloc] init];
		[noIwadAlert addButtonWithTitle:@"Choose IWAD"];
		[noIwadAlert addButtonWithTitle:@"Cancel"];
		[noIwadAlert setMessageText:@"No IWAD file prepared."];
		[noIwadAlert setInformativeText:@"You need to choose a main game WAD before playing Eternity."];
		[noIwadAlert setAlertStyle:NSInformationalAlertStyle];
		
		badIwadAlert = [[NSAlert alloc] init];
		[badIwadAlert addButtonWithTitle:@"Try Another"];
		[badIwadAlert addButtonWithTitle:@"Cancel"];
		[badIwadAlert setMessageText:@"Selected file is not a valid IWAD file."];
		[badIwadAlert setInformativeText:@"You cannot load patch WAD (PWAD) files," 
		 " or the selected file may be corrupted or invalid. You need to load a main WAD that comes shipped with the game."];
		[badIwadAlert setAlertStyle:NSInformationalAlertStyle];
		
		nothingForGfsAlert = [[NSAlert alloc] init];
		[nothingForGfsAlert addButtonWithTitle:@"OK"];
		[nothingForGfsAlert setMessageText:@"There are no files to list in a GFS."];
		[nothingForGfsAlert setInformativeText:@"A GFS needs to refer to an IWAD and/or to a set of add-on files."];
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
		
		param = [[NSMutableArray alloc] initWithCapacity:0];
		
		console = [[ELDumpConsole alloc] initWithWindowNibName:@"DumpConsole"];
	}
	return self;
}

//
// initNibData
//
// Added after Nib got loaded
//
-(void)initNibData
{
	NSSize size = {525, 429};
	[window setContentMinSize:size];
	
	[iwadPopUp setMenu:iwadPopMenu];
	[self loadDefaults];
}

//
// setupWorkingDirectory:
//
// Set the working directory to the .app's parent directory
//
- (void) setupWorkingDirectory:(BOOL)shouldChdir
{
    if (shouldChdir)
    {
        char parentdir[MAXPATHLEN];
        CFURLRef url = CFBundleCopyBundleURL(CFBundleGetMainBundle());
        CFURLRef url2 = CFURLCreateCopyDeletingLastPathComponent(0, url);
        if (CFURLGetFileSystemRepresentation(url2, 1, (UInt8 *)parentdir, MAXPATHLEN)) {
            chdir(parentdir);   /* chdir to the binary app's parent */
        }
        CFRelease(url);
        CFRelease(url2);
    }
}

#if SDL_USE_NIB_FILE

//
// fixMenu:withAppName:
//
// Fix menu to contain the real app name instead of "SDL App"
//
- (void)fixMenu:(NSMenu *)aMenu withAppName:(NSString *)appName
{
    NSRange aRange;
    NSEnumerator *enumerator;
    NSMenuItem *menuItem;

    aRange = [[aMenu title] rangeOfString:@"SDL App"];
    if (aRange.length != 0)
        [aMenu setTitle: [[aMenu title] stringByReplacingRange:aRange with:appName]];

    enumerator = [[aMenu itemArray] objectEnumerator];
    while ((menuItem = [enumerator nextObject]))
    {
        aRange = [[menuItem title] rangeOfString:@"SDL App"];
        if (aRange.length != 0)
            [menuItem setTitle: [[menuItem title] stringByReplacingRange:aRange with:appName]];
        if ([menuItem hasSubmenu])
            [self fixMenu:[menuItem submenu] withAppName:appName];
    }
}

#else

//
// setApplicationMenu
//
static void setApplicationMenu(void)
{
    /* warning: this code is very odd */
    NSMenu *appleMenu;
    NSMenuItem *menuItem;
    NSString *title;
    NSString *appName;
    
    appName = getApplicationName();
    appleMenu = [[NSMenu alloc] initWithTitle:@""];
    
    /* Add menu items */
    title = [@"About " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Hide " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(hide:) keyEquivalent:@"h"];

    menuItem = (NSMenuItem *)[appleMenu addItemWithTitle:@"Hide Others" action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
    [menuItem setKeyEquivalentModifierMask:(NSAlternateKeyMask|NSCommandKeyMask)];

    [appleMenu addItemWithTitle:@"Show All" action:@selector(unhideAllApplications:) keyEquivalent:@""];

    [appleMenu addItem:[NSMenuItem separatorItem]];

    title = [@"Quit " stringByAppendingString:appName];
    [appleMenu addItemWithTitle:title action:@selector(terminate:) keyEquivalent:@"q"];

    
    /* Put menu into the menubar */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    [menuItem setSubmenu:appleMenu];
    [[NSApp mainMenu] addItem:menuItem];

    /* Tell the application object that this is now the application menu */
    [NSApp setAppleMenu:appleMenu];

    /* Finally give up our references to the objects */
    [appleMenu release];
    [menuItem release];
}

//
// setupWindowMenu
//
// Create a window menu
//
static void setupWindowMenu(void)
{
    NSMenu      *windowMenu;
    NSMenuItem  *windowMenuItem;
    NSMenuItem  *menuItem;

    windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    
    /* "Minimize" item */
    menuItem = [[NSMenuItem alloc] initWithTitle:@"Minimize" action:@selector(performMiniaturize:) keyEquivalent:@"m"];
    [windowMenu addItem:menuItem];
    [menuItem release];
    
    /* Put menu into the menubar */
    windowMenuItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    [windowMenuItem setSubmenu:windowMenu];
    [[NSApp mainMenu] addItem:windowMenuItem];
    
    /* Tell the application object that this is now the window menu */
    [NSApp setWindowsMenu:windowMenu];

    /* Finally give up our references to the objects */
    [windowMenu release];
    [windowMenuItem release];
}

//
// CustomApplicationMain
//
// Replacement for NSApplicationMain
//
static void CustomApplicationMain (int argc, char **argv)
{
    NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
    SDLMain				*sdlMain;

    /* Ensure the application object is initialised */
    [NSMyApplication sharedApplication];
    
#ifdef SDL_USE_CPS
    {
        CPSProcessSerNum PSN;
        /* Tell the dock about us */
        if (!CPSGetCurrentProcess(&PSN))
            if (!CPSEnableForegroundOperation(&PSN,0x03,0x3C,0x2C,0x1103))
                if (!CPSSetFrontProcess(&PSN))
                    [NSMyApplication sharedApplication];
    }
#endif /* SDL_USE_CPS */

    /* Set up the menubar */
    [NSApp setMainMenu:[[NSMenu alloc] init]];
    setApplicationMenu();
    setupWindowMenu();

    /* Create SDLMain and make it the app delegate */
    sdlMain = [[SDLMain alloc] init];
    [NSApp setDelegate:sdlMain];
    
    /* Start the main event loop */
    [NSApp run];
    
    [sdlMain release];
    [pool release];
}

#endif


/*
 * Catch document open requests...this lets us notice files when the app
 *  was launched by double-clicking a document, or when a document was
 *  dragged/dropped on the app's icon. You need to have a
 *  CFBundleDocumentsType section in your Info.plist to get this message,
 *  apparently.
 *
 * Files are added to gArgv, so to the app, they'll look like command line
 *  arguments. Previously, apps launched from the finder had nothing but
 *  an argv[0].
 *
 * This message may be received multiple times to open several docs on launch.
 *
 * This message is ignored once the app's mainline has been called.
 */
- (BOOL)application:(NSApplication *)theApplication openFile:(NSString *)filename
{
	NSString *ext;
	
	if (gCalledAppMainline)
		return NO;	// ignore this document, it's too late within the game
	
	for(ext in pwadTypes)
	{
		if([[filename pathExtension] caseInsensitiveCompare:ext] == NSOrderedSame)
		{
			[self addIwadOrPwad:[NSURL fileURLWithPath:filename]];
			return YES;
		}
	}
	return NO;
}

//
// applicationDidFinishLaunching:
//
- (void) applicationDidFinishLaunching: (NSNotification *) note
{
	
    /* Set the working directory to the .app's parent directory */
    [self setupWorkingDirectory:gFinderLaunch];

#if SDL_USE_NIB_FILE
    /* Set the main menu to contain the real app name instead of "SDL App" */
    [self fixMenu:[NSApp mainMenu] withAppName:getApplicationName()];
#endif

	return;
}

//
// windowWillClose:
//
// Close this application
//
- (void)windowWillClose:(NSNotification *)notification
{
	if(!gSDLStarted)
		[self updateParameters:self];
	[self saveDefaults];
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
		NSURL *wURL;
		for(wURL in pwadArray)
			if ([[[wURL path] pathExtension] caseInsensitiveCompare:@"gfs"] == NSOrderedSame) {
				goto iwadMightBe;
			}
		
		[noIwadAlert beginSheetModalForWindow:[self window] modalDelegate:self 
						 didEndSelector:@selector(chooseIwadAlertDidEnd:returnCode:contextInfo:) contextInfo:nil];
		
		return;
	}
iwadMightBe:
	
	[self updateParameters:sender];

	gArgc = [param count] + 1;
	
	char *name0;
	name0 = (char*)SDL_malloc((strlen(gArgv[0]) + 1)*sizeof(char));
	strcpy(name0, gArgv[0]);
	
	gArgv = (char**)SDL_malloc(gArgc*sizeof(char*));
	gArgv[0] = name0;
	
	NSString *compon;
	NSUInteger i = 1, len;
	for(compon in param)
	{
		len = [compon length] + 1;
		gArgv[i] = (char*)SDL_malloc(len*sizeof(char));
		
		strcpy(gArgv[i], [compon UTF8String]);
		i++;
//		[compon getCString:gArgv[i++] maxLength:len encoding:NSASCIIStringEncoding];
	}
	
	// Start console
//	[console startLogging];
	
	
	
	
	int status;
	gCalledAppMainline = TRUE;
	gSDLStarted = YES;	// IOAN 16.06.2012
	[window close];
	
    status = SDL_main (gArgc, gArgv);
	
//	[console showInstantLog];
    /* We're done, thank you for playing */
    exit(status);
}

//
// URLPointsToIwad:
//
// Verify the header
//
-(BOOL)URLPointsToIwad:(NSURL *)wURL
{
	NSFileHandle *iwadFile = [NSFileHandle fileHandleForReadingAtPath:[wURL path]];
	NSData *headerData = [iwadFile readDataOfLength:4];
	NSString *headerString = [[NSString alloc] initWithData:headerData encoding:NSASCIIStringEncoding];
	
	BOOL answer = YES;
	
	if(![headerString isEqualToString:@"IWAD"])
	{
		answer = NO;
	}

	// Leave the rest to Eternity (only check the header label)
	
	[headerString release];
	return answer;
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
		[[NSDocumentController sharedDocumentController] noteNewRecentDocumentURL:wURL];
		
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
// numberOfRowsInTableView:
//
-(NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
	
	NSInteger count=0;
	if (pwadArray)
		count=[pwadArray count];
	return count;
}

//
// tableView:objectValueForTableColumn:row:
//
-(id)tableView:(NSTableView *)aTableView objectValueForTableColumn:(NSTableColumn *)aTableColumn row:(NSInteger)rowIndex
{
	id returnValue=nil;
	NSString *columnIdentifer = [aTableColumn identifier];
	
	NSURL *pwadURL = [pwadArray objectAtIndex:rowIndex];
	NSString *pwadString = [pwadURL path];
	
	if ([columnIdentifer isEqualToString:@"pwadIcon"]) 
	{
		returnValue = [[NSWorkspace sharedWorkspace] iconForFile:pwadString];
	}
	else
	{
		NSString *niceName = [pwadString stringByAbbreviatingWithTildeInPath];
		returnValue = niceName;
	}

	return returnValue;
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
	[panel beginSheetForDirectory:nil file:nil types:pwadTypes
				   modalForWindow:[NSApp mainWindow] modalDelegate:self
				   didEndSelector:@selector(addPwadEnded:returnCode:contextInfo:)
					  contextInfo:nil];
}

//
// addAllRecentPwads:
//
-(IBAction)addAllRecentPwads:(id)sender
{
	NSURL *wURL;
	
	for(wURL in [[NSDocumentController sharedDocumentController] recentDocumentURLs])
		[self addIwadOrPwad:wURL];
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
// addIwadOrPwad:
//
-(void)addIwadOrPwad:(NSURL *)wURL
{
	if ([self URLPointsToIwad:wURL])
	{
		[self doAddIwadFromURL:wURL];
	}
	else
	{
		[self doAddPwadFromURL:wURL];
	}
	[self updateParameters:self];
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
	
	// Look thru the array to locate the IWAD and put that on.
	NSURL *openCandidate;
	
	for(openCandidate in [panel URLs])
	{
		[self addIwadOrPwad:openCandidate];
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
		else if ([[pwadView selectedRowIndexes] count] >= 1)
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
	[panel beginSheetForDirectory:nil file:nil modalForWindow:[NSApp mainWindow] modalDelegate:self 
				   didEndSelector:@selector(chooseRecordDidEnd:returnCode:contextInfo:) contextInfo:nil];
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
	[panel beginSheetForDirectory:nil file:nil types:types
				   modalForWindow:[NSApp mainWindow] modalDelegate:self
				   didEndSelector:@selector(choosePlayDidEnd:returnCode:contextInfo:)
					  contextInfo:nil];
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
		[nothingForGfsAlert beginSheetModalForWindow:[self window] modalDelegate:self 
							   didEndSelector:NULL contextInfo:nil];
		return;
	}
	
	NSSavePanel *panel = [NSSavePanel savePanel];
	NSArray *types = [NSArray arrayWithObject:@"gfs"];
	[panel setAllowedFileTypes:types];
	[panel beginSheetForDirectory:nil file:nil modalForWindow:[NSApp mainWindow] modalDelegate:self 
				   didEndSelector:@selector(saveAsGFSDidEnd:returnCode:contextInfo:) contextInfo:nil];
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
		[gfsOut appendString:[[[iwadPopUp selectedItem] representedObject] path]];
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
			for(wURL in pwadArray)
			{
				path = [wURL path];
				root = [[path pathComponents] objectAtIndex:pathPosition];
				
				if(oldroot && [root caseInsensitiveCompare:oldroot] != NSOrderedSame)
				{
					goto foundOut;
				}
				oldroot = root;
			}
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
			else if([extension caseInsensitiveCompare:@"wad"] == NSOrderedSame ||
					[extension caseInsensitiveCompare:@"lmp"] == NSOrderedSame)
			{
				[gfsOut appendString:@"wadfile = \""];
				[gfsOut appendString:path];
				[gfsOut appendString:@"\"\n"];
			}
		}

		[pathArray release];
	}
	[gfsOut writeToURL:[panel URL] atomically:YES encoding:NSUTF8StringEncoding error:NULL];
	[gfsOut release];
}

//
// updateParmRsp:
//
-(void)updateParmRsp:(id)sender
{
	[parmRsp removeAllObjects];
	// This is more complex, so divide it further
	// pwadTypes = [NSArray arrayWithObjects:@"cfg", @"bex", @"deh", @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", nil];
	
	NSUInteger i;
	NSString *extension, *path;
	for(i = 0; i < [pwadArray count]; i++)
	{
		path = [[pwadArray objectAtIndex:i] path];
		extension = [path pathExtension];
		// -config
		if([extension caseInsensitiveCompare:@"rsp"] == NSOrderedSame)
		{
			if([parmRsp count] == 0)
			{
				NSString *rstr = [[NSMutableString alloc] init];
				
				[rstr appendString:@"@"];
				[rstr appendString:path];
				
				[parmRsp addObject:rstr];
				
				[rstr release];
				break;
			}
		}
	}
}

//
// updateParmIwad:
//
-(void)updateParmIwad:(id)sender
{
	if([iwadPopUp numberOfItems] <= 0)
		[parmIwad removeAllObjects];
	else {
		[parmIwad setArray:[NSArray arrayWithObjects:@"-iwad", 
							[[[iwadPopUp selectedItem] representedObject] path], nil]];
	}
}

//
// updateParmPwad:
//
-(void)updateParmPwad:(id)sender
{
	[parmPwad removeAllObjects];
	// This is more complex, so divide it further
	// pwadTypes = [NSArray arrayWithObjects:@"cfg", @"bex", @"deh", @"edf", @"csc", @"wad", @"gfs", @"rsp", @"lmp", nil];
	
	NSMutableArray *sparmConfig, *sparmDeh, *sparmEdf, *sparmExec, *sparmFile, *sparmGfs;
	sparmConfig = [[NSMutableArray alloc] initWithCapacity:0];
	sparmDeh    = [[NSMutableArray alloc] initWithCapacity:0];
	sparmEdf    = [[NSMutableArray alloc] initWithCapacity:0];
	sparmExec   = [[NSMutableArray alloc] initWithCapacity:0];
	sparmFile   = [[NSMutableArray alloc] initWithCapacity:0];
	sparmGfs    = [[NSMutableArray alloc] initWithCapacity:0];
	
	NSUInteger i;
	NSString *extension, *path;
	for(i = 0; i < [pwadArray count]; i++)
	{
		path = [[pwadArray objectAtIndex:i] path];
		extension = [path pathExtension];
		// -config
		if([extension caseInsensitiveCompare:@"cfg"] == NSOrderedSame)
		{
			if([sparmConfig count] == 0)
				[sparmConfig addObject:@"-config"];
			[sparmConfig addObject:path];
		}
		else if([extension caseInsensitiveCompare:@"bex"] == NSOrderedSame ||
				[extension caseInsensitiveCompare:@"deh"] == NSOrderedSame)
		{
			if([sparmDeh count] == 0)
				[sparmDeh addObject:@"-deh"];
			[sparmDeh addObject:path];
		}
		else if([extension caseInsensitiveCompare:@"edf"] == NSOrderedSame)
		{
			if([sparmEdf count] == 0)
				[sparmEdf addObject:@"-edf"];
			[sparmEdf addObject:path];
		}
		else if([extension caseInsensitiveCompare:@"csc"] == NSOrderedSame)
		{
			if([sparmExec count] == 0)
				[sparmExec addObject:@"-exec"];
			[sparmExec addObject:path];
		}
		else if([extension caseInsensitiveCompare:@"wad"] == NSOrderedSame ||
				[extension caseInsensitiveCompare:@"lmp"] == NSOrderedSame)
		{
			if([sparmFile count] == 0)
				[sparmFile addObject:@"-file"];
			[sparmFile addObject:path];
		}
		else if([extension caseInsensitiveCompare:@"gfs"] == NSOrderedSame)
		{
			if([sparmGfs count] == 0)
				[sparmGfs addObject:@"-gfs"];
			[sparmGfs addObject:path];
		}
	}
	[parmPwad addObjectsFromArray:sparmGfs];
	[parmPwad addObjectsFromArray:sparmFile];
	[parmPwad addObjectsFromArray:sparmEdf];
	[parmPwad addObjectsFromArray:sparmDeh];
	[parmPwad addObjectsFromArray:sparmConfig];
	[parmPwad addObjectsFromArray:sparmExec];
	
	[sparmConfig release];
	[sparmDeh release];
	[sparmEdf release];
	[sparmExec release];
	[sparmFile release];
	[sparmGfs release];
}

//
// updateParmOthers:
//
-(IBAction)updateParmOthers:(id)sender
{
	NSUInteger i, j;
	NSRange shit, shit2;
	NSMutableString *aggr = [[NSMutableString alloc] init], *comp2 = [[NSMutableString alloc] init];
	BOOL quittest;
	
	[parmOthers setArray:[[otherField stringValue] 
						  componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	
	
	NSString *token;
	for(i = 0; i < [parmOthers count]; ++i)
	{
		token = [parmOthers objectAtIndex:i];
		if([token length] > 0 && [token characterAtIndex:0] == '\"')
		{
			[aggr appendString:[[parmOthers objectAtIndex:i] substringFromIndex:1]];
			j = i;
			shit.location = j;
			for(++i; i < [parmOthers count]; ++i)
			{
				quittest = NO;
				[comp2 setString:[parmOthers objectAtIndex:i]];
				if([comp2 length] > 0 && [comp2 characterAtIndex:[comp2 length] - 1] == '\"')
				{
					shit2.location = [comp2 length] - 1;
					shit2.length = 1;
					[comp2 deleteCharactersInRange:shit2];
					quittest = YES;
				}
				[aggr appendString:@" "];
				[aggr appendString:comp2];
				if(quittest || i + 1 == [parmOthers count])
					break;
			}
			shit.length = i - j + 1;
			[parmOthers removeObjectsInRange:shit];
			[parmOthers insertObject:aggr atIndex:j];
			i = j;
		}
	}
	[aggr release];
	[comp2 release];
}

//
// updateParmWarp:
//
-(IBAction)updateParmWarp:(id)sender
{
	[parmWarp removeAllObjects];
	if([[warpField stringValue] length] > 0)
	{
		[parmWarp addObject:@"-warp"];
		[parmWarp addObjectsFromArray:[[warpField stringValue] 
									   componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmSkill:
//
-(IBAction)updateParmSkill:(id)sender
{
	[parmSkill removeAllObjects];
	if([[skillField stringValue] length] > 0)
	{
		[parmSkill addObject:@"-skill"];
		[parmSkill addObjectsFromArray:[[skillField stringValue] 
									   componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmFlags:
//
-(IBAction)updateParmFlags:(id)sender
{
	[parmFlags removeAllObjects];
	if([respawn state] == NSOnState)
		[parmFlags addObject:@"-respawn"];
	if([fast state] == NSOnState)
		[parmFlags addObject:@"-fast"];
	if([nomons state] == NSOnState)
		[parmFlags addObject:@"-nomonsters"];
	if([vanilla state] == NSOnState)
		[parmFlags addObject:@"-vanilla"];
}

//
// updateParmRecord:
//
-(IBAction)updateParmRecord:(id)sender
{
	[parmRecord removeAllObjects];
	if([[recordDemoField stringValue] length] > 0)
	{
		[parmRecord addObject:@"-record"];
		[parmRecord addObject:[recordDemoField stringValue]];
	}
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
	
	[parmPlayDemo removeAllObjects];
	if([[playDemoField stringValue] length] > 0)
	{
		if([timedemo state] == NSOnState)
			[parmPlayDemo addObject:@"-timedemo"];
		else if([fastdemo state] == NSOnState)
			[parmPlayDemo addObject:@"-fastdemo"];
		else
			[parmPlayDemo addObject:@"-playdemo"];
		[parmPlayDemo addObject:[playDemoField stringValue]];
	}
}

//
// updateParmGameType:
//
-(IBAction)updateParmGameType:(id)sender
{
	[parmGameType removeAllObjects];
	
	switch([gameTypePopUp indexOfSelectedItem])
	{
		case 1:
			[parmGameType addObject:@"-deathmatch"];
			break;
		case 2:
			[parmGameType addObject:@"-altdeath"];
			break;
		case 3:
			[parmGameType addObject:@"-trideath"];
			break;
	}
}

//
// updateParmFragLimit:
//
-(IBAction)updateParmFragLimit:(id)sender
{
	[parmFragLimit removeAllObjects];
	if([[fragField stringValue] length] > 0)
	{
		[parmFragLimit addObject:@"-frags"];
		[parmFragLimit addObjectsFromArray:[[fragField stringValue] 
									   componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmTimeLimit:
//
-(IBAction)updateParmTimeLimit:(id)sender
{
	[parmTimeLimit removeAllObjects];
	if([[timeField stringValue] length] > 0)
	{
		[parmTimeLimit addObject:@"-timer"];
		[parmTimeLimit addObjectsFromArray:[[timeField stringValue] 
											componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmTurbo:
//
-(IBAction)updateParmTurbo:(id)sender
{
	[parmTurbo removeAllObjects];
	if([[turboField stringValue] length] > 0)
	{
		[parmTurbo addObject:@"-turbo"];
		[parmTurbo addObjectsFromArray:[[turboField stringValue] 
											componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmDmflags:
//
-(IBAction)updateParmDmflags:(id)sender
{
	[parmDmflags removeAllObjects];
	if([[dmflagField stringValue] length] > 0)
	{
		[parmDmflags addObject:@"-dmflags"];
		[parmDmflags addObjectsFromArray:[[dmflagField stringValue] 
										componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
	}
}

//
// updateParmNet:
//
-(IBAction)updateParmNet:(id)sender
{
	[parmNet removeAllObjects];
	if([[netField stringValue] length] > 0)
	{
		[parmNet addObject:@"-net"];
		[parmNet addObjectsFromArray:[[netField stringValue] 
										  componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]]];
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
	[self updateParmSkill:sender];
	[self updateParmWarp:sender];
	[self updateParmFlags:sender];
	[self updateParmRecord:sender];
	[self updateParmPlayDemo:sender];
	[self updateParmGameType:sender];
	[self updateParmFragLimit:sender];
	[self updateParmTimeLimit:sender];
	[self updateParmTurbo:sender];
	[self updateParmDmflags:sender];
	[self updateParmNet:sender];
	[self updateParmOthers:sender];
	
	[param removeAllObjects];
	
	[param addObjectsFromArray:parmRsp];
	[param addObjectsFromArray:parmIwad];
	[param addObjectsFromArray:parmPwad];
	[param addObjectsFromArray:parmSkill];
	[param addObjectsFromArray:parmWarp];
	[param addObjectsFromArray:parmFlags];
	[param addObjectsFromArray:parmRecord];
	[param addObjectsFromArray:parmPlayDemo];
	[param addObjectsFromArray:parmGameType];
	[param addObjectsFromArray:parmFragLimit];
	[param addObjectsFromArray:parmTimeLimit];
	[param addObjectsFromArray:parmTurbo];
	[param addObjectsFromArray:parmDmflags];
	[param addObjectsFromArray:parmNet];
	[param addObjectsFromArray:parmOthers];
	
	NSMutableString *infotext = [[NSMutableString alloc] init];
	
	NSString *componConst;
	NSMutableString *compon = [[NSMutableString alloc] init];
	NSInteger i, len;
	NSRange shit, shit2;
	BOOL addedEllipsis;
	for(componConst in param)
	{
		if([componConst isEqualToString:@""])
		{
			[param removeObjectIdenticalTo:componConst];
			continue;
		}
		[compon setString:componConst];
		// take care of quoted parameters add quotes for spaced parameters
		
		// put ellipsis for too long names: BEFORE last slash and AFTER a slash.
		// option ; (…)
		len = [compon length];
		if(len > 20)
		{
			addedEllipsis = NO;
			shit2.length = -1;
			for(i = len - 1; i >= 0; i--)
			{
				if([compon characterAtIndex:i] == '/')
				{
					if(shit2.length == -1)
						shit2.length = i + 1;
					else {
						shit2.location = i;
						shit2.length -= shit2.location;
						if(!addedEllipsis)
						{
							[compon replaceCharactersInRange:shit2 withString:@"…"];
							addedEllipsis = YES;
						}
						else
						{
							
							[compon deleteCharactersInRange:shit2];
						}

						if(([compon length]) <= 20)
							break;
						shit2.length = i;	// wow
					}
					
				}
			}
		}
		
		shit.location = NSNotFound;
		shit.length  = 0;
		shit2 = [compon rangeOfString:@" "];
		if(shit2.location != shit.location || shit2.length != shit.length)
		{
			[infotext appendString:@"\""];
			[infotext appendString:compon];
			[infotext appendString:@"\""];
		}
		else {
			[infotext appendString:compon];
		}
		
		
		
		[infotext appendString:@" "];
	}
	
	[compon release];
	[infoDisplay setStringValue:infotext];
	
	[infotext release];
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
	
	// IWAD list
	
	// need to save: URL array
	// IWAD set man!
	NSURL *URI;
	NSMutableArray *archArr = [[NSMutableArray alloc] init];
	for(URI in iwadSet)
		[archArr addObject:[URI relativeString]];
	
	[defaults setObject:archArr forKey:@"iwadSet"];
	[defaults setInteger:[iwadPopUp indexOfSelectedItem] forKey:@"iwadPopUpIndex"];
	// PWAD array
	[archArr removeAllObjects];
	for(URI in pwadArray)
		[archArr addObject:[URI relativeString]];
	
	[defaults setObject:archArr forKey:@"pwadArray"];
	
	[archArr release];
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
		
		// IWAD list
		
		// need to save: URL array
		// IWAD set man!
		NSArray *archArr;
		NSString *archStr;
		
		archArr = [defaults stringArrayForKey:@"iwadSet"];
		for(archStr in archArr)
			[self addIwadOrPwad:[NSURL URLWithString:archStr]];
		[iwadPopUp selectItemAtIndex:[defaults integerForKey:@"iwadPopUpIndex"]];
		// PWAD array
		archArr = [defaults stringArrayForKey:@"pwadArray"];
		for(archStr in archArr)
			[self addIwadOrPwad:[NSURL URLWithString:archStr]];
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
@end

//
// NSString (ReplaceSubString)
//
// From original SDLMain.m, I don't know what it does or why it's here...
//
@implementation NSString (ReplaceSubString)

- (NSString *)stringByReplacingRange:(NSRange)aRange with:(NSString *)aString
{
    unsigned int bufferSize;
    unsigned int selfLen = [self length];
    unsigned int aStringLen = [aString length];
    unichar *buffer;
    NSRange localRange;
    NSString *result;

    bufferSize = selfLen + aStringLen - aRange.length;
    buffer = (unichar *)NSAllocateMemoryPages(bufferSize*sizeof(unichar));
    
    /* Get first part into buffer */
    localRange.location = 0;
    localRange.length = aRange.location;
    [self getCharacters:buffer range:localRange];
    
    /* Get middle part into buffer */
    localRange.location = 0;
    localRange.length = aStringLen;
    [aString getCharacters:(buffer+aRange.location) range:localRange];
     
    /* Get last part into buffer */
    localRange.location = aRange.location + aRange.length;
    localRange.length = selfLen - localRange.location;
    [self getCharacters:(buffer+aRange.location+aStringLen) range:localRange];
    
    /* Build output string */
    result = [NSString stringWithCharacters:buffer length:bufferSize];
    
    NSDeallocateMemoryPages(buffer, bufferSize);
    
    return result;
}

@end

//
// Setup the name swap
//
#ifdef main
#  undef main
#endif

//
// NSMyApplicationMain
//
// Custom NSApplicationMain called from main below
//
void NSMyApplicationMain(int argc, char *argv[])
{
	NSAutoreleasePool	*pool = [[NSAutoreleasePool alloc] init];
    SDLMain				*sdlMain;
	
	[NSMyApplication sharedApplication];
	sdlMain = [[SDLMain alloc] init];
    [NSApp setDelegate:sdlMain];
	
	[NSBundle loadNibNamed:@"MainMenu" owner:sdlMain];
	[sdlMain initNibData];
	[NSApp run];
	
	[sdlMain release];
    [pool release];
}

//
// main
//
// Main entry point to executable - should *not* be SDL_main!
//
int main (int argc, char **argv)
{
    // Copy the arguments into a global variable
    // This is passed if we are launched by double-clicking
    if ( argc >= 2 && strncmp (argv[1], "-psn", 4) == 0 )
	{
        gArgv = (char **) SDL_malloc(sizeof (char *) * 2);
        gArgv[0] = argv[0];
        gArgv[1] = NULL;
        gArgc = 1;
        gFinderLaunch = YES;
    }
	else
	{
        int i;
        gArgc = argc;
        gArgv = (char **) SDL_malloc(sizeof (char *) * (argc+1));
        for (i = 0; i <= argc; i++)
            gArgv[i] = argv[i];
        gFinderLaunch = NO;
    }

#if SDL_USE_NIB_FILE
    NSMyApplicationMain (argc, argv);
#else
    CustomApplicationMain (argc, argv);
#endif
    return 0;
}

