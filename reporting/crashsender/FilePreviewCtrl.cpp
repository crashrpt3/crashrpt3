/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

#include "stdafx.h"
#include "FilePreviewCtrl.h"
#include "strconv.h"

#pragma warning(disable:4611)
// DIBSIZE calculates the number of bytes required by an image

#define WIDTHBYTES(bits) ((DWORD)(((bits)+31) & (~31)) / 8)
#define DIBWIDTHBYTES(bi) (DWORD)WIDTHBYTES((DWORD)(bi).biWidth * (DWORD)(bi).biBitCount)
#define _DIBSIZE(bi) (DIBWIDTHBYTES(bi) * (DWORD)(bi).biHeight)
#define DIBSIZE(bi) ((bi).biHeight < 0 ? (-1)*(_DIBSIZE(bi)) : _DIBSIZE(bi))

static inline
unsigned char CLAMP(int x)
{
   return  (unsigned char)((x > 255) ? 255 : (x < 0) ? 0 : x);
}

//-----------------------------------------------------------------------------
// CFileMemoryMapping implementation
//-----------------------------------------------------------------------------

CFileMemoryMapping::CFileMemoryMapping()
{
    // Set member vars to the default values
    m_hFile = INVALID_HANDLE_VALUE;
    m_uFileLength = 0;
    m_hFileMapping = NULL;

    SYSTEM_INFO si;
    GetSystemInfo(&si);
    m_dwAllocGranularity = si.dwAllocationGranularity;
}

CFileMemoryMapping::~CFileMemoryMapping()
{
    Destroy();
}


