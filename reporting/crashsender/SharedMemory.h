#pragma once

// Generic block header.
struct GENERIC_HEADER
{
    BYTE m_uchMagic[3]; // Magic sequence.
    WORD m_wSize;       // Total bytes occupied by this block.
};

// String block description.
struct STRING_DESC
{
    BYTE m_uchMagic[3]; // Magic sequence "STR".
    WORD m_wSize;       // Total bytes occupied by this block.
    // This structure is followed by (m_wSize-sizeof(STRING_DESC) bytes of string data.
};

// File item entry.
struct FILE_ITEM
{
    BYTE m_uchMagic[3]; // Magic sequence "FIL"
    WORD m_wSize;       // Total bytes occupied by this block.
    DWORD m_dwSrcFilePathOffs; // Path to the original file.
    DWORD m_dwDstFileNameOffs; // Name of the destination file.
    DWORD m_dwDescriptionOffs; // File description.
    BOOL  m_bMakeCopy;         // Should we make a copy of this file on crash?
    BOOL  m_bAllowDelete;      // Should allow user to delete the file from crash report?
};

// User-defined property.
struct CUSTOM_PROP
{
    BYTE m_uchMagic[3];  // Magic sequence "CPR"
    WORD m_wSize;        // Total bytes occupied by this block.
    DWORD m_dwNameOffs;  // Property name.
    DWORD m_dwValueOffs; // Property value.
};

// Crash description.
struct CRASH_DESCRIPTION
{
    BYTE chMagic[3];  // Magic sequence "CRD"
    WORD cb;        // Total bytes occupied by this block.
    DWORD dwTotalSize; // Total size of the whole used shared mem.
    DWORD dwCrashRptVersion;         // Version of CrashRpt.
    UINT uFileCount;             // Count of file item records.
    UINT uPropertyCount;           // Count of user-defined properties.
    MINIDUMP_TYPE uMinidumpType;  // Minidump type.
    DWORD m_dwUrlOffs;             // Offset of recipient URL.
    DWORD m_dwAppNameOffs;         // Offset of application name.
    DWORD m_dwAppVersionOffs;      // Offset of app version.
    DWORD m_dwCrashGUIDOffs;       // Offset to crash GUID.
    DWORD m_dwUnsentCrashReportsFolderOffs;  // Offset of folder name where error reports are stored.
    DWORD m_dwPrivacyPolicyURLOffs; // Offset of privacy policy URL.
    DWORD m_dwPathToDebugHelpDllOffs; // Offset of dbghelp path.
    DWORD m_dwImageNameOffs;       // Offset to image name.
    DWORD dwProcessId;           // Process ID.
    DWORD m_dwThreadId;            // Thread ID.
    int m_nExceptionType;          // Exception type.
    DWORD m_dwExceptionCode;       // SEH exception code.
    DWORD m_dwInvParamExprOffs;    // Invalid parameter expression.
    DWORD m_dwInvParamFunctionOffs; // Invalid parameter function.
    DWORD m_dwInvParamFileOffs;    // Invalid parameter file.
    UINT  m_uInvParamLine;         // Invalid parameter line.
    UINT m_uFPESubcode;            // FPE subcode.
    PEXCEPTION_POINTERS m_pExceptionPtrs; // Exception pointers.
};

class SharedMemory
{
public:
    SharedMemory();
    ~SharedMemory();

    bool create(LPCTSTR szName);
    bool open(LPCTSTR szName);
    void close();

    bool isOpened();

    LPBYTE mapView(DWORD dwOffset, DWORD dwLength);
    void unmapView(LPBYTE pViewPtr);

private:
    CString                  m_szName;
    HANDLE                   m_hFileMapping = nullptr;
    DWORD                    m_dwAllocationGranularity = 0;
    ULONG64                  m_uSize = 0;
    std::map<LPBYTE, LPBYTE> m_mapViewStartPtrs;
};
