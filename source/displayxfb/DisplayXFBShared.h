/** @file       DisplayXFBShared.h
 *  @brief      Frame buffer shared data definitions (between the kernel-mode user-client and the application library).
 *  @warning    Structures and definitions here must be suitable for sharing between user and kernel
 *              space code and must allow for 64-32 bit issues.
 *
 *  Copyright (c) 2010 tsoniq. All rights reserved.
 *
 *  This file defines the structures shared between the framebuffer and user-land code. The following
 *  types are used:
 *
 *      DisplayXFBInfo          Driver system info (supplied from the driver when first opened)
 *      DisplayXFBMode          Structure describing a display mode (supplied from the user-mode code to the driver)
 *      DisplayXFBConfiguration Structure supplying a list of modes and additional shared data (from user to driver)
 *      DisplayXFBState         Structure describing the current display state (from driver to user)
 *      DisplayXFBCursor        Structure describing the cursor position and image.
 *
 *  In use, the user opens the driver, returning the info structure. The user then creates a configuration specifying
 *  a list of display modes and common parameters such as refresh rate and padding information. This is passed to the
 *  driver in order to connect a virtual display. When the system is running, the user can read both the framebuffer
 *  memory and the state object to describe the current operating mode. The state can be changed asynchronously by the
 *  end user, via the monitors system preference pane.
 */

#ifndef COM_TSONIQ_DisplayXFBShared_H
#define COM_TSONIQ_DisplayXFBShared_H   (1)

#include <stdint.h>
#include <string.h>
#include "DisplayXFBNames.h"

/** Macro used to verify at compile-time that a structure is suitable for kernel-user-mode exchange.
 */
#define TSFBVDS_CHECK_STRUCTURE(SNAME)  extern char temp_struct_size_check_ ## SNAME[(sizeof(SNAME) % 8) == 0 ? 1 : -1]

namespace ts
{
    // The driver version. This defines the format of structures exchanged between user and kernel land.
    // If any structural changes are made, adjust the version number accordingly.
    static const uint32_t kDisplayXFBVersionMajor   = 2;            //! The major version number (change for incompatible changes)
    static const uint32_t kDisplayXFBVersionMinor   = 1;            //! The minor version number (change for bug-fixes or compatible changes)

    // Global limits.
    static const unsigned kDisplayXFBMaxDisplays    = 4;            //! The maximum number of displays that can be used (note that there is also a hard limit of 16 displays due to the use of 16 bit integer bit-masks in some code)
    static const unsigned kDisplayXFBMaxClients     = 8;            //! The maximum number of concurrent user-clients (must be at least 2, for the GA and the client app)
    static const unsigned kDisplayXFBMinWidth       = 320;          //! The minimum display width considered valid
    static const unsigned kDisplayXFBMinHeight      = 200;          //! The minimum display height considered valid
    static const unsigned kDisplayXFBDefaultWidth   = 1280;         //! The default display width
    static const unsigned kDisplayXFBDefaultHeight  = 800;          //! The default display width
    static const unsigned kDisplayXFBMaxWidth       = 8192;         //! The maximum display width considered valid
    static const unsigned kDisplayXFBMaxHeight      = 8192;         //! The maximum display height considered valid
    static const unsigned kDisplayXFBWidthQuantise  = 8;            //! The width must be an integer multiple of this many pixels
    static const unsigned kDisplayXFBMinRefresh1616 = 0x00010000;   //! The maximum refresh period (16.16 fixed point Hz)
    static const unsigned kDisplayXFBMaxRefresh1616 = 0x00800000;   //! The maximum refresh period (16.16 fixed point Hz)

    // Limits on the configured VRAM size, to prevent silly allocations.
    // A 1280x900 display with 32 bit pixels requires ((1280*4+32)*900+128) ~= 4.5MBytes.
    // A 1920x1200 display with 32 bit pixels requires ((1920*4+32)*1200+128) ~= 8.8MBytes.
    // A 2560x1600 display with 32 bit pixels requires ((2560*4+32)*1600+128) ~= 15.7MBytes.
    // Hence we set a minimum buffer size of 8MBytes and an arbitrary maximum of 64Mbytes, chosen
    // to prevent configuration errors either stopping operation or hammering system memory.
    static const unsigned kDisplayXFBMinVRAMSize = 8 * 1024 * 1024;         //! Smallest permitted VRAM size (bytes)
    static const unsigned kDisplayXFBMaxVRAMSize = 64 * 1024 * 1024;        //! Largest permitted VRAM size (bytes)
    static const unsigned kDisplayXFBDefaultVRAMSize = 32 * 1024 * 1024;    //! The default VRAM size (bytes) if no registry key is present


