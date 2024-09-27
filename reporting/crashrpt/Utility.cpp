/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: Utility.cpp
// Description: Miscellaneous helper functions
// Authors: mikecarruth, zexspectrum
// Date:

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

// GetBaseFileName
// This helper function returns file name without extension
CString Utility::getFileName(CString sPath)
{
    CString sBase = sPath;
    int pos1 = sPath.ReverseFind('\\');
    if (pos1 >= 0)
        sBase = sBase.Mid(pos1 + 1);

    return sBase;
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

BOOL Utility::isFileSearchPattern(CString sFileName)
{
    // Remove the "\\?\" prefix in case of a long path name
    if (sFileName.Left(4).Compare(_T("\\\\?\\")) == 0)
        sFileName = sFileName.Mid(4);

    // Check if the file name is a search template.
    BOOL bSearchPattern = FALSE;
    int nPos = sFileName.FindOneOf(_T("*?"));
    if (nPos >= 0)
        bSearchPattern = true;
    return bSearchPattern;
}
