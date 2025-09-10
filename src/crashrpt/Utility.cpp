#include "stdafx.h"
#include "Utility.h"

CString Utility::getModuleBaseName()
{
    TCHAR szFileName[MAX_PATH + 1] = { 0 };
    ::GetModuleFileName(NULL, szFileName, MAX_PATH);
    CString szAppName = szFileName;
    szAppName = szAppName.Mid(szAppName.ReverseFind(_T('\\')) + 1).SpanExcluding(_T("."));
    return szAppName;
}

CString Utility::getModuleFullPath(HMODULE hModule)
{
    CString szFullPath;
    LPTSTR szBuffer = szFullPath.GetBufferSetLength(MAX_PATH + 1);
    ::GetModuleFileName(hModule, szBuffer, MAX_PATH);
    szFullPath.ReleaseBuffer();
    return szFullPath;
}

CString Utility::getModuleDirectory(HMODULE hModule)
{
    CString szFullPath = getModuleFullPath(hModule);
    return pathToParentDir(szFullPath);
}

int Utility::generateGUID(CString& szGUID)
{
    int ret = 1;
    szGUID.Empty();
    RPC_WSTR szUUID = nullptr;
    GUID* pGUID = new GUID;
    if (pGUID)
    {
        HRESULT hr = ::CoCreateGuid(pGUID);
        if (SUCCEEDED(hr))
        {
            hr = ::UuidToStringW(pGUID, &szUUID);
            if (SUCCEEDED(hr) && szUUID)
            {
                szGUID = (LPCWSTR)szUUID;
                ::RpcStringFree(&szUUID);
                ret = 0;
            }
        }
        delete pGUID;
    }
    return ret;
}

// Creates a folder. If some intermediate folders in the path do not exist,
// it creates them.
BOOL Utility::createFolder(CString sFolderName)
{
    CString sIntermediateFolder;

    // Skip disc drive name "X:\" if presents
    int start = sFolderName.Find(':', 0);
    if (start >= 0)
        start += 2;

    int pos = start;
    for (;;)
    {
        pos = sFolderName.Find('\\', pos);
        if (pos < 0)
        {
            sIntermediateFolder = sFolderName;
        }
        else
        {
            sIntermediateFolder = sFolderName.Left(pos);
        }

        BOOL bCreate = CreateDirectory(sIntermediateFolder, NULL);
        if (!bCreate && GetLastError() != ERROR_ALREADY_EXISTS)
            return FALSE;

        DWORD dwAttrs = GetFileAttributes(sIntermediateFolder);
        if ((dwAttrs & FILE_ATTRIBUTE_DIRECTORY) == 0)
            return FALSE;

        if (pos == -1)
            break;

        pos++;
    }

    return TRUE;
}

CString Utility::pathToParentDir(LPCTSTR szPath)
{
    int len = lstrlen(szPath);
    int i = len - 2;
    for (; i >= 0; --i)
    {
        if (_T('\\') == szPath[i] || _T('/') == szPath[i])
        {
            break;
        }
    }

    CString parent;
    if (i > 0)
    {
        parent.Append(szPath, i + 1);
    }
    return parent;
}

std::string Utility::w2u(const wchar_t* wstr)
{
    std::string ret;
    if (wstr)
    {
        ret = (const char*)CW2A(wstr, CP_UTF8);
    }
    return ret;
}

std::wstring Utility::u2w(const char* str)
{
    std::wstring wstr;
    if (str)
    {
        wstr = (const wchar_t*)CA2W(str, CP_UTF8);
    }
    return wstr;
}
