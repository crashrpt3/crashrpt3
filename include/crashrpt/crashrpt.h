#pragma once

#include <Windows.h>
#include <DbgHelp.h>

#define CRASHRPT_VER 3100 // crashrpt version

#ifdef __cplusplus
#define CRASHRPT_EXTERN_C extern "C"
#else
#define CRASHRPT_EXTERN_C
#endif

#ifdef CRASHRPT_EXPORTS
#define CRASHRPT_API(rettype) CRASHRPT_EXTERN_C __declspec(dllexport) rettype WINAPI
#else
#define CRASHRPT_API(rettype) CRASHRPT_EXTERN_C __declspec(dllimport) rettype WINAPI
#endif

#if !defined(CRASHRPT_NO_AUTOMATIC_LIBS) && !defined(CRASHRPT_EXPORTS)
#ifdef _DEBUG
#define CRASHRPT_SUFFIX "d.lib"
#else
#define CRASHRPT_SUFFIX ".lib"
#endif
#define CRASHRPT_STRINGIFY(x) #x
#define CRASHRPT_TOSTRING(x) CRASHRPT_STRINGIFY(x)
#pragma comment(lib, "crashrpt" CRASHRPT_TOSTRING(CRASHRPT_VER) CRASHRPT_SUFFIX)
#endif

// Crash handlers
#define CR_CRASH_HANDLER_SEH                   0x1    // Install SEH handler.
#define CR_CRASH_HANDLER_TERMINATE_CALL        0x2    // Install terminate handler.
#define CR_CRASH_HANDLER_UNEXPECTED_CALL       0x4    // Install unexpected handler.
#define CR_CRASH_HANDLER_CPP_PURE              0x8    // Install pure call handler (VS .NET and later).
#define CR_CRASH_HANDLER_NEW_OPERATOR          0x10   // Install new operator error handler (VS .NET and later).
#define CR_CRASH_HANDLER_SECURITY              0x20   // Install security error handler (VS .NET and later).
#define CR_CRASH_HANDLER_INVALID_PARAMETER     0x40   // Install invalid parameter handler (VS 2005 and later).
#define CR_CRASH_HANDLER_SIGABRT               0x80   // Install SIGABRT signal handler.
#define CR_CRASH_HANDLER_SIGFPE                0x100  // Install SIGFPE signal handler.
#define CR_CRASH_HANDLER_SIGILL                0x200  // Install SIGILL signal handler.
#define CR_CRASH_HANDLER_SIGINT                0x400  // Install SIGINT signal handler.
#define CR_CRASH_HANDLER_SIGSEGV               0x800  // Install SIGSEGV signal handler.
#define CR_CRASH_HANDLER_SIGTERM               0x1000 // Install SIGTERM signal handler.
#define CR_CRASH_HANDLER_ALL                   0xFFFF // Install all possible exception handlers.

// Crash types
#define CR_CRASH_TYPE_SEH                      0      // SEH exception.
#define CR_CRASH_TYPE_TERMINATE_CALL           1      // C++ terminate() call.
#define CR_CRASH_TYPE_UNEXPECTED_CALL          2      // C++ unexpected() call.
#define CR_CRASH_TYPE_CPP_PURE                 3      // C++ pure virtual function call (VS .NET and later).
#define CR_CRASH_TYPE_CPP_NEW_OPERATOR         4      // C++ new operator fault (VS .NET and later).
#define CR_CRASH_TYPE_SECURITY                 5      // Buffer overrun error (VS .NET only, No longer crashes since VS2017).
#define CR_CRASH_TYPE_INVALID_PARAMETER        6      // Invalid parameter exception (VS 2005 and later).
#define CR_CRASH_TYPE_SIGABRT                  7      // C++ SIGABRT signal (abort).
#define CR_CRASH_TYPE_SIGFPE                   8      // C++ SIGFPE signal (flotating point exception).
#define CR_CRASH_TYPE_SIGILL                   9      // C++ SIGILL signal (illegal instruction, win-api created threads only).
#define CR_CRASH_TYPE_SIGINT                   10     // C++ SIGINT signal (CTRL+C).
#define CR_CRASH_TYPE_SIGSEGV                  11     // C++ SIGSEGV signal (invalid storage access, win-api created threads only).
#define CR_CRASH_TYPE_SIGTERM                  12     // C++ SIGTERM signal (termination request).
#define CR_CRASH_TYPE_NONCONTINUABLE           13     // Non continuable sofware exception.
#define CR_CRASH_TYPE_CPP_THROW                14     // Throw C++ typed exception (win-api created threads only).
#define CR_CRASH_TYPE_STACK_OVERFLOW           15     // Stack overflow.

