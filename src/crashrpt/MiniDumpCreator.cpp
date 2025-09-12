#include "stdafx.h"
#include "MiniDumpCreator.h"
#include "Utility.h"
#include "SharedMemory.h"
#include "ErrorStack.h"

struct JsonFileContent
{
    DWORD crashrptVersion = 0;
    std::string creationTime;
    std::string applicationPath;
    std::string crashModulePath;
    std::string crashModuleVersion;
    std::string crashModuleTimestamp;
    std::map<std::string, std::string> properties;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(JsonFileContent,
        crashrptVersion,
        creationTime,
        applicationPath,
        crashModulePath,
        crashModuleVersion,
        crashModuleTimestamp,
        properties);
};

void MiniDumpCreator::setCallback(CR_CREATEMINIDUMP_CALLBACK callback, void* param)
{
    m_callback = callback;
    m_callbackParam = param;
}

int MiniDumpCreator::createMiniDump(const wchar_t* crashGUID)
{
    int ret = 1;
    HMODULE hDbgHelp = nullptr;
    HANDLE hFile = nullptr;
    HANDLE hProcess = nullptr;

    do 
    {
        SharedMemory shdmem;

        if (!shdmem.open(crashGUID))
        {
            CString msg = L"Open shared memory error, crashGUID=";
            msg += crashGUID;
            ErrorStack::push(msg);
            break;
        }

        auto headerView = (CR_SHARED_MEMORY_HEADER*)shdmem.mapView(0, sizeof(CR_SHARED_MEMORY_HEADER));
        if (!headerView)
        {
            ErrorStack::push(L"Map shared memory error(header)");
            break;
        }

        if (headerView->magic != CR_SHARED_MEMORY_HEADER_MAGIC)
        {
            ErrorStack::push(L"Shared memory header magic error");
            break;
        }

        if (headerView->ver != CRASHRPT_VER)
        {
            ErrorStack::push(L"Shared memory version error");
            break;
        }

        if (headerView->len >= (MAX_SHARED_MEMORY_SIZE - sizeof(CR_SHARED_MEMORY_HEADER)))
        {
            ErrorStack::push(L"Shared memory length error");
            break;
        }

        auto jsonView = shdmem.mapView(sizeof(CR_SHARED_MEMORY_HEADER), headerView->len);
        if (!jsonView)
        {
            ErrorStack::push(L"Map shared memory error(json)");
            break;
        }

        m_ipcmsg = std::make_unique<IPCMessage>();
        try
        {
            auto j = nlohmann::json::parse(jsonView);
            *m_ipcmsg = j.get<IPCMessage>();
        }
        catch (...)
        {
            break;
        }
        shdmem.close();

        // Load dbghelp.dll
        hDbgHelp = LoadLibrary(L"dbghelp.dll");
        if (hDbgHelp == nullptr)
        {
            ErrorStack::push(L"dbghelp.dll not found");
            break;
        }

        // Try to adjust process privilegies to be able to generate minidumps.
        setDumpPrivileges();
        readExceptionAddr();

        CString workPath = Utility::u2w(m_ipcmsg->dumpOutDirectory.c_str()).c_str();
        workPath += crashGUID;
        if (FALSE == Utility::createFolder(workPath))
        {
            ErrorStack::push(L"Create directory error, dir=" + workPath);
            break;
        }

        // Create the minidump file
        hFile = ::CreateFile(workPath + L"\\crashdump.dmp", GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        // Check if file has been created
        if (hFile == INVALID_HANDLE_VALUE)
        {
            ErrorStack::push(L"CreateFile error, file=" + workPath + L"\\crashdump.dmp");
            break;
        }

        // Set valid dbghelp API version
        typedef LPAPI_VERSION(WINAPI* LPIMAGEHLPAPIVERSIONEX)(LPAPI_VERSION AppVersion);
        LPIMAGEHLPAPIVERSIONEX lpImagehlpApiVersionEx = (LPIMAGEHLPAPIVERSIONEX)GetProcAddress(hDbgHelp, "ImagehlpApiVersionEx");
        ATLASSERT(lpImagehlpApiVersionEx != nullptr);
        if (lpImagehlpApiVersionEx != nullptr)
        {
            API_VERSION CompiledApiVer;
            CompiledApiVer.MajorVersion = 10;
            CompiledApiVer.MinorVersion = 0;
            CompiledApiVer.Revision = 12;
            CompiledApiVer.Reserved = 0;
            LPAPI_VERSION pActualApiVer = lpImagehlpApiVersionEx(&CompiledApiVer);
            pActualApiVer;
            ATLASSERT(CompiledApiVer.MajorVersion == pActualApiVer->MajorVersion);
            ATLASSERT(CompiledApiVer.MinorVersion == pActualApiVer->MinorVersion);
            ATLASSERT(CompiledApiVer.Revision == pActualApiVer->Revision);
        }

        MINIDUMP_EXCEPTION_INFORMATION mei;
        mei.ThreadId = m_ipcmsg->threadId;
        mei.ExceptionPointers = reinterpret_cast<PEXCEPTION_POINTERS>(m_ipcmsg->exceptionPtrsAddr);
        mei.ClientPointers = TRUE;

        MINIDUMP_CALLBACK_INFORMATION mci;
        mci.CallbackRoutine = miniDumpCallback;
        mci.CallbackParam = this;

        typedef BOOL(WINAPI* LPMINIDUMPWRITEDUMP)(
            HANDLE hProcess,
            DWORD dwProcessId,
            HANDLE hFile,
            MINIDUMP_TYPE miniumpType,
            CONST PMINIDUMP_EXCEPTION_INFORMATION exceptionParam,
            CONST PMINIDUMP_USER_STREAM_INFORMATION userEncoderParam,
            CONST PMINIDUMP_CALLBACK_INFORMATION callbackParam);

        // Get address of MiniDumpWirteDump function
        LPMINIDUMPWRITEDUMP pfnMiniDumpWriteDump = (LPMINIDUMPWRITEDUMP)::GetProcAddress(hDbgHelp, "MiniDumpWriteDump");
        if (!pfnMiniDumpWriteDump)
        {
            ErrorStack::push(L"MiniDumpWriteDump() not found in dbghelp.dll");
            break;
        }

        hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, FALSE, m_ipcmsg->processId);
        BOOL bWriteDump = pfnMiniDumpWriteDump(hProcess, m_ipcmsg->processId, hFile, m_ipcmsg->minidumpType, &mei, nullptr, &mci);
        if (!bWriteDump)
        {
            ErrorStack::push(L"Call MiniDumpWriteDump() return FALSE");
            break;
        }

        CString evtName;
        evtName.Format(_T("Local\\CrashRptEvent_%s"), (LPCTSTR)crashGUID);
        HANDLE hEvent = ::CreateEvent(nullptr, FALSE, FALSE, evtName);
        if (hEvent)
        {
            ::SetEvent(hEvent);
        }

        createTextFile(workPath + L"\\crashdump.json");
        if (m_callback)
        {
            m_callback(m_callbackParam, workPath);
        }
        launchCrashRptUI(workPath, FALSE);
        ret = 0;
    } while (false);

    if (hProcess)
    {
        ::CloseHandle(hProcess);
    }

    if (hFile)
    {
        ::CloseHandle(hFile);
    }

    if (hDbgHelp)
    {
        ::FreeLibrary(hDbgHelp);
    }
    return ret;
}