    // EDID data.
    static const uint16_t kDisplayXManufacturer = 0x5233u;  // Manufacturer ID ("TQS" == 20/17/19 = %10100.10001.10011 = 0x5233

    /** Driver information. This is returned via the user-client when a new user-mode connection is established.
     *
     *  @warning    This structure must be a multiple of 64 bits in length (due to kernel-user-space crossing).
     */
    struct DisplayXFBInfo
    {
        static const uint32_t kMagic = 0x78464269;  //! The value for m_magic ("xFBi")

        uint32_t m_magic;               //! The value kMagic
        uint32_t m_versionMajor;        //! The driver major version number (protocol compatibility)
        uint32_t m_versionMinor;        //! The driver minor version number (revision)
        uint32_t m_displayCount;        //! The number of supported displays (display index ranges from 0 to n-1)

        static const uint32_t kVersionMajor = kDisplayXFBVersionMajor;  //! The current major version.
        static const uint32_t kVersionMinor = kDisplayXFBVersionMinor;  //! The current minor version.

        DisplayXFBInfo()
        {
            invalidate();
        }

        void initialise(uint32_t initDisplayCount)
        {
            m_magic = kMagic;
            m_versionMajor = kVersionMajor;
            m_versionMinor = kVersionMinor;
            m_displayCount = initDisplayCount;
        }

        void invalidate()
        {
            m_magic = ~kMagic;
            m_versionMajor = 0;
            m_versionMinor = 0;
            m_displayCount = 0;
        }

        bool isValid() const
        {
            return m_magic == kMagic && kVersionMajor == m_versionMajor;
        }

        unsigned displayCount() const
        {
            return isValid() ? m_displayCount : 0;
        }
    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBInfo);



    /** Display mode description.
     *
     *  @warning    This structure must be a multiple of 64 bits in length (due to kernel-user-space crossing).
     *
     *  An instance of this structure describes a single display mode. It defines the display size and the
     *  format of the data that is encoded in the framebuffer.
     */
    struct DisplayXFBMode
    {
        static const uint32_t kDefaultWidth = 1280;     //!< The default width
        static const uint32_t kDefaultHeight = 900;     //!< The default height

        uint32_t m_width;           //!< The display width for the mode
        uint32_t m_height;          //!< The display height for the mode
        uint32_t m_reserved0;       //!< Reserved for future use
        uint32_t m_reserved1;       //!< Reserved for future use

        DisplayXFBMode() { initialise(); }
        DisplayXFBMode(unsigned w, unsigned h) { initialise(w, h); }

        bool initialise() { return initialise(kDefaultWidth, kDefaultHeight); }

        bool initialise(unsigned w, unsigned h)
        {
            bool wok = setWidth(w);
            bool wol = setHeight(h);
            m_reserved0 = 0;
            m_reserved1 = 0;
            return wok && wol;
        }

        unsigned width() const { return m_width; }                                  //!< Return the image width (pixels)
        unsigned height() const { return m_height; }                                //!< Return the image height (pixels)

        bool setWidth(unsigned w)
        {
            m_width = (uint32_t)(w - (w % kDisplayXFBWidthQuantise));               // Round down to the next lowest round width
            if (m_width < kDisplayXFBMinWidth) m_width = kDisplayXFBMinWidth;       // Truncate at the minimum width
            else if (m_width > kDisplayXFBMaxWidth) m_width = kDisplayXFBMaxWidth;  // Cap at the maximum width
            return m_width == w;                                                    // Return true if the requested value was used unmolested
        }

        bool setHeight(unsigned h)
        {
            m_height = (uint32_t)h;
            if (m_height < kDisplayXFBMinHeight) m_height = kDisplayXFBMinHeight;       // Truncate at the minimum height
            else if (m_height > kDisplayXFBMaxHeight) m_height = kDisplayXFBMaxHeight;  // Cap at the maximum height
            return m_height == h;                                                       // Return true if the requested value was used unmolested
        }

    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBMode);




