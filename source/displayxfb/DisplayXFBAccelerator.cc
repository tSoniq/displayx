/** @file   DisplayXFBAccelerator.cc
 *  @brief  IOAccelerator implementation.
 *
 *  Copyright (c) 2011 tSoniq. All rights reserved.
 */

#include "DisplayXFBAccelerator.h"


#pragma mark    -
#pragma mark    Miscellaneous Support


// Console logging macro.
#if 0
#define TSLog(fmt, args...) do { IOLog("AC%p: %s: " fmt "\n", this, __PRETTY_FUNCTION__, ## args); } while (false)
#define TSTrace()           do { IOLog("AC%p: %s\n", this, __PRETTY_FUNCTION__); }while (false)
#else
#define TSLog(args...)      do { } while (false)
#define TSTrace()           do { } while (false)
#endif




#pragma mark    -
#pragma mark    DisplayXFBAccelerator  (IOService methods)




#define super IOAccelerator
OSDefineMetaClassAndStructors(DisplayXFBAccelerator, IOAccelerator);



/** Object initialisation.
 */
bool DisplayXFBAccelerator::init(OSDictionary* dictionary)
{
    TSTrace();

    // Perform superclass initialisation.
    if (!super::init(dictionary)) return false;

    // Do local initialisation here.
    return true;
}


/** Object release.
 */
void DisplayXFBAccelerator::free()
{
    TSTrace();

    // Do local teardown here.
    super::free();
}



/** Start the driver.
 */
bool DisplayXFBAccelerator::start(IOService* provider)
{
    TSTrace();

    // Initialise the provider.
    if (!super::start(provider)) return false;

    // Handle local start here.
    int pathLen = sizeof m_types - 1;
    if (!getPath(m_types, &pathLen, gIOServicePlane))
    {
        m_types[0] = 0;
    }
    else
    {
        if (pathLen < 0) pathLen = 0;
        else if ((size_t)pathLen >= sizeof m_types) pathLen = sizeof m_types - 1;
        m_types[pathLen] = 0;
    }

    /* Set up the registry keys. These are only vaguely documented by Apple (ie: only their existence is noted
     * and there is little or no information about usage).
     *
     * Registry keys for MacBook with GMA950 10.6.8:
     *
     *      KEY                 SYSTEM                  VALUE
     *      AccelCaps:          MB 10.6.8 (GMA950)      0x03    QGL|MIPMAP
     *                          MBP 10.7.2 (9400,9600)  0x0b    QEX|QGL|MIPMAP
     *
     *      IOAccelRevision:    <all>                   0x02
     *
     *      IODVDBundleName:    MB 10.6.8 (GMA950)      AppleIntelGMA950VADriver
     *                          MBP 10.7.2 (9400,9600)  GeForceVADriver
     *
     *      IOGLBundleName:     MB 10.6.8 (GMA950)      eAppleIntelGMA950GLDriver
     *                          MBP 10.7.2 (9400,9600)  GeForceGLDriver
     *
     *      IOVABundleName:     MB 10.6.8 (GMA950)      (not present)
     *                          MBP 10.7.2 (9400,9600)  GeForceVADriver (this is sometimes different from IODVDBundleName)
     *
     *      IOVARendererID:     MB 10.6.8 (GMA950)      (not present)
     *                          MBP 10.7.2 (9400,9600)  { 0x01040002, 0x01040004 } on each accelerator for { 9600MGT, 9400M } respectively
     *
     * Some drivers (GeForce variants) set IOVABundleName to "AppleVADriver" (seemingly a generic driver) and
     * IOGLBundleName to "Unknown". There was apparently a generic IODVDBundleName "AppleAltiVecDVDDriver", but this is obviously
     * not present in 10.7 and appears to have no direct Intel equivalent.
     *
     * Other keys that may be needed:
     *
     *      IOClass:            <all>                   the driver class name
     *
     * For information on these, see the IOKIt/graphics source code release.
     *
     */

    // The follow properties are required on all OS tested.
    setProperty("IOClass", DisplayXFBAcceleratorClassName);
    setProperty("AccelCaps", (unsigned long long)getAccelCaps(), 32);
    setProperty("IOAccelRevision", (unsigned long long)getAccelRevision(), 32);

    // The following properties empirically fix a DVD player crash bug on 10.7.0 to 10.7.2 (this
    // appears to be a bug in the Apple GPU drivers, unrelated to this driver - the system gets
    // confused about which accelerators are attached to which displays).
    setProperty("IOGLBundleName", "");
    setProperty("IOVABundleName", "AppleVADriver");
    setProperty("IODVDBundleName", "AppleVADriver");
    setProperty("IOVARendererID", (unsigned long long)0xa5020001, 32);

    return true;
}


/** Stop the driver.
 */
void DisplayXFBAccelerator::stop(IOService* provider)
{
    TSTrace();

    // Handle local stop here.

    super::stop(provider);
}

