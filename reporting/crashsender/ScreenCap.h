/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

#ifndef __SCREENCAP_H__
#define __SCREENCAP_H__

#include "stdafx.h"

// Window information
struct WindowInfo
{
    CString m_sTitle; // Window title
    CRect m_rcWnd;    // Window rect
    DWORD dwStyle;
    DWORD dwExStyle;
};

// Monitor info
struct MonitorInfo
{
    CString m_sDeviceID; // Device ID
    CRect m_rcMonitor;   // Monitor rectangle in screen coordinates
    CString m_sFileName; // Image file name corresponding to this monitor
};

// Desktop screen shot info
struct ScreenshotInfo
{
	// Constructor
    ScreenshotInfo()
    {
        m_bValid = FALSE;
    }

    BOOL m_bValid;           // If TRUE, this structure's fields are valid.
	time_t m_CreateTime;     // Time of screenshot capture.
    CRect m_rcVirtualScreen; // Coordinates of virtual screen rectangle.
    std::vector<MonitorInfo> m_aMonitors; // The list of monitors captured.
    std::vector<WindowInfo> m_aWindows; // The list of windows captured.
};

// Screenshot type
enum SCREENSHOT_TYPE
{
	SCREENSHOT_TYPE_VIRTUAL_SCREEN      = 0, // Screenshot of entire desktop.
	SCREENSHOT_TYPE_MAIN_WINDOW         = 1, // Screenshot of given process' main window.
	SCREENSHOT_TYPE_ALL_PROCESS_WINDOWS = 2  // Screenshot of all process windows.
};

// What format to use when saving screenshots
enum SCREENSHOT_IMAGE_FORMAT
{
    SCREENSHOT_FORMAT_PNG = 0, // Use PNG format
    SCREENSHOT_FORMAT_JPG = 1, // Use JPG format
    SCREENSHOT_FORMAT_BMP = 2  // Use BMP format
};

#endif //__SCREENCAP_H__


