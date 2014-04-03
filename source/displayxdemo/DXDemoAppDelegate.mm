//
//  DXDemoAppDelegate.mm
//
//  Copyright 2010 tSoniq. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#import "DXDemoInstaller.h"
#import "DXDemoAppDelegate.h"

using namespace ts;

@interface DXDemoAppDelegate (Private)
- (void)debugLogCurrentDisplays;
- (BOOL)isDisplayStreamEnabled;
- (void)disableDisplayStream;
- (BOOL)enableDisplayStream;
- (void)updateControlState;
- (void)notificationDisplayState:(unsigned)displayIndex;
- (void)notificationCursorState:(unsigned)displayIndex;
- (void)notificationCursorImage:(unsigned)displayIndex;
- (void)notificationDisplayReconfiguration:(CGDirectDisplayID)displayID flags:(CGDisplayChangeSummaryFlags)flags;
@end




@implementation DXDemoAppDelegate


/** The display event handler. This is the callback issued on any display event from the driver.
 *
 *  These are notification issued by the display driver. Be aware that display state changes may be issued
 *  the Quartz layer internally has processed these. Therefore use the Quartz display configuration callback
 *  to detect events that require new Quartz objects to be constructed (for example, the display stream).
 */
static void displayNotificationCallback(DisplayXFBInterface::Notification notification, unsigned displayIndex, void* context)
{
    DXDemoAppDelegate* object = (__bridge DXDemoAppDelegate*)context;

    switch (notification)
    {
        case DisplayXFBInterface::kNotificationDisplayState:  [object notificationDisplayState:displayIndex];   break;
        case DisplayXFBInterface::kNotificationCursorState:   [object notificationCursorState:displayIndex];    break;
        case DisplayXFBInterface::kNotificationCursorImage:   [object notificationCursorImage:displayIndex];    break;
        default:    NSLog(@">>> Display %d unknown event %d", displayIndex, (int)notification);                 break;
    }
}


/** Display reconfiguration notification handler.
 *
 *  This is used by Quartz to notify display events.
 */
static void displayReconfigurationCallBack(CGDirectDisplayID displayID, CGDisplayChangeSummaryFlags flags, void* context)
{
    DXDemoAppDelegate* object = (__bridge DXDemoAppDelegate*)context;
    [object notificationDisplayReconfiguration:displayID flags:flags];
}





- (id)init
{
    if ((self = [super init]))
    {
        _displayInterface = new DisplayXFBInterface;
        _visibleDisplayIndex = 0;
    }
    return self;
}


- (void)dealloc
{
    CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallBack, (__bridge void*)self);

    delete _displayInterface;
    _displayInterface = 0;
}



- (void)applicationDidFinishLaunching:(NSNotification*)aNotification
{
    [self debugLogCurrentDisplays];


    // Check for an perform installation if reqiured.
    if ([DXDemoInstaller needsInstall])
    {
        if ([DXDemoInstaller doInstall]) [DXDemoInstaller doQuitAndRestart];
        else [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
        return;
    }


    // Set up the display(s).
    bool ok = _displayInterface->open();
    if (!ok)
    {
        NSLog(@"Unable to open the display interface - possible driver installation problem?");
    }
    else
    {
        // Map the display objects.
        for (unsigned i = 0; i != kDisplayXFBMaxDisplays; ++i)
        {
            DisplayXFBMap map;
            _displayInterface->displayMapFramebuffer(map, i, true);
            _displayMemory[i] = (const uint8_t*)map.address();
            _displayInterface->displayMapCursor(map, i, true);
            _cursor[i] = (const DisplayXFBCursor*)map.address();
        }

        // Setup the display configuration.
        // If the display is connected, we read whatever it was previously configured with.
        // If the display is not connected, we re-program it with a set of example resolutions.
        // This is not necessarily the best approach in a real application - it is shown here as
        // a demonstration of two possible ways of working with the display setup (use what you find
        // vs reprogramming the display modes).
        for (unsigned i = 0; i < _displayInterface->displayCount(); i++)
        {
            if (_displayInterface->displayIsConnected(i))
            {
                _displayInterface->displayGetConfiguration(_configuration[i], i);
            }
            else
            {
                char buffer[32];
                snprintf(buffer, sizeof buffer, "Display %u", i);

                // Construct a display.
                _configuration[i].appendMode(1280, 800, true);
                _configuration[i].appendMode(1440, 900);
                _configuration[i].appendMode(1600, 900);
                _configuration[i].appendMode(1920, 1080);
                _configuration[i].appendMode(1920, 1200);
                _configuration[i].appendMode(2560, 1440);
                _configuration[i].appendMode(2560, 1600);
                _configuration[i].appendMode(2880, 1800);
                _configuration[i].setRefreshRate(0.0);
                _configuration[i].setName(buffer);

                bool ok = _displayInterface->displaySetConfiguration(_configuration[i], i);
                if (!ok) NSLog(@"Warning: failed to set configuration for display %u", i);
            }
        }

        // Setup notification callbacks from the driver.
        _displayInterface->setNotificationHandler(displayNotificationCallback, (__bridge void*)self);


        // Setup reconfiguration callbacks from Quartz.
        CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, (__bridge void*)self);
    }


    [self updateControlState];

    // If the display was connected at startup, start displaying the content.
    if (_displayInterface->displayIsConnected(_visibleDisplayIndex))
    {
        [self enableDisplayStream];
    }
}


