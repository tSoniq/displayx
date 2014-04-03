/** @file
 *  @brief      Class used to manage driver and/or application installation and uninstallation.
 *  @copyright  Copyright (c) 2012 tSoniq. All rights reserved.
 */

#import <ServiceManagement/ServiceManagement.h>
#import <Security/Authorization.h>
#import "ts_privilegedscriptrunner.h"
#import "DisplayXFBInterface.h"
#import "DXDemoInstaller.h"



#define kInstallerScriptName    "com_tsoniq_dxdemo_installer.sh"     //!< The filename of the install script (in the app's "Resources" folder).



@interface DXDemoInstaller (Private)
+ (NSInteger)modalAlert:(NSString*)title info:(NSString*)info style:(NSAlertStyle)style button0:(NSString*)bitle0 button1:(NSString*)bitle1;
@end



@implementation DXDemoInstaller


/** Method called to present a modal alert.
 *
 *  @private
 *  @param  title   The alert title.
 *  @param  info    The informational text.
 *  @param  style   The alert style (NSWarningAlertStyle, NSInformationalAlertStyle or NSCriticalAlertStyle)
 *  @param  bitle0  The title for button #0
 *  @param  bitle1  The title for button #1 (or nil for none)
 *  @return         The index of the button that was clicked to dismiss the alert.
 */
+ (NSInteger)modalAlert:(NSString*)title info:(NSString*)info style:(NSAlertStyle)style button0:(NSString*)bitle0 button1:(NSString*)bitle1
{
    NSAlert* alert = [[NSAlert alloc] init];

    [alert setMessageText:title];
    [alert setInformativeText:info];
    if (bitle0) [alert addButtonWithTitle:bitle0];
    if (bitle1) [alert addButtonWithTitle:bitle1];
    [alert setAlertStyle:style];

    return [alert runModal];
}



