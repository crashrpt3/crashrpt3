/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//  are changed infrequently
//

#pragma once

#include <mutex>

// Change these values to use different versions
#define WINVER          0x0501
#define _WIN32_WINNT    0x0501
#define _WIN32_IE       0x0600
#define _RICHEDIT_VER   0x0300
#define _WTL_USE_CSTRING

#include <errno.h>
#include <atldef.h>
#include <atlbase.h>
#include <atlapp.h>
#include <atlwin.h>
#include <atlmisc.h>
#include <new.h>
#include <map>
#include <set>
#include <vector>
#include <stdlib.h>
#include <string>
#include <dbghelp.h>
#include <shlobj.h>
#include <Psapi.h>
#include <time.h>
#include <shellapi.h>
#include <signal.h>
#include <exception>
#include <sys/stat.h>
#include <psapi.h>
#include <rtcapi.h>

extern HANDLE g_hModule;
