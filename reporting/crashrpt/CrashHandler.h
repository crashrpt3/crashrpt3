#pragma once
#include "CrashRpt.h"
#include "SharedMemory.h"

struct FileItem
{
    CString srcFilePath;      // Path to the original file.
    CString dstFileName;      // Destination file name (as seen in ZIP archive).
    CString description;      // Description.
    BOOL isMakeCopy = FALSE;    // Should we make a copy of this file on crash?
                                 // If set, the file will be copied to crash report folder and that copy will be included into crash report,
                                 // otherwise the file will be included from its original location (not guaranteing that file is the same it was
                                 // at the moment of crash).
    BOOL isAllowDelete = FALSE; // Whether to allow user deleting the file from context menu of Error Report Details dialog.
};

namespace {
    class LockGurad;
    class BeforeGenerateErrorReportGuard;
}

class CCrashHandler
{
    friend class LockGurad;
    friend class BeforeGenerateErrorReportGuard;

public:
    CCrashHandler();
    virtual ~CCrashHandler();

    static CCrashHandler* instance();

    int install(const CrInstallInfo* pInfo);
    int uninstall();
    BOOL isInstalled();

    int generateErrorReport(CR_EXCEPTION_INFO* pExceptionInfo = NULL);

    int addFile(LPCTSTR lpFile, LPCTSTR lpDestFile, LPCTSTR lpDesc, DWORD dwFlags);
    int addProperty(const CString& sPropName, const CString& sPropValue);

private:
    // Exception callbacks
    static LONG WINAPI onHandleSEH(PEXCEPTION_POINTERS pExceptionPointers);
    static unsigned __stdcall onHandleSEHStackOverflow(void* pvParam);
    static void __cdecl onHandlePureCall();
    static int __cdecl onHandleCppNew(size_t);
    static void onHandleSIGABRT(int);
    static void onHandleSIGILL(int);
    static void onHandleSIGINT(int);
    static void onHandleSIGEGV(int);
    static void onHandleSIGTERM(int);
    static void __cdecl onHandleInvalidParameter(
        const wchar_t* expression,
        const wchar_t* funcName,
        const wchar_t* file,
        unsigned int line,
        uintptr_t pReserved);

private:
    int setupExceptionHandlers(UINT32 uCrashHandlers);
    int tearDownExceptionHandlers();
    int launchCrashSender(LPCTSTR szCmdLineParams, BOOL bWait, HANDLE* phProcess);
    void getExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers);

    CRASH_DESCRIPTION* serializeCrashInfo();
    DWORD packString(const CString& str);
    DWORD packFileItem(FileItem& fi);
    DWORD packProperty(const CString& sName, const CString& sValue);

    void clearExceptionHandlers();

private:
    void lock();
    void unlock();
    int beforeGenerateErrorReport();

private:
    typedef void(__cdecl* _sigabrt_handler)(int);
    typedef void(__cdecl* _sigint_handler)(int);
    typedef void(__cdecl* _sigterm_handler)(int);

private:
    static CCrashHandler* m_pInstance;
    BOOL m_bInstalled;

    // For struct CrInstallInfo
    CString       m_szAppName;
    CString       m_szAppVersion;
    CString       m_szCrashSenderPath;
    CString       m_szServerURL;
    CString       m_szPrivacyPolicyURL;
    CString       m_szDBGHelpPath;
    CString       m_szOutputDirectory;
    UINT32        m_uCrashHandler = 0;
    MINIDUMP_TYPE m_uMinidumpType;

    CString m_szExeFullPath;
    CString m_szCrashGUID;

    // Exception old handlers
    LPTOP_LEVEL_EXCEPTION_FILTER m_hExcSEH;
    _purecall_handler            m_hExcPureCall;
    _PNH                         m_hExcCppNew;
    _invalid_parameter_handler   m_hExcInvalidParameter;
    _sigabrt_handler             m_hExcSIGABRT;
    _sigabrt_handler             m_hExcSIGILL;
    _sigint_handler              m_hExcSIGINT;
    _sigint_handler              m_hExcSIGEGV;
    _sigterm_handler             m_hExcSIGTERM;

    std::map<CString, FileItem> m_files; // File items to include.
    std::map<CString, CString> m_props;  // User-defined properties to include.
    std::recursive_mutex m_lock;
    HANDLE m_hEvent;               // Event used to synchronize CrashRpt.dll with CrashSender.exe.
    SharedMemory m_SharedMem;        // Shared memory.
    CRASH_DESCRIPTION* m_pCrashDesc; // Pointer to crash description shared mem view.
    CRASH_DESCRIPTION* m_pTmpCrashDesc; // Used temporarily
    HANDLE m_hSenderProcess;       // Handle to CrashSender.exe process.
    BOOL m_bContinueExecution;      // Whether to terminate process (the default) or to continue execution after crash.
    BOOL m_bContinueExecutionNow;   // After GenerateErrorReport() m_bContinueExecution is reset to FALSE. This is the current value
};
