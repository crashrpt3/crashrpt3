/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: VideoRec.cpp
// Description: Video recording functionality.
// Authors: zexspectrum
// Date: Sep 2013

#include "stdafx.h"
#include "VideoRec.h"
#include "Utility.h"
#include "math.h"

static int ilog(unsigned _v){
  int ret;
  for(ret=0;_v;ret++)_v>>=1;
  return ret;
}

//-----------------------------------------------
// CVideoRecorder impl
//-----------------------------------------------

CVideoRecorder::CVideoRecorder()
{
    m_ScreenshotType = SCREENSHOT_TYPE_VIRTUAL_SCREEN;
    m_nVideoDuration = 60000;
    m_nVideoFrameInterval = 300;
    m_dwProcessId = 0;
    m_nFrameCount = 0;
    m_nFileId = 0;
    m_nFrameId = 0;
    m_nVideoQuality = 5;
    m_DesiredFrameSize.cx = 0;
    m_DesiredFrameSize.cy = 0;
    m_hbmpFrame = NULL;
    m_pFrameBits = NULL;
    m_pDIB = NULL;
    m_bInitialized = FALSE;
}

CVideoRecorder::~CVideoRecorder()
{
    Destroy();
}

BOOL CVideoRecorder::Init(LPCTSTR szSaveToDir,
    SCREENSHOT_TYPE type,
    DWORD dwProcessId,
    int nVideoDuration,
    int nVideoFrameInterval,
    int nVideoQuality,
    SIZE* pDesiredFrameSize)
{
    // Validate input params
    if(nVideoDuration<=0 || nVideoFrameInterval<=0)
    {
        // Invalid arg
        return FALSE;
    }

    // Save params
    m_sSaveToDir = szSaveToDir;
    m_ScreenshotType = type;
    m_nVideoDuration = nVideoDuration;
    m_nVideoFrameInterval = nVideoFrameInterval;
    m_nVideoQuality = nVideoQuality;
    m_dwProcessId = dwProcessId;

    // Save desired frame size
    if(pDesiredFrameSize)
        m_DesiredFrameSize = *pDesiredFrameSize;
    else
    {
        m_DesiredFrameSize.cx = 0; // auto
        m_DesiredFrameSize.cy = 0;
    }

    // Calculate max frame count
    m_nFrameCount = m_nVideoDuration/m_nVideoFrameInterval;
    m_nFileId = 0;
    m_nFrameId = 0;

    // Create folder where to save video frames
    CString sDirName = m_sSaveToDir + _T("\\~temp_video");
    if(!Utility::CreateFolder(sDirName))
    {
        // Error creating temp folder
        return FALSE;
    }

    // Done
    m_bInitialized = TRUE;
    return TRUE;
}

BOOL CVideoRecorder::IsInitialized()
{
    return m_bInitialized;
}

void CVideoRecorder::Destroy()
{
    // Remove temp files
    if(!m_sSaveToDir.IsEmpty())
    {
        CString sDirName = m_sSaveToDir + _T("\\~temp_video");
        Utility::RecycleFile(sDirName, TRUE);
    }

    m_bInitialized=FALSE;
}

BOOL CVideoRecorder::RecordVideoFrame()
{
    // The following method records a single video frame and returns.

    ScreenshotInfo ssi; // Screenshot params

    CString sDirName = m_sSaveToDir + _T("\\~temp_video");

    // Take the screen shot and save it as raw BMP file.
    BOOL bTakeScreenshot = m_sc.TakeDesktopScreenshot(
        sDirName,
        ssi, m_ScreenshotType, m_dwProcessId,
        SCREENSHOT_FORMAT_BMP, 0, FALSE, m_nFileId);
    if(bTakeScreenshot==FALSE)
    {
        // Failed to take screenshot
        return FALSE;
    }

    // Save video frame info
    SetVideoFrameInfo(m_nFrameId, ssi);

    // Increment file ID
    m_nFileId += (int)ssi.m_aMonitors.size();

    // Increment frame number
    m_nFrameId++;

    // Reuse files cyclically
    if(m_nFrameId>=m_nFrameCount)
    {
        m_nFileId = 0;
        m_nFrameId = 0;
    }

    return TRUE;
}

void CVideoRecorder::SetVideoFrameInfo(int nFrameId, ScreenshotInfo& ssi)
{
    if((int)m_aVideoFrames.size()<=nFrameId)
    {
        // Add frame to the end of sequence
        m_aVideoFrames.push_back(ssi);
    }
    else
    {
        // Replace existing frame

        /*size_t j;
        for(j=0; j<m_aVideoFrames[nFrameId].m_aMonitors.size(); j++)
        {
        Utility::RecycleFile(m_aVideoFrames[nFrameId].m_aMonitors[j].m_sFileName, TRUE);
        }*/

        m_aVideoFrames[nFrameId]=ssi;
    }
}