BOOL CFileMemoryMapping::Init(LPCTSTR szFileName)
{
    if(m_hFile!=INVALID_HANDLE_VALUE)
    {
        // If a file mapping already created, destroy it
        Destroy();
    }

    // Open file handle
    m_hFile = CreateFile(szFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if(m_hFile == INVALID_HANDLE_VALUE)
        return FALSE;

    // Create file mapping
    m_hFileMapping = CreateFileMapping(m_hFile, 0, PAGE_READONLY, 0, 0, 0);
    LARGE_INTEGER size;
    GetFileSizeEx(m_hFile, &size);
    m_uFileLength = size.QuadPart;

    return TRUE;
}

BOOL CFileMemoryMapping::Destroy()
{
    // Unmap all views
    std::map<DWORD, LPBYTE>::iterator it;
    for(it=m_aViewStartPtrs.begin(); it!=m_aViewStartPtrs.end(); it++)
    {
        if(it->second != NULL)
            UnmapViewOfFile(it->second);
    }
    m_aViewStartPtrs.clear();

    // Close file mapping handle
    if(m_hFileMapping!=NULL)
    {
        CloseHandle(m_hFileMapping);
    }

    // Close file handle
    if(m_hFile!=INVALID_HANDLE_VALUE)
    {
        CloseHandle(m_hFile);
    }

    m_hFileMapping = NULL;
    m_hFile = INVALID_HANDLE_VALUE;
    m_uFileLength = 0;

    return TRUE;
}

ULONG64 CFileMemoryMapping::GetSize()
{
    return m_uFileLength;
}

LPBYTE CFileMemoryMapping::CreateView(DWORD dwOffset, DWORD dwLength)
{
    DWORD dwThreadId = ::GetCurrentThreadId();
    DWORD dwBaseOffs = dwOffset-dwOffset%m_dwAllocGranularity;
    DWORD dwDiff = dwOffset-dwBaseOffs;
    LPBYTE pPtr = NULL;

    std::lock_guard<std::recursive_mutex> guard(m_lock);

    std::map<DWORD, LPBYTE>::iterator it = m_aViewStartPtrs.find(dwThreadId);
    if(it!=m_aViewStartPtrs.end())
    {
        ::UnmapViewOfFile(it->second);
    }

    pPtr = (LPBYTE)::MapViewOfFile(m_hFileMapping, FILE_MAP_READ, 0, dwBaseOffs, dwLength+dwDiff);
    if(it!=m_aViewStartPtrs.end())
    {
        it->second = pPtr;
    }
    else
    {
        m_aViewStartPtrs[dwThreadId] = pPtr;
    }

    return (pPtr+dwDiff);
}

//-----------------------------------------------------------------------------
// CFilePreviewCtrl implementation
//-----------------------------------------------------------------------------

CFilePreviewCtrl::CFilePreviewCtrl()
{
    m_xChar = 10;
    m_yChar = 10;
    m_nHScrollPos = 0;
    m_nHScrollMax = 0;
    m_nMaxColsPerPage = 0;
    m_nMaxLinesPerPage = 0;
    m_nMaxDisplayWidth = 0;
    m_uNumLines = 0;
    m_nVScrollPos = 0;
    m_nVScrollMax = 0;
    m_nBytesPerLine = 16;
    m_cchTabLength = 4;
    m_sEmptyMsg = _T("No data to display");
    m_hFont = CreateFont(14, 7, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, 0, 0,
        ANTIALIASED_QUALITY, FIXED_PITCH, _T("Courier"));

    m_hWorkerThread = NULL;
    m_bCancelled = FALSE;
    m_PreviewMode = PREVIEW_HEX;
    m_TextEncoding = ENC_ASCII;
    m_nEncSignatureLen = 0;
}

CFilePreviewCtrl::~CFilePreviewCtrl()
{
    m_fm.Destroy();

    ::DeleteObject(m_hFont);
}

LPCTSTR CFilePreviewCtrl::GetFile()
{
    if(m_sFileName.IsEmpty())
        return NULL;
    return m_sFileName;
}

BOOL CFilePreviewCtrl::SetFile(LPCTSTR szFileName, PreviewMode mode, TextEncoding enc)
{
    // If we are currently processing some file in background,
    // stop the worker thread
    if(m_hWorkerThread!=NULL)
    {
        m_bCancelled = TRUE;
        ::WaitForSingleObject(m_hWorkerThread, INFINITE);
        m_hWorkerThread = NULL;
    }

    std::lock_guard<std::recursive_mutex> lock(m_lock);

    m_sFileName = szFileName;

    if(mode==PREVIEW_AUTO)
        m_PreviewMode = DetectPreviewMode(m_sFileName);
    else
        m_PreviewMode = mode;

    if(szFileName==NULL)
    {
        m_fm.Destroy();
    }
    else
    {
        if(!m_fm.Init(m_sFileName))
        {
            m_sFileName.Empty();
            return FALSE;
        }
    }

    CRect rcClient;
    GetClientRect(&rcClient);

    HDC hDC = GetDC();
    HFONT hOldFont = (HFONT)::SelectObject(hDC, m_hFont);

    LOGFONT lf;
    ZeroMemory(&lf, sizeof(LOGFONT));
    GetObject(m_hFont, sizeof(LOGFONT), &lf);
    m_xChar = lf.lfWidth;
    m_yChar = lf.lfHeight;

    SelectObject(hDC, hOldFont);

    m_nVScrollPos = 0;
    m_nVScrollMax = 0;
    m_nHScrollPos = 0;
    m_nHScrollMax = 0;
    m_aTextLines.clear();
    m_uNumLines = 0;
    m_nMaxDisplayWidth = 0;

    if(m_PreviewMode==PREVIEW_HEX)
    {
        if(m_fm.GetSize()!=0)
        {
            m_nMaxDisplayWidth =
                8 +				//adress
                2 +				//padding
                m_nBytesPerLine * 3 +	//hex column
                1 +				//padding
                m_nBytesPerLine;	//ascii column
        }

        m_uNumLines = m_fm.GetSize() / m_nBytesPerLine;

        if(m_fm.GetSize() % m_nBytesPerLine)
            m_uNumLines++;
    }
    else if(m_PreviewMode==PREVIEW_TEXT)
    {
        if(enc==ENC_AUTO)
            m_TextEncoding = DetectTextEncoding(m_sFileName, m_nEncSignatureLen);
        else
        {
            m_TextEncoding = enc;
            // Determine the length of the signature.
            int nSignatureLen = 0;
            TextEncoding enc2 = DetectTextEncoding(m_sFileName, nSignatureLen);
            if(enc==enc2)
                m_nEncSignatureLen = nSignatureLen;
        }

        m_bCancelled = FALSE;
        m_hWorkerThread = CreateThread(NULL, 0, WorkerThread, this, 0, NULL);
        ::SetTimer(m_hWnd, 0, 250, NULL);
    }

    SetupScrollbars();
    InvalidateRect(NULL, FALSE);
    UpdateWindow();

    return TRUE;
}

PreviewMode CFilePreviewCtrl::GetPreviewMode()
{
    return m_PreviewMode;
}

void CFilePreviewCtrl::SetPreviewMode(PreviewMode mode)
{
    SetFile(m_sFileName, mode);
}

TextEncoding CFilePreviewCtrl::GetTextEncoding()
{
    return m_TextEncoding;
}

void CFilePreviewCtrl::SetTextEncoding(TextEncoding enc)
{
    SetFile(m_sFileName, m_PreviewMode, enc);
}

PreviewMode CFilePreviewCtrl::DetectPreviewMode(LPCTSTR szFileName)
{
    PreviewMode mode = PREVIEW_HEX;
    CString sFileName;
    std::set<CString>::iterator it;
    std::set<CString> aTextFileExtensions;
    CString sExtension;

    {
        sFileName = szFileName;

        int backslash_pos = sFileName.ReverseFind('\\');
        if(backslash_pos>=0)
            sFileName = sFileName.Mid(backslash_pos+1);
        int dot_pos = sFileName.ReverseFind('.');
        if(dot_pos>0)
            sExtension = sFileName.Mid(dot_pos+1);
        sExtension.MakeUpper();

        aTextFileExtensions.insert(_T("TXT"));
        aTextFileExtensions.insert(_T("INI"));
        aTextFileExtensions.insert(_T("LOG"));
        aTextFileExtensions.insert(_T("XML"));
        aTextFileExtensions.insert(_T("HTM"));
        aTextFileExtensions.insert(_T("HTML"));
        aTextFileExtensions.insert(_T("JS"));
        aTextFileExtensions.insert(_T("C"));
        aTextFileExtensions.insert(_T("H"));
        aTextFileExtensions.insert(_T("CPP"));
        aTextFileExtensions.insert(_T("HPP"));

        it = aTextFileExtensions.find(sExtension);
        if(it!=aTextFileExtensions.end())
        {
            mode = PREVIEW_TEXT;
        }
    }

    return mode;
}

TextEncoding CFilePreviewCtrl::DetectTextEncoding(LPCTSTR szFileName, int& nSignatureLen)
{
    TextEncoding enc = ENC_ASCII;
    nSignatureLen = 0;

    FILE* f = NULL;

#if _MSC_VER<1400
    f = _tfopen(szFileName, _T("rb"));
#else
    _tfopen_s(&f, szFileName, _T("rb"));
#endif

    {
        if(f==NULL)
            goto cleanup;

        BYTE signature2[2];
        size_t nRead = fread(signature2, 1, 2, f);
        if(nRead!=2)
            goto cleanup;

        // Compare with UTF-16 LE signature
        if(signature2[0]==0xFF &&
            signature2[1]==0xFE
            )
        {
            enc = ENC_UTF16_LE;
            nSignatureLen = 2;
            goto cleanup;
        }

        // Compare with UTF-16 BE signature
        if(signature2[0]==0xFE &&
            signature2[1]==0xFF
            )
        {
            enc = ENC_UTF16_BE;
            nSignatureLen = 2;
            goto cleanup;
        }

        rewind(f);

        BYTE signature3[3];
        nRead = fread(signature3, 1, 3, f);
        if(nRead!=3)
            goto cleanup;

        // Compare with UTF-8 signature
        if(signature3[0]==0xEF &&
            signature3[1]==0xBB &&
            signature3[2]==0xBF
            )
        {
            enc = ENC_UTF8;
            nSignatureLen = 3;
            goto cleanup;
        }
    }

cleanup:

    fclose(f);

    return enc;
}

DWORD WINAPI CFilePreviewCtrl::WorkerThread(LPVOID lpParam)
{
    CFilePreviewCtrl* pCtrl = (CFilePreviewCtrl*)lpParam;
    pCtrl->DoInWorkerThread();
    return 0;
}

void CFilePreviewCtrl::DoInWorkerThread()
{
    if(m_PreviewMode==PREVIEW_TEXT)
        ParseText();
}

WCHAR swap_bytes(WCHAR src_char)
{
    return (WCHAR)MAKEWORD((src_char>>8), (src_char&0xFF));
}

void CFilePreviewCtrl::ParseText()
{
    DWORD dwFileSize = (DWORD)m_fm.GetSize();
    DWORD dwOffset = 0;
    DWORD dwPrevOffset = 0;
    int nTabs = 0;

    if(dwFileSize!=0)
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        if(m_PreviewMode==PREVIEW_TEXT)
        {
            dwOffset+=m_nEncSignatureLen;
            m_aTextLines.push_back(dwOffset);
        }
        else
        {
            m_aTextLines.push_back(0);
        }

        m_uNumLines++;
    }

    for(;;)
    {
        {
            std::lock_guard<std::recursive_mutex> lock(m_lock);
            if(m_bCancelled)
                break;
        }

        DWORD dwLength = 4096;
        if(dwOffset+dwLength>=dwFileSize)
            dwLength = dwFileSize-dwOffset;

        if(dwLength==0)
            break;

        LPBYTE ptr = m_fm.CreateView(dwOffset, dwLength);

        UINT i;
        for(i=0; i<dwLength; )
        {
            {
                std::lock_guard<std::recursive_mutex> lock(m_lock);
                if(m_bCancelled)
                    break;
            }

            if(m_TextEncoding==ENC_UTF16_LE || m_TextEncoding==ENC_UTF16_BE)
            {
                WCHAR src_char = ((WCHAR*)ptr)[i/2];

                WCHAR c = m_TextEncoding==ENC_UTF16_LE?src_char:swap_bytes(src_char);

                if(c=='\t')
                {
                    nTabs++;
                }
                else if(c=='\n')
                {
                    std::lock_guard<std::recursive_mutex> lock(m_lock);
                    m_aTextLines.push_back(dwOffset+i+2);
                    int cchLineLength = dwOffset+i+2-dwPrevOffset;
                    if(nTabs!=0)
                        cchLineLength += nTabs*(m_cchTabLength-1);

                    m_nMaxDisplayWidth = max(m_nMaxDisplayWidth, cchLineLength);
                    m_uNumLines++;
                    dwPrevOffset = dwOffset+i+2;
                    nTabs = 0;
                }

                i+=2;
            }
            else
            {
                char c = ((char*)ptr)[i];

                if(c=='\t')
                {
                    nTabs++;
                }
                else if(c=='\n')
                {
                    std::lock_guard<std::recursive_mutex> lock(m_lock);
                    m_aTextLines.push_back(dwOffset+i+1);
                    int cchLineLength = dwOffset+i+1-dwPrevOffset;
                    if(nTabs!=0)
                        cchLineLength += nTabs*(m_cchTabLength-1);

                    m_nMaxDisplayWidth = max(m_nMaxDisplayWidth, cchLineLength);
                    m_uNumLines++;
                    dwPrevOffset = dwOffset+i+1;
                    nTabs = 0;
                }

                i++;
            }
        }

        dwOffset += dwLength;
    }

    PostMessage(WM_FPC_COMPLETE);
}

