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
#include "LastErrorThreaded.h"

namespace {
    class LockGurad
    {
    public:
        LockGurad(CCrashHandler& self) : m_self(self) { m_self.lock(); }
        ~LockGurad() { m_self.unlock(); }

    private:
        CCrashHandler& m_self;
    };

    class BeforeGenerateErrorReportGuard
    {
    public:
        BeforeGenerateErrorReportGuard(CCrashHandler& self) : m_self(self) {}
        ~BeforeGenerateErrorReportGuard() { m_self.beforeGenerateErrorReport(); }

    private:
        CCrashHandler& m_self;
    };
}


// Taken from: http://msdn.microsoft.com/en-us/library/s975zw7k(VS.71).aspx
// _ReturnAddress and _AddressOfReturnAddress should be prototyped before use
extern "C" void* _AddressOfReturnAddress(void);
extern "C" void* _ReturnAddress(void);

CCrashHandler* CCrashHandler::m_pInstance = nullptr;

CCrashHandler::CCrashHandler()
{
    // Init member variables to their defaults
    m_bInstalled = FALSE;
    m_uMinidumpType = MiniDumpNormal;
    m_hEvent = nullptr;
    m_pCrashDesc = nullptr;
    m_hSenderProcess = nullptr;
    m_pfnCallback = nullptr;
    m_pCallbackParam = nullptr;
    m_nCallbackRetCode = CR_CB_NOTIFY_NEXT_STAGE;
    m_bContinueExecution = TRUE;

    clearOldExceptionHandlers();

    m_pInstance = this;
}

CCrashHandler::~CCrashHandler()
{
    uninstall();
    m_pInstance = nullptr;
}

