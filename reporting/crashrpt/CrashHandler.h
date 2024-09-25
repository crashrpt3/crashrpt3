/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: CrashHandler.h
// Description: Exception handling functionality.
// Authors: mikecarruth, zexspectrum
// Date: 2009

#pragma once
#include "CrashRpt.h"
#include "Utility.h"
#include "CritSec.h"
#include "SharedMem.h"

/* This structure contains pointer to the exception handlers for a thread. */
struct ThreadExceptionHandlers
{
    terminate_handler pfnTerminateHandlerPrev;   // Previous terminate handler
    unexpected_handler pfnUnexceptedHandlerPrev; // Previous unexpected handler
    void(__cdecl* pfnSigFPEHandlerPrev)(int);    // Previous FPE handler
    void(__cdecl* pfnSigILLHandlerPrev)(int);    // Previous SIGILL handler
    void(__cdecl* pfnSigSEGVHandlerPrev)(int);   // Previous illegal storage access handler

    ThreadExceptionHandlers()
    {
        pfnTerminateHandlerPrev = NULL;
        pfnUnexceptedHandlerPrev = NULL;
        pfnSigFPEHandlerPrev = NULL;
        pfnSigILLHandlerPrev = NULL;
        pfnSigSEGVHandlerPrev = NULL;
    }
};

// This structure describes a file item (a file included into crash report).
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

class CCrashHandler
{
    struct Locker
    {
        Locker(CCrashHandler& self) : self_(self) { self_.CrashLock(TRUE); }
        ~Locker() { self_.CrashLock(FALSE); }

    private:
        CCrashHandler& self_;
    };

    struct ReInitializer
    {
        ReInitializer(CCrashHandler& self) : self_(self) {}
        ~ReInitializer() { self_.PerCrashInit(); }

    private:
        CCrashHandler& self_;
    };

public:
    CCrashHandler();
    virtual ~CCrashHandler();

    int initialize(
        LPCWSTR szAppName,
        LPCWSTR szAppVersion,
        LPCWSTR szCrashSenderDirectory,
        LPCWSTR szServerURL,
        UINT32 uCrashHandler,
        LPCWSTR szPrivacyPolicyURL,
        LPCWSTR szDBGHelpDirectory,
        MINIDUMP_TYPE uMinidumpType,
        LPCWSTR szOutputDirectory);

    int uninitialize();
    BOOL isInitialized();

    int SetCrashCallback(PFN_CRASH_CALLBACK pfnCallback, LPVOID pUserParam);
    int AddFile(LPCTSTR lpFile, LPCTSTR lpDestFile, LPCTSTR lpDesc, DWORD dwFlags);
    int AddProperty(const CString& sPropName, const CString& sPropValue);
    int GenerateErrorReport(CR_EXCEPTION_INFO* pExceptionInfo = NULL);

    int setupProcessExceptionHandlers(UINT32 uCrashHandlers);
    int tearDownProcessExceptionHandlers();

    static CCrashHandler* instance();
    static LONG WINAPI SEHHandler(PEXCEPTION_POINTERS pExceptionPtrs);
    static DWORD WINAPI StackOverflowThreadFunction(LPVOID threadParameter);
    static void __cdecl TerminateHandler();
    static void __cdecl UnexpectedHandler();

#if _MSC_VER>=1300
    static void __cdecl PureCallHandler();
#endif

#if _MSC_VER>=1300 && _MSC_VER<1400
    static void __cdecl SecurityHandler(int code, void* x);
#endif

#if _MSC_VER>=1400
    static void __cdecl InvalidParameterHandler(const wchar_t* expression,
        const wchar_t* funcName, const wchar_t* file, unsigned int line, uintptr_t pReserved);
#endif

#if _MSC_VER>=1300
    static int __cdecl NewHandler(size_t);
#endif

