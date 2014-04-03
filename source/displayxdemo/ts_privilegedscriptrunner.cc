/** @file
 *  @brief      Class used to run a script with root privileges.
 *  @copyright  Copyright (c) 2012 tSoniq. All rights reserved.
 */

#include <assert.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include "ts_privilegedscriptrunner.h"


#define kPrivilegedScriptRunnerProgramPath CFSTR("/bin/sh")                //!< The program used to run the script
#define kPrivilegedScriptRunnerDefaultJobPrefixId CFSTR("com.tsoniq.id")   //!< Used if the bundle ID can not be accessed

namespace ts
{
    PrivilegedScriptRunner::PrivilegedScriptRunner(const char* script, const char* ident)
        :
        m_scriptName(0),
        m_scriptLabel(0),
        m_stdoutPath(0),
        m_stderrPath(0),
        m_auth(),
        m_haveAuth(false),
        m_timeoutSecs(kDefaultTimeout),
        m_scriptIsActive(false),
        m_scriptPid(0)
    {
        // Construct a label for the job.
        CFMutableStringRef label = 0;
        CFUUIDRef uuid = 0;
        CFStringRef postfix = 0;

        do
        {
            // Initialise the script name.
            m_scriptName = CFStringCreateWithCString(kCFAllocatorDefault, script, kCFStringEncodingUTF8);
            if (!m_scriptName) break;

            // Create the job label.
            label = CFStringCreateMutable(kCFAllocatorDefault, 0);
            if (!label) break;

            // Get the job prefix, applying a default if the bundle ID is not available.
            CFStringRef prefix = 0;
            CFBundleRef bundle = CFBundleGetMainBundle();
            if (bundle) prefix = CFBundleGetIdentifier(bundle);
            if (!prefix) prefix = kPrivilegedScriptRunnerDefaultJobPrefixId;

            // Create the postfix.
            if (ident)
            {
                postfix = CFStringCreateWithCString(kCFAllocatorDefault, ident, kCFStringEncodingUTF8);
                if (!postfix) break;
            }
            else
            {
                uuid = CFUUIDCreate(kCFAllocatorDefault);
                if (!uuid) break;
                postfix = CFUUIDCreateString(kCFAllocatorDefault, uuid);
                if (!postfix) break;
            }

            CFStringAppend(label, prefix);
            CFStringAppend(label, CFSTR(".script."));
            CFStringAppend(label, postfix);
            m_scriptLabel = label;
            CFRetain(m_scriptLabel);

        } while (false);

        if (postfix) CFRelease(postfix);
        if (uuid) CFRelease(uuid);
        if (label) CFRelease(label);

        if (!m_scriptName || !m_scriptLabel) throw std::bad_alloc();
    }


    PrivilegedScriptRunner::~PrivilegedScriptRunner()
    {
        deauthorise();

        if (m_scriptName)  { CFRelease(m_scriptName);  m_scriptName  = 0; }
        if (m_scriptLabel) { CFRelease(m_scriptLabel); m_scriptLabel = 0; }
        if (m_stdoutPath)  { CFRelease(m_stdoutPath);  m_stdoutPath  = 0; }
        if (m_stderrPath)  { CFRelease(m_stderrPath);  m_stderrPath  = 0; }
    }


    void PrivilegedScriptRunner::setTimeout(int tmsecs)
    {
        if (tmsecs < 0) tmsecs = 0;
        m_timeoutSecs = tmsecs;
    }


    void PrivilegedScriptRunner::setRedirect(const char* stdoutPath, const char* stderrPath)
    {
        if (m_stdoutPath)  { CFRelease(m_stdoutPath);  m_stdoutPath  = 0; }
        if (m_stderrPath)  { CFRelease(m_stderrPath);  m_stderrPath  = 0; }

        if (stdoutPath) m_stdoutPath =  CFStringCreateWithCString(kCFAllocatorDefault, stdoutPath, kCFStringEncodingUTF8);
        if (stderrPath) m_stderrPath =  CFStringCreateWithCString(kCFAllocatorDefault, stdoutPath, kCFStringEncodingUTF8);
    }


