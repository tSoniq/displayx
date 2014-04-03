/** @file
 *  @brief      Class used to manage driver and/or application installation and uninstallation.
 *  @copyright  Copyright (c) 2012 tSoniq. All rights reserved.
 */
#import <Cocoa/Cocoa.h>

/** This is a Cocoa class used to assist application installation and removal.
 *
 *  Although the application itself is installed by drag-and-drop, additional
 *  steps are required to install the kernel driver or to uninstall the application.
 *  This class provides a simple API to perform these tasks.
 *
 *  To install:
 *
 *      if ([DXDemoInstaller needsInstall])
 *      {
 *          if (![DXDemoInstaller doInstall])              { handle-failed-install; }
 *          else if (![DXDemoInstaller waitInstall:nil])   { handle-driver-not-functional; }
 *      }
 *
 *  To uninstall:
 *
 *      if ([DXDemoInstaller doRemove])
 *      {
 *          [NSApp terminate:self];
 *          exit(0);
 *      }
 *
 *  Note that the installer is is split in to needsInstall, doInstall and waitInstall to allow a UI
 *  status display updates (if desired). The installer methods will provide any necessary prompts
 *  using modal dialogues where necessary.
 */
@interface DXDemoInstaller : NSObject


/** Quit the current application, requesting that the Finder perform a restart.
 */
+ (void)doQuitAndRestart;


/** Test if a call to doInstall is required.
 *
 *  @return     Logical true if a new installation should be performed.
 *
 *  This will test if the driver is currently installed. If not, use doInstall to install it.
 */
+ (BOOL)needsInstall;


/** Install the kernel extension.
 *
 *  @return     Logical true for success, else the installation is not valid.
 *
 *  This will test if the driver is currently installed. If not, it will automatically prompt the user and perform
 *  a new installation. This method does nothing (and will not prompt the user) if the driver is already installed
 *  and functional.
 *
 *  It is recommended that application code call this method once each time it starts, once the UI is available.
 *  If false is returned, either the driver is incompatible with the host computer, or the user did not authorise
 *  its installation. In this case the application should exit.
 *
 *  See the doInstall() method in the install script for the underlying implementation.
 *
 *  After calling this method, the client should call waitInstall to delay until the driver is available.
 *  Although this method returns as soon as the driver is installed and loaded, the OS appears to need
 *  several seconds to make the driver available to applications, and attempting to use the driver immediately
 *  after doInstall may fail.
 */
+ (BOOL)doInstall;


/** Remove the kernel extension and move the current application to the trash.
 *
 *  @return     Logical true for success, false if uninstallation failed for any reason (such as the user saying "don't").
 *
 *  This will test if the driver is currently installed. If it is, it will (may) automatically prompt the user before
 *  removing the files and moving the application to the trash.
 *
 *  This method may prompt the user if authorisation is required.
 *
 *  See the doRemove() method in install script for the underlying implementation. Note that the
 *  kextunload call in the script may fail if the client is still holding the driver open at the time that doRemove
 *  is called. In this case, the driver will remain loaded until the next reboot (but the uninstall should otherwise
 *  complete correctly).
 */
+ (BOOL)doRemove;

@end