void CFilePreviewCtrl::SetEmptyMessage(CString sText)
{
    m_sEmptyMsg = sText;
}

BOOL CFilePreviewCtrl::SetBytesPerLine(int nBytesPerLine)
{
    if(nBytesPerLine<0)
        return FALSE;

    m_nBytesPerLine = nBytesPerLine;
    return TRUE;
}

void CFilePreviewCtrl::SetupScrollbars()
{
    std::lock_guard<std::recursive_mutex> lock(m_lock);
    CRect rcClient;
    GetClientRect(&rcClient);

    SCROLLINFO sInfo;

    //	Vertical scrollbar

    m_nMaxLinesPerPage = (int)min(m_uNumLines, rcClient.Height() / m_yChar);
    m_nVScrollMax = (int)max(0, m_uNumLines-1);
    m_nVScrollPos = (int)min(m_nVScrollPos, m_nVScrollMax-m_nMaxLinesPerPage+1);

    sInfo.cbSize = sizeof(SCROLLINFO);
    sInfo.fMask = SIF_PAGE | SIF_POS | SIF_RANGE;
    sInfo.nMin	= 0;
    sInfo.nMax	= m_nVScrollMax;
    sInfo.nPos	= m_nVScrollPos;
    sInfo.nPage	= min(m_nMaxLinesPerPage, m_nVScrollMax+1);
    SetScrollInfo (SB_VERT, &sInfo, TRUE);

    //	Horizontal scrollbar

    m_nMaxColsPerPage = min(m_nMaxDisplayWidth+1, rcClient.Width() / m_xChar);
    m_nHScrollMax = max(0, m_nMaxDisplayWidth-1);
    m_nHScrollPos = min(m_nHScrollPos, m_nHScrollMax-m_nMaxColsPerPage+1);

    sInfo.cbSize = sizeof(SCROLLINFO);
    sInfo.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
    sInfo.nMin	= 0;
    sInfo.nMax	= m_nHScrollMax;
    sInfo.nPos	= m_nHScrollPos;
    sInfo.nPage	= min(m_nMaxColsPerPage, m_nHScrollMax+1);

    SetScrollInfo (SB_HORZ, &sInfo, TRUE);

}