    bool PrivilegedScriptRunner::authorise()
    {
        deauthorise();

        assert(!isAuthorised());

        AuthorizationItem authItem = { kSMRightModifySystemDaemons, 0, NULL, 0 };
        AuthorizationRights authRights = { 1, &authItem };
        AuthorizationFlags flags = kAuthorizationFlagInteractionAllowed | kAuthorizationFlagPreAuthorize | kAuthorizationFlagExtendRights;

        if (errAuthorizationSuccess == AuthorizationCreate(&authRights, kAuthorizationEmptyEnvironment, flags, &m_auth))
        {
            m_haveAuth = true;
            assert(isAuthorised());
        }

        return m_haveAuth;
    }


    void PrivilegedScriptRunner::deauthorise()
    {
        if (m_haveAuth)
        {
            AuthorizationFree(m_auth, 0);
            m_haveAuth = false;
        }
    }


    bool PrivilegedScriptRunner::isRunning()
    {
        bool result = false;
        if (m_scriptIsActive)
        {
            assert(0 != m_scriptPid);
            int n = kill(m_scriptPid, 0);
            result = (0 == n || EPERM == errno);
        }
        return result;
    }


    void PrivilegedScriptRunner::wait()
    {
        while (isRunning())
        {
            usleep(50000);
        }
    }






#pragma mark    -
#pragma mark    Private Methods


