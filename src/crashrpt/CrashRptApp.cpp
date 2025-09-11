#include "stdafx.h"
#include "CrashRptApp.h"
#include "IPCMessage.h"
#include "Utility.h"
#include "ErrorStack.h"
#include "SharedMemory.h"

#ifdef _X86_
// Taken from: http://msdn.microsoft.com/en-us/library/s975zw7k(VS.71).aspx
// _ReturnAddress and _AddressOfReturnAddress should be prototyped before use
extern "C" void* _AddressOfReturnAddress(void);
extern "C" void* _ReturnAddress(void);
#endif

typedef void(__cdecl* _sigabrt_handler)(int);

struct ExceptionInfo
{
    WORD                cb;                         // Size of this structure in bytes; should be initialized before using.
    DWORD               crashType;                  // See macro CR_CRASH_TYPE_SEH.
    DWORD               exceptionCode;              // Code of exception.
    INT32               fpeSubCode; // Floating point exception subcode.
    LPCWSTR             assertionExpression;        // Assertion expression.
    LPCWSTR             funcionName;                // Function in which assertion happened.
    LPCWSTR             fileName;                   // File in which assertion happened.
    UINT32              fileLine;                   // Line number.
    PEXCEPTION_POINTERS exceptionPointers;          // Exception pointers.

    ExceptionInfo()
    {
        ZeroMemory(this, sizeof(ExceptionInfo));
    }
};

struct ProcessExceptionHandlers
{
    LPTOP_LEVEL_EXCEPTION_FILTER hSEH; // Structured Exception Handling
#if _MSC_VER>=1300
    _purecall_handler hPureCall; // Previous pure virtual call exception filter.
    _PNH hCppNew; // Previous new operator exception filter.
#endif
#if _MSC_VER>=1400
    _invalid_parameter_handler hInvalidParameter; // Previous invalid parameter exception filter.
#endif
    _sigabrt_handler hSIGABRT;
    _sigabrt_handler hSIGINT;
    _sigabrt_handler hSIGTERM;

    ProcessExceptionHandlers()
    {
        ZeroMemory(this, sizeof(ProcessExceptionHandlers));
    }
};

struct ThreadExceptionHandlers
{
    BOOL isInstalled;
    terminate_handler  hTerminate;
    unexpected_handler hUnexpected;
    _sigabrt_handler   hSIGFPE;
    _sigabrt_handler   hSIGILL;
    _sigabrt_handler   hSIGEGV;

    ThreadExceptionHandlers()
    {
        ZeroMemory(this, sizeof(ThreadExceptionHandlers));
    }

    static ThreadExceptionHandlers& forThisThread()
    {
        static thread_local ThreadExceptionHandlers tls;
        return tls;
    }
};

struct InstallInfo
{
    CString crashrptExePath;
    CString dumpOutDirectory;
    UINT32 crashHandles = 0;
    MINIDUMP_TYPE minidumpType = MiniDumpNormal;

    int reset(const CR_INSTALL_INFO* pInstallInfo)
    {
        this->crashrptExePath = pInstallInfo->crashrptExePath;
        this->dumpOutDirectory = pInstallInfo->dumpOutDirectory;
        this->crashHandles = pInstallInfo->crashHandlers;
        this->minidumpType = pInstallInfo->minidumpType;

        if (this->crashrptExePath.IsEmpty())
        {
            CString name;
#ifdef _DEBUG
            name.Format(_T("crashrptdump%dd.exe"), CRASHRPT_VER);
#else
            name.Format(_T("crashrptdump%d.exe"), CRASHRPT_VER);
#endif
            CString fullPath = Utility::getModuleDirectory((HMODULE)g_module) + name;
            this->crashrptExePath = fullPath;
        }

        if (!::PathFileExists(this->crashrptExePath))
        {
            ErrorStack::push(L"File not exists, path: " + this->crashrptExePath);
            return 1;
        }

        if (this->dumpOutDirectory.IsEmpty())
        {
            this->dumpOutDirectory = Utility::getModuleDirectory((HMODULE)g_module) + L"dump\\";
        }

        if (FALSE == Utility::createFolder(this->dumpOutDirectory))
        {
            ErrorStack::push(L"Create directory failed, path: " + this->dumpOutDirectory);
            return 1;
        }
        return 0;
    }
};