//
//	Create 1 line of a hex-dump, given a buffer of BYTES
//
CString CFilePreviewCtrl::FormatHexLine(LPBYTE pData, int nBytesInLine, ULONG64 uLineOffset)
{
    CString sResult;
    CString str;
    int i;

    //print the hex address
    str.Format(_T("%08X  "), uLineOffset);
    sResult += str;

    //print hex data
    for(i = 0; i < nBytesInLine; i++)
    {
        str.Format(_T("%02X "), pData[i]);
        sResult += str;
    }

    //print some blanks if this isn't a full line
    for(; i < m_nBytesPerLine; i++)
    {
        str.Format(_T("   "));
        sResult += str;
    }

    //print a gap between the hex and ascii
    sResult += _T(" ");

    //print the ascii
    for(i = 0; i < nBytesInLine; i++)
    {
        BYTE c = pData[i];
        if(c < 32 || c > 128) c = '.';
        str.Format( _T("%c"), c);
        sResult += str;
    }

    //print some blanks if this isn't a full line
    for(; i < m_nBytesPerLine; i++)
    {
        sResult += _T(" ");
    }

    return sResult;
}

//
//	Draw 1 line to the display
//
void CFilePreviewCtrl::DrawHexLine(HDC hdc, DWORD nLineNo)
{
    int nBytesPerLine = m_nBytesPerLine;

    if(m_fm.GetSize() - nLineNo * m_nBytesPerLine < (UINT)m_nBytesPerLine)
        nBytesPerLine= (DWORD)m_fm.GetSize() - nLineNo * m_nBytesPerLine;

    //get data from our file mapping
    LPBYTE ptr = m_fm.CreateView(nLineNo * m_nBytesPerLine, nBytesPerLine);

    //convert the data into a one-line hex-dump
    CString str = FormatHexLine(ptr, nBytesPerLine, nLineNo*m_nBytesPerLine );

    //draw this line to the screen
    TextOut(hdc, -(int)(m_nHScrollPos * m_xChar),
        (nLineNo - m_nVScrollPos) * (m_yChar-1) , str, str.GetLength());
}

