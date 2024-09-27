#ifndef CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF
#define CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF

#include <windows.h>
#include <dbghelp.h>

#define CRASHRPT_VER 3100 // crashrpt version

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
    unsigned short cb;                   //!< Size of this structure in bytes; must be initialized before using!
    const wchar_t* applicationName;      //!< Application name.
    const wchar_t* applicationVersion;   //!< Application version.
    const wchar_t* serverURL;            //!< URL of server-side script (used in HTTP connection).
    const wchar_t* privacyPolicyURL;     //!< URL of privacy policy agreement.
    const wchar_t* crashsenderPath;      //!< File path of CrashSender.exe.
    const wchar_t* dbghelpPath;          //!< File path of dbghelp.dll.
    const wchar_t* outputDirectory;      //!< Directory where to save dump error reports.
    unsigned int   crashHandlers;        //!< See micro CR_CRASH_HANDLER_ALL
    MINIDUMP_TYPE  minidumpType;         //!< Minidump type.
} CrInstallInfo;

CRASHRPT_API(int) crInstall(const CrInstallInfo* info);
CRASHRPT_API(int) crUninstall();

CRASHRPT_API(int) crAddProperty(const wchar_t* name, const wchar_t* value);
CRASHRPT_API(int) crAddFile(const wchar_t* srcPath, const wchar_t* dstPath, const wchar_t* desc);

CRASHRPT_API(int) crGetLastError(wchar_t* buffer, int len);
CRASHRPT_API(int) crGenerateErrorReport(CR_EXCEPTION_INFO* pExceptionInfo);

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

#endif // CRASHRPT_986D12C8_D0C5_4FB0_B298_DB720C91A8DF