class ReInitializer
{
public:
    ReInitializer(CrashRptApp& app) : m_app(app) {}
    ~ReInitializer() { m_ret = m_app.resetForPerCrash(); }
    bool isSuccess() const { return m_ret == 0; }

private:
    CrashRptApp& m_app;
    int m_ret = 0;
};

CrashRptApp* CrashRptApp::m_instance = nullptr;

CrashRptApp::CrashRptApp()
{
    m_installInfo = std::make_shared<InstallInfo>();
    m_oldProcessHandlers = std::make_shared<ProcessExceptionHandlers>();
    m_instance = this;
}

CrashRptApp::~CrashRptApp()
{
    uninstall();
    m_instance = nullptr;
}

int CrashRptApp::install(const CR_INSTALL_INFO* info)
{
    int ret = 1;
    do
    {
        if (m_isInstalled)
        {
            ErrorStack::push(L"Already installed");
            break;
        }

        ret = m_installInfo->reset(info);
        if (0 != ret)
        {
            break;
        }

        ret = resetForPerCrash();
        if (0 != ret)
        {
            break;
        }

        ret = setProcessExceptionHandlers(info->crashHandlers);
        if (ret != 0)
        {
            break;
        }

        ret = setThreadExceptionHandlers(info->crashHandlers);
        if (ret != 0)
        {
            break;
        }

        ret = bugfix64And32Env();
        if (ret != 0)
        {
            break;
        }

        m_isInstalled = true;
        ret = 0;
    } while (false);
    return ret;
}