void CFilePreviewCtrl::DrawTextLine(HDC hdc, DWORD nLineNo)
{
    CRect rcClient;
    GetClientRect(&rcClient);

    DWORD dwOffset = 0;
    DWORD dwLength = 0;
    {
        std::lock_guard<std::recursive_mutex> lock(m_lock);
        dwOffset = m_aTextLines[nLineNo];
        if(nLineNo==m_uNumLines-1)
            dwLength = (DWORD)m_fm.GetSize() - dwOffset;
        else
            dwLength = m_aTextLines[nLineNo+1]-dwOffset-1;
    }

    if(dwLength==0)
        return;

    //get data from our file mapping
    LPBYTE ptr = m_fm.CreateView(dwOffset, dwLength);

    //draw this line to the screen
    CRect rcText;
    rcText.left = -(int)(m_nHScrollPos * m_xChar);
    rcText.top = (nLineNo - m_nVScrollPos) * m_yChar;
    rcText.right = rcClient.right;
    rcText.bottom = rcText.top + m_yChar;
    DRAWTEXTPARAMS params;
    memset(&params, 0, sizeof(DRAWTEXTPARAMS));
    params.cbSize = sizeof(DRAWTEXTPARAMS);
    params.iTabLength = m_xChar*m_cchTabLength;

    DWORD dwFlags = DT_LEFT|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_EXPANDTABS;

    if(m_TextEncoding==ENC_UTF8)
    {
        // Decode line
        strconv_t strconv;
        LPCWSTR szLine = strconv.utf82w((char*)ptr, dwLength-1);
        DrawTextExW(hdc, (LPWSTR)szLine, -1,  &rcText, dwFlags, &params);
    }
    else if(m_TextEncoding==ENC_UTF16_LE)
    {
        DrawTextExW(hdc, (WCHAR*)ptr, dwLength/2-1,  &rcText,
            dwFlags, &params);
    }
    else if(m_TextEncoding==ENC_UTF16_BE)
    {
        // Decode line
        strconv_t strconv;
        LPCWSTR szLine = strconv.w2w_be((WCHAR*)ptr, dwLength/2-1);
        DrawTextExW(hdc, (LPWSTR)szLine, -1,  &rcText, dwFlags, &params);
    }
    else // ASCII
    {
        DrawTextExA(hdc, (char*)ptr, dwLength-1,  &rcText,
            dwFlags, &params);
    }
}