    /** Display buffer memory format description.
     *
     *  @warning    This structure must be a multiple of 64 bits in length (due to kernel-user-space crossing).
     */
    struct DisplayXFBState
    {
        static const uint32_t kMagic = 0x78464266;          //! The value for m_magic ("xFBf")
        static const uint32_t kFlagConnected = (1u << 0);   //! Flag bit that is set if the display is currently connected

        uint32_t m_magic;                                   //! The value kMagic
        uint32_t m_offset;                                  //! The byte offset to the first pixel in the frame buffer
        uint32_t m_pad;                                     //! The number of padding bytes at the end of each row
        uint32_t m_flags;                                   //! Bit flags (kFlagXxx)
        uint32_t m_modeIndex;                               //! The current mode index number
        uint32_t m_reserved[3];                             //! Reserved for future use
        struct DisplayXFBMode m_mode;                       //! The mode description

        DisplayXFBState()
        {
            invalidate();
        }

        void initialise(const DisplayXFBMode& m, unsigned off, unsigned pd)
        {
            m_magic = kMagic;
            m_offset = off;
            m_pad = pd;
            m_flags = 0;
            for (unsigned i = 0; i < sizeof m_reserved / sizeof m_reserved[0]; i++) m_reserved[i] = 0;
            m_mode = m;
        }

        void invalidate()
        {
            m_magic = ~kMagic;
            m_offset = 0;
            m_pad = 0;
            m_flags = 0;
            for (unsigned i = 0; i < sizeof m_reserved / sizeof m_reserved[0]; i++) m_reserved[i] = 0;
            m_mode.initialise();
        }

        bool isValid() const
        {
            return (kMagic == m_magic);
        }

        DisplayXFBMode& mode() { return m_mode; }               //! Return the display mode
        const DisplayXFBMode& mode() const { return m_mode; }   //! Return the display mode
        unsigned modeIndex() const { return m_modeIndex; }      //! Return the current mode index

        unsigned bytesPerPixel() const                          //! Return the number of bytes in a single pixel
        {
            return 4;   // Only ARGB32 is supported at present
        }

        unsigned width() const { return m_mode.width(); }                               //! Return the image width (pixels)
        unsigned height() const { return m_mode.height(); }                             //! Return the image height (pixels)
        unsigned offset() const { return m_offset; }                                    //! Return the byte offset to the first pixel in the framebuffer
        unsigned pad() const { return m_pad; }                                          //! Return the byte padding at the end of each row
        unsigned bitsPerPixel() const { return bytesPerPixel() * 8; }                   //! Return the number of bits per-pixel
        unsigned bytesPerRow() const  { return pad() + (bytesPerPixel()*width()); }     //! Return the number of bytes in each row (the stride)
        unsigned bytesPerFrame() const { return bytesPerRow()*height(); }               //! Return the total frame buffer data size, excluding offset
        bool isConnected() const { return 0 != (m_flags & kFlagConnected); }            //! Return the current connection state for the display

        void setOffset(unsigned o) { m_offset = (uint32_t)o; }                          //! Set the offset
        void setPad(unsigned p) { m_pad = (uint32_t)p; }                                //! Set the pad
        void setIsConnected(bool con) { m_flags = (con) ? (m_flags | kFlagConnected) : ( m_flags & ~kFlagConnected); }; //! Set the connection state

        /** Set the current mode information.
         */
        void setMode(const DisplayXFBMode& m, unsigned mi)
        {
            m_modeIndex = (uint32_t) mi;
            m_mode = m;
        }

    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBState);


    /** Display configuration. Instances of this structure are passed to the driver to define the supported
     *  video modes, or read from the driver to find current information.
     *
     *  @warning    This structure must be a multiple of 64 bits in length (due to kernel-user-space crossing).
     *
     *  A newly created configuration will have no modes defined and so can not be used directly. To construct a
     *  usable configuration, the client should call appendMode() to define the supported display mode set. If
     *  no default mode is specified to appendMode(), the first mode will be used as the default.
     */
    struct DisplayXFBConfiguration
    {
        static const uint32_t kMagic = 0x78464263;          //! The value for m_magic ("xFBc")
        static const unsigned kMaxModes = 32;               //! The maximum number of display modes that can be configured
        static const unsigned kDefaultRefresh = 0x003c0000; //! The default refresh rate (in 16.16 fixed point Hz)
        static const unsigned kDefaultRowPadding = 0;       //! The default row padding, in bytes. Early OS (10.4) require that this is at least 32 bytes.
        static const unsigned kDefaultFramePadding = 1024;  //! The default frame pading, in bytes