int CrashRptApp::uninstall()
{
    if (!m_isInstalled)
    {
        ErrorStack::push(L"Not installed yet");
        return 1;
    }

    // Free events
    if (m_hEvent)
    {
        ::CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    unSetProcessExceptionHandlers();
    unSetThreadExceptionHandlers();
    m_props.clear();
    m_isInstalled = false;
    return 0;
}

int CrashRptApp::addProperty(LPCWSTR name, LPCWSTR value)
{
    std::string name8 = Utility::w2u(name);
    std::string value8 = Utility::w2u(value);
    m_props[name8] = value8;
    return 0;
}

// Returns singleton of the crash handler
CrashRptApp* CrashRptApp::instance()
{
    return m_instance;
}

int CrashRptApp::setProcessExceptionHandlers(UINT32 crashHandlers)
{
    if ((crashHandlers & CR_CRASH_HANDLER_ALL) == 0)
    {
        crashHandlers |= CR_CRASH_HANDLER_ALL;
    }

    if (crashHandlers & CR_CRASH_HANDLER_SEH)
    {
        m_oldProcessHandlers->hSEH = ::SetUnhandledExceptionFilter(onHandleSEH);
    }

    _set_error_mode(_OUT_TO_STDERR);

#if _MSC_VER>=1300
    if (crashHandlers & CR_CRASH_HANDLER_CPP_PURE)
    {
        // Catch pure virtual function calls.
        // Because there is one _purecall_handler for the whole process,
        // calling this function immediately impacts all threads. The last
        // caller on any thread sets the handler.
        // http://msdn.microsoft.com/en-us/library/t296ys27.aspx
        m_oldProcessHandlers->hPureCall = _set_purecall_handler(onHandlePureCall);
    }

    if (crashHandlers & CR_CRASH_HANDLER_NEW_OPERATOR)
    {
        // Catch new operator memory allocation exceptions
        _set_new_mode(1); // Force malloc() to call new handler too
        m_oldProcessHandlers->hCppNew = _set_new_handler(onHandleCppNew);
    }
#endif

#if _MSC_VER>=1400
    if (crashHandlers & CR_CRASH_HANDLER_INVALID_PARAMETER)
    {
        m_oldProcessHandlers->hInvalidParameter = _set_invalid_parameter_handler(onHandleInvalidParameter);
    }
#endif

    if (crashHandlers & CR_CRASH_HANDLER_SIGABRT)
    {
#if _MSC_VER>=1400
        _set_abort_behavior(_CALL_REPORTFAULT, _CALL_REPORTFAULT);
#endif
        // Catch an abnormal program termination
        m_oldProcessHandlers->hSIGABRT = signal(SIGABRT, onHandleSIGABRT);
    }

    if (crashHandlers & CR_CRASH_HANDLER_SIGINT)
    {
        // Catch illegal instruction handler
        m_oldProcessHandlers->hSIGINT = signal(SIGINT, onHandleSIGINT);
    }

    if (crashHandlers & CR_CRASH_HANDLER_SIGTERM)
    {
        // Catch a termination request
        m_oldProcessHandlers->hSIGTERM = signal(SIGTERM, onHandleSIGTERM);
    }

    return 0;
}

int CrashRptApp::unSetProcessExceptionHandlers()
{
#if _MSC_VER>=1300
    if (m_oldProcessHandlers->hPureCall != nullptr)
    {
        _set_purecall_handler(m_oldProcessHandlers->hPureCall);
        m_oldProcessHandlers->hPureCall = nullptr;
    }

    if (m_oldProcessHandlers->hCppNew != nullptr)
    {
        _set_new_handler(m_oldProcessHandlers->hCppNew);
        m_oldProcessHandlers->hCppNew = nullptr;
    }
#endif

#if _MSC_VER>=1400
    if (m_oldProcessHandlers->hInvalidParameter != nullptr)
    {
        _set_invalid_parameter_handler(m_oldProcessHandlers->hInvalidParameter);
        m_oldProcessHandlers->hInvalidParameter = nullptr;
    }
#endif

    if (m_oldProcessHandlers->hSIGABRT != nullptr)
    {
        signal(SIGABRT, m_oldProcessHandlers->hSIGABRT);
        m_oldProcessHandlers->hSIGABRT = nullptr;
    }

    if (m_oldProcessHandlers->hSIGINT != nullptr)
    {
        signal(SIGINT, m_oldProcessHandlers->hSIGINT);
        m_oldProcessHandlers->hSIGINT = nullptr;
    }

    if (m_oldProcessHandlers->hSIGTERM != nullptr)
    {
        signal(SIGTERM, m_oldProcessHandlers->hSIGTERM);
        m_oldProcessHandlers->hSIGTERM = nullptr;
    }

    if (m_oldProcessHandlers->hSEH)
    {
        ::SetUnhandledExceptionFilter(m_oldProcessHandlers->hSEH);
        m_oldProcessHandlers->hSEH = nullptr;
    }
    return 0;
}

int CrashRptApp::setThreadExceptionHandlers(UINT32 crashHandlers)
{
    if ((crashHandlers & CR_CRASH_HANDLER_ALL) == 0)
    {
        crashHandlers |= CR_CRASH_HANDLER_ALL;
    }

    auto& handlers = ThreadExceptionHandlers::forThisThread();
    if (handlers.isInstalled)
    {
        return 0;
    }

    if (crashHandlers & CR_CRASH_HANDLER_TERMINATE_CALL)
    {
        // Catch terminate() calls.
        // In a multithreaded environment, terminate functions are maintained
        // separately for each thread. Each new thread needs to install its own
        // terminate function. Thus, each thread is in charge of its own termination handling.
        // http://msdn.microsoft.com/en-us/library/t6fk7h29.aspx
        handlers.hTerminate = set_terminate(onHandleTerminate);
    }

    if (crashHandlers & CR_CRASH_HANDLER_UNEXPECTED_CALL)
    {
        // Catch unexpected() calls.
        // In a multithreaded environment, unexpected functions are maintained
        // separately for each thread. Each new thread needs to install its own
        // unexpected function. Thus, each thread is in charge of its own unexpected handling.
        // http://msdn.microsoft.com/en-us/library/h46t5b69.aspx
        handlers.hUnexpected = set_unexpected(onHandleTerminateUnexpected);
    }

    if (crashHandlers & CR_CRASH_HANDLER_SIGFPE)
    {
        // Catch a floating point error
        handlers.hSIGFPE = signal(SIGFPE, (_sigabrt_handler)onHandleSIGFPE);
    }

    if (crashHandlers & CR_CRASH_HANDLER_SIGILL)
    {
        // Catch an illegal instruction
        handlers.hSIGILL = signal(SIGILL, onHandleSIGILL);
    }

    if (crashHandlers & CR_CRASH_HANDLER_SIGSEGV)
    {
        // Catch illegal storage access errors
        handlers.hSIGEGV = signal(SIGSEGV, onHandleSIGEGV);
    }

    handlers.isInstalled = TRUE;
    return 0;
}

int CrashRptApp::unSetThreadExceptionHandlers()
{
    auto& handlers = ThreadExceptionHandlers::forThisThread();
    if (!handlers.isInstalled)
    {
        return 0;
    }

    if (handlers.hTerminate)
    {
        set_terminate(handlers.hTerminate);
        handlers.hTerminate = nullptr;
    }

    if (handlers.hUnexpected)
    {
        set_unexpected(handlers.hUnexpected);
        handlers.hUnexpected = nullptr;
    }

    if (handlers.hSIGFPE)
    {
        signal(SIGFPE, handlers.hSIGFPE);
        handlers.hSIGFPE = nullptr;
    }

    if (handlers.hSIGILL)
    {
        signal(SIGILL, handlers.hSIGILL);
        handlers.hSIGILL = nullptr;
    }

    if (handlers.hSIGEGV)
    {
        signal(SIGSEGV, handlers.hSIGEGV);
        handlers.hSIGEGV = nullptr;
    }

    handlers.isInstalled = FALSE;
    return 0;
}

int CrashRptApp::generateErrorReport(ExceptionInfo* pException)
{
    if (pException == nullptr)
    {
        ErrorStack::push(L"Invalid parameter");
        return 1;
    }

    ReInitializer reInitGurad(*this);
    if (!reInitGurad.isSuccess())
    {
        ErrorStack::push(L"ReInitializer failed");
        return 1;
    }

    EXCEPTION_RECORD exceptionRecord;
    ZeroMemory(&exceptionRecord, sizeof(EXCEPTION_RECORD));

    CONTEXT contextRecord;
    ZeroMemory(&contextRecord, sizeof(CONTEXT));

    EXCEPTION_POINTERS exceptionPtrs;
    ZeroMemory(&exceptionPtrs, sizeof(EXCEPTION_POINTERS));
    exceptionPtrs.ExceptionRecord = &exceptionRecord;
    exceptionPtrs.ContextRecord = &contextRecord;

    // Get exception pointers if they were not provided by the caller.
    if (pException->exceptionPointers == nullptr)
    {
        getExceptionPointers(pException->exceptionCode, &exceptionPtrs);
        pException->exceptionPointers = &exceptionPtrs;
    }

    if (pException->exceptionCode == 0)
    {
        pException->exceptionCode = pException->exceptionPointers->ExceptionRecord->ExceptionCode;
    }

    // Save current process ID, thread ID and exception pointers address to shared mem.
    auto ipcmsg = std::make_unique<IPCMessage>();
    ipcmsg->crashrptVersion = CRASHRPT_VER;
    ipcmsg->appExePath = Utility::w2u(Utility::getModuleFullPath(nullptr));
    ipcmsg->dumpOutDirectory = Utility::w2u(m_installInfo->dumpOutDirectory);
    ipcmsg->processId = ::GetCurrentProcessId();
    ipcmsg->threadId = ::GetCurrentThreadId();
    ipcmsg->crashType = pException->crashType;
    ipcmsg->minidumpType = m_installInfo->minidumpType;
    ipcmsg->exceptionCode = pException->exceptionCode;
    ipcmsg->exceptionPtrsAddr = reinterpret_cast<INT64>(pException->exceptionPointers);
    ipcmsg->properties = m_props;

    if (pException->crashType == CR_CRASH_TYPE_SIGFPE)
    {
        // Set FPE (floating point exception) subcode
        ipcmsg->fpeSubCode = pException->fpeSubCode;
    }
    else if (pException->crashType == CR_CRASH_TYPE_INVALID_PARAMETER)
    {
        // Set invalid parameter exception info fields
        ipcmsg->invalidParamExpr = Utility::w2u(pException->assertionExpression);
        ipcmsg->invalidParamFunc = Utility::w2u(pException->funcionName);
        ipcmsg->invalidParamFile = Utility::w2u(pException->fileName);
        ipcmsg->invalidParamLine = pException->fileLine;
    }

    std::string json = nlohmann::json(*ipcmsg).dump();
    SharedMemory shdmem;
    shdmem.create(m_crashGUID);

    auto headerView = (CR_SHARED_MEMORY_HEADER*)shdmem.mapView(0, sizeof(CR_SHARED_MEMORY_HEADER));
    headerView->magic = CR_SHARED_MEMORY_HEADER_MAGIC;
    headerView->ver = CRASHRPT_VER;
    headerView->len = (DWORD)json.length();
    shdmem.unmapView((LPBYTE)headerView);

    auto jsonView = shdmem.mapView(sizeof(CR_SHARED_MEMORY_HEADER), (DWORD)json.length());
    memcpy_s(jsonView, json.length(), json.c_str(), json.length());
    shdmem.unmapView(jsonView);

    if (0 != launchCrashRptDump(m_crashGUID, TRUE))
    {
        CString caption = L"%s has stopped working" + Utility::getModuleBaseName();
        CString msg = L"Create process failed, path: " + m_installInfo->crashrptExePath;
        ::MessageBox(nullptr, msg, caption, MB_OK | MB_ICONERROR);
        return 1;
    }

    return 0;
}

// The following code gets exception pointers using a workaround found in CRT code.
void CrashRptApp::getExceptionPointers(DWORD dwExceptionCode, EXCEPTION_POINTERS* pExceptionPointers)
{
    // The following code was taken from VC++ 8.0 CRT (invarg.c: line 104)
    CONTEXT ContextRecord;
    memset(&ContextRecord, 0, sizeof(CONTEXT));

#ifdef _X86_

    __asm {
        mov dword ptr[ContextRecord.Eax], eax
        mov dword ptr[ContextRecord.Ecx], ecx
        mov dword ptr[ContextRecord.Edx], edx
        mov dword ptr[ContextRecord.Ebx], ebx
        mov dword ptr[ContextRecord.Esi], esi
        mov dword ptr[ContextRecord.Edi], edi
        mov word ptr[ContextRecord.SegSs], ss
        mov word ptr[ContextRecord.SegCs], cs
        mov word ptr[ContextRecord.SegDs], ds
        mov word ptr[ContextRecord.SegEs], es
        mov word ptr[ContextRecord.SegFs], fs
        mov word ptr[ContextRecord.SegGs], gs
        pushfd
        pop[ContextRecord.EFlags]
    }

    ContextRecord.ContextFlags = CONTEXT_CONTROL;
#pragma warning(push)
#pragma warning(disable:4311)
    ContextRecord.Eip = (ULONG)_ReturnAddress();
    ContextRecord.Esp = (ULONG)_AddressOfReturnAddress();
#pragma warning(pop)
    ContextRecord.Ebp = *((ULONG*)_AddressOfReturnAddress() - 1);

#elif defined (_IA64_) || defined (_AMD64_)

    /* Need to fill up the Context in IA64 and AMD64. */
    RtlCaptureContext(&ContextRecord);

#else  /* defined (_IA64_) || defined (_AMD64_) */

    ZeroMemory(&ContextRecord, sizeof(ContextRecord));

#endif  /* defined (_IA64_) || defined (_AMD64_) */

    memcpy(pExceptionPointers->ContextRecord, &ContextRecord, sizeof(CONTEXT));

    ZeroMemory(pExceptionPointers->ExceptionRecord, sizeof(EXCEPTION_RECORD));

    pExceptionPointers->ExceptionRecord->ExceptionCode = dwExceptionCode;
    pExceptionPointers->ExceptionRecord->ExceptionAddress = _ReturnAddress();
}

int CrashRptApp::launchCrashRptDump(LPCWSTR cmdlineParams, BOOL bWait)
{
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));

    CString cmdline;
    cmdline.Format(L"\"%s\" %s", m_installInfo->crashrptExePath.GetString(), cmdlineParams);

    BOOL bCreateProcess = ::CreateProcess(nullptr, (LPWSTR)cmdline.GetString(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!bCreateProcess)
    {
        ErrorStack::push(L"CreateProcess failed. command line: " + cmdline);
        return 1;
    }

    if (bWait)
    {
        ::WaitForSingleObject(m_hEvent, INFINITE);
    }

    if (pi.hThread)
    {
        ::CloseHandle(pi.hThread);
        pi.hThread = nullptr;
    }

    ::CloseHandle(pi.hProcess);
    pi.hProcess = nullptr;
    return 0;
}