void CFilePreviewCtrl::DoPaintEmpty(HDC hDC)
{
    RECT rcClient;
    GetClientRect(&rcClient);

    HFONT hOldFont = (HFONT)SelectObject(hDC, m_hFont);

    FillRect(hDC, &rcClient, (HBRUSH)GetStockObject(WHITE_BRUSH));

    CRect rcText;
    DrawTextEx(hDC, m_sEmptyMsg.GetBuffer(0), -1, &rcText, DT_CALCRECT, NULL);

    rcText.MoveToX(rcClient.right/2-rcText.right/2);
    DrawTextEx(hDC, m_sEmptyMsg.GetBuffer(0), -1, &rcText, DT_LEFT, NULL);

    SelectObject(hDC, hOldFont);
}

void CFilePreviewCtrl::DoPaintText(HDC hDC)
{
    HFONT hOldFont = (HFONT)SelectObject(hDC, m_hFont);

    RECT rcClient;
    GetClientRect(&rcClient);

    HRGN hRgn = CreateRectRgn(0, 0, rcClient.right, rcClient.bottom);
    SelectClipRgn(hDC, hRgn);

    FillRect(hDC, &rcClient, (HBRUSH)GetStockObject(WHITE_BRUSH));

    int iPaintBeg = max(0, m_nVScrollPos);			//only update the lines that
    int iPaintEnd = (int)min(m_uNumLines, m_nVScrollPos + rcClient.bottom / m_yChar);		//need updating!!!!!!!!!!!!!

    if(rcClient.bottom % m_yChar) iPaintEnd++;
    if(iPaintEnd > m_uNumLines) iPaintEnd--;

    //
    //	Only paint what needs to be!
    //
    int i;
    for(i = iPaintBeg; i < iPaintEnd; i++)
    {
        if(m_PreviewMode==PREVIEW_HEX)
            DrawHexLine(hDC, i);
        else
            DrawTextLine(hDC, i);
    }

    SelectObject(hDC, hOldFont);
}