        uint32_t m_magic;                           //! DisplayXFBConfiguration::kMagic
        uint32_t m_defaultModeIndex;                //! The default mode index
        uint32_t m_modeCount;                       //! The number of valid modes
        uint32_t m_refreshRate;                     //! The refresh rate (16.16 fixed point Hz)
        uint32_t m_rowPadding;                      //! The number of bytes of padding to use on each row
        uint32_t m_framePadding;                    //! THe number of bytes of padding to add to each frame
        uint32_t m_reserved0;                       //! Reserved
        uint32_t m_reserved1;                       //! Reserved
        uint8_t m_name[16];                         //! The display name (zero terminated UTF8 string)
        DisplayXFBMode m_modes[kMaxModes];          //! Array of display mode definitions

        DisplayXFBConfiguration() { initialise(); }
        explicit DisplayXFBConfiguration(const char* n) { initialise(n); }

        void initialise(const char* n="")           //! Create a configuration with a single mode at default resolution
        {
            m_magic = kMagic;
            m_defaultModeIndex = 0;
            m_modeCount = 0;
            m_refreshRate = kDefaultRefresh;
            m_rowPadding = kDefaultRowPadding;
            m_framePadding = kDefaultFramePadding;
            m_reserved0 = 0;
            m_reserved1 = 0;
            setName(n);
            for (unsigned i = 0; i < kMaxModes; i++) m_modes[i].initialise();
        }

        void invalidate()
        {
            initialise();
            m_magic = ~kMagic;
        }

        bool isValid() const                                                //! Test if the structure is valid
        {
            return m_magic == kMagic                            &&          // Magic must be good
                   m_refreshRate >= kDisplayXFBMinRefresh1616   &&          // Not too slow
                   m_refreshRate <= kDisplayXFBMaxRefresh1616   &&          // Not too fast
                   m_modeCount <= kMaxModes                     &&          // There is a hard limit to the maximum number of modes
                   m_defaultModeIndex < kMaxModes               &&          // Default mode must be plausible
                   ((m_name[sizeof m_name / sizeof m_name[0] - 1]) == 0);   // Null terminator must be intact in name string
        }

        const char* name() const { return (const char*)&m_name[0]; }
        void setName(const char* initName)
        {
            unsigned i = 0;
            while (i != sizeof m_name / sizeof m_name[0] - 1 && 0 != initName[i]) { m_name[i] = initName[i]; i++; }
            while (i != sizeof m_name / sizeof m_name[0]) { m_name[i] = 0; i++; }
        }

        double refreshRate() const { return m_refreshRate / 65536.0; }      //! Return the refresh rate in Hz
        unsigned refreshRate1616() const { return m_refreshRate; }          //! Return the refresh rate as 16.16 fixed point Hz
        uint32_t refreshPeriodUS() const { return (uint32_t) (65536000000ull / m_refreshRate); }    // Return the refresh period, in us.

        void setRefreshRate(double r)                                       //! Set the refresh rate in Hz
        {
            if (r <= 0) setRefreshRate1616(kDefaultRefresh);
            else setRefreshRate1616((unsigned)(r * 65536.0 + 0.5));
        }

        void setRefreshRate1616(unsigned r)                                 //! Set the refresh rate as 16.16 fixed point Hz
        {
            if (r < kDisplayXFBMinRefresh1616) r = kDisplayXFBMinRefresh1616;
            else if (r > kDisplayXFBMaxRefresh1616) r = kDisplayXFBMaxRefresh1616;
            m_refreshRate = r;
        }

        unsigned rowPadding() const { return m_rowPadding; }                //! Return the row padding, in bytes.
        void setRowPadding(unsigned n) { m_rowPadding = (uint32_t)n; }      //! Set the row padding, in bytes.

        unsigned framePadding() const { return m_framePadding; }            //! Return the row padding, in bytes.
        void setFramePadding(unsigned n) { m_framePadding = (uint32_t)n; }  //! Set the row padding, in bytes.