int CrashRptApp::resetForPerCrash()
{
    m_isContinueExecutionNow = m_isContinueExecution;
    m_isContinueExecution = true;

    if (0 != Utility::generateGUID(m_crashGUID))
    {
        ErrorStack::push(L"Create guid failed");
        return 1;
    }

    if (m_hEvent)
    {
        ::CloseHandle(m_hEvent);
        m_hEvent = nullptr;
    }

    CString evtName;
    evtName.Format(L"Local\\CrashRptEvent_%s", (LPCWSTR)m_crashGUID);
    m_hEvent = ::CreateEvent(nullptr, FALSE, FALSE, evtName);
    return 0;
}

LONG WINAPI CrashRptApp::onHandleSEH(PEXCEPTION_POINTERS pExceptionPtrs)
{
    // Handle stack overflow in a separate thread.
    // Vojtech: Based on martin.bis...@gmail.com comment in
    // http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
    if (pExceptionPtrs &&
        pExceptionPtrs->ExceptionRecord &&
        pExceptionPtrs->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW)
    {
        // Special case to handle the stack overflow exception.
        // The dump will be realized from another thread.
        // Create another thread that will do the dump.
        HANDLE hThread = (HANDLE)_beginthreadex(nullptr, 0, &runThreadSEHStackOverflow, pExceptionPtrs, 0, nullptr);
        if (hThread)
        {
            ::WaitForSingleObject(hThread, INFINITE);
            ::CloseHandle(hThread);
        }
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }

    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SEH;
    ei.exceptionPointers = pExceptionPtrs;
    if (pExceptionPtrs && pExceptionPtrs->ExceptionRecord)
    {
        ei.exceptionCode = pExceptionPtrs->ExceptionRecord->ExceptionCode;
    }
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

//Vojtech: Based on martin.bis...@gmail.com comment in
// http://groups.google.com/group/crashrpt/browse_thread/thread/a1dbcc56acb58b27/fbd0151dd8e26daf?lnk=gst&q=stack+overflow#fbd0151dd8e26daf
// Thread procedure doing the dump for stack overflow.
unsigned __stdcall CrashRptApp::runThreadSEHStackOverflow(void* pvParam)
{
    PEXCEPTION_POINTERS pExceptionPointers = reinterpret_cast<PEXCEPTION_POINTERS>(pvParam);
    CrashRptApp* pApp = CrashRptApp::instance();
    if (pApp)
    {
        std::unique_lock<std::mutex> lock(pApp->m_mutex);

        // Treat this type of crash critical by default
        pApp->m_isContinueExecution = false;

        ExceptionInfo ei;
        ei.cb = sizeof(ExceptionInfo);
        ei.crashType = CR_CRASH_TYPE_SEH;
        ei.exceptionPointers = pExceptionPointers;
        ei.exceptionCode = pExceptionPointers->ExceptionRecord->ExceptionCode;
        pApp->generateErrorReport(&ei);

        if (!pApp->m_isContinueExecutionNow)
        {
            ::TerminateProcess(::GetCurrentProcess(), 1);
        }
    }

    return 0;
}

int CrashRptApp::bugfix64And32Env()
{
    // The following code is intended to fix the issue with 32-bit applications in 64-bit environment.
    // http://support.microsoft.com/kb/976038/en-us
    // http://code.google.com/p/crashrpt/issues/detail?id=104
#define PROCESS_CALLBACK_FILTER_ENABLED 0x1
    typedef BOOL(WINAPI* SETPROCESSUSERMODEEXCEPTIONPOLICY)(DWORD dwFlags);
    typedef BOOL(WINAPI* GETPROCESSUSERMODEEXCEPTIONPOLICY)(LPDWORD lpFlags);
    HMODULE hKernel32 = ::LoadLibrary(_T("kernel32.dll"));
    if (hKernel32)
    {
        SETPROCESSUSERMODEEXCEPTIONPOLICY pfnSetPolicy = (SETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "SetProcessUserModeExceptionPolicy");
        GETPROCESSUSERMODEEXCEPTIONPOLICY pfnGetPolicy = (GETPROCESSUSERMODEEXCEPTIONPOLICY)GetProcAddress(hKernel32, "GetProcessUserModeExceptionPolicy");
        if (pfnSetPolicy && pfnGetPolicy)
        {
            DWORD dwFlags = 0;
            if (pfnGetPolicy(&dwFlags))
            {
                pfnSetPolicy(dwFlags & ~PROCESS_CALLBACK_FILTER_ENABLED);
            }
        }
        ::FreeLibrary(hKernel32);
    }
    return 0;
}

#if _MSC_VER>=1300
void __cdecl CrashRptApp::onHandlePureCall()
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_CPP_PURE;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}
#endif

