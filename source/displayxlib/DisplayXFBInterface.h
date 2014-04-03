/** @file   DisplayXFBInterface.h
 *  @brief  User-space client interface to the frame buffer.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_DisplayXFBInterface_H
#define COM_TSONIQ_DisplayXFBInterface_H   (1)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOMessage.h>
#include "DisplayXFBShared.h"

namespace ts
{
    /** This is the application interface to the framebuffer driver.
     *
     *  See DisplayXFBUserClient for a description of the messaging protocol. Note that the APIs
     *  provided here are not necessarily a one-to-one mapping of the messages, but are provided
     *  for client convenience.
     */
    class DisplayXFBInterface
    {
    public:

        /** Enumeration for notification callbacks.
         *
         *      Notification    Associated Data         Source
         *      ------------    ---------------         ------
         *      state           DisplayXFBState         framebuffer notification (IOKit)
         *      cursor          DisplayXFBCursor        framebuffer notification (IOKit)
         *      update          DisplayXFBUpdateBuffer  GA plug-in (mach messaging)
         */
        enum Notification
        {
            kNotificationDisplayState       =   1,      //! A display's state has been changed by the user
            kNotificationCursorState        =   2,      //! The cursor has moved or changed image
            kNotificationCursorImage        =   3,      //! The cursor has moved or changed image
            kNotificationDisplayUpdate      =   4       //! The display data has been updated
        };


        /** The prototype for a c-function callback signalling an update to the display.
         *
         *  @param  notification    The notification type.
         *  @param  displayIndex    The display for which the notification is being raised.
         *  @param  context         The client specific context handle.
         */
        typedef void (*NotificationHandler)(enum Notification notification, unsigned displayIndex, void* context);


        /** Test if the driver is installed.
         *
         *  @return     true if a compatible driver is present, false otherwise.
         */
        static bool isInstalled();


        /** Find the Quartz display ID for a connected virtual display.
         *
         *  @param      displayID           Returns the display ID.
         *  @param      displayIndex        The virtual display number.
         *  @return                         Logical true for success, false for failure.
         *
         *  This method does not require an open driver connection to function. It will fail if the requested
         *  display index is out of range or if the requested display is not currently connected.
         *
         *  Application code can use this method to determine the CGDirectDisplayID to allow the use of normal
         *  Quartz low-level calls for a specific display. Typically, the ID is used to connect to the IOSurface
         *  stream methods.
         */
        static bool displayIndexToID(CGDirectDisplayID& displayID, unsigned displayIndex);


        /** Get the virtual display index given a Quartz display ID.
         *
         *  @param  displayIndex            Returns the display index.
         *  @param  displayID               The Quartz display ID to query.
         *  @return                         true for success, false if the Quartz ID does not correspond to one of our displays.
         */
        static bool displayIDToIndex(unsigned& displayIndex, CGDirectDisplayID displayID);


        /** Constructor.
         */
        DisplayXFBInterface();


        /** Destructor.
         */
        ~DisplayXFBInterface();


        /** Open the driver.
         *
         *  @return     true for success, false for failure. This can fail if either no compatible driver is installed, or
         *              if the driver is already open by another client.
         */
        bool open();


        /** Close the driver.
         */
        void close();


        /** Test if the driver is open.
         */
        bool isOpen() const;


        /** Return the number of available displays.
         */
        unsigned displayCount() const;


        /** Get a current display's configuration.
         *
         *  @param  configuration       Returns the configuration.
         *  @param  displayIndex        The display to query.
         *  @return                     true for success, false for failure.
         */
        bool displayGetConfiguration(DisplayXFBConfiguration& configuration, unsigned displayIndex);


        /** Set a display configuration.
         *
         *  @param  configuration       Specifies the configuration.
         *  @param  displayIndex        The display to query.
         *  @return                     true for success, false for failure.
         *
         *  This method may fail if the configuration requires too much resource for the driver.
         */
        bool displaySetConfiguration(const DisplayXFBConfiguration& configuration, unsigned displayIndex);


        /** Get a display's current state.
         *
         *  @param  state               Returns the state information.
         *  @param  displayIndex        The display number.
         *  @return                     Logical true for success, false for failure.
         */
        bool displayGetState(DisplayXFBState& state, unsigned displayIndex);


        /** Test if a display is connected. This is a shortcut alternative to using the
         *  more capable displayGetState() method.
         */
        bool displayIsConnected(unsigned displayIndex);


        /** Connect a display (make it available).
         *
         *  @param  displayIndex        The display number.
         *  @return                     Logical true for success, false for failure.
         */
        bool displayConnect(unsigned displayIndex);


        /** Disconnect a display.
         *
         *  @param  displayIndex        The display number.
         *  @return                     Logical true for success, false for failure.
         */
        bool displayDisconnect(unsigned displayIndex);



        /** Map the framebuffer memory for a display in to the current task's address space.
         *
         *  @param  map                 Returns the address mapping information.
         *  @param  displayIndex        The display number.
         *  @param  readOnly            Logical true if the mapping should be read-only (recommended).
         *  @return                     Logical true for success, false for failure.
         */
        bool displayMapFramebuffer(DisplayXFBMap& map, unsigned displayIndex, bool readOnly=true);


        /** Map the cursor memory for a display in to the current task's address space.
         *
         *  @param  map                 Returns the address mapping information.
         *  @param  displayIndex        The display number.
         *  @param  readOnly            Logical true if the mapping should be read-only (recommended).
         *  @return                     Logical true for success, false for failure.
         */
        bool displayMapCursor(DisplayXFBMap& map, unsigned displayIndex, bool readOnly=true);


        /** Set the notification callback handler.
         *
         *  @param  handler             The function to call with notifications.
         *  @param  context             The client-specific context to pass on callbacks.
         *  @param  runloop             The runloop for callbacks. If NULL, the current runloop will be used.
         *  @return                     Logical true for success, false for failure.
         *
         *  On completion, notification callbacks will be issued to the specificied function
         *  using the target runloop.
         */
        bool setNotificationHandler(NotificationHandler handler, void* context, CFRunLoopRef runloop=0);


        /** Stop further notification callbacks.
         */
        void clearNotificationHandler();

    private:

        bool m_isOpen;                                                              //!< Logical true if the interface is bound
        io_service_t m_service;                                                     //!< The service handle
        io_connect_t m_connect;                                                     //!< The connection handle
        DisplayXFBInfo m_info;                                                      //!< The driver info structure (from open)
        NotificationHandler m_notificationHandler;                                  //!< The registered notification handler, or zero if none
        void* m_notificationHandlerContext;                                         //!< Client supplied context for notification callbacks
        IONotificationPortRef m_notificationHandlerNotificationPort;                //!< What it says
        io_object_t m_notificationObject;                                           //!< Notification context.
        CFRunLoopRef m_notificationRunloop;                                         //!< The runloop where notifications are posted

        // Methods implementing the RPC. These ultimately map directly to the methods in com_tsoniq_driver_DisplayXFB via the user-client.
        bool userOpen(DisplayXFBInfo* info);
        void userClose();
        bool userDisplayGetConfiguration(DisplayXFBConfiguration* configuration, unsigned displayIndex);
        bool userDisplaySetConfiguration(const DisplayXFBConfiguration* configuration, unsigned displayIndex);
        bool userDisplayGetState(DisplayXFBState* state, unsigned displayIndex);
        bool userDisplayConnect(unsigned displayIndex);
        bool userDisplayDisconnect(unsigned displayIndex);
        bool userMap(unsigned displayIndex, unsigned mapType, bool readOnly, DisplayXFBMap* map);

        // Class methods
        static void interestCallback(void* refcon, io_service_t service, natural_t messageType, void* messageArgument);
    };

}   // namespace

#endif      // COM_TSONIQ_DisplayXFBInterface_H
