//
//  ELSetupController.m
//  EternityLaunch
//
//  Created by MacBook on 16.06.2012.
//  Copyright 2012 __MyCompanyName__. All rights reserved.
//

#import "ELSetupController.h"


@implementation ELSetupController
- (void)windowDidResignKey:(NSNotification *)notification
{
	[[self window] close];
}
@end