- (IBAction)actionResetResolution:(id)sender
{
    // Get the current list of displays.
    uint32_t activeDisplayCount = 0;
    CGDirectDisplayID activeDisplays[64];

    CGError result = CGGetActiveDisplayList(sizeof activeDisplays / sizeof activeDisplays[0],activeDisplays, &activeDisplayCount);
    if (result != kCGErrorSuccess)
    {
        NSLog(@"Unable to get active display list");
        return;
    }

    // Find the display ID for the current screen.
    for (unsigned i = 0; i != activeDisplayCount; i++)
    {
        unsigned targetWidth = _configuration[_visibleDisplayIndex].defaultMode().width();
        unsigned targetHeight = _configuration[_visibleDisplayIndex].defaultMode().height();
        CGDirectDisplayID did = activeDisplays[i];
        uint32_t manufacturer = CGDisplayVendorNumber(did);
        uint32_t serial = CGDisplaySerialNumber(did);
        NSLog(@"ID %08x: mfgr %08x serial %08x", (unsigned)did, (unsigned)manufacturer, (unsigned)serial);
        if (manufacturer == kDisplayXManufacturer)
        {
            CFArrayRef allModes = CGDisplayCopyAllDisplayModes(did, NULL);
            if (!allModes) continue;
            if (0 != CFArrayGetCount(allModes))
            {
                CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(allModes, 0);
                for (unsigned j = 0 ; j < CFArrayGetCount(allModes); j++)
                {
                    CGDisplayModeRef thisMode = (CGDisplayModeRef)CFArrayGetValueAtIndex(allModes, j);
                    if (CGDisplayModeGetWidth(thisMode) == targetWidth && CGDisplayModeGetHeight(thisMode) == targetHeight)
                    {
                        mode = thisMode;
                        break;
                    }
                }
                CGDisplayConfigRef config;
                CGError rc = CGBeginDisplayConfiguration(&config);
                if (rc == kCGErrorSuccess)
                {
                    CGConfigureDisplayWithDisplayMode(config, did, mode, NULL);
                    CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
                }
            }
            CFRelease(allModes);
        }
    }
}


/** Connect the virtual display. Streaming updates will be started when the display is available.
 */
- (IBAction)actionConnect:(id)sender
{
    (void)sender;

    if (!_displayInterface->isOpen()) NSLog(@"Connect: framebuffer interface not open");
    else _displayInterface->displayConnect(_visibleDisplayIndex);
    // Do not call [self enableDisplayStream] here. The display connection takes some time for the system to
    // notice, and the display stream setup will be potentially broken if it is set up immediately after displayConnect().
    // Instead, streaming should be re-initialised in the Quartz reconfiguration callback.
    [self updateControlState];
}


/** Disconnect the virtual display.
 */
- (IBAction)actionDisconnect:(id)sender
{
    (void)sender;


    if (!_displayInterface->isOpen()) NSLog(@"Disconnect: framebuffer interface not open");
    else _displayInterface->displayDisconnect(_visibleDisplayIndex);
    [self disableDisplayStream];
    [self updateControlState];
}


/** Uninstall the driver and application.
 */
- (IBAction)actionRemove:(id)sender
{
    (void)sender;
    if ([DXDemoInstaller doRemove]) [DXDemoInstaller doQuitAndRestart];
}


/** Perform a single display capture.
 */
