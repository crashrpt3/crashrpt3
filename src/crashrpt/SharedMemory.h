#pragma once
#include "crashrpt/crashrpt.h"

#define MAX_SHARED_MEMORY_SIZE (10 * 1024 * 1024) // 10 MB
#define CR_SHARED_MEMORY_HEADER_MAGIC 0x43525054 // "CRPT"

struct CR_SHARED_MEMORY_HEADER
{
    DWORD magic; // CR_SHARED_MEMORY_HEADER_MAGIC
    DWORD ver;
    DWORD len;
    // std::string json;
};

class SharedMemory
{
public:
    SharedMemory();
    ~SharedMemory();

    bool create(LPCTSTR szName);
    bool open(LPCTSTR szName);
    bool isOpened();
    void close();

    LPBYTE mapView(DWORD dwOffset, DWORD dwLength);
    void unmapView(LPBYTE pViewPtr);

private:
    CString                  m_szName;
    HANDLE                   m_hFileMapping = nullptr;
    DWORD                    m_dwAllocationGranularity = 0;
    ULONG64                  m_uSize = 0;
    std::map<LPBYTE, LPBYTE> m_mapViewStartPtrs;
};
