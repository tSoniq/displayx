/** @file
 *  @brief      Class used to run a script with root privileges.
 *  @copyright  Copyright (c) 2012 tSoniq. All rights reserved.
 */

#ifndef COM_TSONIQ_ts_drivershieldinstaller_H
#define COM_TSONIQ_ts_drivershieldinstaller_H   (1)

#include <ServiceManagement/ServiceManagement.h>
#include <Security/Authorization.h>
#include <string>

namespace ts
{
    /** Class used to manage driver installation and uninstallation.
     *
     *  This class requires that the script kPrivilegedScriptRunnerScriptName exist in the current program's resources folder
     *  and is an executable 'sh' script.
     *
     *  The script is run with the following arguments:
     *
     *      $0      the script name (as passed to the constructor)
     *      $1      a 'command' string (as passed to start())
     *      $2      a fully qualified path to the application launching the script
     *      $3      a fully qualified path to a resource directory (usually, but not necessarily, the directory containing the script)
     *
     *  The script should perform the requested action and then append a single line completion status to the specified file.
     *  The completion line must be terminated by a newline and must be "success" for successful completion. Any other string
     *  denotes a failure.
     *
     *  If the script needs to locate other files (eg to install), it should do so relative to the script path.
     *
     *  Be aware that the script may be terminated early via kill() if the client supplied timeout to install() or remove()
     *  expires.
     */
    class PrivilegedScriptRunner
    {
    public:

        static const int kDefaultTimeout = 20;          //!< The default timeout for scripts (seconds).


        /** Constructor.
         *
         *  @param  script  The name of the script to run.
         *  @param  ident   An identifier for the job. This will be appended to the application CFBundleID
         *                  to create the job identifier. If zero, a UUID is used. Default zero.
         *
         *  The script name must contain any file extension but must not contain any path specifiers. The script must reside in
         *  in the application's Resources directory directly and must be executable by root.
         *
         *  You can use an explicit identifier if you are sure that there will only be once instance of
         *  the script running concurrently. One advantage of this is that any left-over resources in the
         *  avent of an application crash during execution are implicitly cleaned up the next time that
         *  it is run.
         */
        explicit PrivilegedScriptRunner(const char* script, const char* ident=0);


        ~PrivilegedScriptRunner();                             //!< Destructor.

        bool isAuthorised() const { return m_haveAuth; }                //!< Test if the installer is authorised


        /** Set the script timeout.
         *
         *  @param  tmsecs  The timeout, in seconds. Pass zero or negative for none. Default kDefaultTimeout.
         *
         *  If the script has not completed by after @e timeout seconds from starting, the OS will
         *  automatically kill it.
         */
        void setTimeout(int tmsecs);


        /** Configure IO redirection.
         *
         *  @param  stdoutPath  The file path for stdout, or zero for none. Default zero.
         *  @param  stderrPath  The file path for stderr, or zero for none. Default zero.
         *
         *  If the script has not completed by after @e timeout seconds from starting, the OS will
         *  automatically kill it.
         */
        void setRedirect(const char* stdoutPath=0, const char* stderrPath=0);


        /** Authorise the client.
         *
         *  @return     Logical true for success, false for failure.
         *
         *  This method must be called prior to performing any other operation. It may prompt the
         *  user for a password.
         */
        bool authorise();


        /** Deathorise the client.
         */
        void deauthorise();


        /** Run the script.
         *
         *  @param  arg1            String passed as $1 to the script, or zero for none. Default zero.
         *  @param  arg2            String passed as $2 to the script, or zero for none. Default zero.
         *  @param  arg3            String passed as $3 to the script, or zero for none. Default zero.
         *  @param  arg4            String passed as $4 to the script, or zero for none. Default zero.
         *  @param  arg5            String passed as $5 to the script, or zero for none. Default zero.
         *  @param  arg6            String passed as $6 to the script, or zero for none. Default zero.
         *  @param  arg7            String passed as $7 to the script, or zero for none. Default zero.
         *  @param  arg8            String passed as $8 to the script, or zero for none. Default zero.
         *  @return                 Logical true for success, false for failure.
         *
         *  This method starts the script running, but does not wait for completion. Only one script may be running
         *  at a time (this call will return false otherwise).
         *
         *  The optional arguments are passed to the script in order of definition. Any string beginning with
         *  "#$" is reserved for automatic value substitution, with the following methods currently defined:
         *
         *  |  #$APPLICATION_PATH   |   Replaced with the path to the current application.
         *  |  #$RESOURCE_PATH      |   Replaced with the path to the resource folder for the current application.
         *  |  #$=<env>             |   Replaced with the value of getenv("<env>") from the app environment invoking the script.
         *  |  #$?<key>             |   Replaced with the value for <key> in the application info.plist file.
         *
         *  This method will attempt to run the requested script as root (as if via sudo). To obtain information
         *  about the environment launching the script, you can use the special '#$' prefixed substitution keys.
         *  Note that plist queries can only access the top level entries, and then only string, number and bool
         *  types. Any query that fails is passed to the script as an empty string rather than failing this call.
         *
         *  If the script does not complete before the specified timeout elapses, it is terminated.
         *
         *  An example script:
         *
         *      # MyScript.sh
         *      echo "Script $0 run with scriptCommand $1, resourcePath $2, user $3"
         *      echo "script is exiting"
         *
         *  This could be run using the following:
         *
         *      PrivilegedScriptRunner isr("MyScript.sh");              // create an object to run the specified script
         *      isr.authorise();                                        // get authorisation
         *      isr.start("a command", "#$RESOURCE_PATH", "#$=USER");   // start the script running (in the background)
         *      isr.wait();                                             // wait for it to complete
         *      isr.deauthorise();                                      // tear down the authorisation rights
         *
         */
        bool start(const char* arg1=0, const char* arg2=0, const char* arg3=0, const char* arg4=0, const char* arg5=0, const char* arg6=0, const char* arg7=0, const char* arg8=0)
        {
            return startScript(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
        }


        /** Test if the script is running.
         *
         *  @return     Logical true if the script is active, false otherwise.
         */
        bool isRunning();


        /** Wait for the script to finish running (or return immediately if no script is active).
         */
        void wait();


        /** Stop the script running.
         *
         *  This terminates the script via kill. It does nothing if no script is current active.
         */
        void stop()
        {
            return stopScript();
        }


    private:

        CFStringRef m_scriptName;               //!< The script to run.
        CFStringRef m_scriptLabel;              //!< The label associated with the current script.
        CFStringRef m_stdoutPath;               //!< The stdout re-direct, or nil if none
        CFStringRef m_stderrPath;               //!< The stderr re-direct, or nil if none
        AuthorizationRef m_auth;                //!< The system authorisation.
        bool m_haveAuth;                        //!< Logical true if we have authorisation.
        int m_timeoutSecs;                      //!< The script timeout.
        bool m_scriptIsActive;                  //!< Logical true if a script is active.
        pid_t m_scriptPid;                      //!< The PID associated with the current script.

        bool startScript(const char* arg1, const char* arg2, const char* arg3, const char* arg4, const char* arg5, const char* arg6, const char* arg7, const char* arg8);
        void stopScript();
        void cleanupScript();
        bool readLine(std::string& str, int fd, unsigned timeoutSeconds);
        CFStringRef urlToAbsPath(CFURLRef url);

        PrivilegedScriptRunner(const PrivilegedScriptRunner&);              // prevent use
        PrivilegedScriptRunner& operator=(const PrivilegedScriptRunner&);   // prevent use
    };

}   // namespace

#endif      // COM_TSONIQ_ts_drivershieldinstaller_H
