/************************************************************************************* 
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// crashcon.cpp : Defines the entry point for the console application.
//

#include <iostream>
#include <tchar.h>
#include <assert.h>

#include "CrashRpt.h" // Include CrashRpt header

LPVOID lpvState = NULL; // Not used, deprecated

int main(int argc, char* argv[])
{
    argc; // this is to avoid C4100 unreferenced formal parameter warning
    argv; // this is to avoid C4100 unreferenced formal parameter warning

    // Install crash reporting

    CrInstallInfo info;
    memset(&info, 0, sizeof(CrInstallInfo));
    info.cb = sizeof(CrInstallInfo);             // Size of the structure
    info.applicationName = _T("CrashRpt Console Test"); // App name
    info.applicationVersion = _T("1.0.0");              // App version

    // Install crash handlers
    int nInstResult = crInstall(&info);
    assert(nInstResult==0);

    // Check result
    if(nInstResult!=0)
    {
        TCHAR buff[256];
        crGetLastError(buff, 256); // Get last error
        _tprintf(_T("%s\n"), buff); // and output it to the screen
        return FALSE;
    }

    printf("Press crash type you wanna test:\n");
    UINT32 crashType = 0;
    std::cin >> crashType;
    crTestCrash(crashType);

#ifdef TEST_DEPRECATED_FUNCS
    Uninstall(lpvState); // Uninstall exception handlers
#else
    int nUninstRes = crUninstall(); // Uninstall exception handlers
    assert(nUninstRes==0);
    nUninstRes;
#endif //TEST_DEPRECATED_FUNCS

    // Exit
    return 0;
}