#if _MSC_VER>=1300
int __cdecl CrashRptApp::onHandleCppNew(size_t)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_CPP_NEW_OPERATOR;
    ei.exceptionPointers = nullptr;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        // Terminate process
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }

    return 0;
}
#endif

#if _MSC_VER>=1400
void __cdecl CrashRptApp::onHandleInvalidParameter(const wchar_t* pszExpression, const wchar_t* pszFunction, const wchar_t* pszFile, unsigned int uLine, uintptr_t /*pReserved*/)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_INVALID_PARAMETER;
    ei.assertionExpression = pszExpression;
    ei.funcionName = pszFunction;
    ei.fileName = pszFile;
    ei.fileLine = uLine;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}
#endif

void __cdecl CrashRptApp::onHandleTerminate()
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_TERMINATE_CALL;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(GetCurrentProcess(), 1);
    }
}

void __cdecl CrashRptApp::onHandleTerminateUnexpected()
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_UNEXPECTED_CALL;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(GetCurrentProcess(), 1);
    }
}

void CrashRptApp::onHandleSIGABRT(int)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGABRT;

    pApp->generateErrorReport(&ei);

    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

void CrashRptApp::onHandleSIGILL(int)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    pApp->m_isContinueExecution = false;
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGILL;

    pApp->generateErrorReport(&ei);

    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

