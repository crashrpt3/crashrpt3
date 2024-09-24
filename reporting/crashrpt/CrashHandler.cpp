/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: CrashHandler.cpp
// Description: Exception handling and report generation functionality.
// Authors: mikecarruth, zexspectrum
// Date:

#include "stdafx.h"
#include "CrashHandler.h"
#include "Utility.h"
#include "resource.h"
#include "strconv.h"

#ifndef _AddressOfReturnAddress

// Taken from: http://msdn.microsoft.com/en-us/library/s975zw7k(VS.71).aspx
#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

// _ReturnAddress and _AddressOfReturnAddress should be prototyped before use
EXTERNC void* _AddressOfReturnAddress(void);
EXTERNC void* _ReturnAddress(void);

#endif

extern HANDLE g_hModuleCrashRpt;
CCrashHandler* CCrashHandler::m_pProcessCrashHandler = NULL;

CCrashHandler::CCrashHandler()
{
    // Init member variables to their defaults
    m_bInitialized = FALSE;
    m_MinidumpType = MiniDumpNormal;
    m_hEvent = NULL;
    m_hEvent2 = NULL;
    m_pCrashDesc = NULL;
    m_hSenderProcess = NULL;
    m_pfnCallback2W = NULL;
    m_pCallbackParam = NULL;
    m_nCallbackRetCode = CR_CB_NOTIFY_NEXT_STAGE;
    m_bContinueExecution = TRUE;

    // Init exception handler pointers
    InitPrevExceptionHandlerPointers();
}

CCrashHandler::~CCrashHandler()
{
    // Clean up
    Destroy();
}