int CCrashHandler::install(const CrInstallInfo* pInfo)
{
    m_szAppName = pInfo->szAppName;
    m_szAppVersion = pInfo->szAppVersion;
    m_szCrashSenderPath = pInfo->szCrashSenderPath;
    m_szServerURL = pInfo->szServerURL;
    m_szPrivacyPolicyURL = pInfo->szPrivacyPolicyURL;
    m_szDBGHelpPath = pInfo->szDBGHelpPath;
    m_szOutputDirectory = pInfo->szOutputDirectory;
    m_uCrashHandler = pInfo->uCrashHandler;
    m_uMinidumpType = pInfo->uMinidumpType;
    m_szExeFullPath = Utility::GetModuleFullPath(nullptr);

    if (m_szAppName.IsEmpty())
    {
        m_szAppName = Utility::getModuleBaseName();
    }

    if (m_szAppVersion.IsEmpty())
    {
        m_szAppVersion = Utility::GetProductVersion(m_szExeFullPath);
    }

    if (m_szCrashSenderPath.IsEmpty())
    {
        CString szCrashSenderExeName;

#ifdef _DEBUG
        szCrashSenderExeName.Format(_T("CrashSender%dd.exe"), CRASHRPT_VER);
#else
        szCrashSenderExeName.Format(_T("CrashSender%d.exe"), CRASHRPT_VER);
#endif

        CString szCrashSenderPath = Utility::getModuleDirectory((HMODULE)g_hModule) + szCrashSenderExeName;
        if (!::PathFileExists(szCrashSenderPath))
        {
            crLastErrorAdd(L"CrashSender.exe is not found in the specified path.");
            return 1;
        }
        m_szCrashSenderPath = szCrashSenderPath;
    }

    if (m_szDBGHelpPath.IsEmpty())
    {
        CString szDBGHelpPath = Utility::getModuleDirectory((HMODULE)g_hModule) + L"dbghelp.dll";
        if (::PathFileExists(szDBGHelpPath))
        {
            m_szDBGHelpPath = szDBGHelpPath;
        }

        if (m_szDBGHelpPath.IsEmpty())
        {
            szDBGHelpPath = L"dbghelp.dll";
            if (::PathFileExists(szDBGHelpPath))
            {
                m_szDBGHelpPath = szDBGHelpPath;
            }
        }

        if (m_szDBGHelpPath.IsEmpty())
        {
            crLastErrorAdd(_T("dbghelp.dll not found."));
            return 1;
        }
    }

    if (m_szOutputDirectory.IsEmpty())
    {
        m_szOutputDirectory = Utility::getModuleDirectory(nullptr) + L"dump";
    }

    BOOL bCreateDir = Utility::CreateFolder(m_szOutputDirectory);
    if (!bCreateDir)
    {
        crLastErrorAdd(_T("Couldn't create crash output directory."));
        return 1;
    }

    CString szLogDir = m_szOutputDirectory + _T("\\logs");
    bCreateDir = Utility::CreateFolder(szLogDir);
    if (!bCreateDir)
    {
        crLastErrorAdd(_T("Couldn't create crash logs directory."));
        return 1;
    }

    int nRet = beforeGenerateErrorReport();
    if (0 != nRet)
    {
        return 1;
    }

    clearOldExceptionHandlers();

    nRet = setupExceptionHandlers(m_uCrashHandler);
    if (nRet != 0)
    {
        crLastErrorAdd(_T("Couldn't set C++ exception handlers for current process."));
        return 1;
    }

    // The following code is intended to fix the issue with 32-bit applications in 64-bit environment.
    // http://support.microsoft.com/kb/976038/en-us
    // http://code.google.com/p/crashrpt/issues/detail?id=104

    typedef BOOL(WINAPI* SETPROCESSUSERMODEEXCEPTIONPOLICY)(DWORD dwFlags);
    typedef BOOL(WINAPI* GETPROCESSUSERMODEEXCEPTIONPOLICY)(LPDWORD lpFlags);
#define PROCESS_CALLBACK_FILTER_ENABLED 0x1

    HMODULE hKernel32 = ::LoadLibrary(_T("kernel32.dll"));
    if (hKernel32 != nullptr)
    {
        SETPROCESSUSERMODEEXCEPTIONPOLICY pfnSetProcessUserModeExceptionPolicy =
            (SETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "SetProcessUserModeExceptionPolicy");
        GETPROCESSUSERMODEEXCEPTIONPOLICY pfnGetProcessUserModeExceptionPolicy =
            (GETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "GetProcessUserModeExceptionPolicy");

        if (pfnSetProcessUserModeExceptionPolicy != nullptr &&
            pfnGetProcessUserModeExceptionPolicy != nullptr)
        {
            DWORD _dwFlags = 0;
            if (pfnGetProcessUserModeExceptionPolicy(&_dwFlags))
            {
                pfnSetProcessUserModeExceptionPolicy(_dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }

        ::FreeLibrary(hKernel32);
    }

    m_bInstalled = TRUE;
    return 0;
}

int CCrashHandler::setCrashCallback(PFN_CRASH_CALLBACK pfnCallback, LPVOID pUserParam)
{
    m_pfnCallback = pfnCallback;
    m_pCallbackParam = pUserParam;

    return 0;
}

// Packs config info to shared mem.
CRASH_DESCRIPTION* CCrashHandler::packCrashInfoIntoSharedMem(CSharedMem* pSharedMem, BOOL bTempMem)
{
    m_pTmpSharedMem = pSharedMem;

    CString sSharedMemName;
    if (bTempMem)
        sSharedMemName.Format(_T("%s-tmp"), (LPCTSTR)m_szCrashGUID);
    else
        sSharedMemName = m_szCrashGUID;

    if (!pSharedMem->IsInitialized())
    {
        // Initialize shared memory.
        BOOL bSharedMem = pSharedMem->Init(sSharedMemName, FALSE, SHARED_MEM_MAX_SIZE);
        if (!bSharedMem)
        {
            ATLASSERT(0);
            crLastErrorAdd(_T("Couldn't initialize shared memory."));
            return nullptr;
        }
    }

    // Create memory view.
    m_pTmpCrashDesc =
        (CRASH_DESCRIPTION*)pSharedMem->CreateView(0, sizeof(CRASH_DESCRIPTION));
    if (m_pTmpCrashDesc == nullptr)
    {
        ATLASSERT(0);
        crLastErrorAdd(_T("Couldn't create shared memory view."));
        return nullptr;
    }

    // Pack config information to shared memory
    memset(m_pTmpCrashDesc, 0, sizeof(CRASH_DESCRIPTION));
    memcpy(m_pTmpCrashDesc->m_uchMagic, "CRD", 3);
    m_pTmpCrashDesc->m_wSize = sizeof(CRASH_DESCRIPTION);
    m_pTmpCrashDesc->m_dwTotalSize = sizeof(CRASH_DESCRIPTION);
    m_pTmpCrashDesc->m_dwCrashRptVer = CRASHRPT_VER;
    m_pTmpCrashDesc->m_MinidumpType = m_uMinidumpType;
    m_pTmpCrashDesc->m_dwProcessId = GetCurrentProcessId();
    m_pTmpCrashDesc->m_bClientAppCrashed = FALSE;

    m_pTmpCrashDesc->m_dwAppNameOffs = packString(m_szAppName);
    m_pTmpCrashDesc->m_dwAppVersionOffs = packString(m_szAppVersion);
    m_pTmpCrashDesc->m_dwCrashGUIDOffs = packString(m_szCrashGUID);
    m_pTmpCrashDesc->m_dwImageNameOffs = packString(m_szExeFullPath);
    m_pTmpCrashDesc->m_dwPathToDebugHelpDllOffs = packString(m_szDBGHelpPath);
    m_pTmpCrashDesc->m_dwPrivacyPolicyURLOffs = packString(m_szPrivacyPolicyURL);
    m_pTmpCrashDesc->m_dwUnsentCrashReportsFolderOffs = packString(m_szOutputDirectory);
    m_pTmpCrashDesc->m_dwUrlOffs = packString(m_szServerURL);

    // Pack file items
    std::map<CString, FileItem>::iterator fit;
    for (fit = m_files.begin(); fit != m_files.end(); fit++)
    {
        FileItem& fi = fit->second;

        // Pack this file item into shared mem.
        packFileItem(fi);
    }

    // Pack custom props
    std::map<CString, CString>::iterator pit;
    for (pit = m_props.begin(); pit != m_props.end(); pit++)
    {
        // Pack this prop into shared mem.
        packProperty(pit->first, pit->second);
    }

    return m_pTmpCrashDesc;
}

// Packs a string to shared memory
DWORD CCrashHandler::packString(const CString& str)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    int nStrLen = str.GetLength() * sizeof(TCHAR);
    WORD wLength = (WORD)(sizeof(STRING_DESC) + nStrLen);

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    STRING_DESC* pStrDesc = (STRING_DESC*)pView;
    memcpy(pStrDesc->m_uchMagic, "STR", 3);
    pStrDesc->m_wSize = wLength;
    memcpy(pView + sizeof(STRING_DESC), str.GetString(), nStrLen);

    m_pTmpCrashDesc->m_dwTotalSize += wLength;

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Packs file item to shared memory
DWORD CCrashHandler::packFileItem(FileItem& fi)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    WORD wLength = sizeof(FILE_ITEM);
    m_pTmpCrashDesc->m_dwTotalSize += wLength;
    m_pTmpCrashDesc->m_uFileItems++;

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    FILE_ITEM* pFileItem = (FILE_ITEM*)pView;

    memcpy(pFileItem->m_uchMagic, "FIL", 3);
    pFileItem->m_dwSrcFilePathOffs = packString(fi.srcFilePath);
    pFileItem->m_dwDstFileNameOffs = packString(fi.dstFileName);
    pFileItem->m_dwDescriptionOffs = packString(fi.description);
    pFileItem->m_bMakeCopy = fi.isMakeCopy;
    pFileItem->m_bAllowDelete = fi.isAllowDelete;
    pFileItem->m_wSize = (WORD)(m_pTmpCrashDesc->m_dwTotalSize - dwTotalSize);

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Packs custom property to shared memory
DWORD CCrashHandler::packProperty(const CString& sName, const CString& sValue)
{
    DWORD dwTotalSize = m_pTmpCrashDesc->m_dwTotalSize;
    WORD wLength = sizeof(CUSTOM_PROP);
    m_pTmpCrashDesc->m_dwTotalSize += wLength;
    m_pTmpCrashDesc->m_uCustomProps++;

    LPBYTE pView = m_pTmpSharedMem->CreateView(dwTotalSize, wLength);
    CUSTOM_PROP* pProp = (CUSTOM_PROP*)pView;

    memcpy(pProp->m_uchMagic, "CPR", 3);
    pProp->m_dwNameOffs = packString(sName);
    pProp->m_dwValueOffs = packString(sValue);
    pProp->m_wSize = (WORD)(m_pTmpCrashDesc->m_dwTotalSize - dwTotalSize);

    m_pTmpSharedMem->DestroyView(pView);
    return dwTotalSize;
}

// Returns TRUE if initialized, otherwise FALSE
BOOL CCrashHandler::isInstalled()
{
    return m_bInstalled;
}

// Destroys the object
int CCrashHandler::uninstall()
{
    crLastErrorAdd(_T("Unspecified error."));

    if (!m_bInstalled)
    {
        crLastErrorAdd(_T("Can't destroy not initialized crash handler."));
        return 1;
    }

    // Free handle to CrashSender.exe process.
    if (m_hSenderProcess != nullptr)
        CloseHandle(m_hSenderProcess);

    // Free events
    if (m_hEvent)
    {
        CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    // Reset SEH exception filter
    if (m_hExcSEH)
    {
        ::SetUnhandledExceptionFilter(m_hExcSEH);
        m_hExcSEH = nullptr;
    }

    m_bInstalled = FALSE;
    crLastErrorAdd(_T("Success."));
    return 0;
}

// Sets internal pointers to previously used exception handlers to nullptr
void CCrashHandler::clearOldExceptionHandlers()
{
    m_hExcSEH = nullptr;
    m_hExcPureCall = nullptr;
    m_hExcCppNew = nullptr;
    m_hExcInvalidParameter = nullptr;
    m_hExcSIGABRT = nullptr;
    m_hExcSIGINT = nullptr;
    m_hExcSIGTERM = nullptr;
}

// Returns singleton of the crash handler
CCrashHandler* CCrashHandler::instance()
{
    return m_pInstance;
}

// Sets exception handlers that work on per-process basis
int CCrashHandler::setupExceptionHandlers(UINT32 uCrashHandlers)
{
    if ((uCrashHandlers & CR_CRASH_HANDLER_ALL) == 0)
    {
        uCrashHandlers |= CR_CRASH_HANDLER_ALL;
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_SEH)
    {
        m_hExcSEH = ::SetUnhandledExceptionFilter(onHandleSEH);
    }

    _set_error_mode(_OUT_TO_STDERR);

    if (uCrashHandlers & CR_CRASH_HANDLER_CPP_PURE)
    {
        // Catch pure virtual function calls.
        // Because there is one _purecall_handler for the whole process,
        // calling this function immediately impacts all threads. The last
        // caller on any thread sets the handler.
        // http://msdn.microsoft.com/en-us/library/t296ys27.aspx
        m_hExcPureCall = _set_purecall_handler(onHandlePureCall);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_NEW_OPERATOR)
    {
        _set_new_mode(1);
        m_hExcCppNew = _set_new_handler(onHandleCppNew);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_INVALID_PARAMETER)
    {
        m_hExcInvalidParameter = _set_invalid_parameter_handler(onHandleInvalidParameter);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_SIGABRT)
    {
        _set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);
        m_hExcSIGABRT = signal(SIGABRT, onHandleSIGABRT);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_SIGINT)
    {
        m_hExcSIGINT = signal(SIGINT, onHandleSIGINT);
    }

    if (uCrashHandlers & CR_CRASH_HANDLER_TERMINATE_CALL)
    {
        m_hExcSIGTERM = signal(SIGTERM, onHandleSIGTERM);
    }

    return 0;
}

// Unsets exception pointers that work on per-process basis
int CCrashHandler::tearDownExceptionHandlers()
{
#if _MSC_VER>=1300
    if (m_hExcPureCall != nullptr)
    {
        _set_purecall_handler(m_hExcPureCall);
        m_hExcPureCall = nullptr;
    }

    if (m_hExcCppNew != nullptr)
    {
        _set_new_handler(m_hExcCppNew);
        m_hExcCppNew = nullptr;
    }
#endif

#if _MSC_VER>=1400
    if (m_hExcInvalidParameter != nullptr)
    {
        _set_invalid_parameter_handler(m_hExcInvalidParameter);
        m_hExcInvalidParameter = nullptr;
    }
#endif //_MSC_VER>=1400

#if _MSC_VER>=1300 && _MSC_VER<1400
    if (m_hOldSecurity != nullptr)
    {
        _set_security_error_handler(m_hOldSecurity);
        m_hOldSecurity = nullptr;
    }
#endif //_MSC_VER<1400

    if (m_hExcSIGABRT != nullptr)
    {
        signal(SIGABRT, m_hExcSIGABRT);
        m_hExcSIGABRT = nullptr;
    }

    if (m_hExcSIGINT != nullptr)
    {
        signal(SIGINT, m_hExcSIGINT);
        m_hExcSIGINT = nullptr;
    }

    if (m_hExcSIGTERM != nullptr)
    {
        signal(SIGTERM, m_hExcSIGTERM);
        m_hExcSIGTERM = nullptr;
    }

    return 0;
}

// Adds a file item to the error report
int CCrashHandler::addFile(LPCTSTR pszFile, LPCTSTR pszDestFile, LPCTSTR pszDesc, DWORD dwFlags)
{
    // Check if source file name or search pattern is specified
    if (pszFile == nullptr)
    {
        crLastErrorAdd(_T("Invalid file name specified."));
        return 1;
    }

    // Check that the destination file name is valid
    if (pszDestFile != nullptr)
    {
        CString sDestFile = pszDestFile;
        sDestFile.TrimLeft();
        sDestFile.TrimRight();
        if (sDestFile.GetLength() == 0)
        {
            crLastErrorAdd(_T("Invalid destination file name specified."));
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
            crLastErrorAdd(_T("The file does not exists or not a regular file."));
            return 1;
        }

        // Add file to file list.
        FileItem fi;
        fi.description = pszDesc;
        fi.srcFilePath = pszFile;
        fi.isMakeCopy = (dwFlags & CR_AF_MAKE_FILE_COPY) != 0;
        fi.isAllowDelete = (dwFlags & CR_AF_ALLOW_DELETE) != 0;
        if (pszDestFile != nullptr)
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
            crLastErrorAdd(_T("A file with such a destination name already exists."));
            return 1;
        }

        m_files[fi.dstFileName] = fi;

        // Pack this file item into shared mem.
        packFileItem(fi);
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
        packFileItem(fi);
    }

    // OK.
    return 0;
}

// Adds a custom property to the error report
int CCrashHandler::addProperty(const CString& sPropName, const CString& sPropValue)
{
    if (sPropName.IsEmpty())
    {
        crLastErrorAdd(_T("Invalid property name specified."));
        return 1;
    }

    m_props[sPropName] = sPropValue;

    packProperty(sPropName, sPropValue);
    return 0;
}

int CCrashHandler::generateErrorReport(CR_EXCEPTION_INFO* pExceptionInfo)
{
    BeforeGenerateErrorReportGuard guard(*this);

    EXCEPTION_RECORD stExceptionRecord;
    CONTEXT stContextRecord;
    EXCEPTION_POINTERS pExceptionPointers;
    pExceptionPointers.ExceptionRecord = &stExceptionRecord;
    pExceptionPointers.ContextRecord = &stContextRecord;

    if (pExceptionInfo == nullptr)
    {
        crLastErrorAdd(_T("parameter is nullptr."));
        return 1;
    }

    // Get exception pointers if they were not provided by the caller.
    if (pExceptionInfo->pExceptionPointers == nullptr)
    {
        getExceptionPointers(pExceptionInfo->dwSEHCode, &pExceptionPointers);
        pExceptionInfo->pExceptionPointers = &pExceptionPointers;
    }

    // Set "client app crashed" flag.
    m_pCrashDesc->m_bClientAppCrashed = TRUE;

    // Save current process ID, thread ID and exception pointers address to shared mem.
    m_pCrashDesc->m_dwProcessId = ::GetCurrentProcessId();
    m_pCrashDesc->m_dwThreadId = ::GetCurrentThreadId();
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
        m_pCrashDesc->m_dwInvParamExprOffs = packString(pExceptionInfo->lpAssertionExpression);
        m_pCrashDesc->m_dwInvParamFunctionOffs = packString(pExceptionInfo->lpFunction);
        m_pCrashDesc->m_dwInvParamFileOffs = packString(pExceptionInfo->lpFile);
        m_pCrashDesc->m_uInvParamLine = pExceptionInfo->uLine;
    }

    if (CR_CB_CANCEL == notifyCallback(CR_CB_STAGE_PREPARE, pExceptionInfo))
    {
        crLastErrorAdd(_T("The operation was cancelled by client."));
        return 1;
    }

    int nRet = launchCrashSender(m_szCrashGUID, TRUE, &pExceptionInfo->hSenderProcess);
    notifyCallback(CR_CB_STAGE_FINISH, pExceptionInfo);

    if (nRet != 0)
    {
        crLastErrorAdd(_T("Error launching CrashSender.exe"));

        CString szCaption;
        szCaption.Format(_T("%s has stopped working"), (LPCTSTR)Utility::getModuleBaseName());
        CString szMessage;
        szMessage.Format(_T("Error launching CrashSender.exe"));
        ::MessageBox(nullptr, szMessage, szCaption, MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}

// The following code gets exception pointers using a workaround found in CRT code.
void CCrashHandler::getExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers)
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
int CCrashHandler::launchCrashSender(LPCTSTR szCmdLineParams, BOOL bWait, HANDLE* phProcess)
{
    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(PROCESS_INFORMATION));

    CString szCmdLine;
    szCmdLine.Format(_T("\"%s\" %s"), m_szCrashSenderPath.GetString(), szCmdLineParams);

    BOOL bCreateProcess = ::CreateProcess(nullptr, (LPWSTR)szCmdLine.GetString(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (pi.hThread)
    {
        CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }

    if (!bCreateProcess)
    {
        crLastErrorAdd(_T("Error creating CrashSender process."));
        return 1;
    }

    if (bWait)
    {
        WaitForSingleObject(m_hEvent, INFINITE);
    }

    if (phProcess)
    {
        *phProcess = pi.hProcess;
    }
    else
    {
        CloseHandle(pi.hProcess);
        pi.hProcess = nullptr;
    }

    return 0;
}

void CCrashHandler::lock()
{
    m_lock.lock();
}

void CCrashHandler::unlock()
{
    m_lock.unlock();
}

int CCrashHandler::beforeGenerateErrorReport()
{
    m_bContinueExecutionNow = m_bContinueExecution;
    m_bContinueExecution = TRUE;

    m_nCallbackRetCode = CR_CB_NOTIFY_NEXT_STAGE;

    Utility::GenerateGUID(m_szCrashGUID);

    if (m_hEvent != nullptr)
    {
        ::CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    CString szEvtName;
    szEvtName.Format(_T("Local\\CrashRptEvent_%s"), (LPCTSTR)m_szCrashGUID);
    m_hEvent = ::CreateEvent(nullptr, FALSE, FALSE, szEvtName);

    if (m_sharedMem.IsInitialized())
    {
        m_sharedMem.Destroy();
        m_pCrashDesc = nullptr;
    }

    repack();
    return 0;
}

void CCrashHandler::repack()
{
    // Pack configuration info into shared memory.
    // It will be passed to CrashSender.exe later.
    m_pCrashDesc = packCrashInfoIntoSharedMem(&m_sharedMem, FALSE);
}

int CCrashHandler::notifyCallback(int nStage, CR_EXCEPTION_INFO* pExInfo)
{
    if (m_nCallbackRetCode != CR_CB_NOTIFY_NEXT_STAGE)
    {
        return CR_CB_DODEFAULT;
    }

    if (m_pfnCallback)
    {
        CR_CRASH_CALLBACK_INFO info;
        memset(&info, 0, sizeof(CR_CRASH_CALLBACK_INFO));
        info.cb = sizeof(CR_CRASH_CALLBACK_INFO);
        info.nStage = nStage;
        info.pExceptionInfo = pExInfo;
        info.pUserParam = m_pCallbackParam;
        info.bContinueExecution = m_bContinueExecution;

        m_nCallbackRetCode = m_pfnCallback(&info);
        m_bContinueExecution = info.bContinueExecution;
    }

    return m_nCallbackRetCode;
}

LONG WINAPI CCrashHandler::onHandleSEH(PEXCEPTION_POINTERS pExceptionPtrs)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();

    // Handle stack overflow in a separate thread.
    // Vojtech: Based on martin.bis...@gmail.com comment in
    // http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
    if (pExceptionPtrs &&
        pExceptionPtrs->ExceptionRecord &&
        pExceptionPtrs->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
    {
        // Special case to handle the stack overflow exception.
        // The dump will be realized from another thread.
        // Create another thread that will do the dump.
        HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, &onHandleSEHStackOverflow, pExceptionPtrs, 0, nullptr);
        if (hThread)
        {
            ::WaitForSingleObject(hThread, INFINITE);
            ::CloseHandle(hThread);
        }
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }

    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Generate error report.
        CR_EXCEPTION_INFO ei;
        ZeroMemory(&ei, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SEH;
        ei.pExceptionPointers = pExceptionPtrs;
        if (pExceptionPtrs && pExceptionPtrs->ExceptionRecord)
        {
            ei.dwSEHCode = pExceptionPtrs->ExceptionRecord->ExceptionCode;
        }
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

//Vojtech: Based on martin.bis...@gmail.com comment in
// http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
// Thread procedure doing the dump for stack overflow.
unsigned __stdcall CCrashHandler::onHandleSEHStackOverflow(void* pvParam)
{
    PEXCEPTION_POINTERS pExceptionPointers = reinterpret_cast<PEXCEPTION_POINTERS>(pvParam);
    CCrashHandler* pCrashHandler = CCrashHandler::instance();

    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SEH;
        ei.pExceptionPointers = pExceptionPointers;
        ei.dwSEHCode = pExceptionPointers->ExceptionRecord->ExceptionCode;
        pCrashHandler->generateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }

    return 0;
}

void __cdecl CCrashHandler::onHandlePureCall()
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_CPP_PURE;
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }
}

void __cdecl CCrashHandler::onHandleInvalidParameter(
    const wchar_t* expression,
    const wchar_t* funcName,
    const wchar_t* file,
    unsigned int line,
    uintptr_t /*pReserved*/)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

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
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }
}

int __cdecl CCrashHandler::onHandleCppNew(size_t)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_CPP_NEW_OPERATOR;
        ei.pExceptionPointers = nullptr;
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            // Terminate process
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }

    return 0;
}

void CCrashHandler::onHandleSIGABRT(int)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGABRT;

        pCrashHandler->generateErrorReport(&ei);

        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }
}

void CCrashHandler::onHandleSIGINT(int)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        // Fill in the exception info
        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGINT;
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }
}

// CRT SIGTERM signal handler
void CCrashHandler::onHandleSIGTERM(int)
{
    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler)
    {
        LockGurad gurad(*pCrashHandler);

        // Treat this type of crash critical by default
        pCrashHandler->m_bContinueExecution = FALSE;

        CR_EXCEPTION_INFO ei;
        memset(&ei, 0, sizeof(CR_EXCEPTION_INFO));
        ei.cb = sizeof(CR_EXCEPTION_INFO);
        ei.nExceptionType = CR_TEST_CRASH_SIGTERM;
        pCrashHandler->generateErrorReport(&ei);
        if (!pCrashHandler->m_bContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }
}
