DisplayX Virtual Display Software
=================================


Description
-----------

DisplayX is a virtual display driver for MacOSX, offering the following capabilities:

    - dynamic display configuration and control from the application level
    - high performance data capture via Quartz display streaming
    - supports MacOSX 10.6 or later
    - integrated application installer/uninstaller
    - supports multiple display units (via build option)

The sample application provides a window that shows the content of a virtual display in real-time. To test,
run the application and click 'Connect' to connect the virtual display and shows its contents. Once connected,
the display resolution and arrangement/mirroring can be configured via System Preferences exactly as for a
conventional 2nd physical display.

Please note that by default Quartz will select the highest permitted resolution when the display is connected.
If this is too high to read successfully (for example, when trying to use System Preferences to set the display
resolution), the resolution can be reset to a default level via the 'Reset Resolution' display command.

The c++ class DisplayXFBInterface provides the application interface to the display driver. Please see the
doxygen markup for a description of the available methods. Driver configuration and state structures are defined
in DisplayXFBShared.h.

For more information contact support@tsoniq.com or visit http://tsoniq.com.




Licence
-------

DisplayX may be used freely for research and academic work.

However, the DisplayX software remains the Copyright of tSoniq. DisplayX may not be used in a commercial
application without prior approval. For more information contact support@tsoniq.com.





Building and Code-signing
-------------------------

The supplied project requires Xcode 5.1 and can be used on MacOSX 10.8 or later.

The "DXDemo" target in the project will build both the kernel extension and the demonstration application.

For earlier OS support the Quartz DisplayStream API used by the demonstration application should be replaced with
alternative code that uses direct access to the framebuffer memory. The driver and installation process support
MacOSX 10.6 or later.

The kernel extension can be code-signed by entering the SHA1 hash for the appropriate certificate
in DisplayX.xcconfig. Without an appropriate certificate the extension will cause security warnings on
installation and any system reboot. The application code-signing can be made in the standard way.

The virtual display count and per-display VRAM can also be configured in DisplayX.xcconfig.




About tSoniq
------------

tSoniq is a Barcelona based software consultancy specialising in MacOS and iOS application and driver development.

For more information on what we offer, please visit http://tsoniq.com.