- (IBAction)actionCapture:(id)sender
{
    (void)sender;

    // Manually capture the raw data
    if (_displayInterface->isOpen())
    {
        DisplayXFBState state;
        _displayInterface->displayGetState(state, _visibleDisplayIndex);

        // Render the texture
        [_openGLView setDesktop:(uint32_t*)_displayMemory[_visibleDisplayIndex] width:state.width() height:state.height()];

        // Render the cursor
        const ts::DisplayXFBCursor* crsr = _cursor[_visibleDisplayIndex];
        [_openGLView setCursor:(uint32_t*)crsr->pixelData() width:crsr->width() height:crsr->height() x:crsr->x() y:crsr->y() isVisible:crsr->isVisible()];
    }
    [self updateControlState];
}


/** Select the display to display. The UI element's tag should contain the virtual display index.
 */
- (IBAction)actionSelectDisplay:(id)sender
{
    unsigned newDisplayIndex = (unsigned)[sender tag];
    if (newDisplayIndex != _visibleDisplayIndex)
    {
        [self disableDisplayStream];

        _visibleDisplayIndex = newDisplayIndex;

        if (_displayInterface->displayIsConnected(_visibleDisplayIndex))
        {
            [self enableDisplayStream];
        }

        [self updateControlState];
    }
}


@end





@implementation DXDemoAppDelegate (Private)


/** Display on the console information about the currently available displays.
 */
- (void)debugLogCurrentDisplays
{
    uint32_t allocDisplayCount = 0;
    CGError result = CGGetActiveDisplayList(0, NULL, &allocDisplayCount);
    if (result != kCGErrorSuccess && allocDisplayCount == 0)
    {
        NSLog(@"No displays currently active?");
    }
    else
    {
        uint32_t activeDisplayCount = 0;
        CGDirectDisplayID* activeDisplays = (CGDirectDisplayID*)calloc(sizeof (CGDirectDisplayID), allocDisplayCount);
        if (!activeDisplays)
        {
            NSLog(@"No memory for %u display IDs?", (unsigned)allocDisplayCount);
        }
        else
        {
            result = CGGetActiveDisplayList(allocDisplayCount, activeDisplays, &activeDisplayCount);
            if (result != kCGErrorSuccess)
            {
                NSLog(@"Error reading display IDs?");
            }
            else
            {
                // Find the display ID for the current screen.
                // The display exports EDID data with a custom manufacturer and the displayIndex value as the serial number.
                for (unsigned i = 0; i != activeDisplayCount; ++i)
                {
                    CGDirectDisplayID displayID = activeDisplays[i];

                    uint32_t manufacturer = CGDisplayVendorNumber(displayID);
                    char mstring[4];
                    mstring[0] = ((manufacturer >> 10) & 31) + 'A' - 1;
                    mstring[1] = ((manufacturer >> 5) & 31) + 'A' - 1;
                    mstring[2] = ((manufacturer >> 0) & 31) + 'A' - 1;
                    mstring[3] = 0;

                    uint32_t serial = CGDisplaySerialNumber(displayID);

                    unsigned index = 0;
                    if (DisplayXFBInterface::displayIDToIndex(index, displayID))
                    {
                        NSLog(@"Display #%u:  ident %08x, manufacturer %s, virtual display index %u", i, (unsigned)displayID, mstring, index);
                    }
                    else
                    {
                        NSLog(@"Display #%u:  ident %08x, manufacturer %s, serial %u", i, (unsigned)displayID, mstring, (unsigned)serial);
                    }
                }
            }
            free(activeDisplays);
        }
    }

}



- (BOOL)isDisplayStreamEnabled
{
    return (_displayStream != NULL);
}



- (void)disableDisplayStream
{
    if (_displayStream)
    {
        CFRelease(_displayStream);
        _displayStream = 0;
    }
}




