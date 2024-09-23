/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

/*! \file   CrashRpt.h
*  \brief  Defines the interface for the CrashRpt.DLL.
*  \date   2003
*  \author Michael Carruth
*  \author Oleg Krivtsov (zeXspectrum)
*/
#pragma once

#include <windows.h>
#include <dbghelp.h>

#ifdef __cplusplus
#define CRASHRPT_EXTERNC extern "C"
#else
#define CRASHRPT_EXTERNC
#endif

#define CRASHRPTAPI(rettype) CRASHRPT_EXTERNC rettype WINAPI

//! Current CrashRpt version
#define CRASHRPT_VER 1500

// Exception types used in CR_EXCEPTION_INFO::exctype structure member.
#define CR_SEH_EXCEPTION                0    //!< SEH exception.
#define CR_CPP_TERMINATE_CALL           1    //!< C++ terminate() call.
#define CR_CPP_UNEXPECTED_CALL          2    //!< C++ unexpected() call.
#define CR_CPP_PURE_CALL                3    //!< C++ pure virtual function call (VS .NET and later).
#define CR_CPP_NEW_OPERATOR_ERROR       4    //!< C++ new operator fault (VS .NET and later).
#define CR_CPP_SECURITY_ERROR           5    //!< Buffer overrun error (VS .NET only).
#define CR_CPP_INVALID_PARAMETER        6    //!< Invalid parameter exception (VS 2005 and later).
#define CR_CPP_SIGABRT                  7    //!< C++ SIGABRT signal (abort).
#define CR_CPP_SIGFPE                   8    //!< C++ SIGFPE signal (flotating point exception).
#define CR_CPP_SIGILL                   9    //!< C++ SIGILL signal (illegal instruction).
#define CR_CPP_SIGINT                   10   //!< C++ SIGINT signal (CTRL+C).
#define CR_CPP_SIGSEGV                  11   //!< C++ SIGSEGV signal (invalid storage access).
#define CR_CPP_SIGTERM                  12   //!< C++ SIGTERM signal (termination request).

typedef struct tagCR_EXCEPTION_INFO
{
    WORD cb;                   //!< Size of this structure in bytes; should be initialized before using.
    PEXCEPTION_POINTERS pexcptrs; //!< Exception pointers.
    int exctype;               //!< Exception type.
    DWORD code;                //!< Code of SEH exception.
    unsigned int fpe_subcode;  //!< Floating point exception subcode.
    const wchar_t* expression; //!< Assertion expression.
    const wchar_t* function;   //!< Function in which assertion happened.
    const wchar_t* file;       //!< File in which assertion happened.
    unsigned int line;         //!< Line number.
    BOOL bManual;              //!< Flag telling if the error report is generated manually or not.
    HANDLE hSenderProcess;     //!< Handle to the CrashSender.exe process.
} CR_EXCEPTION_INFO;

typedef CR_EXCEPTION_INFO* PCR_EXCEPTION_INFO;

// Stages of crash report generation (used by the crash callback function).
#define CR_CB_STAGE_PREPARE      10  //!< Stage after exception pointers've been retrieved.
#define CR_CB_STAGE_FINISH       20  //!< Stage after the launch of CrashSender.exe process.

typedef struct tagCR_CRASH_CALLBACK_INFO
{
    WORD cb;                            //!< Size of this structure in bytes.
	int nStage;                         //!< Stage.
	LPCWSTR pszErrorReportFolder;       //!< Directory where crash report files are located.
    CR_EXCEPTION_INFO* pExceptionInfo;  //!< Pointer to information about the crash.
	LPVOID pUserParam;                  //!< Pointer to user-defined data.
	BOOL bContinueExecution;            //!< Whether to terminate the process (the default) or to continue program execution.
} CR_CRASH_CALLBACK_INFO;

// Constants that may be returned by the crash callback function.
#define CR_CB_CANCEL             0 //!< Cancel crash report generation on the current stage.
#define CR_CB_DODEFAULT          1 //!< Proceed to the next stages of crash report generation without calling crash callback function.
#define CR_CB_NOTIFY_NEXT_STAGE  2 //!< Proceed and call the crash callback for the next stage.

typedef int (CALLBACK *PFNCRASHCALLBACK) (CR_CRASH_CALLBACK_INFO* pInfo);

// Array indices for CR_INSTALL_INFO::uPriorities.
#define CR_HTTP 0  //!< Send error report via HTTP (or HTTPS) connection.

//! Special priority constant that allows to skip certain delivery method.
#define CR_NEGATIVE_PRIORITY ((UINT)-1)

