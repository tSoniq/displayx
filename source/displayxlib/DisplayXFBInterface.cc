/** @file   DisplayXFBInterface.cc
 *  @brief  User-space client interface object.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 */

#include "DisplayXFBInterface.h"
#include <IOKit/IOKitKeys.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>


#define kDisplayXFBServiceName0(str)    #str
#define kDisplayXFBServiceName1(str)    kDisplayXFBServiceName0(str)
#define kDisplayXFBServiceName          kDisplayXFBServiceName1(DisplayXFBDriver)


namespace ts
{
    DisplayXFBInterface::DisplayXFBInterface()
        :
        m_isOpen(false),
        m_service(0),
        m_connect(0),
        m_info(),
        m_notificationHandler(0),
        m_notificationHandlerContext(0),
        m_notificationHandlerNotificationPort(0),
        m_notificationObject(0),
        m_notificationRunloop(0)
    {
        m_info.invalidate();
    }


    DisplayXFBInterface::~DisplayXFBInterface()
    {
        close();
    }


    bool DisplayXFBInterface::open()
    {
        if (!m_isOpen)
        {
            assert(!m_service);
            assert(!m_connect);

            m_service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kDisplayXFBServiceName));
            if (!m_service) fprintf(stderr, "%s:no virtual display service running\n", __PRETTY_FUNCTION__);
            else
            {
                IOReturn status = IOServiceOpen(m_service, mach_task_self(), 0, &m_connect);
                if (kIOReturnSuccess != status) fprintf(stderr, "%s: status %08x from IOServiceOpen\n", __PRETTY_FUNCTION__, (unsigned)status);
                else m_isOpen = userOpen(&m_info);
            }

            // Cleanup in the event of an error.
            if (!m_isOpen)
            {
                if (m_service) { IOObjectRelease(m_service); m_service = 0;}
                if (m_connect) { IOServiceClose(m_connect); m_connect = 0; }
            }
        }

        return m_isOpen;
    }


    void DisplayXFBInterface::close()
    {
        clearNotificationHandler();
        if (m_isOpen)
        {
            userClose();
            IOServiceClose(m_connect);
            m_connect = 0;
            IOObjectRelease(m_service);
            m_service = 0;
            m_info.invalidate();
            m_isOpen = false;
        }
    }


    bool DisplayXFBInterface::isOpen() const
    {
        return m_isOpen;
    }


    unsigned DisplayXFBInterface::displayCount() const
    {
        if (!isOpen()) return 0;
        else return m_info.displayCount();
    }


    bool DisplayXFBInterface::displayGetConfiguration(DisplayXFBConfiguration& configuration, unsigned displayIndex)
    {
        if (!isOpen() || !userDisplayGetConfiguration(&configuration, displayIndex)) { configuration.invalidate(); return false; }
        else return configuration.isValid();
    }


    bool DisplayXFBInterface::displaySetConfiguration(const DisplayXFBConfiguration& configuration, unsigned displayIndex)
    {
        if (!isOpen() || !userDisplaySetConfiguration(&configuration, displayIndex)) return false;
        else return true;
    }


    bool DisplayXFBInterface::displayGetState(DisplayXFBState& state, unsigned displayIndex)
    {
        if (!isOpen() || !userDisplayGetState(&state, displayIndex)) { state.invalidate(); return false; }
        else return state.isValid();
    }


    bool DisplayXFBInterface::displayIsConnected(unsigned displayIndex)
    {
        DisplayXFBState state;
        bool result = displayGetState(state, displayIndex);
        return result && (state.isConnected());
    }


    bool DisplayXFBInterface::displayConnect(unsigned displayIndex)
    {
        if (!isOpen() || !userDisplayConnect(displayIndex)) return false;
        else return true;
    }


    bool DisplayXFBInterface::displayDisconnect(unsigned displayIndex)
    {
        if (!isOpen() || !userDisplayDisconnect(displayIndex)) return false;
        else return true;
    }


    bool DisplayXFBInterface::displayMapFramebuffer(DisplayXFBMap& map, unsigned displayIndex, bool readOnly)
    {
        if (!isOpen() || !userMap(displayIndex, kDisplayXFBMapTypeDisplay, readOnly, &map)) { map.invalidate(); return false; }
        else return true;
    }


    bool DisplayXFBInterface::displayMapCursor(DisplayXFBMap& map, unsigned displayIndex, bool readOnly)
    {
        if (!isOpen() || !userMap(displayIndex, kDisplayXFBMapTypeCursor, readOnly, &map)) { map.invalidate(); return false; }
        else return true;
    }


    bool DisplayXFBInterface::setNotificationHandler(NotificationHandler handler, void* context, CFRunLoopRef runloop)
    {
        clearNotificationHandler();

        if (!handler) return false;
        if (!isOpen()) return false;

        assert(!m_notificationHandlerNotificationPort);
        assert(!m_notificationRunloop);

        m_notificationHandlerNotificationPort = IONotificationPortCreate(kIOMasterPortDefault);
        if (!m_notificationHandlerNotificationPort) return false;

        if (!runloop) m_notificationRunloop = CFRunLoopGetCurrent();
        else m_notificationRunloop = runloop;

        CFRunLoopAddSource(
            m_notificationRunloop,
            IONotificationPortGetRunLoopSource(m_notificationHandlerNotificationPort),
            kCFRunLoopCommonModes);

        m_notificationHandler = handler;
        m_notificationHandlerContext = context;

        IOReturn status = IOServiceAddInterestNotification(
                m_notificationHandlerNotificationPort,
                m_service,
                kIOGeneralInterest,
                &interestCallback, this,
                &m_notificationObject);

        if (kIOReturnSuccess != status)
        {
            clearNotificationHandler();
            return false;
        }
        else return true;
    }


    void DisplayXFBInterface::clearNotificationHandler()
    {
        // This method is also used to clean up in the event of some failure - don't make it conditional on everything being ok.
        if (m_notificationObject)
        {
            IOObjectRelease(m_notificationObject);
            m_notificationObject = 0;
        }
        if (m_notificationHandlerNotificationPort)
        {
            if (m_notificationRunloop)
            {
                CFRunLoopRemoveSource(
                    m_notificationRunloop,
                    IONotificationPortGetRunLoopSource(m_notificationHandlerNotificationPort),
                    kCFRunLoopCommonModes);
            }

            IONotificationPortDestroy(m_notificationHandlerNotificationPort);

            m_notificationHandlerNotificationPort = 0;
            m_notificationRunloop = 0;
        }

        m_notificationHandler = 0;
        m_notificationHandlerContext = 0;
    }