- (BOOL)enableDisplayStream
{
    [self disableDisplayStream];

    // Set up the display stream notification callbacks.
    CGDirectDisplayID displayID;
    if (DisplayXFBInterface::displayIndexToID(displayID, _visibleDisplayIndex))
    {
        unsigned width = (unsigned)CGDisplayPixelsWide(displayID);
        unsigned height = (unsigned)CGDisplayPixelsHigh(displayID);
        const void* keys[1] = { kCGDisplayStreamSourceRect };
        const void* values[1] = { CGRectCreateDictionaryRepresentation(CGRectMake(0, 0, width, height)) };

        CFDictionaryRef properties = CFDictionaryCreate(NULL, keys, values, 1, NULL, NULL);

        _displayStream = CGDisplayStreamCreate(
            displayID,
            width,
            height,
            '420f',
            properties,
                ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
                {
                    // This is a very simplistic use of the display-stream API, refreshing the entire display content on
                    // each update. In practise, it is possible to get significant performance improvements by peforming
                    // selective updates.
                    [self actionCapture:nil];

                    /*
                    size_t rectCount = 0;
                    const CGRect* rect = CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateDirtyRects, &rectCount);
                    NSLog(@"Notify: %16llx %u", (unsigned long long)displayTime, (unsigned)rectCount);
                    while (rectCount != 0)
                    {
                        NSLog(@"        %f %f : %f %f", rect->origin.x, rect->origin.y, rect->size.width, rect->size.height);
                        ++ rect;
                        -- rectCount;
                    }
                    */
                }
            );
    }


    if (_displayStream)
    {
        CFRunLoopSourceRef runLoopSource = CGDisplayStreamGetRunLoopSource(_displayStream);
        CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource, kCFRunLoopCommonModes);

        CGError err = CGDisplayStreamStart(_displayStream);
        if (err != CGDisplayNoErr)
        {
            NSLog(@"Error %u starting display stream", (unsigned)err);
            CFRelease(_displayStream);
            _displayStream = 0;
        }
    }

    return (0 != _displayStream);
}



- (void)updateControlState
{
    // Set the button enables and status field(s) based on current state.
    BOOL enableConnect = FALSE;
    BOOL enableDisconnect = FALSE;
    NSString* statusText = @"Failed";

    DisplayXFBState state;

    if (![self isDisplayStreamEnabled])
    {
        [_openGLView setBlank];
    }

    if (!_displayInterface->displayGetState(state, _visibleDisplayIndex))
    {
        statusText = @"Failed to get display state";
    }
    else if (state.isConnected())
    {
        enableDisconnect = TRUE;
        statusText = [NSString stringWithFormat:@"%u x %u", (unsigned)state.width(), (unsigned)state.height()];
    }
    else
    {
        enableConnect = TRUE;
        statusText = @"Disconnected";
    }

    [_connectButton setEnabled:enableConnect];
    [_disconnectButton setEnabled:enableDisconnect];

    NSString* windowTitle = [NSString stringWithFormat:@"Display:%u -- %@ -- %@",
        (unsigned)_visibleDisplayIndex,
        statusText,
        (_displayStream) ? @"Running" : @"Stopped"];

    [_window setTitle:windowTitle];
}



- (void)notificationDisplayState:(unsigned)displayIndex
{
    [self actionCapture:nil];
    [self updateControlState];
}


- (void)notificationCursorState:(unsigned)displayIndex
{
    const DisplayXFBCursor* crsr = _cursor[_visibleDisplayIndex];
    [_openGLView setCursor:(uint32_t*)crsr->pixelData() width:crsr->width() height:crsr->height() x:crsr->x() y:crsr->y() isVisible:crsr->isVisible()];
}


- (void)notificationCursorImage:(unsigned)displayIndex
{
    const DisplayXFBCursor* crsr = _cursor[_visibleDisplayIndex];
    [_openGLView setCursor:(uint32_t*)crsr->pixelData() width:crsr->width() height:crsr->height() x:crsr->x() y:crsr->y() isVisible:crsr->isVisible()];
}


- (void)notificationDisplayReconfiguration:(CGDirectDisplayID)displayID flags:(CGDisplayChangeSummaryFlags)flags
{
    unsigned displayIndex;
    bool isVirtualDisplay = _displayInterface->displayIDToIndex(displayIndex, displayID);
    if (isVirtualDisplay && displayIndex == _visibleDisplayIndex)
    {
        // This is a reconfiguration notification for the display that we are currently showing.
        if (0 != (flags & kCGDisplayBeginConfigurationFlag))
        {
            // Starting a re-configure.
            [self disableDisplayStream];
        }
        else if (0 == (flags & kCGDisplayRemoveFlag) && 0 == (flags & kCGDisplayDisabledFlag))
        {
            // Not removed or disabled, so try to (re)start the stream.
            [self enableDisplayStream];
        }
        else
        {
            // Being removed or disabled.
            [self disableDisplayStream];
        }


        [self updateControlState];
    }
}


@end




