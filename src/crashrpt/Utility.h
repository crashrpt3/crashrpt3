#pragma once

namespace Utility
{
    CString getModuleBaseName();
    CString getModuleDirectory(HMODULE hModule);
    CString getModuleFullPath(HMODULE hModule);
    int generateGUID(CString& sGUID);
    BOOL createFolder(CString sFolderName);
    CString pathToParentDir(LPCTSTR szPath);
};