#pragma     -
#pragma     RPC Methods


    bool DisplayXFBInterface::userOpen(DisplayXFBInfo* info)
    {
        bool result;

        assert(!isOpen());
        assert(m_connect);

        size_t structOutSize = sizeof *info;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorOpen,                            // selector
            NULL,                                               // array of input values
            0,                                                  // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            0,                                                  // number of output values (pass max, return actual)
            info,                                               // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        assert(structOutSize == sizeof *info);

        if (kr != KERN_SUCCESS)
        {
            fprintf(stderr, "%s: error %08x from open\n", __PRETTY_FUNCTION__, (unsigned)kr);
            result = false;
        }
        else if (structOutSize != sizeof *info || info->m_versionMajor != DisplayXFBInfo::kVersionMajor)
        {
            fprintf(stderr, "%s: incompatible version (want %08x, got %08x)\n", __PRETTY_FUNCTION__, (unsigned)info->m_versionMajor, (unsigned)DisplayXFBInfo::kVersionMajor);
            userClose();
            result = false;
        }
        else
        {
            // The open request succeeded and the FB version is compatible with what we are compiled with
            result = true;
        }

        return result;
    }



    void DisplayXFBInterface::userClose()
    {
        //assert(isOpen());     // This may be called from userOpen() before m_isOpen is set, to clean up an incompatible open() operation.
        assert(m_connect);
        IOConnectCallScalarMethod(m_connect, kDisplayXFBSelectorClose, NULL, 0, NULL, NULL);
    }



    bool DisplayXFBInterface::userDisplayGetConfiguration(DisplayXFBConfiguration* configuration, unsigned displayIndex)
    {
        assert(configuration);
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[1] = { displayIndex };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        size_t structOutSize = sizeof *configuration;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorGetConfiguration,                // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            0,                                                  // number of output values (pass max, return actual)
            (void*)configuration,                               // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        assert(structOutSize == sizeof *configuration);

        return (structOutSize == sizeof *configuration) && (KERN_SUCCESS == kr);
    }



    bool DisplayXFBInterface::userDisplaySetConfiguration(const DisplayXFBConfiguration* configuration, unsigned displayIndex)
    {
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[1] = { displayIndex };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        uint32_t scalarOutCount = 0;
        size_t structOutSize = 0;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorSetConfiguration,                // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            (void*)configuration,                               // input structure
            sizeof *configuration,                              // input structure size (pass actual)
            NULL,                                               // array of output values
            &scalarOutCount,                                    // number of output values (pass max, return actual)
            NULL,                                               // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        return (kr == KERN_SUCCESS);
    }


    bool DisplayXFBInterface::userDisplayGetState(DisplayXFBState* state, unsigned displayIndex)
    {
        assert(state);
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[1] = { displayIndex };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        size_t structOutSize = sizeof *state;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorGetState,                        // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            0,                                                  // number of output values (pass max, return actual)
            (void*)state,                                       // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        assert(structOutSize == sizeof *state);

        return (structOutSize == sizeof *state) && (KERN_SUCCESS == kr);
    }


    bool DisplayXFBInterface::userDisplayConnect(unsigned displayIndex)
    {
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[1] = { displayIndex };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        uint32_t scalarOutCount = 0;
        size_t structOutSize = 0;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorConnect,                         // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            &scalarOutCount,                                    // number of output values (pass max, return actual)
            NULL,                                               // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        return (kr == KERN_SUCCESS);
    }


    bool DisplayXFBInterface::userDisplayDisconnect(unsigned displayIndex)
    {
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[1] = { displayIndex };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        uint32_t scalarOutCount = 0;
        size_t structOutSize = 0;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorDisconnect,                      // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            &scalarOutCount,                                    // number of output values (pass max, return actual)
            NULL,                                               // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        return (kr == KERN_SUCCESS);
    }


    bool DisplayXFBInterface::userMap(unsigned displayIndex, unsigned mapType, bool readOnly, DisplayXFBMap* map)
    {
        assert(map);
        assert(isOpen());
        assert(m_connect);

        uint64_t scalarInData[3] = { displayIndex, mapType, (readOnly) ? 1u : 0u };
        uint32_t scalarInCount = (uint32_t) (sizeof scalarInData / sizeof scalarInData[0]);
        uint32_t scalarOutCount = 0;
        size_t structOutSize = sizeof *map;

        kern_return_t kr = IOConnectCallMethod(
            m_connect,                                          // service handle
            kDisplayXFBSelectorMap,                             // selector
            scalarInData,                                       // array of input values
            scalarInCount,                                      // number of input values (pass actual)
            NULL,                                               // input structure
            0,                                                  // input structure size (pass actual)
            NULL,                                               // array of output values
            &scalarOutCount,                                    // number of output values (pass max, return actual)
            map,                                                // output structure
            &structOutSize                                      // output structure size (pass max, return actual)
            );

        assert(structOutSize == sizeof *map);

        return (structOutSize == sizeof *map) && (KERN_SUCCESS == kr);
    }


    /** IOService callback on a notification from the driver.
     */
    void DisplayXFBInterface::interestCallback(void* refcon, io_service_t service, natural_t messageType, void* messageArgument)
    {
        DisplayXFBInterface& interface = *((DisplayXFBInterface*)refcon);
        unsigned arg = (unsigned)((uint64_t)messageArgument);

        if (!interface.m_notificationHandler)
        {
            /* No handler */
        }
        else if (kDisplayXFBNotificationCursorState == messageType)
        {
            interface.m_notificationHandler(kNotificationCursorState, arg, interface.m_notificationHandlerContext);
        }
        else if (kDisplayXFBNotificationCursorImage == messageType)
        {
            interface.m_notificationHandler(kNotificationCursorImage, arg, interface.m_notificationHandlerContext);
        }
        else if (kDisplayXFBNotificationDisplayState == messageType)
        {
            interface.m_notificationHandler(kNotificationDisplayState, arg, interface.m_notificationHandlerContext);
        }
        else
        {
            /* Unknown notification */
        }
    }