int CCrashHandler::Init(
    LPCTSTR lpcszAppName,
    LPCTSTR lpcszAppVersion,
    LPCTSTR lpcszCrashSenderPath,
    LPCTSTR lpcszUrl,
    UINT32 uCrashHandlers,
    LPCTSTR lpcszPrivacyPolicyURL,
    LPCTSTR lpcszDebugHelpDLLPath,
    MINIDUMP_TYPE MiniDumpType,
    LPCTSTR lpcszErrorReportSaveDir)
{
    // This method initializes configuration parameters,
    // creates shared memory buffer and saves the configuration parameters there,
    // installs process-wide exception handlers.

    crSetErrorMsg(_T("Unspecified error."));

    // Save minidump type
    m_MinidumpType = MiniDumpType;

    // Save application name
    m_sAppName = lpcszAppName;
    if (m_sAppName.IsEmpty())
    {
        m_sAppName = Utility::getAppName();
    }

    // Save app version
    m_sAppVersion = lpcszAppVersion;

    // If no app version provided, use the default (EXE product version)
    if (m_sAppVersion.IsEmpty())
    {
        // Get EXE image name
        CString sImageName = Utility::GetModuleName(NULL);
        m_sAppVersion = Utility::GetProductVersion(sImageName);
        if (m_sAppVersion.IsEmpty())
        {
            // If product version missing, return error.
            crSetErrorMsg(_T("Application version is not specified."));
            return 1;
        }
    }

    // Get process image name
    m_sImageName = Utility::GetModuleName(NULL);

    // Save URL to send reports via HTTP/HTTPS
    if (lpcszUrl != NULL)
    {
        m_sUrl = CString(lpcszUrl);
    }

    // Save privacy policy URL (if exists)
    if (lpcszPrivacyPolicyURL != NULL)
        m_sPrivacyPolicyURL = lpcszPrivacyPolicyURL;

    // Get the name of CrashRpt DLL
    LPTSTR pszCrashRptModule = NULL;
    CString sCrashRptModule;
#ifndef CRASHRPT_LIB
#ifdef _DEBUG
    sCrashRptModule.Format(_T("CrashRpt%dd.dll"), CRASHRPT_VER);
    pszCrashRptModule = sCrashRptModule.GetBuffer(0);
#else
    sCrashRptModule.Format(_T("CrashRpt%d.dll"), CRASHRPT_VER);
    pszCrashRptModule = sCrashRptModule.GetBuffer(0);
#endif //_DEBUG
#else //!CRASHRPT_LIB
    pszCrashRptModule = NULL;
#endif

    // Save path to CrashSender.exe
    if (lpcszCrashSenderPath == NULL)
    {
        // By default assume that CrashSender.exe is located in the same dir as CrashRpt.dll
        m_sPathToCrashSender = Utility::GetModulePath((HMODULE)g_hModuleCrashRpt);
    }
    else
    {
        // Save user-specified path
        m_sPathToCrashSender = CString(lpcszCrashSenderPath);
    }

    // Get CrashSender EXE name
    CString sCrashSenderName;

#ifdef _DEBUG
    sCrashSenderName.Format(_T("CrashSender%dd.exe"), CRASHRPT_VER);
#else
    sCrashSenderName.Format(_T("CrashSender%d.exe"), CRASHRPT_VER);
#endif //_DEBUG

    // Check that CrashSender.exe file exists
    if (m_sPathToCrashSender.Right(1) != '\\')
        m_sPathToCrashSender += "\\";

    HANDLE hFile = CreateFile(m_sPathToCrashSender + sCrashSenderName, FILE_GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        crSetErrorMsg(_T("CrashSender.exe is not found in the specified path."));
        return 1;
    }
    else
    {
        CloseHandle(hFile);
    }

    m_sLangFileName = m_sPathToCrashSender + _T("crashrpt_lang.ini");
    m_sPathToCrashSender += sCrashSenderName;

    // Check if lang file exists on disk
    CString sLangFileVer = Utility::GetINIString(m_sLangFileName, _T("Settings"), _T("CrashRptVersion"));
    int lang_file_ver = _ttoi(sLangFileVer);
    if (lang_file_ver != CRASHRPT_VER)
    {
        crSetErrorMsg(_T("Missing language file or wrong language file version."));
        return 1; // Language INI file has wrong version!
    }

    // If path to dbghelp.dll not provided, use the default one
    if (lpcszDebugHelpDLLPath == NULL)
    {
        // By default assume that debughlp.dll is located in the same dir as CrashRpt.dll
        m_sPathToDebugHelpDll = Utility::GetModulePath((HMODULE)g_hModuleCrashRpt);
    }
    else
    {
        m_sPathToDebugHelpDll = CString(lpcszDebugHelpDLLPath);
    }

    CString sDebugHelpDLL_name = "dbghelp.dll";

    if (m_sPathToDebugHelpDll.Right(1) != '\\')
        m_sPathToDebugHelpDll += "\\";

    // Load dbghelp.dll library
    HANDLE hDbgHelpDll = LoadLibrary(m_sPathToDebugHelpDll + sDebugHelpDLL_name);
    // and check result
    if (!hDbgHelpDll)
    {
        //try again ... fallback to dbghelp.dll in path
        m_sPathToDebugHelpDll = _T("");
        hDbgHelpDll = LoadLibrary(sDebugHelpDLL_name);
        if (!hDbgHelpDll)
        {
            crSetErrorMsg(_T("Couldn't load dbghelp.dll."));
            return 1;
        }
    }

    m_sPathToDebugHelpDll += sDebugHelpDLL_name;

    if (hDbgHelpDll != NULL)
    {
        FreeLibrary((HMODULE)hDbgHelpDll);
        hDbgHelpDll = NULL;
    }

    // Determine the directory where to save error reports
    if (lpcszErrorReportSaveDir == NULL)
    {
        // Create %LOCAL_APPDATA%\CrashRpt\UnsentCrashReports\AppName_AppVer folder.
        CString sLocalAppDataFolder;
        DWORD dwCSIDL = CSIDL_LOCAL_APPDATA;
        Utility::GetSpecialFolder(dwCSIDL, sLocalAppDataFolder);
        m_sUnsentCrashReportsFolder.Format(_T("%s\\CrashRpt\\UnsentCrashReports\\%s_%s"),
            (LPCTSTR)sLocalAppDataFolder, (LPCTSTR)m_sAppName, (LPCTSTR)m_sAppVersion);
    }
    else
    {
        m_sUnsentCrashReportsFolder = lpcszErrorReportSaveDir;
    }

    BOOL bCreateDir = Utility::CreateFolder(m_sUnsentCrashReportsFolder);
    if (!bCreateDir)
    {
        ATLASSERT(0);
        crSetErrorMsg(_T("Couldn't create crash report directory."));
        return 1;
    }

    // Create directory where we will store recent crash logs
    CString sLogDir = m_sUnsentCrashReportsFolder + _T("\\Logs");
    bCreateDir = Utility::CreateFolder(sLogDir);
    if (!bCreateDir)
    {
        ATLASSERT(0);
        crSetErrorMsg(_T("Couldn't create logs directory."));
        return 1;
    }

    // Init some fields that should be reinitialized before each new crash.
    if (0 != PerCrashInit())
        return 1;

    // Associate this handler object with the caller process.
    m_pProcessCrashHandler = this;

    // Set exception handlers with initial values (NULLs)
    InitPrevExceptionHandlerPointers();

    // Set exception handlers that work on per-process basis
    int nSetProcessHandlers = SetProcessExceptionHandlers(uCrashHandlers);
    if (nSetProcessHandlers != 0)
    {
        ATLASSERT(nSetProcessHandlers == 0);
        crSetErrorMsg(_T("Couldn't set C++ exception handlers for current process."));
        return 1;
    }

    // Set exception handlers that work on per-thread basis
    int nSetThreadHandlers = SetThreadExceptionHandlers(uCrashHandlers);
    if (nSetThreadHandlers != 0)
    {
        ATLASSERT(nSetThreadHandlers == 0);
        crSetErrorMsg(_T("Couldn't set C++ exception handlers for main execution thread."));
        return 1;
    }

    // The following code is intended to fix the issue with 32-bit applications in 64-bit environment.
    // http://support.microsoft.com/kb/976038/en-us
    // http://code.google.com/p/crashrpt/issues/detail?id=104

    typedef BOOL(WINAPI* SETPROCESSUSERMODEEXCEPTIONPOLICY)(DWORD dwFlags);
    typedef BOOL(WINAPI* GETPROCESSUSERMODEEXCEPTIONPOLICY)(LPDWORD lpFlags);
#define PROCESS_CALLBACK_FILTER_ENABLED     0x1

    HMODULE hKernel32 = LoadLibrary(_T("kernel32.dll"));
    if (hKernel32 != NULL)
    {
        SETPROCESSUSERMODEEXCEPTIONPOLICY pfnSetProcessUserModeExceptionPolicy =
            (SETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "SetProcessUserModeExceptionPolicy");
        GETPROCESSUSERMODEEXCEPTIONPOLICY pfnGetProcessUserModeExceptionPolicy =
            (GETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "GetProcessUserModeExceptionPolicy");

        if (pfnSetProcessUserModeExceptionPolicy != NULL &&
            pfnGetProcessUserModeExceptionPolicy != NULL)
        {
            DWORD _dwFlags = 0;
            if (pfnGetProcessUserModeExceptionPolicy(&_dwFlags))
            {
                pfnSetProcessUserModeExceptionPolicy(_dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }

        FreeLibrary(hKernel32);
    }

    // Initialization OK.
    m_bInitialized = TRUE;
    crSetErrorMsg(_T("Success."));
    return 0;
}

int CCrashHandler::SetCrashCallbackW(PFN_CRASH_CALLBACK pfnCallback, LPVOID pUserParam)
{
    m_pfnCallback2W = pfnCallback;
    m_pCallbackParam = pUserParam;

    return 0;
}

// Packs config info to shared mem.
CRASH_DESCRIPTION* CCrashHandler::PackCrashInfoIntoSharedMem(CSharedMem* pSharedMem, BOOL bTempMem)
{
    m_pTmpSharedMem = pSharedMem;

    CString sSharedMemName;
    if (bTempMem)
        sSharedMemName.Format(_T("%s-tmp"), (LPCTSTR)m_sCrashGUID);
    else
        sSharedMemName = m_sCrashGUID;

    if (!pSharedMem->IsInitialized())
    {
        // Initialize shared memory.
        BOOL bSharedMem = pSharedMem->Init(sSharedMemName, FALSE, SHARED_MEM_MAX_SIZE);
        if (!bSharedMem)
        {
            ATLASSERT(0);
            crSetErrorMsg(_T("Couldn't initialize shared memory."));
            return NULL;
        }
    }

    // Create memory view.
    m_pTmpCrashDesc =
        (CRASH_DESCRIPTION*)pSharedMem->CreateView(0, sizeof(CRASH_DESCRIPTION));
    if (m_pTmpCrashDesc == NULL)
    {
        ATLASSERT(0);
        crSetErrorMsg(_T("Couldn't create shared memory view."));
        return NULL;
    }

    // Pack config information to shared memory
    memset(m_pTmpCrashDesc, 0, sizeof(CRASH_DESCRIPTION));
    memcpy(m_pTmpCrashDesc->m_uchMagic, "CRD", 3);
    m_pTmpCrashDesc->m_wSize = sizeof(CRASH_DESCRIPTION);
    m_pTmpCrashDesc->m_dwTotalSize = sizeof(CRASH_DESCRIPTION);
    m_pTmpCrashDesc->m_dwCrashRptVer = CRASHRPT_VER;
    m_pTmpCrashDesc->m_MinidumpType = m_MinidumpType;
    m_pTmpCrashDesc->m_dwProcessId = GetCurrentProcessId();
    m_pTmpCrashDesc->m_bClientAppCrashed = FALSE;

    m_pTmpCrashDesc->m_dwAppNameOffs = PackString(m_sAppName);
    m_pTmpCrashDesc->m_dwAppVersionOffs = PackString(m_sAppVersion);
    m_pTmpCrashDesc->m_dwCrashGUIDOffs = PackString(m_sCrashGUID);
    m_pTmpCrashDesc->m_dwImageNameOffs = PackString(m_sImageName);
    m_pTmpCrashDesc->m_dwLangFileNameOffs = PackString(m_sLangFileName);
    m_pTmpCrashDesc->m_dwPathToDebugHelpDllOffs = PackString(m_sPathToDebugHelpDll);
    m_pTmpCrashDesc->m_dwPrivacyPolicyURLOffs = PackString(m_sPrivacyPolicyURL);
    m_pTmpCrashDesc->m_dwUnsentCrashReportsFolderOffs = PackString(m_sUnsentCrashReportsFolder);
    m_pTmpCrashDesc->m_dwUrlOffs = PackString(m_sUrl);

    // Pack file items
    std::map<CString, FileItem>::iterator fit;
    for (fit = m_files.begin(); fit != m_files.end(); fit++)
    {
        FileItem& fi = fit->second;

        // Pack this file item into shared mem.
        PackFileItem(fi);
    }

    // Pack custom props
    std::map<CString, CString>::iterator pit;
    for (pit = m_props.begin(); pit != m_props.end(); pit++)
    {
        // Pack this prop into shared mem.
        PackProperty(pit->first, pit->second);
    }

    return m_pTmpCrashDesc;
}

// Packs a string to shared memory
DWORD CCrashHandler::PackString(CString str)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    int nStrLen = str.GetLength() * sizeof(TCHAR);
    WORD wLength = (WORD)(sizeof(STRING_DESC) + nStrLen);

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    STRING_DESC* pStrDesc = (STRING_DESC*)pView;
    memcpy(pStrDesc->m_uchMagic, "STR", 3);
    pStrDesc->m_wSize = wLength;
    memcpy(pView + sizeof(STRING_DESC), str.GetBuffer(0), nStrLen);

    m_pTmpCrashDesc->m_dwTotalSize += wLength;

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Packs file item to shared memory
DWORD CCrashHandler::PackFileItem(FileItem& fi)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    WORD wLength = sizeof(FILE_ITEM);
    m_pTmpCrashDesc->m_dwTotalSize += wLength;
    m_pTmpCrashDesc->m_uFileItems++;

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    FILE_ITEM* pFileItem = (FILE_ITEM*)pView;

    memcpy(pFileItem->m_uchMagic, "FIL", 3);
    pFileItem->m_dwSrcFilePathOffs = PackString(fi.srcFilePath);
    pFileItem->m_dwDstFileNameOffs = PackString(fi.dstFileName);
    pFileItem->m_dwDescriptionOffs = PackString(fi.description);
    pFileItem->m_bMakeCopy = fi.isMakeCopy;
    pFileItem->m_bAllowDelete = fi.isAllowDelete;
    pFileItem->m_wSize = (WORD)(m_pTmpCrashDesc->m_dwTotalSize - dwTotalSize);

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Packs custom property to shared memory
DWORD CCrashHandler::PackProperty(CString sName, CString sValue)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    WORD wLength = sizeof(CUSTOM_PROP);
    m_pTmpCrashDesc->m_dwTotalSize += wLength;
    m_pTmpCrashDesc->m_uCustomProps++;

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    CUSTOM_PROP* pProp = (CUSTOM_PROP*)pView;

    memcpy(pProp->m_uchMagic, "CPR", 3);
    pProp->m_dwNameOffs = PackString(sName);
    pProp->m_dwValueOffs = PackString(sValue);
    pProp->m_wSize = (WORD)(m_pTmpCrashDesc->m_dwTotalSize - dwTotalSize);

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Returns TRUE if initialized, otherwise FALSE
BOOL CCrashHandler::IsInitialized()
{
    return m_bInitialized;
}

// Destroys the object
int CCrashHandler::Destroy()
{
    crSetErrorMsg(_T("Unspecified error."));

    if (!m_bInitialized)
    {
        crSetErrorMsg(_T("Can't destroy not initialized crash handler."));
        return 1;
    }

    // Free handle to CrashSender.exe process.
    if (m_hSenderProcess != NULL)
        CloseHandle(m_hSenderProcess);

    // Free events
    if (m_hEvent)
    {
        CloseHandle(m_hEvent);
        m_hEvent = NULL;
    }

    if (m_hEvent2)
    {
        CloseHandle(m_hEvent2);
        m_hEvent2 = NULL;
    }

    // Reset SEH exception filter
    if (m_hOldSEH)
    {
        ::SetUnhandledExceptionFilter(m_hOldSEH);
        m_hOldSEH = NULL;
    }

    // All installed per-thread C++ exception handlers should be uninstalled
    // using crUninstallFromCurrentThread() before calling Destroy()

    {
        CAutoLock lock(&m_csThreadExceptionHandlers);
        ATLASSERT(m_ThreadExceptionHandlers.size() == 0);
    }

    //
    m_pProcessCrashHandler = NULL;

    // OK.
    m_bInitialized = FALSE;
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Sets internal pointers to previously used exception handlers to NULL
void CCrashHandler::InitPrevExceptionHandlerPointers()
{
    m_hOldSEH = NULL;

#if _MSC_VER>=1300
    m_hOldPure = NULL;
    m_hOldOperatorNew = NULL;
#endif

#if _MSC_VER>=1300 && _MSC_VER<1400
    m_hOldSecurity = NULL;
#endif

#if _MSC_VER>=1400
    m_hOldInvalidParam = NULL;
#endif

    m_hOldSIGABRT = NULL;
    m_hOldSIGINT = NULL;
    m_hOldSIGTERM = NULL;
}

// Returns singleton of the crash handler
CCrashHandler* CCrashHandler::GetCurrentProcessCrashHandler()
{
    return m_pProcessCrashHandler;
}

// Releases the crash handler pointer
void CCrashHandler::ReleaseCurrentProcessCrashHandler()
{
    if (m_pProcessCrashHandler != NULL)
    {
        delete m_pProcessCrashHandler;
        m_pProcessCrashHandler = NULL;
    }
}

// Sets exception handlers that work on per-process basis
int CCrashHandler::SetProcessExceptionHandlers(UINT32 uCrashHandlers)
{
    crSetErrorMsg(_T("Unspecified error."));

    // If 0 is specified as dwFlags, assume all handlers should be
    // installed
    if ((uCrashHandlers & CR_CRASH_HANDLER_ALL) == 0)
    {
        uCrashHandlers |= CR_CRASH_HANDLER_ALL;
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_SEH)
    {
        m_hOldSEH = ::SetUnhandledExceptionFilter(SEHHandler);
    }

    _set_error_mode(_OUT_TO_STDERR);

#if _MSC_VER>=1300
    if (uCrashHandlers & CR_CRASH_HANDLER_CPP_PURE)
    {
        // Catch pure virtual function calls.
        // Because there is one _purecall_handler for the whole process,
        // calling this function immediately impacts all threads. The last
        // caller on any thread sets the handler.
        // http://msdn.microsoft.com/en-us/library/t296ys27.aspx
        m_hOldPure = _set_purecall_handler(PureCallHandler);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_NEW_OPERATOR)
    {
        // Catch new operator memory allocation exceptions
        _set_new_mode(1); // Force malloc() to call new handler too
        m_hOldOperatorNew = _set_new_handler(NewHandler);
    }
#endif // _MSC_VER>=1300

#if _MSC_VER>=1400
    if (uCrashHandlers & CR_CRASH_HANDLER_INVALID_PARAMETER)
    {
        // Catch invalid parameter exceptions.
        m_hOldInvalidParam = _set_invalid_parameter_handler(InvalidParameterHandler);
    }
#endif // _MSC_VER>=1400

#if _MSC_VER>=1300 && _MSC_VER<1400
    if (uCrashHandlers & CR_CRASH_HANDLER_SECURITY)
    {
        // Catch buffer overrun exceptions
        // The _set_security_error_handler is deprecated in VC8 C++ run time library
        m_hOldSecurity = _set_security_error_handler(SecurityHandler);
    }
#endif // _MSC_VER>=1300 && _MSC_VER<1400

    // Set up C++ signal handlers
    if (uCrashHandlers & CR_CRASH_HANDLER_SIGABRT)
    {
#if _MSC_VER>=1400
        _set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);
#endif
        // Catch an abnormal program termination
        m_hOldSIGABRT = signal(SIGABRT, SigabrtHandler);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_SIGINT)
    {
        // Catch illegal instruction handler
        m_hOldSIGINT = signal(SIGINT, SigintHandler);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_TERMINATE_CALL)
    {
        // Catch a termination request
        m_hOldSIGTERM = signal(SIGTERM, SigtermHandler);
    }

    crSetErrorMsg(_T("Success."));
    return 0;
}

// Unsets exception pointers that work on per-process basis
int CCrashHandler::UnSetProcessExceptionHandlers()
{
    crSetErrorMsg(_T("Unspecified error."));

    // Unset all previously set handlers

#if _MSC_VER>=1300
    if (m_hOldPure != NULL)
        _set_purecall_handler(m_hOldPure);

    if (m_hOldOperatorNew != NULL)
        _set_new_handler(m_hOldOperatorNew);
#endif

#if _MSC_VER>=1400
    if (m_hOldInvalidParam != NULL)
        _set_invalid_parameter_handler(m_hOldInvalidParam);
#endif //_MSC_VER>=1400

#if _MSC_VER>=1300 && _MSC_VER<1400
    if (m_hOldSecurity != NULL)
        _set_security_error_handler(m_hOldSecurity);
#endif //_MSC_VER<1400

    if (m_hOldSIGABRT != NULL)
        signal(SIGABRT, m_hOldSIGABRT);

    if (m_hOldSIGINT != NULL)
        signal(SIGINT, m_hOldSIGINT);

    if (m_hOldSIGTERM != NULL)
        signal(SIGTERM, m_hOldSIGTERM);

    crSetErrorMsg(_T("Success."));
    return 0;
}

// Installs C++ exception handlers that function on per-thread basis
int CCrashHandler::SetThreadExceptionHandlers(DWORD dwFlags)
{
    crSetErrorMsg(_T("Unspecified error."));

    // If 0 is specified as dwFlags, assume all available exception handlers should be
    // installed
    if ((dwFlags & CR_CRASH_HANDLER_ALL) == 0)
        dwFlags |= CR_CRASH_HANDLER_ALL;

    // Get current thread ID.
    DWORD dwThreadId = GetCurrentThreadId();

    // Lock the critical section.
    CAutoLock lock(&m_csThreadExceptionHandlers);

    // Try and find our thread ID in the list of threads.
    std::map<DWORD, ThreadExceptionHandlers>::iterator it =
        m_ThreadExceptionHandlers.find(dwThreadId);

    if (it != m_ThreadExceptionHandlers.end())
    {
        // handlers are already set for the thread
        crSetErrorMsg(_T("Can't install handlers for current thread twice."));
        return 1; // failed
    }

    ThreadExceptionHandlers handlers;

    if (dwFlags & CR_CRASH_HANDLER_TERMINATE_CALL)
    {
        // Catch terminate() calls.
        // In a multithreaded environment, terminate functions are maintained
        // separately for each thread. Each new thread needs to install its own
        // terminate function. Thus, each thread is in charge of its own termination handling.
        // http://msdn.microsoft.com/en-us/library/t6fk7h29.aspx
        handlers.pfnTerminateHandlerPrev = set_terminate(TerminateHandler);
    }

    if (dwFlags & CR_CRASH_HANDLER_UNEXPECTED_CALL)
    {
        // Catch unexpected() calls.
        // In a multithreaded environment, unexpected functions are maintained
        // separately for each thread. Each new thread needs to install its own
        // unexpected function. Thus, each thread is in charge of its own unexpected handling.
        // http://msdn.microsoft.com/en-us/library/h46t5b69.aspx
        handlers.pfnUnexceptedHandlerPrev = set_unexpected(UnexpectedHandler);
    }

    if (dwFlags & CR_CRASH_HANDLER_SIGFPE)
    {
        // Catch a floating point error
        typedef void (*sigh)(int);
        handlers.pfnSigFPEHandlerPrev = signal(SIGFPE, (sigh)SigfpeHandler);
    }


    if (dwFlags & CR_CRASH_HANDLER_SIGILL)
    {
        // Catch an illegal instruction
        handlers.pfnSigILLHandlerPrev = signal(SIGILL, SigillHandler);
    }

    if (dwFlags & CR_CRASH_HANDLER_SIGSEGV)
    {
        // Catch illegal storage access errors
        handlers.pfnSigSEGVHandlerPrev = signal(SIGSEGV, SigsegvHandler);
    }

    // Insert the structure to the list of handlers
    m_ThreadExceptionHandlers[dwThreadId] = handlers;

    // OK.
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Unsets exception handlers for the current thread
int CCrashHandler::UnSetThreadExceptionHandlers()
{
    crSetErrorMsg(_T("Unspecified error."));

    DWORD dwThreadId = GetCurrentThreadId();

    CAutoLock lock(&m_csThreadExceptionHandlers);

    std::map<DWORD, ThreadExceptionHandlers>::iterator it =
        m_ThreadExceptionHandlers.find(dwThreadId);

    if (it == m_ThreadExceptionHandlers.end())
    {
        // No exception handlers were installed for the caller thread?
        crSetErrorMsg(_T("Crash handler wasn't previously installed for current thread."));
        return 1;
    }

    ThreadExceptionHandlers* handlers = &(it->second);

    if (handlers->pfnTerminateHandlerPrev != NULL)
        set_terminate(handlers->pfnTerminateHandlerPrev);

    if (handlers->pfnUnexceptedHandlerPrev != NULL)
        set_unexpected(handlers->pfnUnexceptedHandlerPrev);

    if (handlers->pfnSigFPEHandlerPrev != NULL)
        signal(SIGFPE, handlers->pfnSigFPEHandlerPrev);

    if (handlers->pfnSigILLHandlerPrev != NULL)
        signal(SIGINT, handlers->pfnSigILLHandlerPrev);

    if (handlers->pfnSigSEGVHandlerPrev != NULL)
        signal(SIGSEGV, handlers->pfnSigSEGVHandlerPrev);

    // Remove from the list
    m_ThreadExceptionHandlers.erase(it);

    // OK.
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Adds a file item to the error report
int CCrashHandler::AddFile(LPCTSTR pszFile, LPCTSTR pszDestFile, LPCTSTR pszDesc, DWORD dwFlags)
{
    crSetErrorMsg(_T("Unspecified error."));

    // Check if source file name or search pattern is specified
    if (pszFile == NULL)
    {
        crSetErrorMsg(_T("Invalid file name specified."));
        return 1;
    }

    // Check that the destination file name is valid
    if (pszDestFile != NULL)
    {
        CString sDestFile = pszDestFile;
        sDestFile.TrimLeft();
        sDestFile.TrimRight();
        if (sDestFile.GetLength() == 0)
        {
            crSetErrorMsg(_T("Invalid destination file name specified."));
            return 1;
        }
    }

    // Check if pszFile is a search pattern or not
    BOOL bPattern = Utility::IsFileSearchPattern(pszFile);

    if (!bPattern) // Usual file name
    {
        // Make sure the file exists
        DWORD dwAttrs = GetFileAttributes(pszFile);

        BOOL bIsFile = dwAttrs != INVALID_FILE_ATTRIBUTES && (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0;

        if (!bIsFile && (dwFlags & CR_AF_MISSING_FILE_OK) == 0)
        {
            crSetErrorMsg(_T("The file does not exists or not a regular file."));
            return 1;
        }

        // Add file to file list.
        FileItem fi;
        fi.description = pszDesc;
        fi.srcFilePath = pszFile;
        fi.isMakeCopy = (dwFlags & CR_AF_MAKE_FILE_COPY) != 0;
        fi.isAllowDelete = (dwFlags & CR_AF_ALLOW_DELETE) != 0;
        if (pszDestFile != NULL)
        {
            fi.dstFileName = pszDestFile;
        }
        else
        {
            CString sDestFile = pszFile;
            int pos = -1;
            sDestFile.Replace('/', '\\');
            pos = sDestFile.ReverseFind('\\');
            if (pos != -1)
                sDestFile = sDestFile.Mid(pos + 1);

            fi.dstFileName = sDestFile;
        }

        // Check if file is already in our list
        std::map<CString, FileItem>::iterator it = m_files.find(fi.dstFileName);
        if (it != m_files.end())
        {
            crSetErrorMsg(_T("A file with such a destination name already exists."));
            return 1;
        }

        m_files[fi.dstFileName] = fi;

        // Pack this file item into shared mem.
        PackFileItem(fi);
    }
    else // Search pattern
    {
        // Add file search pattern to file list.
        FileItem fi;
        fi.description = pszDesc;
        fi.srcFilePath = pszFile;
        fi.dstFileName = Utility::GetFileName(pszFile);
        fi.isMakeCopy = (dwFlags & CR_AF_MAKE_FILE_COPY) != 0;
        fi.isAllowDelete = (dwFlags & CR_AF_ALLOW_DELETE) != 0;
        m_files[fi.dstFileName] = fi;

        // Pack this file item into shared mem.
        PackFileItem(fi);
    }

    // OK.
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Adds a custom property to the error report
int CCrashHandler::AddProperty(CString sPropName, CString sPropValue)
{
    crSetErrorMsg(_T("Unspecified error."));

    if (sPropName.IsEmpty())
    {
        crSetErrorMsg(_T("Invalid property name specified."));
        return 1;
    }

    m_props[sPropName] = sPropValue;

    PackProperty(sPropName, sPropValue);

    // OK.
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Generates error report
int CCrashHandler::GenerateErrorReport(
    PCR_EXCEPTION_INFO pExceptionInfo)
{
    ReInitializer re_initializer(*this);

    crSetErrorMsg(_T("Unspecified error."));

    // Allocate memory in stack for storing exception pointers.
    EXCEPTION_RECORD ExceptionRecord;
    CONTEXT ContextRecord;
    EXCEPTION_POINTERS ExceptionPointers;
    ExceptionPointers.ExceptionRecord = &ExceptionRecord;
    ExceptionPointers.ContextRecord = &ContextRecord;

    // Validate input parameters
    if (pExceptionInfo == NULL)
    {
        crSetErrorMsg(_T("Exception info is NULL."));
        return 1;
    }

    // Get exception pointers if they were not provided by the caller.
    if (pExceptionInfo->pExceptionPointers == NULL)
    {
        GetExceptionPointers(pExceptionInfo->dwSEHCode, &ExceptionPointers);
        pExceptionInfo->pExceptionPointers = &ExceptionPointers;
    }

    // Set "client app crashed" flag.
    m_pCrashDesc->m_bClientAppCrashed = TRUE;

    // Save current process ID, thread ID and exception pointers address to shared mem.
    m_pCrashDesc->m_dwProcessId = GetCurrentProcessId();
    m_pCrashDesc->m_dwThreadId = GetCurrentThreadId();
    m_pCrashDesc->m_pExceptionPtrs = pExceptionInfo->pExceptionPointers;
    m_pCrashDesc->m_bSendRecentReports = FALSE;
    m_pCrashDesc->m_nExceptionType = pExceptionInfo->nExceptionType;

    if (pExceptionInfo->dwSEHCode == 0)
    {
        pExceptionInfo->dwSEHCode = pExceptionInfo->pExceptionPointers->ExceptionRecord->ExceptionCode;
    }

    m_pCrashDesc->m_dwExceptionCode = pExceptionInfo->dwSEHCode;

    if (pExceptionInfo->nExceptionType == CR_TEST_CRASH_SIGFPE)
    {
        // Set FPE (floating point exception) subcode
        m_pCrashDesc->m_uFPESubcode = pExceptionInfo->uFloatPointExceptionSubcode;
    }
    else if (pExceptionInfo->nExceptionType == CR_TEST_CRASH_INVALID_PARAMETER)
    {
        // Set invalid parameter exception info fields
        m_pCrashDesc->m_dwInvParamExprOffs = PackString(pExceptionInfo->lpAssertionExpression);
        m_pCrashDesc->m_dwInvParamFunctionOffs = PackString(pExceptionInfo->lpFunction);
        m_pCrashDesc->m_dwInvParamFileOffs = PackString(pExceptionInfo->lpFile);
        m_pCrashDesc->m_uInvParamLine = pExceptionInfo->uLine;
    }

    // New-style callback
    if (CR_CB_CANCEL == CallBack(CR_CB_STAGE_PREPARE, pExceptionInfo))
    {
        // User has canceled error report generation!
        crSetErrorMsg(_T("The operation was cancelled by client."));
        return 2;
    }

    int result = LaunchCrashSender(m_sCrashGUID, TRUE, &pExceptionInfo->hSenderProcess);

    // New-style callback. Notify client about the second stage
    // (CR_CB_STAGE_FINISH) of crash report generation.
    CallBack(CR_CB_STAGE_FINISH, pExceptionInfo);

    // Check the result of launching the crash sender process.
    if (result != 0)
    {
        ATLASSERT(result == 0);
        crSetErrorMsg(_T("Error launching CrashSender.exe"));

        // Failed to launch crash sender process.
        // Try notifying user about crash using message box.
        CString szCaption;
        szCaption.Format(_T("%s has stopped working"), (LPCTSTR)Utility::getAppName());
        CString szMessage;
        szMessage.Format(_T("Error launching CrashSender.exe"));
        MessageBox(NULL, szMessage, szCaption, MB_OK | MB_ICONERROR);
        return 3;
    }

    // OK
    crSetErrorMsg(_T("Success."));
    return 0;
}

BOOL CCrashHandler::IsSenderProcessAlive()
{
    // If process handle is still accessible, check its exit code
    DWORD dwExitCode = 1;
    BOOL bRes = GetExitCodeProcess(m_hSenderProcess, &dwExitCode);
    if (!bRes || (bRes && dwExitCode != STILL_ACTIVE))
    {
        return FALSE; // Process seems to exit!
    }

    return TRUE;
}

// The following code gets exception pointers using a workaround found in CRT code.
void CCrashHandler::GetExceptionPointers(DWORD dwExceptionCode,
    EXCEPTION_POINTERS* pExceptionPointers)
{
    // The following code was taken from VC++ 8.0 CRT (invarg.c: line 104)

    CONTEXT ContextRecord;
    memset(&ContextRecord, 0, sizeof(CONTEXT));

#ifdef _X86_

    __asm {
        mov dword ptr[ContextRecord.Eax], eax
        mov dword ptr[ContextRecord.Ecx], ecx
        mov dword ptr[ContextRecord.Edx], edx
        mov dword ptr[ContextRecord.Ebx], ebx
        mov dword ptr[ContextRecord.Esi], esi
        mov dword ptr[ContextRecord.Edi], edi
        mov word ptr[ContextRecord.SegSs], ss
        mov word ptr[ContextRecord.SegCs], cs
        mov word ptr[ContextRecord.SegDs], ds
        mov word ptr[ContextRecord.SegEs], es
        mov word ptr[ContextRecord.SegFs], fs
        mov word ptr[ContextRecord.SegGs], gs
        pushfd
        pop[ContextRecord.EFlags]
    }

    ContextRecord.ContextFlags = CONTEXT_CONTROL;
#pragma warning(push)
#pragma warning(disable:4311)
    ContextRecord.Eip = (ULONG)_ReturnAddress();
    ContextRecord.Esp = (ULONG)_AddressOfReturnAddress();
#pragma warning(pop)
    ContextRecord.Ebp = *((ULONG*)_AddressOfReturnAddress() - 1);

#elif defined (_IA64_) || defined (_AMD64_)

    /* Need to fill up the Context in IA64 and AMD64. */
    RtlCaptureContext(&ContextRecord);

#else  /* defined (_IA64_) || defined (_AMD64_) */

    ZeroMemory(&ContextRecord, sizeof(ContextRecord));

#endif  /* defined (_IA64_) || defined (_AMD64_) */

    memcpy(pExceptionPointers->ContextRecord, &ContextRecord, sizeof(CONTEXT));

    ZeroMemory(pExceptionPointers->ExceptionRecord, sizeof(EXCEPTION_RECORD));

    pExceptionPointers->ExceptionRecord->ExceptionCode = dwExceptionCode;
    pExceptionPointers->ExceptionRecord->ExceptionAddress = _ReturnAddress();
}

// Launches CrashSender.exe process
int CCrashHandler::LaunchCrashSender(LPCTSTR szCmdLineParams, BOOL bWait, HANDLE* phProcess)
{
    crSetErrorMsg(_T("Unspecified error."));

    /* Create CrashSender.exe process */

    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    // Format command line
    TCHAR szCmdLine[_MAX_PATH] = _T("");
    _tcscat_s(szCmdLine, _MAX_PATH, _T("\""));
    _tcscat_s(szCmdLine, _MAX_PATH, m_sPathToCrashSender.GetBuffer(0));
    _tcscat_s(szCmdLine, _MAX_PATH, _T("\" \""));
    _tcscat_s(szCmdLine, _MAX_PATH, szCmdLineParams);
    _tcscat_s(szCmdLine, _MAX_PATH, _T("\""));

    BOOL bCreateProcess = CreateProcess(
        m_sPathToCrashSender, szCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (pi.hThread)
    {
        CloseHandle(pi.hThread);
        pi.hThread = NULL;
    }
    if (!bCreateProcess)
    {
        ATLASSERT(bCreateProcess);
        crSetErrorMsg(_T("Error creating CrashSender process."));
        return 1;
    }

    if (bWait)
    {
        /* Wait until CrashSender finishes with making screenshot,
        copying files, creating minidump. */

        WaitForSingleObject(m_hEvent, INFINITE);
    }

    // Return handle to the CrashSender.exe process.
    if (phProcess != NULL)
    {
        *phProcess = pi.hProcess;
    }
    else
    {
        // Handle not required by caller so close it.
        CloseHandle(pi.hProcess);
        pi.hProcess = NULL;
    }

    // Done
    crSetErrorMsg(_T("Success."));
    return 0;
}

// Acquires the crash lock. Other threads that may crash while we are
// inside of a crash handler function, will wait until we unlock.
void CCrashHandler::CrashLock(BOOL bLock)
{
    if (bLock)
        m_csCrashLock.Lock();
    else
        m_csCrashLock.Unlock();
}

int CCrashHandler::PerCrashInit()
{
    // This method performs per-crash initialization actions.
    // For example, we have to generate a new GUID string and repack
    // configuration info into shared memory each time.

    // Consider the next crash as non-critical.
    m_bContinueExecutionNow = m_bContinueExecution;
    m_bContinueExecution = TRUE;

    // Set default ret code for callback func.
    m_nCallbackRetCode = CR_CB_NOTIFY_NEXT_STAGE;

    // Generate new GUID for new crash report
    // (if, for example, user will generate new error report manually).
    Utility::GenerateGUID(m_sCrashGUID);

    // Recreate the event that will be used to synchronize with CrashSender.exe process.
    if (m_hEvent != NULL)
        CloseHandle(m_hEvent); // Free old event
    CString sEventName;
    sEventName.Format(_T("Local\\CrashRptEvent_%s"), (LPCTSTR)m_sCrashGUID);
    m_hEvent = CreateEvent(NULL, FALSE, FALSE, sEventName);

    // Format error report dir name for the next crash report.
    CString sErrorReportDirName;
    sErrorReportDirName.Format(_T("%s\\%s"),
        m_sUnsentCrashReportsFolder.GetBuffer(0),
        m_sCrashGUID.GetBuffer(0)
    );

    // Convert it to wide-char and multi-byte charsets.
    strconv_t strconv;
    m_sErrorReportDirW = strconv.t2w(sErrorReportDirName);
    m_sErrorReportDirA = strconv.t2a(sErrorReportDirName);

    // Reset shared memory
    if (m_SharedMem.IsInitialized())
    {
        m_SharedMem.Destroy();
        m_pCrashDesc = NULL;
    }

    Repack();

    // OK
    return 0;
}

void CCrashHandler::Repack()
{
    // Pack configuration info into shared memory.
    // It will be passed to CrashSender.exe later.
    m_pCrashDesc = PackCrashInfoIntoSharedMem(&m_SharedMem, FALSE);
}

int CCrashHandler::CallBack(int nStage, CR_EXCEPTION_INFO* pExInfo)
{
    // This method calls the new-style crash callback function.
    // The client (callee) is able to either permit crash report generation (return CR_CB_DODEFAULT)
    // or prevent it (return CR_CB_CANCEL).

    strconv_t strconv;

    if (m_nCallbackRetCode != CR_CB_NOTIFY_NEXT_STAGE)
        return CR_CB_DODEFAULT;

    if (m_pfnCallback2W != NULL)
    {
        // Call the wide-char version of the callback function.

        // Prepare callback info structure
        CR_CRASH_CALLBACK_INFO cci;
        memset(&cci, 0, sizeof(CR_CRASH_CALLBACK_INFO));
        cci.cb = sizeof(CR_CRASH_CALLBACK_INFO);
        cci.nStage = nStage;
        cci.pExceptionInfo = pExInfo;
        cci.pUserParam = m_pCallbackParam;
        cci.pszErrorReportFolder = m_sErrorReportDirW.c_str();
        cci.bContinueExecution = m_bContinueExecution;

        // Call the function and get the ret code
        m_nCallbackRetCode = m_pfnCallback2W(&cci);

        // Save continue execution flag
        m_bContinueExecution = cci.bContinueExecution;
    }

    return m_nCallbackRetCode;
}

LONG WINAPI CCrashHandler::SEHHandler(PEXCEPTION_POINTERS pExceptionPtrs)
{
    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    // Handle stack overflow in a separate thread.
    // Vojtech: Based on martin.bis...@gmail.com comment in
    //	http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
    if (pExceptionPtrs != 0 && pExceptionPtrs->ExceptionRecord != 0 &&
        pExceptionPtrs->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
    {
        // Special case to handle the stack overflow exception.
        // The dump will be realized from another thread.
        // Create another thread that will do the dump.
        HANDLE thread = ::CreateThread(0, 0,
            &StackOverflowThreadFunction, pExceptionPtrs, 0, 0);
        ::WaitForSingleObject(thread, INFINITE);
        ::CloseHandle(thread);
        // Terminate process
        TerminateProcess(GetCurrentProcess(), 1);
    }

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Generate error report.
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SEH;
        ei.pExceptionPointers = pExceptionPtrs;
        ei.dwSEHCode = pExceptionPtrs->ExceptionRecord->ExceptionCode;
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }

    // NOTE: Not unreachable. Not clear what m_bContinueExecution should mean here, though
    return EXCEPTION_EXECUTE_HANDLER;
}

//Vojtech: Based on martin.bis...@gmail.com comment in
// http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
// Thread procedure doing the dump for stack overflow.
DWORD WINAPI CCrashHandler::StackOverflowThreadFunction(LPVOID lpParameter)
{
    PEXCEPTION_POINTERS pExceptionPtrs =
        reinterpret_cast<PEXCEPTION_POINTERS>(lpParameter);

    CCrashHandler* pCrashHandler =
        CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we	are inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Generate error report.
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SEH;
        ei.pExceptionPointers = pExceptionPtrs;
        ei.dwSEHCode = pExceptionPtrs->ExceptionRecord->ExceptionCode;
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }

    return 0;
}

// CRT terminate() call handler
void __cdecl CCrashHandler::TerminateHandler()
{
    // Abnormal program termination (terminate() function was called)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_TERMINATE_CALL;

        // Generate crash report
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT unexpected() call handler
void __cdecl CCrashHandler::UnexpectedHandler()
{
    // Unexpected error (unexpected() function was called)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_UNEXPECTED_CALL;

        // Generate crash report
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT Pure virtual method call handler
#if _MSC_VER>=1300
void __cdecl CCrashHandler::PureCallHandler()
{
    // Pure virtual function call

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_CPP_PURE;

        // Generate error report.
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}
#endif

// CRT buffer overrun handler. Available in CRT 7.1 only
#if _MSC_VER>=1300 && _MSC_VER<1400
void __cdecl CCrashHandler::SecurityHandler(int code, void* x)
{
    // Security error (buffer overrun).

    code;
    x;

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SECURITY;

        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}
#endif

// CRT invalid parameter handler
#if _MSC_VER>=1400
void __cdecl CCrashHandler::InvalidParameterHandler(
    const wchar_t* expression,
    const wchar_t* funcName,
    const wchar_t* file,
    unsigned int line,
    uintptr_t pReserved)
{
    pReserved;

    // Invalid parameter exception

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_INVALID_PARAMETER;
        ei.lpAssertionExpression = expression;
        ei.lpFunction = funcName;
        ei.lpFile = file;
        ei.uLine = line;

        // Generate error report.
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}
#endif

// CRT new operator fault handler
#if _MSC_VER>=1300
int __cdecl CCrashHandler::NewHandler(size_t)
{
    // 'new' operator memory allocation exception

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_CPP_NEW_OPERATOR;
        ei.pExceptionPointers = NULL;

        // Generate error report.
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }

    // Unreacheable code
    return 0;
}
#endif //_MSC_VER>=1300

// CRT SIGABRT signal handler
void CCrashHandler::SigabrtHandler(int)
{
    // Caught SIGABRT C++ signal

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGABRT;

        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT SIGFPE signal handler
void CCrashHandler::SigfpeHandler(int /*code*/, int subcode)
{
    // Floating point exception (SIGFPE)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGFPE;
        ei.pExceptionPointers = (PEXCEPTION_POINTERS)_pxcptinfoptrs;
        ei.uFloatPointExceptionSubcode = subcode;

        //Generate crash report.
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT sigill signal handler
void CCrashHandler::SigillHandler(int)
{
    // Illegal instruction (SIGILL)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGILL;

        // Generate crash report
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT sigint signal handler
void CCrashHandler::SigintHandler(int)
{
    // Interruption (SIGINT)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGINT;

        // Generate crash report.
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT SIGSEGV signal handler
void CCrashHandler::SigsegvHandler(int)
{
    // Invalid storage access (SIGSEGV)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGSEGV;
        ei.pExceptionPointers = (PEXCEPTION_POINTERS)_pxcptinfoptrs;

        // Generate crash report
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}

// CRT SIGTERM signal handler
void CCrashHandler::SigtermHandler(int)
{
    // Termination request (SIGTERM)

    CCrashHandler* pCrashHandler = CCrashHandler::GetCurrentProcessCrashHandler();
    ATLASSERT(pCrashHandler != NULL);

    if (pCrashHandler != NULL)
    {
        // Acquire lock to avoid other threads (if exist) to crash while we are
        // inside.
        CCrashHandler::Locker const locker(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGTERM;

        // Generate crash report
        pCrashHandler->GenerateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            TerminateProcess(GetCurrentProcess(), 1);
        }
    }
}