// Flags for CR_INSTALL_INFO::dwFlags
#define CR_INST_STRUCTURED_EXCEPTION_HANDLER      0x1 //!< Install SEH handler (deprecated name, use \ref CR_INST_SEH_EXCEPTION_HANDLER instead).
#define CR_INST_SEH_EXCEPTION_HANDLER             0x1 //!< Install SEH handler.
#define CR_INST_TERMINATE_HANDLER                 0x2 //!< Install terminate handler.
#define CR_INST_UNEXPECTED_HANDLER                0x4 //!< Install unexpected handler.
#define CR_INST_PURE_CALL_HANDLER                 0x8 //!< Install pure call handler (VS .NET and later).
#define CR_INST_NEW_OPERATOR_ERROR_HANDLER       0x10 //!< Install new operator error handler (VS .NET and later).
#define CR_INST_SECURITY_ERROR_HANDLER           0x20 //!< Install security error handler (VS .NET and later).
#define CR_INST_INVALID_PARAMETER_HANDLER        0x40 //!< Install invalid parameter handler (VS 2005 and later).
#define CR_INST_SIGABRT_HANDLER                  0x80 //!< Install SIGABRT signal handler.
#define CR_INST_SIGFPE_HANDLER                  0x100 //!< Install SIGFPE signal handler.
#define CR_INST_SIGILL_HANDLER                  0x200 //!< Install SIGILL signal handler.
#define CR_INST_SIGINT_HANDLER                  0x400 //!< Install SIGINT signal handler.
#define CR_INST_SIGSEGV_HANDLER                 0x800 //!< Install SIGSEGV signal handler.
#define CR_INST_SIGTERM_HANDLER                0x1000 //!< Install SIGTERM signal handler.

#define CR_INST_ALL_POSSIBLE_HANDLERS          0x1FFF //!< Install all possible exception handlers.
#define CR_INST_CRT_EXCEPTION_HANDLERS         0x1FFE //!< Install exception handlers for the linked CRT module.

#define CR_INST_NO_GUI                         0x2000 //!< Do not show GUI, send report silently (use for non-GUI apps only).
#define CR_INST_HTTP_BINARY_ENCODING           0x4000 //!< Deprecated, do not use.
#define CR_INST_DONT_SEND_REPORT               0x8000 //!< Don't send error report immediately, just save it locally.
#define CR_INST_APP_RESTART                   0x10000 //!< Restart the application on crash.
#define CR_INST_NO_MINIDUMP                   0x20000 //!< Do not include minidump file to crash report.
#define CR_INST_SEND_QUEUED_REPORTS           0x40000 //!< CrashRpt should send error reports that are waiting to be delivered.
#define CR_INST_STORE_ZIP_ARCHIVES            0x80000 //!< CrashRpt should store both uncompressed error report files and ZIP archives.
#define CR_INST_SEND_MANDATORY				 0x100000 //!< This flag removes the "Close" and "Other actions" buttons from Error Report dialog, thus making the sending procedure mandatory for user.
#define CR_INST_SHOW_ADDITIONAL_INFO_FIELDS	 0x200000 //!< Makes "Your E-mail" and "Describe what you were doing when the problem occurred" fields of Error Report dialog always visible.
#define CR_INST_ALLOW_ATTACH_MORE_FILES		 0x400000 //!< Adds an ability for user to attach more files to crash report by clicking "Attach More File(s)" item from context menu of Error Report Details dialog.
#define CR_INST_AUTO_THREAD_HANDLERS         0x800000 //!< If this flag is set, installs exception handlers for newly created threads automatically.

typedef struct tagCR_INSTALL_INFO
{
    WORD cb;                        //!< Size of this structure in bytes; must be initialized before using!
    LPCWSTR pszAppName;             //!< Name of application.
    LPCWSTR pszAppVersion;          //!< Application version.
    LPCWSTR pszUrl;                 //!< URL of server-side script (used in HTTP connection).
    LPCWSTR pszCrashSenderPath;     //!< Directory name where CrashSender.exe is located.
    UINT uPriorities[5];            //!< Array of error sending transport priorities.
    DWORD dwFlags;                  //!< Flags.
    LPCWSTR pszPrivacyPolicyURL;    //!< URL of privacy policy agreement.
    LPCWSTR pszDebugHelpDLL;        //!< File name or folder of Debug help DLL.
    MINIDUMP_TYPE uMiniDumpType;    //!< Minidump type.
    LPCWSTR pszErrorReportSaveDir;  //!< Directory where to save error reports.
    LPCWSTR pszRestartCmdLine;      //!< Command line for application restart (without executable name).
    LPCWSTR pszLangFilePath;        //!< Path to the language file (including file name).
    LPCWSTR pszCustomSenderIcon;    //!< Custom icon used for Error Report dialog.
    int nRestartTimeout;            //!< Timeout for application restart.
    int nMaxReportsPerDay;          //!< Maximum number of crash reports that will be sent per calendar day.
} CR_INSTALL_INFO;