void CrashRptApp::onHandleSIGINT(int)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    // Fill in the exception info
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGINT;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

void CrashRptApp::onHandleSIGEGV(int)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    pApp->m_isContinueExecution = false;
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGSEGV;
    ei.exceptionPointers = (PEXCEPTION_POINTERS)_pxcptinfoptrs;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

// CRT SIGTERM signal handler
void CrashRptApp::onHandleSIGTERM(int)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = false;

    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGTERM;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}

// Floating point exception (SIGFPE)
void CrashRptApp::onHandleSIGFPE(int /*nCode*/, int nSubcode)
{
    CrashRptApp* pApp = CrashRptApp::instance();
    std::unique_lock<std::mutex> lock(pApp->m_mutex);

    // Treat this type of crash critical by default
    pApp->m_isContinueExecution = FALSE;
    ExceptionInfo ei;
    ei.cb = sizeof(ExceptionInfo);
    ei.crashType = CR_CRASH_TYPE_SIGFPE;
    ei.exceptionPointers = (PEXCEPTION_POINTERS)_pxcptinfoptrs;
    ei.fpeSubCode = nSubcode;
    pApp->generateErrorReport(&ei);
    if (!pApp->m_isContinueExecutionNow)
    {
        ::TerminateProcess(::GetCurrentProcess(), 1);
    }
}
