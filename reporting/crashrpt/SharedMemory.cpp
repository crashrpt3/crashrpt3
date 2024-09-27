#include "stdafx.h"
#include "SharedMemory.h"

#define MAX_SHARED_MEMORY_SIZE (10 * 1024 * 1024) // 10 MB

SharedMemory::SharedMemory()
{
    SYSTEM_INFO si;
    ::GetSystemInfo(&si);
    m_dwAllocationGranularity = si.dwAllocationGranularity;
}

SharedMemory::~SharedMemory()
{
    close();
}

bool SharedMemory::create(LPCTSTR szName)
{
    if (m_hFileMapping)
    {
        return false;
    }

    ULARGE_INTEGER i;
    i.QuadPart = MAX_SHARED_MEMORY_SIZE;
    m_hFileMapping = ::CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, i.HighPart, i.LowPart, szName);
    if (!m_hFileMapping)
    {
        return false;
    }

    m_szName = szName;
    m_uSize = i.QuadPart;
    return true;
}

bool SharedMemory::open(LPCTSTR szName)
{
    if (m_hFileMapping)
    {
        return false;
    }

    m_hFileMapping = ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, szName);
    if (!m_hFileMapping)
    {
        return false;
    }

    m_szName = szName;
    m_uSize = MAX_SHARED_MEMORY_SIZE;
    return true;
}

bool SharedMemory::isOpened()
{
    return m_hFileMapping != nullptr;
}

void SharedMemory::close()
{
    for (auto it = m_mapViewStartPtrs.begin(); it != m_mapViewStartPtrs.end(); it++)
    {
        ::UnmapViewOfFile(it->second);
    }
    m_mapViewStartPtrs.clear();
    if (m_hFileMapping)
    {
        ::CloseHandle(m_hFileMapping);
        m_hFileMapping = nullptr;
    }
    m_szName.Empty();
    m_uSize = 0;
}

LPBYTE SharedMemory::mapView(DWORD dwOffset, DWORD dwLength)
{
    DWORD dwBaseOffset = dwOffset - dwOffset % m_dwAllocationGranularity;
    DWORD dwDiff = dwOffset - dwBaseOffset;
    SIZE_T dwNumberOfBytesToMap = (SIZE_T)dwLength + dwDiff;
    LPBYTE pPtr = (LPBYTE)::MapViewOfFile(m_hFileMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, dwBaseOffset, dwNumberOfBytesToMap);
    m_mapViewStartPtrs[pPtr + dwDiff] = pPtr;
    return (pPtr + dwDiff);
}

void SharedMemory::unmapView(LPBYTE pViewPtr)
{
    auto it = m_mapViewStartPtrs.find(pViewPtr);
    if (it != m_mapViewStartPtrs.end())
    {
        ::UnmapViewOfFile(it->second);
        m_mapViewStartPtrs.erase(it);
    }
}