void CFilePreviewCtrl::DoPaint(HDC hDC)
{
    if(m_PreviewMode==PREVIEW_TEXT ||
        m_PreviewMode==PREVIEW_HEX)
    {
        DoPaintText(hDC);
    }
}

LRESULT CFilePreviewCtrl::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled)
{
    m_fm.Destroy();
    bHandled = FALSE;
    return 0;
}

LRESULT CFilePreviewCtrl::OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SetupScrollbars();

    InvalidateRect(NULL, FALSE);
    UpdateWindow();

    return 0;
}

LRESULT CFilePreviewCtrl::OnHScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SCROLLINFO info;
    int nHScrollInc = 0;
    int  nOldHScrollPos = m_nHScrollPos;

    switch (LOWORD(wParam))
    {
    case SB_LEFT:
        m_nHScrollPos = 0;
        break;

    case SB_RIGHT:
        m_nHScrollPos = m_nHScrollMax + 1;
        break;

    case SB_LINELEFT:
        if(m_nHScrollPos > 0) --m_nHScrollPos;
        break;

    case SB_LINERIGHT:
        m_nHScrollPos++;
        break;

    case SB_PAGELEFT:
        m_nHScrollPos -= m_nMaxColsPerPage;
        if(m_nHScrollPos > nOldHScrollPos)
            m_nHScrollPos = 0;
        break;

    case SB_PAGERIGHT:
        m_nHScrollPos += m_nMaxColsPerPage;
        break;

    case SB_THUMBPOSITION:
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_TRACKPOS;
        GetScrollInfo(SB_HORZ, &info);
        m_nHScrollPos = info.nTrackPos;
        break;

    case SB_THUMBTRACK:
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_TRACKPOS;
        GetScrollInfo(SB_HORZ, &info);
        m_nHScrollPos = info.nTrackPos;
        break;

    default:
        nHScrollInc = 0;
    }

    //keep scroll position in range
    if(m_nHScrollPos  > m_nHScrollMax - m_nMaxColsPerPage + 1)
        m_nHScrollPos = m_nHScrollMax - m_nMaxColsPerPage + 1;

    nHScrollInc = m_nHScrollPos - nOldHScrollPos;

    if (nHScrollInc)
    {

        //finally setup the actual scrollbar!
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_POS;
        info.nPos = m_nHScrollPos;
        SetScrollInfo(SB_HORZ, &info, TRUE);

        InvalidateRect(NULL);
    }

    return 0;
}

