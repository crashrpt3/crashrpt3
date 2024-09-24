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

#ifndef CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF
#define CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF

#include <windows.h>
#include <dbghelp.h>

#define CRASHRPT_VER 3100 // CrashRpt Version

#ifdef __cplusplus
#define CRASHRPT_EXTERN_C extern "C"
#else
#define CRASHRPT_EXTERN_C
#endif

#define CRASHRPT_API(rettype) CRASHRPT_EXTERN_C rettype WINAPI

typedef struct
{
    WORD cb;                                 //!< Size of this structure in bytes; should be initialized before using.
    PEXCEPTION_POINTERS pExceptionPointers;  //!< Exception pointers.
    INT32 nExceptionType;                    //!< Exception type.
    DWORD dwSEHCode;                         //!< Code of SEH exception.
    UINT32 uFloatPointExceptionSubcode;      //!< Floating point exception subcode.
    LPCWSTR lpAssertionExpression;           //!< Assertion expression.
    LPCWSTR lpFunction;                      //!< Function in which assertion happened.
    LPCWSTR lpFile;                          //!< File in which assertion happened.
    UINT32 uLine;                            //!< Line number.
    BOOL bManual;                            //!< Flag telling if the error report is generated manually or not.
    HANDLE hSenderProcess;                   //!< Handle to the CrashSender.exe process.
} CR_EXCEPTION_INFO;

typedef CR_EXCEPTION_INFO* PCR_EXCEPTION_INFO;

// Stages of crash report generation (used by the crash callback function).
#define CR_CB_STAGE_PREPARE      10  //!< Stage after exception pointers've been retrieved.
#define CR_CB_STAGE_FINISH       20  //!< Stage after the launch of CrashSender.exe process.

typedef struct
{
    WORD cb;                              //!< Size of this structure in bytes.
    INT32 nStage;                         //!< Stage.
    LPCWSTR pszErrorReportFolder;       //!< Directory where crash report files are located.
    CR_EXCEPTION_INFO* pExceptionInfo;  //!< Pointer to information about the crash.
    LPVOID pUserParam;                  //!< Pointer to user-defined data.
    BOOL bContinueExecution;            //!< Whether to terminate the process (the default) or to continue program execution.
} CR_CRASH_CALLBACK_INFO;

// Constants that may be returned by the crash callback function.
#define CR_CB_CANCEL             0 //!< Cancel crash report generation on the current stage.
#define CR_CB_DODEFAULT          1 //!< Proceed to the next stages of crash report generation without calling crash callback function.
#define CR_CB_NOTIFY_NEXT_STAGE  2 //!< Proceed and call the crash callback for the next stage.

typedef int (CALLBACK* PFN_CRASH_CALLBACK) (CR_CRASH_CALLBACK_INFO* pInfo);

// Crash handlers
#define CR_CRASH_HANDLER_SEH                            0x1    //!< Install SEH handler.
#define CR_CRASH_HANDLER_TERMINATE_CALL                 0x2    //!< Install terminate handler.
#define CR_CRASH_HANDLER_UNEXPECTED_CALL                0x4    //!< Install unexpected handler.
#define CR_CRASH_HANDLER_CPP_PURE                       0x8    //!< Install pure call handler (VS .NET and later).
#define CR_CRASH_HANDLER_NEW_OPERATOR                   0x10   //!< Install new operator error handler (VS .NET and later).
#define CR_CRASH_HANDLER_SECURITY                       0x20   //!< Install security error handler (VS .NET and later).
#define CR_CRASH_HANDLER_INVALID_PARAMETER              0x40   //!< Install invalid parameter handler (VS 2005 and later).
#define CR_CRASH_HANDLER_SIGABRT                        0x80   //!< Install SIGABRT signal handler.
#define CR_CRASH_HANDLER_SIGFPE                         0x100  //!< Install SIGFPE signal handler.
#define CR_CRASH_HANDLER_SIGILL                         0x200  //!< Install SIGILL signal handler.
#define CR_CRASH_HANDLER_SIGINT                         0x400  //!< Install SIGINT signal handler.
#define CR_CRASH_HANDLER_SIGSEGV                        0x800  //!< Install SIGSEGV signal handler.
#define CR_CRASH_HANDLER_SIGTERM                        0x1000 //!< Install SIGTERM signal handler.
#define CR_CRASH_HANDLER_ALL                            0xFFFF //!< Install all possible exception handlers.

