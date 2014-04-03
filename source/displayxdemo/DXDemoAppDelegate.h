//
//  DXDemoAppDelegate.mm
//
//  Copyright 2010 tSoniq. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "DXDemoInstaller.h"
#import "DXDemoOpenGLView.h"
#import "DisplayXFBInterface.h"

@interface DXDemoAppDelegate : NSObject
{
    IBOutlet NSWindow* _window;
    IBOutlet DXDemoOpenGLView* _openGLView;
    IBOutlet NSButton* _connectButton;
    IBOutlet NSButton* _disconnectButton;

    ts::DisplayXFBInterface* _displayInterface;                                 //!< The virtual display interface (framebuffer)
    unsigned _visibleDisplayIndex;                                              //!< The number of the current display
    CGDisplayStreamRef _displayStream;                                          //!< The display stream of null if stopped
    ts::DisplayXFBConfiguration _configuration[ts::kDisplayXFBMaxDisplays];     //!< The display configuration
    const uint8_t* _displayMemory[ts::kDisplayXFBMaxDisplays];                  //!< Mapped display
    const ts::DisplayXFBCursor* _cursor[ts::kDisplayXFBMaxDisplays];            //!< Cursor data
}

- (IBAction)actionResetResolution:(id)sender;
- (IBAction)actionRemove:(id)sender;
- (IBAction)actionConnect:(id)sender;
- (IBAction)actionDisconnect:(id)sender;
- (IBAction)actionSelectDisplay:(id)sender;

@end
