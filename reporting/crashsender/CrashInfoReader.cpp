/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: CrashInfoReader.cpp
// Description: Retrieves crash information passed from CrashRpt.dll in form of XML files.
// Authors: zexspectrum
// Date: 2010

#include "stdafx.h"
#include "CrashRpt.h"
#include "CrashInfoReader.h"
#include "strconv.h"
#include "tinyxml.h"
#include "Utility.h"
#include "SharedMemory.h"

BOOL ERIFileItem::GetFileInfo(HICON& hIcon, CString& sTypeName, LONGLONG& lSize)
{
    hIcon = NULL;
    sTypeName = _T("Unknown");
    lSize = 0;
    SHFILEINFO sfi;
    HANDLE hFile = INVALID_HANDLE_VALUE;

    // Open file for reading
    hFile = CreateFile(m_sSrcFile,
        GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
        return FALSE; // Error - file may not exist

    // Get file size
    LARGE_INTEGER lFileSize;
    BOOL bGetSize = GetFileSizeEx(hFile, &lFileSize);
    if (bGetSize)
    {
        lSize = lFileSize.QuadPart;
    }

    // Get file icon and type name
    SHGetFileInfo(m_sSrcFile, 0, &sfi, sizeof(sfi),
        SHGFI_DISPLAYNAME | SHGFI_ICON | SHGFI_TYPENAME | SHGFI_SMALLICON);

    hIcon = sfi.hIcon;
    sTypeName = sfi.szTypeName;

    // Clean up
    CloseHandle(hFile);

    // OK
    return TRUE;
}

//---------------------------------------------------------------------
// CErrorReportInfo impl
//---------------------------------------------------------------------

CErrorReportInfo::CErrorReportInfo()
{
    // Initialize variables.
    m_bSelected = TRUE;
    m_DeliveryStatus = PENDING;
    m_dwGuiResources = 0;
    m_dwProcessHandleCount = 0;
    m_uTotalSize = 0;
    m_dwExceptionAddress = 0;
    m_dwExceptionModuleBase = 0;
}

// Destructor.
CErrorReportInfo::~CErrorReportInfo()
{

}

CString CErrorReportInfo::GetErrorReportDirName()
{
    return m_sErrorReportDirName;
}

CString CErrorReportInfo::GetCrashGUID()
{
    return m_sCrashGUID;
}

CString CErrorReportInfo::GetAppName()
{
    return m_sAppName;
}

CString CErrorReportInfo::GetAppVersion()
{
    return m_sAppVersion;
}

CString CErrorReportInfo::GetImageName()
{
    return m_sImageName;
}

CString CErrorReportInfo::GetEmailFrom()
{
    return m_sEmailFrom;
}

CString CErrorReportInfo::GetProblemDescription()
{
    return m_sDescription;
}

CString CErrorReportInfo::GetSystemTimeUTC()
{
    return m_sSystemTimeUTC;
}

ULONG64 CErrorReportInfo::GetTotalSize()
{
    m_uTotalSize = CalcUncompressedReportSize();
    return m_uTotalSize;
}

int CErrorReportInfo::GetFileItemCount()
{
    return (int)m_FileItems.size();
}

BOOL CErrorReportInfo::IsSelected()
{
    return m_bSelected;
}

void CErrorReportInfo::Select(BOOL bSelect)
{
    m_bSelected = bSelect;
}

DELIVERY_STATUS CErrorReportInfo::GetDeliveryStatus()
{
    return m_DeliveryStatus;
}

void CErrorReportInfo::SetDeliveryStatus(DELIVERY_STATUS status)
{
    m_DeliveryStatus = status;
}

ULONG64 CErrorReportInfo::GetExceptionAddress()
{
    return m_dwExceptionAddress;
}

CString CErrorReportInfo::GetExceptionModule()
{
    return m_sExceptionModule;
}

void CErrorReportInfo::SetExceptionModule(LPCTSTR szExceptionModule)
{
    m_sExceptionModule = szExceptionModule;
}

ULONG64 CErrorReportInfo::GetExceptionModuleBase()
{
    return m_dwExceptionModuleBase;
}

void CErrorReportInfo::SetExceptionModuleBase(ULONG64 dwExceptionModuleBase)
{
    m_dwExceptionModuleBase = dwExceptionModuleBase;
}

CString CErrorReportInfo::GetExceptionModuleVersion()
{
    return m_sExceptionModuleVersion;
}

void CErrorReportInfo::SetExceptionModuleVersion(LPCTSTR szVer)
{
    m_sExceptionModuleVersion = szVer;
}

CString CErrorReportInfo::GetOSName()
{
    return m_sOSName;
}

BOOL CErrorReportInfo::IsOS64Bit()
{
    return m_bOSIs64Bit;
}

CString CErrorReportInfo::GetGeoLocation()
{
    return m_sGeoLocation;
}

DWORD CErrorReportInfo::GetGuiResourceCount()
{
    return m_dwGuiResources;
}

DWORD CErrorReportInfo::GetProcessHandleCount()
{
    return m_dwProcessHandleCount;
}

CString CErrorReportInfo::GetMemUsage()
{
    return m_sMemUsage;
}

ERIFileItem* CErrorReportInfo::GetFileItemByIndex(int nItem)
{
    if (nItem < 0 || nItem >= GetFileItemCount())
        return NULL; // No such item

    // Look for n-th item
    int cnt = 0;
    for (auto& fi : m_FileItems) {
        if (cnt == nItem)
            return &(fi.second);
        ++cnt;
    }
    return NULL;
}

ERIFileItem* CErrorReportInfo::GetFileItemByName(LPCTSTR szName)
{
    return &m_FileItems[szName];
}

void CErrorReportInfo::AddFileItem(ERIFileItem* pfi)
{
    // Kaneva - Bug Fix - Use Source File Full Path
    m_FileItems[pfi->m_sSrcFile] = *pfi;
}

BOOL CErrorReportInfo::DeleteFileItemByIndex(int nItem)
{
    // Look for n-th item
    std::map<CString, ERIFileItem>::iterator p = m_FileItems.begin();
    for (int i = 0; i < nItem; i++, p++);
    if (p == m_FileItems.end())
        return FALSE;

    m_FileItems.erase(p);
    return TRUE;
}

// Returns count of custom properties in error report.
int CErrorReportInfo::GetPropCount()
{
    return (int)m_Props.size();
}

// Method that retrieves a property by zero-based index.
BOOL CErrorReportInfo::GetPropByIndex(int nItem, CString& sName, CString& sVal)
{
    sName.Empty();
    sVal.Empty();

    if (nItem < 0 || nItem >= (int)m_Props.size())
        return FALSE; // No such item

    // Look for n-th item
    std::map<CString, CString>::iterator p = m_Props.begin();
    for (int i = 0; i < nItem; i++, p++);
    sName = p->first;
    sVal = p->second;
    return TRUE;
}

// Adds/replaces a property in crash report.
void CErrorReportInfo::AddProp(LPCTSTR szName, LPCTSTR szVal)
{
    m_Props[szName] = szVal;
}

// This method calculates the total size of files included into error report
LONG64 CErrorReportInfo::CalcUncompressedReportSize()
{
    LONG64 lTotalSize = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    CString sMsg;
    BOOL bGetSize = FALSE;
    LARGE_INTEGER lFileSize;

    // Enumerate files contained in the error report
    int i;
    for (i = 0; i < GetFileItemCount(); i++)
    {
        ERIFileItem* pfi = GetFileItemByIndex(i);

        // Get name of the file
        CString sFileName = pfi->m_sSrcFile.GetBuffer(0);
        // Open file for reading
        hFile = CreateFile(sFileName,
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            continue;
        }

        // Get file size
        bGetSize = GetFileSizeEx(hFile, &lFileSize);
        if (!bGetSize)
        {
            CloseHandle(hFile);
            continue;
        }

        // Update totals
        lTotalSize += lFileSize.QuadPart;

        // Close file
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    // Return total file size
    return lTotalSize;
}


//---------------------------------------------------------------------
// CCrashInfoReader impl
//---------------------------------------------------------------------

CCrashInfoReader::CCrashInfoReader()
{
    // Init internal variables.
    m_nCrashRptVersion = 0;
    m_bSilentMode = FALSE;
    m_bSendErrorReport = TRUE;
    m_bShowAdditionalInfoFields = FALSE;
    m_bStoreZIPArchives = FALSE;
    m_bAppRestart = FALSE;
    m_MinidumpType = MiniDumpNormal;
    m_ptCursorPos = CPoint(0, 0);
    m_rcAppWnd = CRect(0, 0, 0, 0);
    m_bQueueEnabled = FALSE;
    m_dwProcessId = 0;
    m_dwThreadId = 0;
    m_pExInfo = NULL;
    m_nExceptionType = 0;
    m_dwExceptionCode = 0;
    m_uFPESubcode = 0;
    m_uInvParamLine = 0;
    m_pCrashDesc = NULL;
}

int CCrashInfoReader::Init(LPCTSTR szFileMappingName)
{
    CErrorReportInfo eri;

    if (!m_SharedMem.isOpened())
    {
        if (!m_SharedMem.open(szFileMappingName))
        {
            m_sErrorMsg = _T("Error initializing shared memory.");
            return 1;
        }
    }

    // Unpack crash description from shared memory
    m_pCrashDesc = (CRASH_DESCRIPTION*)m_SharedMem.mapView(0, sizeof(CRASH_DESCRIPTION));

    int nUnpack = UnpackCrashDescription(eri);
    if (0 != nUnpack)
    {
        m_sErrorMsg = _T("Error unpacking crash description.");
        return 2;
    }

    // Create LOCAL_APP_DATA\UnsentCrashReports folder (if doesn't exist yet).
    BOOL bCreateFolder = Utility::CreateFolder(m_sUnsentCrashReportsFolder);
    if (!bCreateFolder)
        return 3;

    // Save path to INI file storing settings
    m_sINIFile = m_sUnsentCrashReportsFolder + _T("\\~CrashRpt.ini");

    CollectMiscCrashInfo(eri);
    eri.m_sErrorReportDirName = m_sUnsentCrashReportsFolder + _T("\\") + eri.m_sCrashGUID;
    Utility::CreateFolder(eri.m_sErrorReportDirName);
    m_Reports.push_back(eri);

    return 0;
}

int CCrashInfoReader::UnpackCrashDescription(CErrorReportInfo& eri)
{
    // This method unpacks crash description data from shared memory.

    if (memcmp(m_pCrashDesc->chMagic, "CRD", 3) != 0)
        return 1; // Invalid magic word

    if (m_pCrashDesc->dwCrashRptVersion != CRASHRPT_VER)
        return 2; // Invalid CrashRpt version

    // Unpack process ID, thread ID and exception pointers address.
    m_dwProcessId = m_pCrashDesc->dwProcessId;
    m_dwThreadId = m_pCrashDesc->m_dwThreadId;
    m_pExInfo = m_pCrashDesc->m_pExceptionPtrs;
    m_nExceptionType = m_pCrashDesc->m_nExceptionType;
    m_dwExceptionCode = m_pCrashDesc->m_dwExceptionCode;

    if (m_nExceptionType == CR_TEST_CRASH_SIGFPE)
    {
        m_uFPESubcode = m_pCrashDesc->m_uFPESubcode;
    }
    else if (m_nExceptionType == CR_TEST_CRASH_INVALID_PARAMETER)
    {
        UnpackString(m_pCrashDesc->m_dwInvParamExprOffs, m_sInvParamExpr);
        UnpackString(m_pCrashDesc->m_dwInvParamFunctionOffs, m_sInvParamFunction);
        UnpackString(m_pCrashDesc->m_dwInvParamFileOffs, m_sInvParamFile);
        m_uInvParamLine = m_pCrashDesc->m_uInvParamLine;
    }

    // Unpack other info
    UnpackString(m_pCrashDesc->m_dwAppNameOffs, eri.m_sAppName);
    m_sAppName = eri.m_sAppName;
    UnpackString(m_pCrashDesc->m_dwAppVersionOffs, eri.m_sAppVersion);
    UnpackString(m_pCrashDesc->m_dwCrashGUIDOffs, eri.m_sCrashGUID);
    UnpackString(m_pCrashDesc->m_dwImageNameOffs, eri.m_sImageName);
    // Unpack install flags
    m_bSilentMode = FALSE;
    m_bSendErrorReport = TRUE;
    m_bShowAdditionalInfoFields = FALSE;
    m_bStoreZIPArchives = TRUE;
    m_bAppRestart = TRUE;
    m_bQueueEnabled = FALSE;
    m_MinidumpType = m_pCrashDesc->uMinidumpType;
    UnpackString(m_pCrashDesc->m_dwUrlOffs, m_sUrl);
    UnpackString(m_pCrashDesc->m_dwPrivacyPolicyURLOffs, m_sPrivacyPolicyURL);
    UnpackString(m_pCrashDesc->m_dwPathToDebugHelpDllOffs, m_sDbgHelpPath);
    UnpackString(m_pCrashDesc->m_dwUnsentCrashReportsFolderOffs, m_sUnsentCrashReportsFolder);
    m_sLangFileName = Utility::getModuleDirectory(nullptr) + L"crashrpt_lang.ini";


    DWORD dwOffs = m_pCrashDesc->cb;
    while (dwOffs < m_pCrashDesc->dwTotalSize)
    {
        LPBYTE pView = m_SharedMem.mapView(dwOffs, sizeof(GENERIC_HEADER));
        GENERIC_HEADER* pHeader = (GENERIC_HEADER*)pView;

        if (memcmp(pHeader->m_uchMagic, "FIL", 3) == 0)
        {
            // File item entry
            FILE_ITEM* pFileItem = (FILE_ITEM*)m_SharedMem.mapView(dwOffs, pHeader->m_wSize);

            ERIFileItem fi;
            UnpackString(pFileItem->m_dwSrcFilePathOffs, fi.m_sSrcFile);
            UnpackString(pFileItem->m_dwDstFileNameOffs, fi.m_sDestFile);
            UnpackString(pFileItem->m_dwDescriptionOffs, fi.m_sDesc);
            fi.m_bMakeCopy = pFileItem->m_bMakeCopy;
            fi.m_bAllowDelete = pFileItem->m_bAllowDelete;

            // Kaneva - Bug Fix - Use Source File Full Path
            eri.m_FileItems[fi.m_sSrcFile] = fi;

            m_SharedMem.unmapView((LPBYTE)pFileItem);
        }
        else if (memcmp(pHeader->m_uchMagic, "CPR", 3) == 0)
        {
            // Custom prop entry
            CUSTOM_PROP* pProp = (CUSTOM_PROP*)m_SharedMem.mapView(dwOffs, pHeader->m_wSize);

            CString sName;
            CString sValue;
            UnpackString(pProp->m_dwNameOffs, sName);
            UnpackString(pProp->m_dwValueOffs, sValue);

            eri.m_Props[sName] = sValue;

            m_SharedMem.unmapView((LPBYTE)pProp);
        }
        else if (memcmp(pHeader->m_uchMagic, "STR", 3) == 0)
        {
            // Skip string
        }
        else
        {
            ATLASSERT(0); // Unknown header
            return 1;
        }

        dwOffs += pHeader->m_wSize;

        m_SharedMem.unmapView(pView);
    }

    // Success
    return 0;
}

int CCrashInfoReader::UnpackString(DWORD dwOffset, CString& str)
{
    STRING_DESC* pStrDesc = (STRING_DESC*)m_SharedMem.mapView(dwOffset, sizeof(STRING_DESC));
    if (memcmp(pStrDesc, "STR", 3) != 0)
        return 1;

    WORD wLength = pStrDesc->m_wSize;
    if (wLength <= sizeof(STRING_DESC))
        return 2;

    WORD wStrLen = wLength - sizeof(STRING_DESC);

    m_SharedMem.unmapView((LPBYTE)pStrDesc);
    LPBYTE pStrData = m_SharedMem.mapView(dwOffset + sizeof(STRING_DESC), wStrLen);
    str = CString((LPCTSTR)pStrData, wStrLen / sizeof(TCHAR));
    m_SharedMem.unmapView(pStrData);

    return 0;
}

CErrorReportInfo* CCrashInfoReader::GetReport(int nIndex)
{
    if (nIndex >= 0 && nIndex < (int)m_Reports.size())
        return &m_Reports[nIndex];

    return NULL;
}

int CCrashInfoReader::GetReportCount()
{
    return (int)m_Reports.size();
}

void CCrashInfoReader::DeleteReport(int nIndex)
{
    ATLASSERT(nIndex >= 0 && nIndex < (int)m_Reports.size());

    // Delete report files
    Utility::RecycleFile(m_Reports[nIndex].m_sErrorReportDirName, TRUE);

    // Delete from list
    m_Reports[nIndex].m_DeliveryStatus = DELETED;
}

void CCrashInfoReader::DeleteAllReports()
{
    int i;
    for (i = 0; i < (int)m_Reports.size(); i++)
    {
        // Delete report files
        Utility::RecycleFile(m_Reports[i].m_sErrorReportDirName, TRUE);

        m_Reports[i].m_DeliveryStatus = DELETED;
    }
}

void CCrashInfoReader::CollectMiscCrashInfo(CErrorReportInfo& eri)
{
    // Get crash time
    Utility::GetSystemTimeUTC(eri.m_sSystemTimeUTC);

    // Open parent process handle
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE,
        m_dwProcessId);

    if (hProcess != NULL)
    {
        SIZE_T uBytesRead = 0;
        BYTE buff[1024];
        memset(&buff, 0, 1024);

        // Read exception information from process memory
        if (m_pExInfo != NULL)
        {
            if (ReadProcessMemory(hProcess, m_pExInfo, &buff, sizeof(EXCEPTION_POINTERS), &uBytesRead) &&
                uBytesRead == sizeof(EXCEPTION_POINTERS))
            {
                EXCEPTION_POINTERS* pExcPtrs = (EXCEPTION_POINTERS*)buff;

                if (pExcPtrs->ExceptionRecord != NULL)
                {
                    DWORD64 dwExcRecordAddr = (DWORD64)pExcPtrs->ExceptionRecord;
                    if (ReadProcessMemory(hProcess, (LPCVOID)dwExcRecordAddr, &buff, sizeof(EXCEPTION_RECORD), &uBytesRead) &&
                        uBytesRead == sizeof(EXCEPTION_RECORD))
                    {
                        EXCEPTION_RECORD* pExcRec = (EXCEPTION_RECORD*)buff;

                        eri.m_dwExceptionAddress = (DWORD64)pExcRec->ExceptionAddress;
                    }
                }
            }
        }
        else
            eri.m_dwExceptionAddress = 0;

        // Get number of GUI resources in use
        eri.m_dwGuiResources = GetGuiResources(hProcess, GR_GDIOBJECTS);

        // Determine if GetProcessHandleCount function available
        typedef BOOL(WINAPI* LPGETPROCESSHANDLECOUNT)(HANDLE, PDWORD);
        HMODULE hKernel32 = LoadLibrary(_T("kernel32.dll"));
        if (hKernel32 != NULL)
        {
            LPGETPROCESSHANDLECOUNT pfnGetProcessHandleCount =
                (LPGETPROCESSHANDLECOUNT)GetProcAddress(hKernel32, "GetProcessHandleCount");
            if (pfnGetProcessHandleCount != NULL)
            {
                // Get count of opened handles
                DWORD dwHandleCount = 0;
                BOOL bGetHandleCount = pfnGetProcessHandleCount(hProcess, &dwHandleCount);
                if (bGetHandleCount)
                    eri.m_dwProcessHandleCount = dwHandleCount;
                else
                    eri.m_dwProcessHandleCount = 0;
            }

            FreeLibrary(hKernel32);
            hKernel32 = NULL;
        }

        // Get memory usage info
        PROCESS_MEMORY_COUNTERS meminfo;
        BOOL bGetMemInfo = GetProcessMemoryInfo(hProcess, &meminfo,
            sizeof(PROCESS_MEMORY_COUNTERS));
        if (bGetMemInfo)
        {
            CString sMemUsage;
#ifdef _WIN64
            sMemUsage.Format(_T("%I64u"), meminfo.WorkingSetSize / 1024);
#else
            sMemUsage.Format(_T("%lu"), meminfo.WorkingSetSize / 1024);
#endif
            eri.m_sMemUsage = sMemUsage;
        }

        // Determine the period of time the process is working.
        FILETIME CreationTime, ExitTime, KernelTime, UserTime;
        /*BOOL bGetTimes = */GetProcessTimes(hProcess, &CreationTime, &ExitTime, &KernelTime, &UserTime);
        /*ATLASSERT(bGetTimes);*/
        SYSTEMTIME AppStartTime;
        FileTimeToSystemTime(&CreationTime, &AppStartTime);

        SYSTEMTIME CurTime;
        GetSystemTime(&CurTime);
        ULONG64 uCurTime = Utility::SystemTimeToULONG64(CurTime);
        ULONG64 uStartTime = Utility::SystemTimeToULONG64(AppStartTime);

        // Check that the application works for at least one minute before crash.
        // This might help to avoid cyclic error report generation when the applciation
        // crashes on startup.
        double dDiffTime = (double)(uCurTime - uStartTime) * 10E-08;
        if (dDiffTime < 60)
        {
            m_bAppRestart = FALSE; // Disable restart.
        }
    }

    // Get operating system friendly name from registry.
    Utility::GetOSFriendlyName(eri.m_sOSName);

    // Determine if Windows is 64-bit.
    eri.m_bOSIs64Bit = Utility::IsOS64Bit();

    // Get geographic location.
    Utility::GetGeoLocation(eri.m_sGeoLocation);
}

int CCrashInfoReader::ParseFileList(TiXmlHandle& hRoot, CErrorReportInfo& eri)
{
    strconv_t strconv;

    TiXmlHandle fl = hRoot.FirstChild("FileList");
    if (fl.ToElement() == 0)
    {
        return 1;
    }

    TiXmlHandle fi = fl.FirstChild("FileItem");
    while (fi.ToElement() != 0)
    {
        const char* pszDestFile = fi.ToElement()->Attribute("destfile");
        const char* pszSrcFile = fi.ToElement()->Attribute("srcfile");
        const char* pszDesc = fi.ToElement()->Attribute("description");
        const char* pszMakeCopy = fi.ToElement()->Attribute("makecopy");

        if (pszDestFile != NULL)
        {
            CString sDestFile = strconv.utf82t(pszDestFile);
            ERIFileItem item;
            item.m_sDestFile = sDestFile;
            if (pszSrcFile)
                item.m_sSrcFile = strconv.utf82t(pszSrcFile);
            if (pszDesc)
                item.m_sDesc = strconv.utf82t(pszDesc);

            if (pszMakeCopy)
            {
                if (strcmp(pszMakeCopy, "1") == 0)
                    item.m_bMakeCopy = TRUE;
                else
                    item.m_bMakeCopy = FALSE;
            }
            else
                item.m_bMakeCopy = FALSE;

            // Kaneva - Bug Fix - Use Source File Full Path
            eri.m_FileItems[item.m_sSrcFile] = item;
        }

        fi = fi.ToElement()->NextSibling("FileItem");
    }

    return 0;
}

int CCrashInfoReader::ParseCrashDescription(CString sFileName, BOOL bParseFileItems, CErrorReportInfo& eri)
{
    strconv_t strconv;
    FILE* f = NULL;

#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("rb"));
#else
    _tfopen_s(&f, sFileName, _T("rb"));
#endif

    if (f == NULL)
        return 1;

    TiXmlDocument doc;
    bool bOpen = doc.LoadFile(f);
    if (!bOpen)
        return 1;

    TiXmlHandle hRoot = doc.FirstChild("CrashRpt");
    if (hRoot.ToElement() == NULL)
    {
        fclose(f);
        return 1;
    }

    {
        TiXmlHandle hCrashGUID = hRoot.FirstChild("CrashGUID");
        if (hCrashGUID.ToElement() != NULL)
        {
            if (hCrashGUID.FirstChild().ToText() != NULL)
            {
                const char* szCrashGUID = hCrashGUID.FirstChild().ToText()->Value();
                if (szCrashGUID != NULL)
                    eri.m_sCrashGUID = strconv.utf82t(szCrashGUID);
            }
        }
    }

    {
        TiXmlHandle hAppName = hRoot.FirstChild("AppName");
        if (hAppName.ToElement() != NULL)
        {
            const char* szAppName = hAppName.FirstChild().ToText()->Value();
            if (szAppName != NULL)
                eri.m_sAppName = strconv.utf82t(szAppName);
        }
    }

    {
        TiXmlHandle hAppVersion = hRoot.FirstChild("AppVersion");
        if (hAppVersion.ToElement() != NULL)
        {
            TiXmlText* pText = hAppVersion.FirstChild().ToText();
            if (pText != NULL)
            {
                const char* szAppVersion = pText->Value();
                if (szAppVersion != NULL)
                    eri.m_sAppVersion = strconv.utf82t(szAppVersion);
            }
        }
    }

    {
        TiXmlHandle hImageName = hRoot.FirstChild("ImageName");
        if (hImageName.ToElement() != NULL)
        {
            TiXmlText* pText = hImageName.FirstChild().ToText();
            if (pText != NULL)
            {
                const char* szImageName = pText->Value();
                if (szImageName != NULL)
                    eri.m_sImageName = strconv.utf82t(szImageName);
            }
        }
    }

    {
        TiXmlHandle hSystemTimeUTC = hRoot.FirstChild("SystemTimeUTC");
        if (hSystemTimeUTC.ToElement() != NULL)
        {
            TiXmlText* pText = hSystemTimeUTC.FirstChild().ToText();
            if (pText != NULL)
            {
                const char* szSystemTimeUTC = pText->Value();
                if (szSystemTimeUTC != NULL)
                    eri.m_sSystemTimeUTC = strconv.utf82t(szSystemTimeUTC);
            }
        }
    }

    if (bParseFileItems)
    {
        // Get directory name
        CString sReportDir = sFileName;
        int pos = sFileName.ReverseFind('\\');
        if (pos >= 0)
            sReportDir = sFileName.Left(pos);
        if (sReportDir.Right(1) != _T("\\"))
            sReportDir += _T("\\");

        TiXmlHandle fl = hRoot.FirstChild("FileList");
        if (fl.ToElement() == 0)
        {
            fclose(f);
            return 1;
        }

        TiXmlHandle fi = fl.FirstChild("FileItem");
        while (fi.ToElement() != 0)
        {
            const char* pszDestFile = fi.ToElement()->Attribute("name");
            const char* pszDesc = fi.ToElement()->Attribute("description");
            const char* pszOptional = fi.ToElement()->Attribute("optional");

            if (pszDestFile != NULL)
            {
                CString sDestFile = strconv.utf82t(pszDestFile);
                ERIFileItem item;
                item.m_sDestFile = sDestFile;
                item.m_sSrcFile = sReportDir + sDestFile;
                if (pszDesc)
                    item.m_sDesc = strconv.utf82t(pszDesc);
                item.m_bMakeCopy = FALSE;

                if (pszOptional && strcmp(pszOptional, "1") == 0)
                    item.m_bAllowDelete = true;

                // Check that file really exists
                DWORD dwAttrs = GetFileAttributes(item.m_sSrcFile);
                if (dwAttrs != INVALID_FILE_ATTRIBUTES &&
                    (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
                {
                    // Kaneva - Bug Fix - Use Source File Full Path
                    eri.m_FileItems[item.m_sSrcFile] = item;
                }
            }

            fi = fi.ToElement()->NextSibling("FileItem");
        }
    }

    fclose(f);
    return 0;
}

BOOL CCrashInfoReader::UpdateUserInfo(CString sEmail, CString sDesc)
{
    // This method validates user-provided Email and problem description
    // and (if valid) uptdates internal fields.
    BOOL bResult = TRUE;

    // If an email address was entered, verify that
    // it [1] contains a @ and [2] the last . comes
    // after the @.

    if (sEmail.GetLength() != 0 &&
        (sEmail.Find(_T('@')) < 0 ||
            sEmail.ReverseFind(_T('.')) <
            sEmail.Find(_T('@'))))
    {
        // Invalid email
        bResult = FALSE;
    }
    else
    {
        // Update email
        GetReport(0)->m_sEmailFrom = sEmail;
    }

    // Update problem description
    GetReport(0)->m_sDescription = sDesc;

    // Write user email and problem description to XML
    AddUserInfoToCrashDescriptionXML(
        GetReport(0)->m_sEmailFrom,
        GetReport(0)->m_sDescription);

    // Save E-mail entered by user to INI file for later reuse.
    SetPersistentUserEmail(sEmail);

    return bResult;
}

BOOL CCrashInfoReader::AddUserInfoToCrashDescriptionXML(CString sEmail, CString sDesc)
{
    strconv_t strconv;

    TiXmlDocument doc;

    CString sFileName = m_Reports[0].m_sErrorReportDirName + _T("\\crashrpt.xml");

    FILE* f = NULL;
#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("rb"));
#else
    _tfopen_s(&f, sFileName, _T("rb"));
#endif

    if (f == NULL)
        return FALSE;

    bool bLoad = doc.LoadFile(f);
    fclose(f);
    if (!bLoad)
    {
        return FALSE;
    }

    TiXmlNode* root = doc.FirstChild("CrashRpt");
    if (!root)
    {
        return FALSE;
    }

    // Write user e-mail

    TiXmlHandle hEmail = NULL;

    hEmail = root->FirstChild("UserEmail");
    if (hEmail.ToElement() == NULL)
    {
        hEmail = new TiXmlElement("UserEmail");
        root->LinkEndChild(hEmail.ToElement());
    }

    TiXmlText* email_text = NULL;
    if (hEmail.FirstChild().ToText() != NULL)
    {
        email_text = hEmail.FirstChild().ToText();
        email_text->SetValue(strconv.w2utf8(sEmail));
    }
    else
    {
        email_text = new TiXmlText(strconv.t2utf8(sEmail));
        hEmail.ToElement()->LinkEndChild(email_text);
    }

    // Write problem description

    TiXmlHandle hDesc = NULL;

    hDesc = root->FirstChild("ProblemDescription");
    if (hDesc.ToElement() == NULL)
    {
        hDesc = new TiXmlElement("ProblemDescription");
        root->LinkEndChild(hDesc.ToElement());
    }

    TiXmlText* desc_text = NULL;
    if (hDesc.FirstChild().ToText() != NULL)
    {
        desc_text = hDesc.FirstChild().ToText();
        desc_text->SetValue(strconv.w2utf8(sDesc));
    }
    else
    {
        desc_text = new TiXmlText(strconv.t2utf8(sDesc));
        hDesc.ToElement()->LinkEndChild(desc_text);
    }

#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("w"));
#else
    _tfopen_s(&f, sFileName, _T("w"));
#endif

    if (f == NULL)
        return FALSE;

    bool bSave = doc.SaveFile(f);
    fclose(f);
    if (!bSave)
        return FALSE;
    return TRUE;
}

BOOL CCrashInfoReader::AddFilesToCrashReport(int nReport, std::vector<ERIFileItem> FilesToAdd)
{
    strconv_t strconv;

    TiXmlDocument doc;

    CString sFileName = m_Reports[nReport].m_sErrorReportDirName + _T("\\crashrpt.xml");

    FILE* f = NULL;
#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("rb"));
#else
    _tfopen_s(&f, sFileName, _T("rb"));
#endif

    if (f == NULL)
    {
        return FALSE;
    }

    bool bLoad = doc.LoadFile(f);
    fclose(f);
    if (!bLoad)
    {
        return FALSE;
    }

    TiXmlNode* root = doc.FirstChild("CrashRpt");
    if (!root)
    {
        return FALSE;
    }

    TiXmlHandle hFileItems = root->FirstChild("FileList");
    if (hFileItems.ToElement() == NULL)
    {
        hFileItems = new TiXmlElement("FileList");
        root->LinkEndChild(hFileItems.ToNode());
    }

    unsigned i;
    for (i = 0; i < FilesToAdd.size(); i++)
    {
        // Kaneva - Bug Fix - Use Source File Full Path
        auto it = m_Reports[0].m_FileItems.find(FilesToAdd[i].m_sSrcFile);
        if (it != m_Reports[0].m_FileItems.end())
            continue; // Such file item already exists, skip

        TiXmlHandle hFileItem = new TiXmlElement("FileItem");
        hFileItem.ToElement()->SetAttribute("name", strconv.t2utf8(FilesToAdd[i].m_sDestFile));
        hFileItem.ToElement()->SetAttribute("description", strconv.t2utf8(FilesToAdd[i].m_sDesc));
        if (FilesToAdd[i].m_bAllowDelete)
            hFileItem.ToElement()->SetAttribute("optional", "1");
        hFileItems.ToElement()->LinkEndChild(hFileItem.ToNode());

        // Kaneva - Bug Fix - Use Source File Full Path
        m_Reports[nReport].m_FileItems[FilesToAdd[i].m_sSrcFile] = FilesToAdd[i];

        if (FilesToAdd[i].m_bMakeCopy)
        {
            CString sDestPath = m_Reports[nReport].m_sErrorReportDirName + _T("\\") + FilesToAdd[i].m_sDestFile;
            CopyFile(FilesToAdd[i].m_sSrcFile, sDestPath, TRUE);

            // Kaneva - Bug Fix - Use Source File Full Path
            m_Reports[nReport].m_FileItems[FilesToAdd[i].m_sSrcFile].m_sSrcFile = sDestPath;
        }
    }

#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("w"));
#else
    _tfopen_s(&f, sFileName, _T("w"));
#endif

    if (f == NULL)
        return FALSE;

    bool bSave = doc.SaveFile(f);
    if (!bSave)
        return FALSE;
    fclose(f);
    return TRUE;
}

BOOL CCrashInfoReader::RemoveFilesFromCrashReport(int nReport, std::vector<CString> FilesToRemove)
{
    strconv_t strconv;

    TiXmlDocument doc;

    CString sFileName = m_Reports[nReport].m_sErrorReportDirName + _T("\\crashrpt.xml");

    FILE* f = NULL;
#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("rb"));
#else
    _tfopen_s(&f, sFileName, _T("rb"));
#endif

    if (f == NULL)
    {
        return FALSE;
    }

    bool bLoad = doc.LoadFile(f);
    fclose(f);
    if (!bLoad)
    {
        return FALSE;
    }

    TiXmlNode* root = doc.FirstChild("CrashRpt");
    if (!root)
    {
        return FALSE;
    }

    TiXmlHandle hFileItems = root->FirstChild("FileList");
    if (hFileItems.ToElement() == NULL)
    {
        hFileItems = new TiXmlElement("FileList");
        root->LinkEndChild(hFileItems.ToNode());
    }

    unsigned i;
    for (i = 0; i < FilesToRemove.size(); i++)
    {
        auto it = m_Reports[nReport].m_FileItems.find(FilesToRemove[i]);
        if (it == m_Reports[nReport].m_FileItems.end())
            continue; // Such file item name does not exist, skip

        strconv_t strconv;
        TiXmlHandle hElem = hFileItems.ToElement()->FirstChild("FileItem");
        while (hElem.ToElement() != NULL)
        {
            const char* szName = hElem.ToElement()->Attribute("name");
            if (szName != NULL && strcmp(strconv.t2a(FilesToRemove[i]), szName) == 0)
            {
                hFileItems.ToElement()->RemoveChild(hElem.ToElement());
                break;
            }
            hElem = hElem.ToElement()->NextSibling();
        }

        // Remove the file from error report directory
        Utility::RecycleFile(m_Reports[nReport].m_sErrorReportDirName + _T("\\") + it->second.m_sDestFile, TRUE);

        m_Reports[nReport].m_FileItems.erase(it);
    }

#if _MSC_VER<1400
    f = _tfopen(sFileName, _T("w"));
#else
    _tfopen_s(&f, sFileName, _T("w"));
#endif

    if (f == NULL)
        return FALSE;

    bool bSave = doc.SaveFile(f);
    if (!bSave)
        return FALSE;
    fclose(f);
    return TRUE;
}

BOOL CCrashInfoReader::GetLastRemindDate(SYSTEMTIME& LastDate)
{
    CString sDate = Utility::GetINIString(m_sINIFile, _T("General"), _T("LastRemindDate"));
    if (sDate.IsEmpty())
        return FALSE;

    Utility::UTC2SystemTime(sDate, LastDate);
    return TRUE;
}

BOOL CCrashInfoReader::SetLastRemindDateToday()
{
    // Get current time
    CString sTime;
    Utility::GetSystemTimeUTC(sTime);

    // Write it to INI
    Utility::SetINIString(m_sINIFile, _T("General"), _T("LastRemindDate"), sTime);

    return TRUE;
}

REMIND_POLICY CCrashInfoReader::GetRemindPolicy()
{
    CString sPolicy = Utility::GetINIString(m_sINIFile, _T("General"), _T("RemindPolicy"));

    if (sPolicy.Compare(_T("RemindLater")) == 0)
        return REMIND_LATER;
    else if (sPolicy.Compare(_T("NeverRemind")) == 0)
        return NEVER_REMIND;

    Utility::SetINIString(m_sINIFile, _T("General"), _T("RemindPolicy"), _T("RemindLater"));
    return REMIND_LATER;
}

BOOL CCrashInfoReader::SetRemindPolicy(REMIND_POLICY Policy)
{
    CString sPolicy;
    if (Policy == REMIND_LATER)
        sPolicy = _T("RemindLater");
    else if (Policy == NEVER_REMIND)
        sPolicy = _T("NeverRemind");

    Utility::SetINIString(m_sINIFile, _T("General"), _T("RemindPolicy"), sPolicy);

    return TRUE;
}

BOOL CCrashInfoReader::IsRemindNowOK()
{
    if (GetRemindPolicy() != REMIND_LATER)
        return FALSE; // User doesn want us to remind him

    // Get last remind date
    SYSTEMTIME LastRemind;
    if (!GetLastRemindDate(LastRemind))
    {
        // We never reminded user - its time to remind now!
        return TRUE;
    }

    // Determine the period of time elapsed since the last remind.
    SYSTEMTIME CurTimeUTC, CurTimeLocal;
    GetSystemTime(&CurTimeUTC);
    SystemTimeToTzSpecificLocalTime(NULL, &CurTimeUTC, &CurTimeLocal);
    ULONG64 uCurTime = Utility::SystemTimeToULONG64(CurTimeLocal);
    ULONG64 uLastRemindTime = Utility::SystemTimeToULONG64(LastRemind);

    // Check that at lease one week elapsed
    double dDiffTime = (double)(uCurTime - uLastRemindTime) * 10E-08;
    if (dDiffTime < 7 * 24 * 60 * 60)
    {
        // Now, wait more time
        return FALSE;
    }

    // Remind now
    return TRUE;
}

LONG64 CCrashInfoReader::GetUncompressedReportSize(CErrorReportInfo& eri)
{
    // Calculate summary size of all files included into crash report

    LONG64 lTotalSize = 0;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    CString sMsg;
    BOOL bGetSize = FALSE;
    LARGE_INTEGER lFileSize;

    // Walk through all files in the crash report
    for (const auto& it : eri.m_FileItems)
    {
        // Get file name
        CString sFileName = it.second.m_sSrcFile;

        // Check file exists
        hFile = CreateFile(sFileName,
            GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, NULL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            continue; // File does not exist

        // Get file size
        bGetSize = GetFileSizeEx(hFile, &lFileSize);
        if (!bGetSize)
        {
            CloseHandle(hFile);
            continue;
        }

        // Add to the sum
        lTotalSize += lFileSize.QuadPart;

        // Clean up
        CloseHandle(hFile);
        hFile = INVALID_HANDLE_VALUE;
    }

    // Return summary size
    return lTotalSize;
}

// Returns last error message.
CString CCrashInfoReader::GetErrorMsg()
{
    return m_sErrorMsg;
}

CString CCrashInfoReader::GetPersistentUserEmail()
{
    // Read the E-mail last entered by user and stored in INI file.
    return Utility::GetINIString(m_sINIFile, _T("General"), _T("EmailFrom"));
}

void CCrashInfoReader::SetPersistentUserEmail(LPCTSTR szEmail)
{
    // Save user's E-mail to INI file for later reuse.
    Utility::SetINIString(m_sINIFile, _T("General"), _T("EmailFrom"), szEmail);
}

static CString GetSystemDateUTC()
{
    // Get current date
    CString sDate;
    Utility::GetSystemTimeUTC(sDate);
    sDate.GetBufferSetLength(10); // XXXX-XX-XX
    return sDate;
}

int CCrashInfoReader::GetDailyReportCount()
{
    ATLASSERT(!m_sINIFile.IsEmpty());

    int nReports = 0;

    CString sDate = Utility::GetINIString(m_sINIFile, _T("General"), _T("DailyReportDate"));
    if (!sDate.IsEmpty())
    {
        // Get current date
        if (GetSystemDateUTC() == sDate)
        {
            nReports = _ttoi(Utility::GetINIString(m_sINIFile, _T("General"), _T("DailyReportCount")));
        }
    }

    return nReports;
}

void CCrashInfoReader::SetDailyReportCount(int nReports)
{
    ATLASSERT(!m_sINIFile.IsEmpty());

    // Get current date
    CString sDate = GetSystemDateUTC();

    CString sReports;
    sReports.Format(_T("%d"), nReports);

    Utility::SetINIString(m_sINIFile, _T("General"), _T("DailyReportDate"), sDate);
    Utility::SetINIString(m_sINIFile, _T("General"), _T("DailyReportCount"), sReports);
}