    static void SigabrtHandler(int);
    static void SigfpeHandler(int, int subcode);
    static void SigintHandler(int);
    static void SigillHandler(int);
    static void SigsegvHandler(int);
    static void SigtermHandler(int);
    void GetExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers);
    CRASH_DESCRIPTION* PackCrashInfoIntoSharedMem(__in CSharedMem* pSharedMem, BOOL bTempMem);
    DWORD PackString(const CString& str);
    DWORD PackFileItem(FileItem& fi);
    DWORD PackProperty(const CString& sName, const CString& sValue);
    int LaunchCrashSender(LPCTSTR szCmdLineParams, BOOL bWait, HANDLE* phProcess);
    BOOL IsSenderProcessAlive();
    void InitPrevExceptionHandlerPointers();
    int PerCrashInit();
    void Repack();
    void CrashLock(BOOL bLock);
    int CallBack(int nStage, CR_EXCEPTION_INFO* pExInfo);
    static CCrashHandler* m_pInstance;
    LPTOP_LEVEL_EXCEPTION_FILTER  m_hOldSEH;

#if _MSC_VER>=1300
    _purecall_handler m_hOldPure;
    _PNH m_hOldOperatorNew;
#endif

#if _MSC_VER>=1400
    _invalid_parameter_handler m_hOldInvalidParam;
#endif

#if _MSC_VER>=1300 && _MSC_VER<1400
    _secerr_handler_func m_hOldSecurity;
#endif

    void(__cdecl* m_hOldSIGABRT)(int);
    void(__cdecl* m_hOldSIGINT)(int);
    void(__cdecl* m_hOldSIGTERM)(int);

    std::map<DWORD, ThreadExceptionHandlers> m_ThreadExceptionHandlers;
    CCritSec m_csThreadExceptionHandlers;

    BOOL m_bInitialized;           // Flag telling if this object was initialized.
    CString m_szAppName;            // Application name.
    CString m_szAppVersion;         // Application version.
    CString m_sCrashGUID;          // Crash GUID.
    CString m_szImageName;          // Process image name.
    MINIDUMP_TYPE m_uMinidumpType;  // Minidump type.
    CString m_szServerURL;                // Url to use when sending error report over HTTP.
    CString m_sPrivacyPolicyURL;   // Privacy policy URL.
    CString m_szCrashSenderDirectory;  // Path to CrashSender.exe
    CString m_szLangFileName;       // Language file.
    CString m_szDBGHelpDirectory; // Path to dbghelp.dll.
    CString m_szOutputDirectory; // Path to the folder where to save error reports.
    std::map<CString, FileItem> m_files; // File items to include.
    std::map<CString, CString> m_props;  // User-defined properties to include.
    CCritSec m_csCrashLock;        // Critical section used to synchronize thread access to this object.
    HANDLE m_hEvent;               // Event used to synchronize CrashRpt.dll with CrashSender.exe.
    HANDLE m_hEvent2;              // Another event used to synchronize CrashRpt.dll with CrashSender.exe.
    CSharedMem m_SharedMem;        // Shared memory.
    CRASH_DESCRIPTION* m_pCrashDesc; // Pointer to crash description shared mem view.
    CSharedMem* m_pTmpSharedMem;   // Used temporarily
    CRASH_DESCRIPTION* m_pTmpCrashDesc; // Used temporarily
    HANDLE m_hSenderProcess;       // Handle to CrashSender.exe process.
    PFN_CRASH_CALLBACK m_pfnCallback2W; // Client crash callback.
    LPVOID m_pCallbackParam;       // User-specified argument for callback function.
    CString m_sErrorReportDir; // Error report directory name (wide-char).
    int m_nCallbackRetCode;         // Return code of the callback function.
    BOOL m_bContinueExecution;      // Whether to terminate process (the default) or to continue execution after crash.
    BOOL m_bContinueExecutionNow;   // After GenerateErrorReport() m_bContinueExecution is reset to FALSE. This is the current value
};
