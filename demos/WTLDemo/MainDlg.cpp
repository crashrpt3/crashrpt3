/************************************************************************************* 
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// MainDlg.cpp : implementation of the CMainDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"
#include "AboutDlg.h"
#include "DocumentDlg.h"
#include "MainDlg.h"
#include "CrashThread.h"

#define MANUAL_REPORT 128

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
    return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnIdle()
{
    return FALSE;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // center the dialog on the screen
    CenterWindow();

    // set icons
    HICON hIcon = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
        IMAGE_ICON, ::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    SetIcon(hIcon, TRUE);
    HICON hIconSmall = (HICON)::LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(IDR_MAINFRAME), 
        IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    SetIcon(hIconSmall, FALSE);

    int nItem = 0;

    m_cboThread = GetDlgItem(IDC_THREAD);

    nItem = m_cboThread.AddString(_T("Main thread"));
    m_cboThread.SetItemData(nItem, 0);

    nItem = m_cboThread.AddString(_T("Worker thread"));
    m_cboThread.SetItemData(nItem, 1);

    m_cboThread.SetCurSel(0);

    m_cboExcType = GetDlgItem(IDC_EXCTYPE);

    nItem = m_cboExcType.AddString(_T("(0) SEH exception"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SEH);

    nItem = m_cboExcType.AddString(_T("(1) terminate"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_TERMINATE_CALL);

    nItem = m_cboExcType.AddString(_T("(2) unexpected"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_UNEXPECTED_CALL);

    nItem = m_cboExcType.AddString(_T("(3) cpp pure virtual method call"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_CPP_PURE);

    nItem = m_cboExcType.AddString(_T("(4) security buffer overrun"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SECURITY);

    nItem = m_cboExcType.AddString(_T("(5) invalid parameter"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_INVALID_PARAMETER);

    nItem = m_cboExcType.AddString(_T("(6) new operator fault"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_CPP_NEW_OPERATOR);

    nItem = m_cboExcType.AddString(_T("(7) SIGABRT"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGABRT);

    nItem = m_cboExcType.AddString(_T("(8) SIGFPE"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGFPE);

    nItem = m_cboExcType.AddString(_T("(9) SIGILL"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGILL);

    nItem = m_cboExcType.AddString(_T("(10) SIGINT"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGINT);

    nItem = m_cboExcType.AddString(_T("(11) SIGSEGV"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGSEGV);

    nItem = m_cboExcType.AddString(_T("(12) SIGTERM"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_SIGTERM);

    nItem = m_cboExcType.AddString(_T("(13) non continuable"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_NONCONTINUABLE);

    nItem = m_cboExcType.AddString(_T("(14) cpp throw"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_CPP_THROW);

	nItem = m_cboExcType.AddString(_T("(15) stack overflow"));
    m_cboExcType.SetItemData(nItem, CR_TEST_CRASH_STACK_OVERFLOW);

    nItem = m_cboExcType.AddString(_T("Manual report"));
    m_cboExcType.SetItemData(nItem, MANUAL_REPORT);

    m_cboExcType.SetCurSel(0);

    m_dlgAbout.Create(m_hWnd);

    m_nDocNum = 0;

    // register object for message filtering and idle updates
    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != NULL);
    pLoop->AddMessageFilter(this);
    pLoop->AddIdleHandler(this);

    UIAddChildWindowContainer(m_hWnd);
	
    if(m_bRestarted)
    {
        PostMessage(WM_POSTCREATE);    
    }

    return TRUE;
}

LRESULT CMainDlg::OnPostCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    MessageBox(_T("The application was restarted after crash."), _T("Restarted"), MB_OK);
    return 0;
}

LRESULT CMainDlg::OnOK(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    DoCrash();
    return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CloseDialog(wID);
    return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
    DestroyWindow();
    ::PostQuitMessage(nVal);
}

void CMainDlg::DoCrash()
{
    int nSel = m_cboThread.GetCurSel();
    int nThread = (int)m_cboThread.GetItemData(nSel);

    nSel = m_cboExcType.GetCurSel();
    int nExcType = (int)m_cboExcType.GetItemData(nSel);

    if(nThread==0) // The main thread
    {    
        if(nExcType==MANUAL_REPORT)
        {
            test_generate_report();
            return;
        }
        else
        {
            int nResult = crTestCrash(nExcType);
            if(nResult!=0)
            {
                TCHAR szErrorMsg[256];
                CString sError = _T("Error creating exception situation!\nErrorMsg:");
                crGetLastError(szErrorMsg, 256);
                sError+=szErrorMsg;
                MessageBox(sError);
            }
        }  
    }
    else // Worker thread
    {
        extern CrashThreadInfo g_CrashThreadInfo;
        g_CrashThreadInfo.m_ExceptionType = nExcType;
        SetEvent(g_CrashThreadInfo.m_hWakeUpEvent); // wake up the working thread
    }
}

LRESULT CMainDlg::OnFileNewWindow(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CDocumentDlg* pDlg = new CDocumentDlg();
    pDlg->Create(m_hWnd);
    pDlg->ShowWindow(SW_SHOW);
    CString sTitle;
    sTitle.Format(_T("Document %d"), ++m_nDocNum);
    pDlg->SetWindowText(sTitle);
    return 0;
}

LRESULT CMainDlg::OnHelpAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{    
    m_dlgAbout.ShowWindow(SW_SHOW);
    return 0;
}