typedef CR_INSTALL_INFO* PCR_INSTALL_INFO;

// Flags for crAddFile() function.
#define CR_AF_TAKE_ORIGINAL_FILE  0 //!< Take the original file (do not copy it to the error report folder).
#define CR_AF_MAKE_FILE_COPY      1 //!< Copy the file to the error report folder.
#define CR_AF_FILE_MUST_EXIST     0 //!< Function will fail if file doesn't exist at the moment of function call.
#define CR_AF_MISSING_FILE_OK     2 //!< Do not fail if file is missing (assume it will be created later).
#define CR_AF_ALLOW_DELETE        4 //!< If this flag is specified, the file will be deletable from context menu of Error Report Details dialog.

// Flags for crAddScreenshot function.
#define CR_AS_VIRTUAL_SCREEN  0  //!< Take a screenshot of the virtual screen.
#define CR_AS_MAIN_WINDOW     1  //!< Take a screenshot of application's main window.
#define CR_AS_PROCESS_WINDOWS 2  //!< Take a screenshot of all visible process windows.
#define CR_AS_GRAYSCALE_IMAGE 4  //!< Make a grayscale image instead of a full-color one.
#define CR_AS_USE_JPEG_FORMAT 8  //!< Store screenshots as JPG files.
#define CR_AS_ALLOW_DELETE   16  //!< If this flag is specified, the file will be deletable from context menu of Error Report Details dialog.

// Flags for crAddVideo function.
#define CR_AV_VIRTUAL_SCREEN  0  //!< Capture the whole virtual screen.
#define CR_AV_MAIN_WINDOW     1  //!< Capture the area of application's main window.
#define CR_AV_PROCESS_WINDOWS 2  //!< Capture all visible process windows.
#define CR_AV_QUALITY_LOW     0  //!< Low quality video encoding, smaller file size.
#define CR_AV_QUALITY_GOOD    4  //!< Good encoding quality, larger file size.
#define CR_AV_QUALITY_BEST    8  //!< The best encoding quality, the largest file size.
#define CR_AV_NO_GUI         16  //!< Do not display the notification dialog.
#define CR_AV_ALLOW_DELETE   32  //!< If this flag is specified, the file will be deletable from context menu of Error Report Details dialog.

// Flags used by crEmulateCrash() function
#define CR_NONCONTINUABLE_EXCEPTION  32  //!< Non continuable sofware exception.
#define CR_THROW                     33  //!< Throw C++ typed exception.
#define CR_STACK_OVERFLOW			 34  //!< Stack overflow.

CRASHRPTAPI(int) crInstall(PCR_INSTALL_INFO pInfo);
CRASHRPTAPI(int) crUninstall();

CRASHRPTAPI(int) crInstallToCurrentThread2(DWORD dwFlags);
CRASHRPTAPI(int) crUninstallFromCurrentThread();

CRASHRPTAPI(int) crSetCrashCallback(PFNCRASHCALLBACK pfnCallbackFunc, LPVOID lpParam);

CRASHRPTAPI(int) crAddFile(LPCWSTR pszFile, LPCWSTR pszDestFile, LPCWSTR pszDesc, DWORD dwFlags);
CRASHRPTAPI(int) crAddProperty(LPCWSTR pszPropName, LPCWSTR pszPropValue);

CRASHRPTAPI(int) crGetLastErrorMsg(LPWSTR pszBuffer, UINT uBuffSize);

CRASHRPTAPI(int) crGenerateErrorReport(PCR_EXCEPTION_INFO pExceptionInfo);
CRASHRPTAPI(int) crEmulateCrash(unsigned ExceptionType) noexcept(false);

#ifdef __cplusplus

class CrAutoInstallHelper
{
public:
    CrAutoInstallHelper(__in PCR_INSTALL_INFO pInfo)
    {
        m_ret = crInstall(pInfo);
    }

    ~CrAutoInstallHelper()
    {
        if (m_ret == 0)
        {
            crUninstall();
        }
    }

private:
    int m_ret;
};

class CrThreadAutoInstallHelper
{
public:
    CrThreadAutoInstallHelper(DWORD dwFlags = 0)
    {
        m_ret = crInstallToCurrentThread2(dwFlags);
    }

    ~CrThreadAutoInstallHelper()
    {
        if (m_ret == 0)
        {
            crUninstallFromCurrentThread();
        }
    }

private:
    int m_ret;
};

#endif // __cplusplus
