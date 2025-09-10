#pragma once
#include "crashrpt/crashrpt.h"

class ApplicationUniqueLock;
class ReInitializer;

struct InstallInfo;
struct ExceptionInfo;
struct ProcessExceptionHandlers;
struct ThreadExceptionHandlers;

class CrashRptApp
{
public:
    CrashRptApp();
    ~CrashRptApp();
    static CrashRptApp* instance();
    int install(const CR_INSTALL_INFO* info);
    int addProperty(LPCWSTR name, LPCWSTR value);

private:
    int uninstall();

    int setProcessExceptionHandlers(UINT32 crashHandlers);
    int unSetProcessExceptionHandlers();

    int setThreadExceptionHandlers(UINT32 crashHandlers);
    int unSetThreadExceptionHandlers();

    int generateErrorReport(ExceptionInfo* exceptionInfo);
    int launchCrashRptDump(LPCWSTR szCmdLineParams, BOOL bWait);

    int resetForPerCrash();

    // Process exception callbacks
    static LONG WINAPI  onHandleSEH(PEXCEPTION_POINTERS pExceptionPtrs);
#if _MSC_VER>=1300
    static void __cdecl onHandlePureCall();
    static int __cdecl  onHandleCppNew(size_t);
#endif
    static void         onHandleSIGABRT(int);
    static void         onHandleSIGINT(int);
    static void         onHandleSIGTERM(int);
#if _MSC_VER>=1400
    static void __cdecl onHandleInvalidParameter(const wchar_t* pszExpression, const wchar_t* pszFunction, const wchar_t* pszFile, unsigned int uLine, uintptr_t pReserved);
#endif

    // Thread exception callbacks
    static void __cdecl onHandleTerminate();
    static void __cdecl onHandleTerminateUnexpected();
    static void         onHandleSIGFPE(int /*code*/, int subcode);
    static void         onHandleSIGILL(int);
    static void         onHandleSIGEGV(int);

    // Tools and Helpers
    static int bugfix64And32Env();
    static void getExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers);
    static unsigned __stdcall runThreadSEHStackOverflow(void* pvParam);

private:
    static CrashRptApp* m_instance;

    std::map<std::string, std::string> m_props;
    std::shared_ptr<InstallInfo> m_installInfo;
    std::shared_ptr<ProcessExceptionHandlers> m_oldProcessHandlers;
    bool m_isInstalled = false;
    CString m_crashGUID;
    HANDLE m_hEvent = nullptr;

    std::mutex m_mutex;
    bool m_isContinueExecution = true;        // Whether to terminate process (the default) or to continue execution after crash.
    bool m_isContinueExecutionNow = true;     // After GenerateErrorReport() m_bContinueExecution is reset to FALSE. This is the current value

    friend class ApplicationUniqueLock;
    friend class ReInitializer;
};