    /** Start a script running.
     *
     *  See start() for a description of the arguments.
     */
    bool PrivilegedScriptRunner::startScript(const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5, const char* arg6, const char* arg7, const char* arg8)
    {
        bool result = false;
        CFStringRef cfScriptPath = 0;
        CFMutableArrayRef cfProgramArguments = 0;
        CFNumberRef cfExitTimeoutNumber = 0;
        CFMutableDictionaryRef plist = 0;
        CFErrorRef error = 0;

        do
        {
            if (!isAuthorised()) break;

            stopScript();       // terminate/cleanup previously running script

            cfScriptPath = urlToAbsPath(CFBundleCopyResourceURL(CFBundleGetMainBundle(), m_scriptName, NULL, NULL));
            if (!cfScriptPath) break;

            cfExitTimeoutNumber = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &m_timeoutSecs);
            if (!cfExitTimeoutNumber) break;

            cfProgramArguments = CFArrayCreateMutable(kCFAllocatorDefault, 4, &kCFTypeArrayCallBacks);
            if (!cfProgramArguments) break;

            CFArrayAppendValue(cfProgramArguments, kPrivilegedScriptRunnerProgramPath); // argv[0] is the shell program name
            CFArrayAppendValue(cfProgramArguments, cfScriptPath);                       // argv[1] is the script name

            // The remaing arguments are passed by the caller to this method (and massaged a bit).
            const char* argArray[8] = { arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8 };
            for (unsigned i = 0; i != sizeof argArray / sizeof argArray[0]; ++i)
            {
                const char* arg = argArray[i];
                CFStringRef cfArg = 0;

                if (!arg) continue;

                // We have an argument. Try to make sense of it. If we can't make sense of it, we
                // just pass an empty string as the argument (eg if undefined variables are used).
                if (0 == strncmp("#$=", arg, 3))
                {
                    // getenv substitution handling
                    const char* env = getenv(&arg[3]);
                    if (env) cfArg = CFStringCreateWithCString(kCFAllocatorDefault, env, kCFStringEncodingUTF8);
                }
                else if (0 == strncmp("#$?", arg, 3))
                {
                    // info.plist substitution handling
                    CFStringRef cfKey = CFStringCreateWithCString(kCFAllocatorDefault, &arg[3], kCFStringEncodingUTF8);
                    if (cfKey)
                    {
                        CFTypeRef object = CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), cfKey);
                        if (object && CFStringGetTypeID() == CFGetTypeID(object))
                        {
                            cfArg = CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)object);
                        }
                        else if (object && CFBooleanGetTypeID() == CFGetTypeID(object))
                        {
                            cfArg = CFBooleanGetValue((CFBooleanRef)object) ? CFSTR("1") : CFSTR("0");
                        }
                        else if (object && CFNumberGetTypeID() == CFGetTypeID(object))
                        {
                            CFNumberRef number = (CFNumberRef)object;
                            if (CFNumberIsFloatType(number))
                            {
                                double n = 0;
                                CFNumberGetValue(number, kCFNumberDoubleType, &n);
                                cfArg = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%.15f"), n);
                            }
                            else
                            {
                                SInt64 n = 0;
                                CFNumberGetValue(number, kCFNumberSInt64Type, &n);
                                cfArg = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%lld"), (long long)n);
                            }
                        }
                        else
                        {
                            // Other types are not be converted at present.
                        }
                        CFRelease(cfKey);
                    }
                }
                else if (0 == strncmp("#$", arg, 2))
                {
                    // Non-generic substitution handling
                    if (0 == strcmp(&arg[2], "APPLICATION_PATH"))
                    {
                        cfArg = urlToAbsPath(CFBundleCopyBundleURL(CFBundleGetMainBundle()));
                    }
                    else if (0 == strcmp(&arg[2], "RESOURCE_PATH"))
                    {
                        cfArg = urlToAbsPath(CFBundleCopyResourcesDirectoryURL(CFBundleGetMainBundle()));
                    }
                }
                else
                {
                    // Literals
                    cfArg = CFStringCreateWithCString(kCFAllocatorDefault, arg, kCFStringEncodingUTF8);
                }

                if (!cfArg) cfArg = CFSTR("");

                CFArrayAppendValue(cfProgramArguments, cfArg);   // append the new argument
                CFRelease(cfArg);
            }


            // Create the job definition.
            plist = CFDictionaryCreateMutable(kCFAllocatorDefault, 9, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
            if (!plist) break;
            CFDictionaryAddValue(plist, CFSTR("Label"), m_scriptLabel);
            CFDictionaryAddValue(plist, CFSTR("RunAtLoad"), kCFBooleanTrue);
            CFDictionaryAddValue(plist, CFSTR("KeepAlive"), kCFBooleanFalse);
            CFDictionaryAddValue(plist, CFSTR("EnableTransactions"), kCFBooleanFalse);
            CFDictionaryAddValue(plist, CFSTR("ExitTimeout"), cfExitTimeoutNumber);
            CFDictionaryAddValue(plist, CFSTR("Program"), kPrivilegedScriptRunnerProgramPath);
            CFDictionaryAddValue(plist, CFSTR("ProgramArguments"), cfProgramArguments);
            if (m_stdoutPath) CFDictionaryAddValue(plist, CFSTR("StandardOutPath"), m_stdoutPath);
            if (m_stderrPath) CFDictionaryAddValue(plist, CFSTR("StandardErrorPath"), m_stderrPath);
#if DEBUG
            CFDictionaryAddValue(plist, CFSTR("Debug"), kCFBooleanTrue);
#else
            CFDictionaryAddValue(plist, CFSTR("Debug"), kCFBooleanFalse);
#endif


            // Submit the job.
            if (!SMJobSubmit(kSMDomainSystemLaunchd, (CFDictionaryRef)plist, m_auth, &error))
            {
                // Error.
                break;
            }

            // Script is now running. Get the PID. Currently, we use a key from the job dictionary,
            // but if Apple decides to remove this we could always write a wrapper script to export the
            // PID via a file or similar mechanism.
            m_scriptIsActive = true;
            usleep(10000);  // paranoid short sleep to give SMJobSubmit() a chance to start. This *should* be unnecessary.
            pid_t pid = 0;
            CFDictionaryRef dict = SMJobCopyDictionary(kSMDomainSystemLaunchd, m_scriptLabel);
            if (dict)
            {
                CFNumberRef pidNumber = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("PID"));
                SInt64 i64pid = 0;
                if (pidNumber)
                {
                    if (CFNumberGetValue(pidNumber, kCFNumberSInt64Type, &i64pid))
                    {
                        pid = (pid_t) i64pid;
                    }
                }
                CFRelease(dict);
            }

            m_scriptPid = pid;                  // set the PID, or zero if not known (this means that the job already completed)
            if (!m_scriptPid) stopScript();     // if we already finished, take the chance to clean up
            result = true;

        } while (false);


        // Clean up.
        if (error) CFRelease(error);
        if (plist) CFRelease(plist);
        if (cfExitTimeoutNumber) CFRelease(cfExitTimeoutNumber);
        if (cfProgramArguments) CFRelease(cfProgramArguments);
        if (cfScriptPath) CFRelease(cfScriptPath);

        return result;
    }


    /** Cleanup after running a script. If necessary, the script will be terminated.
     */
    void PrivilegedScriptRunner::stopScript()
    {
        if (m_scriptIsActive)
        {
            assert(m_scriptLabel);
            SMJobRemove(kSMDomainSystemLaunchd, m_scriptLabel, m_auth, false, NULL);
            m_scriptPid = 0;
            m_scriptIsActive = false;
        }
    }



    /** Read a line from a file, with timeout.
     *
     *  @param  line            Returns the resulting string.
     *  @param  fd              The file descriptor.
     *  @param  timeoutSeconds  The timeout.
     *  @return                 True for success, false if a timeout or IO error occurred.
     *
     *  This method waits for up to timeoutSeconds to try to read a single text line (terminated
     *  via a newline character) from the specified file descriptor. Any data after the line
     *  is ignored.
     */
    bool PrivilegedScriptRunner::readLine(std::string& line, int fd, unsigned timeoutSeconds)
    {
        // Set up the kernel queue to monitor the file (better than polling).
        int kq = kqueue();
        struct kevent kev;
        memset(&kev, 0, sizeof kev);
        kev.ident = fd;
        kev.flags = EV_ADD | EV_CLEAR;
        kev.filter = EVFILT_READ;
        kev.fflags = 0;
        kev.data = 0;
        kev.udata = 0;
        int kr = kevent(kq, &kev, 1, 0, 0, 0);
        assert(-1 != kr);
        if (-1 == kr) return false;

        // Initialise the timeout parameters.
        time_t timeNow = 0;
        time(&timeNow);
        const time_t timeoutEnd = timeNow + timeoutSeconds + 1;

        // Loop waiting to receive a complete line.
        bool result = false;
        line.clear();
        while (!result)
        {
            time(&timeNow);
            if (timeNow > timeoutEnd) break;        // timeout expired
            long timeoutSecondsRemaining = (long)(timeoutEnd - timeNow);
            struct timespec keventTimeout = { timeoutSecondsRemaining, 1000000 * 500 };

            struct kevent keventResults[1];
            int kr = kevent(kq, 0, 0, keventResults, sizeof (keventResults) / sizeof (keventResults[0]), &keventTimeout);
            if (1 == kr)
            {
                // Something is available to read. We are expecting a 1-line result, terminated with a newline character or EOF.
                char ch = 0;
                while (1 == read(fd, &ch, 1))
                {
                    if ('\n' == ch || 0 == ch) { result = true; break; }
                    else { line += ch; };
                }
            }
            else if (0 == kr) break;        // timeout
            else break;                     // some other error
        }

        return result;
    }


    /** Given a URL, return an absolute string reference to the path.
     *
     *  @param  url     The URL reference. May be nil. A reference to the URL is consumed.
     *  @return         A string giving the absolute file system path, or zero if no possible resolution.
     *
     *  The supplied URL is CFRelease()d. The caller must CFRelease() the returned string if non-nil.
     */
    CFStringRef PrivilegedScriptRunner::urlToAbsPath(CFURLRef url)
    {
        CFStringRef result = 0;
        if (url)
        {
            CFURLRef absUrl = CFURLCopyAbsoluteURL(url);
            if (absUrl)
            {
                result = CFURLCopyFileSystemPath(absUrl, kCFURLPOSIXPathStyle);
                CFRelease(absUrl);
            }
            CFRelease(url);
        }
        return result;
    }

}   // namespace