BOOL CVideoRecorder::CreateFrameDIB(DWORD dwWidth, DWORD dwHeight, int nBits)
{
    if (m_pDIB)
        return FALSE;

    const DWORD dwcBihSize = sizeof(BITMAPINFOHEADER);

    // Calculate the memory required for the DIB
    DWORD dwSize = dwcBihSize +
        (2>>nBits) * sizeof(RGBQUAD) +
        ((nBits * dwWidth) * dwHeight);

    m_pDIB = (LPBITMAPINFO)new BYTE[dwSize];
    if (!m_pDIB)
        return FALSE;


    m_pDIB->bmiHeader.biSize = dwcBihSize;
    m_pDIB->bmiHeader.biWidth = dwWidth;
    m_pDIB->bmiHeader.biHeight = -(LONG)dwHeight;
    m_pDIB->bmiHeader.biBitCount = (WORD)nBits;
    m_pDIB->bmiHeader.biPlanes = 1;
    m_pDIB->bmiHeader.biCompression = BI_RGB;
    m_pDIB->bmiHeader.biXPelsPerMeter = 1000;
    m_pDIB->bmiHeader.biYPelsPerMeter = 1000;
    m_pDIB->bmiHeader.biClrUsed = 0;
    m_pDIB->bmiHeader.biClrImportant = 0;

    LPRGBQUAD lpColors =
        (LPRGBQUAD)(m_pDIB+m_pDIB->bmiHeader.biSize);
    int nColors=2>>m_pDIB->bmiHeader.biBitCount;
    for(int i=0;i<nColors;i++)
    {
        lpColors[i].rgbRed=0;
        lpColors[i].rgbBlue=0;
        lpColors[i].rgbGreen=0;
        lpColors[i].rgbReserved=0;
    }

    m_hDC = CreateCompatibleDC(GetDC(NULL));

    m_hbmpFrame = CreateDIBSection(m_hDC, m_pDIB, DIB_RGB_COLORS, &m_pFrameBits,
        NULL, 0);

    m_hOldBitmap = (HBITMAP)SelectObject(m_hDC, m_hbmpFrame);

    return TRUE;
}

HBITMAP CVideoRecorder::LoadBitmapFromBMPFile(LPCTSTR szFileName)
{
    // This method loads a BMP file.

    // Use LoadImage() to get the image loaded into a DIBSection
    HBITMAP hBitmap = (HBITMAP)LoadImage( NULL, szFileName, IMAGE_BITMAP, 0, 0,
        LR_CREATEDIBSECTION | LR_DEFAULTSIZE | LR_LOADFROMFILE );

    return hBitmap;
}

CString CVideoRecorder::GetOutFile()
{
    return m_sOutFile;
}

void CVideoRecorder::RGB_To_YV12( unsigned char *pRGBData, int nFrameWidth,
    int nFrameHeight, int nRGBStride, unsigned char *pFullYPlane,
    unsigned char *pDownsampledUPlane,
    unsigned char *pDownsampledVPlane )
{
    // Convert RGB -> YV12. We do this in-place to avoid allocating any more memory.
    unsigned char *pYPlaneOut = (unsigned char*)pFullYPlane;
    int nYPlaneOut = 0;

    int x, y;
    for(y=0; y<nFrameHeight;y++)
    {
        for (x=0; x < nFrameWidth; x ++)
        {
            int nRGBOffs = y*nRGBStride+x*3;

            unsigned char B = pRGBData[nRGBOffs+0];
            unsigned char G = pRGBData[nRGBOffs+1];
            unsigned char R = pRGBData[nRGBOffs+2];

            float y = (float)( R*66 + G*129 + B*25 + 128 ) / 256 + 16;
            float u = (float)( R*-38 + G*-74 + B*112 + 128 ) / 256 + 128;
            float v = (float)( R*112 + G*-94 + B*-18 + 128 ) / 256 + 128;

            // NOTE: We're converting pRGBData to YUV in-place here as well as writing out YUV to pFullYPlane/pDownsampledUPlane/pDownsampledVPlane.
            pRGBData[nRGBOffs+0] = (unsigned char)y;
            pRGBData[nRGBOffs+1] = (unsigned char)u;
            pRGBData[nRGBOffs+2] = (unsigned char)v;

            // Write out the Y plane directly here rather than in another loop.
            pYPlaneOut[nYPlaneOut++] = pRGBData[nRGBOffs+0];
        }
    }

    // Downsample to U and V.
    int halfHeight = nFrameHeight/2;
    int halfWidth = nFrameWidth/2;

    for ( int yPixel=0; yPixel < halfHeight; yPixel++ )
    {
        int iBaseSrc = ( (yPixel*2) * nRGBStride );

        for ( int xPixel=0; xPixel < halfWidth; xPixel++ )
        {
            pDownsampledVPlane[yPixel * halfWidth + xPixel] = pRGBData[iBaseSrc + 2];
            pDownsampledUPlane[yPixel * halfWidth + xPixel] = pRGBData[iBaseSrc + 1];

            iBaseSrc += 6;
        }
    }
}