typedef void(*CR_CREATEMINIDUMP_CALLBACK)(void* param, const wchar_t* directory);

typedef struct _CR_INSTALL_INFO {
    unsigned long  cb;                                // Size of this structure in bytes; must be initialized before using!
    const wchar_t* crashrptExePath;                   // File path of your custom crashrptdump.exe.
    const wchar_t* dumpOutDirectory;                  // Directory where to save dump error reports, must ends with '\\'.
    unsigned long  crashHandlers;                     // See micro CR_CRASH_HANDLER_ALL
    MINIDUMP_TYPE  minidumpType;                      // Minidump type.
} CR_INSTALL_INFO;

CRASHRPT_API(int) crInstall(const CR_INSTALL_INFO* info);
CRASHRPT_API(int) crUninstall();
CRASHRPT_API(int) crInstallThisThread(unsigned long crashHandlers = 0);
CRASHRPT_API(int) crUninstallThisThread();
CRASHRPT_API(int) crAddProperty(const wchar_t* name, const wchar_t* value);
CRASHRPT_API(int) crGetLastError(wchar_t* buffer, int sizeInWords);
CRASHRPT_API(int) crTestCrash(unsigned long crashType) noexcept(false);
CRASHRPT_API(int) crCreateMiniDump(const wchar_t* crashGUID, CR_CREATEMINIDUMP_CALLBACK callback = nullptr, void* param = nullptr);

#ifdef __cplusplus
#include <string>
namespace crashrpt {

class CrInstallGurad
{
public:
    explicit CrInstallGurad(const CR_INSTALL_INFO* info)
    {
        m_ret = crInstall(info);
    }

    ~CrInstallGurad()
    {
        if (m_ret == 0)
        {
            crUninstall();
        }
    }

    bool isInstalled() const
    {
        return (m_ret == 0);
    }

    int addProperty(const wchar_t* name, const wchar_t* value)
    {
        return crAddProperty(name, value);
    }

    std::wstring getLastError() const
    {
        std::wstring ret;
        auto len = crGetLastError(nullptr, 0);
        if (len > 0)
        {
            auto buffer = new wchar_t[len + 1];
            len = crGetLastError(buffer, len + 1);
            ret = buffer;
        }
        return ret;
    }

private:
    int m_ret;
};

class CrInstallThisThreadGurad
{
public:
    explicit CrInstallThisThreadGurad(unsigned long crashHandlers = 0)
    {
        m_ret = crInstallThisThread(crashHandlers);
    }

    ~CrInstallThisThreadGurad()
    {
        if (m_ret == 0)
        {
            crUninstallThisThread();
        }
    }

    bool isInstalled() const
    {
        return (m_ret == 0);
    }

    std::wstring getLastError() const
    {
        std::wstring ret;
        auto len = crGetLastError(nullptr, 0);
        if (len > 0)
        {
            auto buffer = new wchar_t[len + 1];
            len = crGetLastError(buffer, len + 1);
            ret = buffer;
        }
        return ret;
    }

private:
    int m_ret;
};
} // namespace crashpt
#endif // __cplusplus