#pragma mark    -
#pragma mark    Class Methods


    bool DisplayXFBInterface::isInstalled()
    {
        io_service_t service = IOServiceGetMatchingService(kIOMasterPortDefault, IOServiceMatching(kDisplayXFBServiceName));
        if (service)
        {
            IOObjectRelease(service);
            return true;
        }
        else
        {
            return false;
        }
    }



    bool DisplayXFBInterface::displayIndexToID(CGDirectDisplayID& displayID, unsigned displayIndex)
    {
        // Get the current list of displays.
        CGDirectDisplayID resultIdent = 0;
        bool resultStatus = false;

        uint32_t allocDisplayCount = 0;
        CGError result = CGGetActiveDisplayList(0, NULL, &allocDisplayCount);
        if (result == kCGErrorSuccess && allocDisplayCount != 0)
        {
            uint32_t activeDisplayCount = 0;
            CGDirectDisplayID* activeDisplays = (CGDirectDisplayID*)calloc(sizeof (CGDirectDisplayID), allocDisplayCount);
            if (activeDisplays)
            {
                result = CGGetActiveDisplayList(allocDisplayCount, activeDisplays, &activeDisplayCount);
                if (result == kCGErrorSuccess && activeDisplayCount != 0)
                {
                    // Find the display ID for the current screen.
                    // The display exports EDID data with a custom manufacturer and the displayIndex value as the serial number.
                    for (unsigned i = 0; i != activeDisplayCount; ++i)
                    {
                        uint32_t manufacturer = CGDisplayVendorNumber(activeDisplays[i]);
                        uint32_t serial = CGDisplaySerialNumber(activeDisplays[i]);
                        if (manufacturer == kDisplayXManufacturer && serial == displayIndex)
                        {
                            resultIdent = activeDisplays[i];
                            resultStatus = true;
                            break;
                        }
                    }
                }
                free(activeDisplays);
            }
        }

        if (displayID) displayID = resultIdent;
        return resultStatus;
    }


    bool DisplayXFBInterface::displayIDToIndex(unsigned& displayIndex, CGDirectDisplayID displayID)
    {
        bool resultStatus = false;
        unsigned resultIndex = ~0u;

        if (kDisplayXManufacturer == CGDisplayVendorNumber(displayID))
        {
            resultIndex = CGDisplaySerialNumber(displayID);
            resultStatus = true;
        }

        if (displayIndex) displayIndex = resultIndex;
        return resultStatus;
    }

}   // namespace