BOOL MiniDumpCreator::onMiniDumpCallback(PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output)
{
    if (input->CallbackType == ModuleCallback && m_exceptionAddr != 0)
    {
        // Check if this is the module where exception has happened
        if (m_exceptionAddr >= input->Module.BaseOfImage &&
            m_exceptionAddr <= input->Module.BaseOfImage + input->Module.SizeOfImage)
        {
            m_crashModulePath = Utility::w2u(input->Module.FullPath);
            m_crashModuleTimestamp = input->Module.TimeDateStamp;

            VS_FIXEDFILEINFO* fi = &input->Module.VersionInfo;
            if (fi)
            {
                WORD major = HIWORD(fi->dwProductVersionMS);
                WORD minor = LOWORD(fi->dwProductVersionMS);
                WORD patch = HIWORD(fi->dwProductVersionLS);
                WORD build = LOWORD(fi->dwProductVersionLS);
                m_crashModuleVer =
                    std::to_string(major) + "." +
                    std::to_string(minor) + "." +
                    std::to_string(patch) + "." +
                    std::to_string(build);
            }
        }
    }
    return TRUE;
}

void MiniDumpCreator::readExceptionAddr()
{
    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, m_ipcmsg->processId);
    if (hProcess)
    {
        SIZE_T uBytesRead = 0;
        BYTE buff[1024];
        memset(&buff, 0, 1024);
        PVOID64 addr = reinterpret_cast<PVOID64>(m_ipcmsg->exceptionPtrsAddr);

        if (::ReadProcessMemory(hProcess, addr, &buff, sizeof(EXCEPTION_POINTERS), &uBytesRead) &&
            uBytesRead == sizeof(EXCEPTION_POINTERS))
        {
            EXCEPTION_POINTERS* pExcPtrs = (EXCEPTION_POINTERS*)buff;
            if (pExcPtrs->ExceptionRecord != nullptr)
            {
                DWORD64 dwExcRecordAddr = (DWORD64)pExcPtrs->ExceptionRecord;
                if (ReadProcessMemory(hProcess, (LPCVOID)dwExcRecordAddr, &buff, sizeof(EXCEPTION_RECORD), &uBytesRead) &&
                    uBytesRead == sizeof(EXCEPTION_RECORD))
                {
                    EXCEPTION_RECORD* pExcRec = (EXCEPTION_RECORD*)buff;
                    m_exceptionAddr = (DWORD64)pExcRec->ExceptionAddress;
                }
            }
        }
    }
}