LRESULT CFilePreviewCtrl::OnVScroll(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // React to the various vertical scroll related actions.
    // CAUTION:
    // All sizes are in unsigned values, so be carefull
    // when testing for < 0 etc

    SCROLLINFO info;
    int nVScrollInc;
    int  nOldVScrollPos = m_nVScrollPos;

    switch (LOWORD(wParam))
    {
    case SB_TOP:
        m_nVScrollPos = 0;
        break;

    case SB_BOTTOM:
        m_nVScrollPos = m_nVScrollMax - m_nMaxLinesPerPage + 1;
        break;

    case SB_LINEUP:
        if(m_nVScrollPos > 0) --m_nVScrollPos;
        break;

    case SB_LINEDOWN:
        m_nVScrollPos++;
        break;

    case SB_PAGEUP:
        m_nVScrollPos -= max(1, m_nMaxLinesPerPage);
        if(m_nVScrollPos > nOldVScrollPos) m_nVScrollPos = 0;
        break;

    case SB_PAGEDOWN:
        m_nVScrollPos += max(1, m_nMaxLinesPerPage);
        break;

    case SB_THUMBPOSITION:
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_TRACKPOS;
        GetScrollInfo(SB_VERT, &info);
        m_nVScrollPos = info.nTrackPos;
        break;

    case SB_THUMBTRACK:
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_TRACKPOS;
        GetScrollInfo(SB_VERT, &info);
        m_nVScrollPos = info.nTrackPos;
        break;

    default:
        nVScrollInc = 0;
    }

    //keep scroll position in range
    if(m_nVScrollPos > m_nVScrollMax - m_nMaxLinesPerPage+1)
        m_nVScrollPos = m_nVScrollMax - m_nMaxLinesPerPage+1;

    if(m_nVScrollPos<0)
        m_nVScrollPos = 0;

    nVScrollInc = m_nVScrollPos - nOldVScrollPos;

    if (nVScrollInc)
    {

        //finally setup the actual scrollbar!
        info.cbSize = sizeof(SCROLLINFO);
        info.fMask = SIF_POS;
        info.nPos = m_nVScrollPos;
        SetScrollInfo(SB_VERT, &info, TRUE);

        InvalidateRect(NULL);
    }

    return 0;
}

LRESULT CFilePreviewCtrl::OnEraseBkgnd(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // Do nothing
    return 0;
}

LRESULT CFilePreviewCtrl::OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    PAINTSTRUCT ps;
    HDC hDC = BeginPaint(&ps);

    {
        CMemoryDC memDC(hDC, ps.rcPaint);

        if(m_fm.GetSize()==0)
            DoPaintEmpty(memDC);
        else
            DoPaint(memDC);
    }

    EndPaint(&ps);

    return 0;
}

LRESULT CFilePreviewCtrl::OnTimer(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SetupScrollbars();
    InvalidateRect(NULL);
    return 0;
}

LRESULT CFilePreviewCtrl::OnComplete(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    KillTimer(0);
    SetupScrollbars();
    InvalidateRect(NULL);
    return 0;
}

LRESULT CFilePreviewCtrl::OnLButtonDown(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    SetFocus();
    return 0;
}

LRESULT CFilePreviewCtrl::OnRButtonUp(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    NMHDR nmhdr;
    nmhdr.hwndFrom = m_hWnd;
    nmhdr.code = NM_RCLICK;
    nmhdr.idFrom = GetWindowLong(GWL_ID);

    HWND hWndParent = GetParent();
    ::SendMessage(hWndParent, WM_NOTIFY, 0, (LPARAM)&nmhdr);
    return 0;
}

LRESULT CFilePreviewCtrl::OnMouseWheel(UINT /*uMsg*/, WPARAM wParam, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    if(m_PreviewMode!=PREVIEW_TEXT &&
        m_PreviewMode!=PREVIEW_HEX)
        return 0;

    int nDistance =  GET_WHEEL_DELTA_WPARAM(wParam)/WHEEL_DELTA;
    int nLinesPerDelta = m_nMaxLinesPerPage!=0?m_nVScrollMax/m_nMaxLinesPerPage:0;

    SCROLLINFO info;
    memset(&info, 0, sizeof(SCROLLINFO));
    info.cbSize = sizeof(SCROLLINFO);
    info.fMask = SIF_ALL;
    GetScrollInfo(SB_VERT, &info);
    info.nPos -=nDistance*nLinesPerDelta;
    SetScrollInfo(SB_VERT, &info, TRUE);

    SendMessage(WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, info.nPos), 0);
    return 0;
}