        unsigned modeCount() const { return isValid() ? m_modeCount : 0; }  //! Return the number of modes, or zero if the structure is invalid

        bool appendMode(unsigned w, unsigned h, bool setAsDefault=false)
        {
            uint32_t index = m_modeCount;
            if (index >= kMaxModes) return false;
            else
            {
                m_modeCount ++;
                if (setAsDefault) m_defaultModeIndex = index;
                return m_modes[index].initialise(w, h);     // Note: this may modify the sizes to accommodate driver restrictions
            }
        }

        bool appendMode(const DisplayXFBMode& m, bool setAsDefault=false)
        {
            return appendMode(m.width(), m.height(), setAsDefault);
        }

        unsigned defaultModeIndex() const { return (m_defaultModeIndex < kMaxModes) ? m_defaultModeIndex : 0; }

        DisplayXFBMode& mode(unsigned index) { return (index < kMaxModes) ? m_modes[index] : m_modes[kMaxModes-1]; }
        const DisplayXFBMode& mode(unsigned index) const { return (index < kMaxModes) ? m_modes[index] : m_modes[kMaxModes-1]; }

        DisplayXFBMode& defaultMode() { return m_modes[defaultModeIndex()]; }
        const DisplayXFBMode& defaultMode() const { return m_modes[defaultModeIndex()]; }

        /** Given a mode index, create a display header to provide the format and size information.
         *
         *  @param  state       Returns the display state information for the specified mode.
         *  @param  modeIndex   The mode index.
         *  @param  offset      The byte offset to the first pixel. Default zero.
         *  @return             Logical true for success.
         */
        bool makeState(DisplayXFBState& state, unsigned modeIndex, unsigned offset=0) const
        {
            if (modeIndex < modeCount()) state.initialise(mode(modeIndex), offset, rowPadding());
            else state.invalidate();
            return state.isValid();
        }

    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBConfiguration);




    /** Memory map information. This is returned via the user-client in response to MapDisplay or MapUpdateBuffer messages.
     *
     *  @warning    This structure must be a multiple of 64 bits in length (due to kernel-user-space crossing).
     *  @note       A uint64_t is used to return the address mapping to avoid SDK & build dependent structure sizes.
     */
    struct DisplayXFBMap
    {
        static const uint32_t kMagic = 0x78464261;  //! The value for m_magic ("xFBa")

        uint32_t m_magic;               //! The value kMagic
        uint32_t m_reserved0;           //! Reserved
        uint64_t m_address;             //! The address of the data in the task's virtual address space, or zero if invalid.
        uint64_t m_size;                //! The size of the mapping (bytes), or zero if invalid.

        DisplayXFBMap()
        {
            invalidate();
        }

        void initialise(uint64_t initAddress, uint64_t initSize)
        {
            m_magic = kMagic;
            m_reserved0 = 0;
            m_address = initAddress;
            m_size = initSize;
        }

        void invalidate()
        {
            m_magic = ~kMagic;
            m_reserved0 = 0;
            m_address = 0;
            m_size = 0;
        }

        bool isValid() const { return m_address != 0; }
        uint64_t address() const { return m_address; }
        uint64_t size() const { return m_size; }

    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBMap);


    /** The cursor state. This is memory mapped as a read-only structure to a client.
     *
     *  REVIEW: break this in to two structures - a cursor position and a cursor image object. These
     *  should be read via user-client calls rather than memory mapping, to avoid asynchronous
     *  changes.
     */
    struct DisplayXFBCursor
    {
        static const uint32_t kMagic        =   0x43434246; //! Magic number ('FBCC' in little-endian form)
        static const uint32_t kMaxWidth     =   128;        //! The maximum supported cursor width
        static const uint32_t kMaxHeight    =   128;        //! The maximum supported cursor height