void MiniDumpCreator::createTextFile(LPCWSTR filePath)
{
    CAtlFile file;
    HRESULT hr = file.Create(filePath, GENERIC_WRITE, 0, CREATE_ALWAYS);
    if (SUCCEEDED(hr))
    {
        JsonFileContent content;
        content.crashrptVersion = m_ipcmsg->crashrptVersion;
        content.creationTime = formatTime(CTime::GetCurrentTime());
        content.applicationPath = m_ipcmsg->appExePath;
        content.crashModulePath = m_crashModulePath;
        content.crashModuleVersion = m_crashModuleVer;
        content.properties = m_ipcmsg->properties;
        if (m_crashModuleTimestamp > 0)
        {
            try
            {
                CTime ct(m_crashModuleTimestamp);
                content.crashModuleTimestamp = formatTime(ct);
            } catch (...) {}
        }

        std::string buffer = nlohmann::json(content).dump(2);
        file.Write(buffer.c_str(), (DWORD)buffer.length());
        file.Close();
    }
}

void MiniDumpCreator::launchCrashRptUI(LPCWSTR param, BOOL bWait)
{
    std::wstring exePath = Utility::u2w(m_ipcmsg->crashrptuiPath.c_str());
    if (!::PathFileExists(exePath.c_str()))
    {
        return;
    }

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    CString cmdline;
    cmdline.Format(L"\"%s\" \"%s\"", exePath.c_str(), param);

    BOOL bCreateProcess = ::CreateProcess(nullptr, (LPWSTR)cmdline.GetString(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!bCreateProcess)
    {
        return;
    }

    if (pi.hThread)
    {
        ::CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }

    if (bWait)
    {
        ::WaitForSingleObject(pi.hProcess, INFINITE);
    }

    ::CloseHandle(pi.hProcess);
    pi.hProcess = nullptr;
}

BOOL CALLBACK MiniDumpCreator::miniDumpCallback(PVOID param, PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output)
{
    MiniDumpCreator* pthis = (MiniDumpCreator*)param;
    return pthis->onMiniDumpCallback(input, output);
}

BOOL MiniDumpCreator::setDumpPrivileges()
{
    // This method is used to have the current process be able to call MiniDumpWriteDump
    // This code was taken from:
    // http://social.msdn.microsoft.com/Forums/en-US/vcgeneral/thread/f54658a4-65d2-4196-8543-7e71f3ece4b6/

    BOOL bSuccess = FALSE;
    HANDLE hTokenHandle = nullptr;
    TOKEN_PRIVILEGES hTokenPrivileges;

    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hTokenHandle))
    {
        goto Cleanup;
    }

    hTokenPrivileges.PrivilegeCount = 1;

    if (!LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &hTokenPrivileges.Privileges[0].Luid))
    {
        goto Cleanup;
    }

    hTokenPrivileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    //Add privileges here.
    if (!AdjustTokenPrivileges(hTokenHandle, FALSE, &hTokenPrivileges, sizeof(hTokenPrivileges), nullptr, nullptr))
    {
        goto Cleanup;
    }

    bSuccess = TRUE;

Cleanup:
    if (hTokenHandle)
    {
        CloseHandle(hTokenHandle);
    }
    return bSuccess;
}

std::string MiniDumpCreator::formatTime(const CTime& ct)
{
    TIME_ZONE_INFORMATION tz;
    ::GetTimeZoneInformation(&tz);
    auto timeZone = -tz.Bias / 60;
    CString wstr = ct.Format(L"%Y-%m-%dT%H:%M:%S");
    wstr.AppendFormat(L"%+03d:00", timeZone);
    std::string ret = Utility::w2u(wstr);
    return ret;
}