typedef struct _CrInstallInfo {
    WORD cb;                        //!< Size of this structure in bytes; must be initialized before using!
    LPCWSTR szAppName;              //!< Application name.
    LPCWSTR szAppVersion;           //!< Application version.
    LPCWSTR szServerURL;            //!< URL of server-side script (used in HTTP connection).
    LPCWSTR szPrivacyPolicyURL;     //!< URL of privacy policy agreement.
    LPCWSTR szCrashSenderDirectory; //!< Directory where CrashSender.exe is located.
    LPCWSTR szDBGHelpDirectory;     //!< Directory where dbghelp.dll is located.
    LPCWSTR szOutputDirectory;      //!< Directory where to save dump error reports.
    UINT32 uCrashHandler;           //!< See micro CR_CRASH_HANDLER_ALL
    MINIDUMP_TYPE uMinidumpType;    //!< Minidump type.
} CrInstallInfo;

// Flags for crAddFile() function.
#define CR_AF_TAKE_ORIGINAL_FILE  0 //!< Take the original file (do not copy it to the error report folder).
#define CR_AF_MAKE_FILE_COPY      1 //!< Copy the file to the error report folder.
#define CR_AF_FILE_MUST_EXIST     0 //!< Function will fail if file doesn't exist at the moment of function call.
#define CR_AF_MISSING_FILE_OK     2 //!< Do not fail if file is missing (assume it will be created later).
#define CR_AF_ALLOW_DELETE        4 //!< If this flag is specified, the file will be deletable from context menu of Error Report Details dialog.

CRASHRPT_API(int) crInstall(const CrInstallInfo* pInfo);
CRASHRPT_API(int) crUninstall();

CRASHRPT_API(int) crAddFile(LPCWSTR pszFile, LPCWSTR pszDestFile, LPCWSTR pszDesc, DWORD dwFlags);
CRASHRPT_API(int) crAddProperty(LPCWSTR pszPropName, LPCWSTR pszPropValue);

CRASHRPT_API(int) crSetCrashCallback(PFN_CRASH_CALLBACK pfnCallback, LPVOID lpParam);
CRASHRPT_API(int) crGetLastError(LPWSTR pszBuffer, UINT uBuffSize);
CRASHRPT_API(int) crGenerateErrorReport(PCR_EXCEPTION_INFO pExceptionInfo);

#ifdef __cplusplus

namespace crashrpt {

class CrInstallGurad
{
public:
    CrInstallGurad(const CrInstallInfo* pInfo)
    {
        m_ret = crInstall(pInfo);
    }

    ~CrInstallGurad()
    {
        if (m_ret == 0)
        {
            crUninstall();
        }
    }

private:
    int m_ret;
};

} // namespace crashpt

#endif // __cplusplus


// Test crashes
#define CR_TEST_CRASH_SEH                      0    //!< SEH exception.
#define CR_TEST_CRASH_TERMINATE_CALL           1    //!< C++ terminate() call.
#define CR_TEST_CRASH_UNEXPECTED_CALL          2    //!< C++ unexpected() call.
#define CR_TEST_CRASH_CPP_PURE                 3    //!< C++ pure virtual function call (VS .NET and later).
#define CR_TEST_CRASH_CPP_NEW_OPERATOR         4    //!< C++ new operator fault (VS .NET and later).
#define CR_TEST_CRASH_SECURITY                 5    //!< Buffer overrun error (VS .NET only).
#define CR_TEST_CRASH_INVALID_PARAMETER        6    //!< Invalid parameter exception (VS 2005 and later).
#define CR_TEST_CRASH_SIGABRT                  7    //!< C++ SIGABRT signal (abort).
#define CR_TEST_CRASH_SIGFPE                   8    //!< C++ SIGFPE signal (flotating point exception).
#define CR_TEST_CRASH_SIGILL                   9    //!< C++ SIGILL signal (illegal instruction).
#define CR_TEST_CRASH_SIGINT                   10   //!< C++ SIGINT signal (CTRL+C).
#define CR_TEST_CRASH_SIGSEGV                  11   //!< C++ SIGSEGV signal (invalid storage access).
#define CR_TEST_CRASH_SIGTERM                  12   //!< C++ SIGTERM signal (termination request).
#define CR_TEST_CRASH_NONCONTINUABLE           13   //!< Non continuable sofware exception.
#define CR_TEST_CRASH_CPP_THROW                14   //!< Throw C++ typed exception.
#define CR_TEST_CRASH_STACK_OVERFLOW           15   //!< Stack overflow.

// Test all supported crashes.
CRASHRPT_API(int) crTestCrash(UINT32 uTestCrash) noexcept(false);

#endif // CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF
