/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: Utility.h
// Description: Miscellaneous helper functions
// Authors: mikecarruth, zexspectrum
// Date:

#ifndef _UTILITY_H_
#define _UTILITY_H_


namespace Utility
{
    CString getModuleBaseName();
    CString getModuleDirectory(HMODULE hModule);
    CString getModuleFullPath(HMODULE hModule);
    int generateGUID(CString& sGUID);
    CString getFileName(CString sPath);
    BOOL isFileSearchPattern(CString sFileName);
    BOOL createFolder(CString sFolderName);
    CString pathToParentDir(LPCTSTR szPath);
};

#endif	// _UTILITY_H_
