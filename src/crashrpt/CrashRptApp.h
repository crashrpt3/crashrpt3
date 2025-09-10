#pragma once
#include "crashrpt/crashrpt.h"

class ApplicationUniqueLock;
class ReInitializer;
class CrInstallInfo;

struct CR_EXCEPTION_INFO;
struct CR_OLD_EXCEPTION_HANDLERS;

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

    int setExceptionHandlers(UINT32 crashHandlers);
    int unsetExceptionHandlers();

    int generateErrorReport(CR_EXCEPTION_INFO* exceptionInfo);
    int launchCrashRptDump(LPCWSTR szCmdLineParams, BOOL bWait);

    int resetForPerCrash();

    // Exception callbacks
    static LONG WINAPI  onHandleSEH(PEXCEPTION_POINTERS pExceptionPointers);
    static void __cdecl onHandlePureCall();
    static int __cdecl  onHandleCppNew(size_t);
    static void         onHandleSIGABRT(int);
    static void         onHandleSIGILL(int);
    static void         onHandleSIGINT(int);
    static void         onHandleSIGEGV(int);
    static void         onHandleSIGTERM(int);
    static void         onHandleSIGFPE(int /*code*/, int subcode);
    static void __cdecl onHandleInvalidParameter(const wchar_t* pszExpression, const wchar_t* pszFunction, const wchar_t* pszFile, unsigned int uLine, uintptr_t pReserved);

    // Tools and Helpers
    static int bugfix64And32Env();
    static void getExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers);
    static unsigned __stdcall runThreadSEHStackOverflow(void* pvParam);

private:
    static CrashRptApp* m_instance;

    std::map<std::string, std::string> m_props;
    std::shared_ptr<CrInstallInfo> m_installInfo;
    std::shared_ptr<CR_OLD_EXCEPTION_HANDLERS> m_oldHandlers;
    bool m_isInstalled = false;
    CString m_crashGUID;
    HANDLE m_hEvent = nullptr;

    std::mutex m_mutex;
    bool m_isContinueExecution = true;        // Whether to terminate process (the default) or to continue execution after crash.
    bool m_isContinueExecutionNow = true;     // After GenerateErrorReport() m_bContinueExecution is reset to FALSE. This is the current value

    friend class ApplicationUniqueLock;
    friend class ReInitializer;
};