        uint32_t m_magic;                               //! The magic number DisplayXFBCursor::kMagic
        uint32_t m_isValid;                             //! Non-zero if the pixel data is valid
        uint32_t m_isVisible;                           //! Non-zero if the cursor is visible
        int32_t m_x;                                    //! The cursor position (note: signed - negative values are legal)
        int32_t m_y;                                    //! The cursor position (note: signed - negative values are legal)
        uint32_t m_width;                               //! The cursor width
        uint32_t m_height;                              //! The cursor height
        int32_t m_hotspotX;                             //! The cursor hostspot position
        int32_t m_hotspotY;                             //! The cursor hostspot position
        uint32_t m_sequenceState;                       //! Counter incremented on state updates
        uint32_t m_sequencePixel;                       //! Counter incremented on pixel data updates
        uint32_t m_reserved[5];                         //! Reserved for future use
        uint32_t m_pixelData[kMaxWidth * kMaxHeight];   //! The RGBA32 pixel data

        void initialise()
        {
            m_magic = kMagic;
            m_isValid = 0;
            m_isVisible = 0;
            m_x = 0;
            m_y = 0;
            m_width = 0;
            m_height = 0;
            m_hotspotX = 0;
            m_hotspotY = 0;
            m_sequenceState = 0;
            m_sequencePixel = 0;
            bzero(m_reserved, sizeof m_reserved);
            bzero(m_pixelData, sizeof m_pixelData);
        }

        bool isValid() const { return m_magic == kMagic; }
        bool isVisible() const { return 0 != m_isVisible; }
        int x() const { return m_x; }
        int y() const { return m_y; }
        unsigned width() const { return m_width; }
        unsigned height() const { return m_height; }
        int hotX() const { return m_hotspotX; }
        int hotY() const { return m_hotspotY; }
        const uint32_t* pixelData() const { return &m_pixelData[0]; }
    };
    TSFBVDS_CHECK_STRUCTURE(DisplayXFBCursor);



    // Definitions for various property keys used by the driver classes.
    // Unless otherwise stated, the values are stored on both the com_tsoniq_driver_DisplayXFBDriver and com_tsoniq_driver_DisplayXFBFramebuffer
    // instances.
    #define kDisplayXFBKeyDisplayCount  "DisplayXFB_DisplayCount"   //! Property key for the number of displays (com_tsoniq_driver_DisplayXFBDriver only)
    #define kDisplayXFBKeyDisplayName   "DisplayXFB_DisplayName"    //! Property key for the display name
    #define kDisplayXFBKeyVRAMSize      "DisplayXFB_VRAMSize"       //! Property key for the video memory size (bytes)
    #define kDisplayXFBKeyDisplayIndex  "DisplayXFB_DisplayIndex"   //! Property key for the displayIndex


    // Notification codes (used to implement asynchronous notifications to a client).
    #define kDisplayXFBNotificationDisplayState     iokit_vendor_specific_msg(0x01)     //! Message sent on a display state change
    #define kDisplayXFBNotificationCursorState      iokit_vendor_specific_msg(0x02)     //! Message sent on a cursor change
    #define kDisplayXFBNotificationCursorImage      iokit_vendor_specific_msg(0x03)     //! Message sent on a cursor change


    /** Type codes for memory mapping. A single API call is used to establish shared memory mappings, indexed
     *  by the display number and an integer that specifies what is being mapped.
     */
    #define kDisplayXFBMapTypeDisplay           (0)     //! Mapping is for the display VRAM
    #define kDisplayXFBMapTypeCursor            (1)     //! Mapping is for the mouse cursor
    #define kDisplayXFBMaxMapTypes              (2)     //! The highest permitted map type


    /** User client method dispatch selectors.
     *
     *  @warning    These *must* match the displach table ordering in DisplayXFBUserClient
     */
    enum
    {
        kDisplayXFBSelectorOpen                     =   0,      //! Open a new user-client session
        kDisplayXFBSelectorClose                    =   1,      //! Terminate a user-client session
        kDisplayXFBSelectorGetState                 =   2,      //! Get the current configuration for a display
        kDisplayXFBSelectorGetConfiguration         =   3,      //! Get the current configuration for a display
        kDisplayXFBSelectorSetConfiguration         =   4,      //! Set the current configuration for a display
        kDisplayXFBSelectorConnect                  =   5,      //! Connect a display
        kDisplayXFBSelectorDisconnect               =   6,      //! Disconnect a display
        kDisplayXFBSelectorMap                      =   7,      //! Map shared memory in to application memory space
        kDisplayXFBNumberSelectors                  =   8
    };

}   // namespace

#endif      // COM_TSONIQ_DisplayXFBShared_H