+ (void)doQuitAndRestart
{
    // Calling this method will ask the Finder to restart the computer and quite the current application.
    NSString *scriptAction = @"restart";
    NSString *scriptSource = [NSString stringWithFormat:@"tell application \"Finder\" to %@", scriptAction];
    NSAppleScript *appleScript = [[NSAppleScript alloc] initWithSource:scriptSource];
    NSDictionary *errDict = nil;
    if (![appleScript executeAndReturnError:&errDict])
    {
        NSLog(@"%@", errDict);
    }
    [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
}



+ (BOOL)needsInstall
{
    return !ts::DisplayXFBInterface::isInstalled();
}



+ (BOOL)doInstall
{
    ts::PrivilegedScriptRunner script(kInstallerScriptName);

    BOOL shouldQuit = NO;
    BOOL shouldInstall = NO;

    bool isInstalled = ts::DisplayXFBInterface::isInstalled();
    bool isCompatible = true;
    bool isOlder = false;

    if (!isInstalled)
    {
        NSInteger option = [DXDemoInstaller
            modalAlert:@"Driver installation required"
            info:@"The software needs authorisation to install a kernel driver in order to function.\n\n"
                  "After a successful installation the computer will automatically restart.\n\n"
                  "Select 'Install' to proceed or 'Quit' to exit the application without installing.\n\n"
            style:NSInformationalAlertStyle
            button0:@"Install"
            button1:@"Quit"];
        if (NSAlertFirstButtonReturn == option) shouldInstall = YES;
        else shouldQuit = YES;
    }
    else if (!isCompatible)
    {
        NSInteger option;
        if (isOlder)
        {
            option = [DXDemoInstaller
                modalAlert:@"Older driver installed"
                info:@"An older version of the kernel driver needed by this application is already installed.\n\n"
                      "Click 'Upgrade' to replace it with the version from this application "
                      "or 'Quit' to exit, leaving the currently installed files untouched."
                style:NSInformationalAlertStyle
                button0:@"Upgrade"
                button1:@"Quit"];
        }
        else
        {
            option = [DXDemoInstaller
                modalAlert:@"Newer driver installed"
                info:@"An newer version of the kernel driver needed by this application is already installed.\n\n"
                      "Click 'Downgrade' to replace it with the version from this application "
                      "or 'Quit' to exit, leaving the currently installed files untouched."
                style:NSInformationalAlertStyle
                button0:@"Downgrade"
                button1:@"Quit"];
        }
        if (NSAlertFirstButtonReturn == option) shouldInstall = YES;
        else shouldQuit = YES;
    }
    else if (isOlder)
    {
        NSInteger option = [DXDemoInstaller
            modalAlert:@"Older driver installed"
            info:@"An older version of the kernel driver needed by this application is already installed.\n\n"
                  "Click 'Upgrade' to replace it with the version from this application "
                  "or 'Continue' to continue using the existing installed files."
            style:NSInformationalAlertStyle
            button0:@"Upgrade"
            button1:@"Continue"];
        if (NSAlertFirstButtonReturn == option) shouldInstall = YES;
        else shouldQuit = NO;
    }

    if (shouldQuit) return FALSE;

    if (shouldInstall)
    {
        while (!script.authorise())
        {
            NSInteger option = [DXDemoInstaller
                modalAlert:@"Authorisation Failed"
                info:@"Select 'Try Again' to re-try the authorisation, or 'Quit' to close the application without installing."
                style:NSInformationalAlertStyle
                button0:@"Try Again"
                button1:@"Quit"];
            if (NSAlertSecondButtonReturn == option) break;
        }

        if (!script.isAuthorised()) return FALSE;

        // Uncomment for debug: script.setRedirect("/tmp/dfu_install_stdout.txt", "/tmp/dfu_install_stderr.txt");
        script.setRedirect("/tmp/dxdemo_install_stdout.txt", "/tmp/dxdemo_install_stderr.txt");
        if (script.start("install", "#$APPLICATION_PATH", "#$RESOURCE_PATH", "#$=USER", "#$=HOME"))
        {
            script.wait();
        }
        else
        {
            [DXDemoInstaller
                modalAlert:@"Installer Failure"
                info:@"The kernel extension could not be installed on this computer."
                      "Please contact support for assistance."
                style:NSInformationalAlertStyle
                button0:@"Quit"
                button1:nil];
            return FALSE;
        }
    }

    // If we got here, the driver should be installed and working. If may take
    // a short time from the kextload performed by the installer before the
    // service is publically accessible.
    return TRUE;
}





+ (BOOL)doRemove
{
    ts::PrivilegedScriptRunner script(kInstallerScriptName);

    NSInteger option = [DXDemoInstaller
        modalAlert:@"Uninstall this software?"
        info:@"Select 'Cancel' to continue without uninstalling, or 'Uninstall' to remove any installed drivers and move this application to the trash.\n\n"
            "After a successful uninstall the computer will automatically restart.\n\n"
        style:NSInformationalAlertStyle
        button0:@"Cancel"
        button1:@"Uninstall"];
    if (NSAlertFirstButtonReturn == option) return FALSE;

    if (!script.isAuthorised())
    {
        while (!script.authorise())
        {
            NSInteger option = [DXDemoInstaller
                modalAlert:@"Authorisation for Uninstall Failed"
                info:@"Select 'Try Again' to re-try the authorisation, or 'Cancel' to continue without uninstalling."
                style:NSInformationalAlertStyle
                button0:@"Try Again"
                button1:@"Cancel"];
            if (NSAlertSecondButtonReturn == option) return FALSE;
        }
    }

    // Uncomment for debug: script.setRedirect("/tmp/dfu_install_stdout.txt", "/tmp/dfu_install_stderr.txt");
    script.setRedirect("/tmp/dfu_install_stdout.txt", "/tmp/dfu_install_stderr.txt");
    if (script.start("remove", "#$APPLICATION_PATH", "#$RESOURCE_PATH", "#$=USER", "#$=HOME"))
    {
        script.wait();
    }
    else
    {
        [DXDemoInstaller
            modalAlert:@"Uninstall Failed"
            info:@"The kernel extension could not be removed from this computer."
                  "Please contact support for assistance."
            style:NSInformationalAlertStyle
            button0:@"Continue"
            button1:nil];
            return FALSE;
    }


    [DXDemoInstaller
        modalAlert:@"Uninstall Complete"
        info:@"The application was moved to the trash and the driver was uninstalled. The application will now quit."
        style:NSInformationalAlertStyle
        button0:@"Quit"
        button1:nil];
    return TRUE;    // The application has been moved to the trash and should exit now.
}


@end
