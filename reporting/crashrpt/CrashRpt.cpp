/*************************************************************************************
This file is a part of CrashRpt library.
Copyright (c) 2003-2013 The CrashRpt project authors. All Rights Reserved.

Use of this source code is governed by a BSD-style license
that can be found in the License.txt file in the root of the source
tree. All contributing project authors may
be found in the Authors.txt file in the root of the source tree.
***************************************************************************************/

// File: CrashRpt.cpp
// Description: CrashRpt API implementation.
// Authors: mikecarruth, zexspectrum
// Date:

#include "stdafx.h"
#include "CrashRpt.h"
#include "CrashHandler.h"
#include "Utility.h"
#include "strconv.h"

HANDLE g_hModuleCrashRpt = nullptr; // Handle to CrashRpt.dll module.
CComAutoCriticalSection g_cs;    // Critical section for thread-safe accessing error messages.
std::map<DWORD, CString> g_sErrorMsg; // Last error messages for each calling thread.

void crClearErrorMsg();

namespace {
    static void _crCrashedByCppThrow()
    {
        throw 13;
    }
}

CRASHRPT_API(int) crInstall(const CrInstallInfo* pInfo)
{
    int ret = 1;
    CCrashHandler* pCrashHandler = nullptr;

    do
    {
        crSetLastError(L"");

        if (pInfo == nullptr || pInfo->cb != sizeof(CrInstallInfo))
        {
            crSetLastError(L"pInfo is nullptr or pInfo->cb member is not valid.");
            break;
        }

        pCrashHandler = CCrashHandler::instance();
        if (pCrashHandler && pCrashHandler->isInitialized())
        {
            crSetLastError(L"Can't install crash handler to the same process twice.");
            break;
        }

        if (!pCrashHandler)
        {
            pCrashHandler = new CCrashHandler();
            if (!pCrashHandler)
            {
                crSetLastError(L"Error allocating memory for crash handler.");
                break;
            }
        }

        ret = pCrashHandler->initialize(
            pInfo->szAppName,
            pInfo->szAppVersion,
            pInfo->szCrashSenderDirectory,
            pInfo->szServerURL,
            pInfo->uCrashHandler,
            pInfo->szPrivacyPolicyURL,
            pInfo->szDBGHelpDirectory,
            pInfo->uMinidumpType,
            pInfo->szOutputDirectory);

        if (ret != 0)
        {
            ret = 1;
            break;
        }
    } while (false);

    if (ret != 0)
    {
        if (pCrashHandler && !pCrashHandler->isInitialized())
        {
            delete pCrashHandler;
        }
    }
    return ret;
}

CRASHRPT_API(int) crUninstall()
{
    crSetLastError(L"");

    CCrashHandler* pCrashHandler = CCrashHandler::instance();

    if (!pCrashHandler)
    {
        crSetLastError(_T("Crash handler wasn't preiviously installed for this process."));
        return 1;
    }

    if (!pCrashHandler->isInitialized())
    {
        crSetLastError(_T("Crash handler wasn't preiviously installed for this process."));
        return 1;
    }

    if (0 != pCrashHandler->uninitialize())
    {
        return 1;
    }

    delete pCrashHandler;
    return 0;
}

CRASHRPT_API(int) crSetCrashCallback(PFN_CRASH_CALLBACK pfnCallbackFunc, LPVOID lpParam)
{
    crSetLastError(_T("Unspecified error."));

    CCrashHandler* pCrashHandler = CCrashHandler::instance();

    if (pCrashHandler == nullptr)
    {
        crSetLastError(_T("Crash handler wasn't previously installed for current process."));
        return 1; // No handler installed for current process?
    }

    pCrashHandler->SetCrashCallback(pfnCallbackFunc, lpParam);

    // OK
    crSetLastError(_T("Success."));
    return 0;
}

CRASHRPT_API(int) crAddFile(PCWSTR pszFile, PCWSTR pszDestFile, PCWSTR pszDesc, DWORD dwFlags)
{
    crSetLastError(_T("Success."));

    strconv_t strconv;

    CCrashHandler* pCrashHandler =
        CCrashHandler::instance();

    if (pCrashHandler == nullptr)
    {
        crSetLastError(_T("Crash handler wasn't previously installed for current process."));
        return 1; // No handler installed for current process?
    }

    LPCTSTR lptszFile = strconv.w2t((LPWSTR)pszFile);
    LPCTSTR lptszDestFile = strconv.w2t((LPWSTR)pszDestFile);
    LPCTSTR lptszDesc = strconv.w2t((LPWSTR)pszDesc);

    int nAddResult = pCrashHandler->AddFile(lptszFile, lptszDestFile, lptszDesc, dwFlags);
    if (nAddResult != 0)
    {
        // Couldn't add file
        return 2;
    }

    // OK.
    return 0;
}

CRASHRPT_API(int)
crAddProperty(
    LPCWSTR pszPropName,
    LPCWSTR pszPropValue
)
{
    crSetLastError(_T("Unspecified error."));

    strconv_t strconv;
    LPCTSTR pszPropNameT = strconv.w2t(pszPropName);
    LPCTSTR pszPropValueT = strconv.w2t(pszPropValue);

    CCrashHandler* pCrashHandler =
        CCrashHandler::instance();

    if (pCrashHandler == nullptr)
    {
        crSetLastError(_T("Crash handler wasn't previously installed for current process."));
        return 1; // No handler installed for current process?
    }

    int nResult = pCrashHandler->AddProperty(CString(pszPropNameT), CString(pszPropValueT));
    if (nResult != 0)
    {
        crSetLastError(_T("Invalid property name specified."));
        return 2; // Failed to add the property
    }

    crSetLastError(_T("Success."));
    return 0;
}

CRASHRPT_API(int)
crGenerateErrorReport(PCR_EXCEPTION_INFO pExceptionInfo)
{
    crSetLastError(_T("Unspecified error."));

    if (pExceptionInfo == nullptr ||
        pExceptionInfo->cb != sizeof(CR_EXCEPTION_INFO))
    {
        crSetLastError(_T("Exception info is nullptr or invalid."));
        return 1;
    }

    CCrashHandler* pCrashHandler =
        CCrashHandler::instance();

    if (pCrashHandler == nullptr)
    {
        // Handler is not installed for current process
        crSetLastError(_T("Crash handler wasn't previously installed for current process."));
        ATLASSERT(pCrashHandler != nullptr);
        return 2;
    }

    return pCrashHandler->GenerateErrorReport(pExceptionInfo);
}

CRASHRPT_API(int)
crGetLastError(LPWSTR pszBuffer, UINT uBuffSize)
{
    if (pszBuffer == nullptr || uBuffSize == 0)
        return -1; // Null pointer to buffer

    strconv_t strconv;

    g_cs.Lock();

    DWORD dwThreadId = GetCurrentThreadId();
    std::map<DWORD, CString>::iterator it = g_sErrorMsg.find(dwThreadId);

    if (it == g_sErrorMsg.end())
    {
        // No error message for current thread.
        CString sErrorMsg = _T("No error.");
        LPCWSTR pwszErrorMsg = strconv.t2w(sErrorMsg.GetBuffer(0));
        int size = min(sErrorMsg.GetLength(), (int)uBuffSize - 1);
        WCSNCPY_S(pszBuffer, uBuffSize, pwszErrorMsg, size);
        g_cs.Unlock();
        return size;
    }

    LPCWSTR pwszErrorMsg = strconv.t2w(it->second.GetBuffer(0));
    int size = min((int)wcslen(pwszErrorMsg), (int)uBuffSize - 1);
    WCSNCPY_S(pszBuffer, uBuffSize, pwszErrorMsg, size);
    pszBuffer[uBuffSize - 1] = 0; // Zero terminator
    g_cs.Unlock();
    return size;
}

void crSetLastError(LPCWSTR szMessage)
{
    g_cs.Lock();
    DWORD dwThreadId = GetCurrentThreadId();
    g_sErrorMsg[dwThreadId] = szMessage;
    g_cs.Unlock();
}

void crClearErrorMsg()
{
    g_cs.Lock();
    DWORD dwThreadId = GetCurrentThreadId();
    auto itMsg = g_sErrorMsg.find(dwThreadId);
    if (itMsg == g_sErrorMsg.end())
    {
        g_sErrorMsg.erase(itMsg);
    }
    g_cs.Unlock();
}

#include <float.h>
#pragma optimize("g", off)

namespace
{
    class CBase
    {
    public:
        CBase()
        {
            function2();
        }
        virtual void function(void) = 0;
        virtual void function2(void)
        {
            function();
        }
    };

    class Derived : public CBase
    {
    public:
        void function() {};
    };

    void purecall_test()
    {
        Derived d;
        d.function();
    }

    void sigfpe_test()
    {
        // Code taken from http://www.devx.com/cplus/Article/34993/1954

        //Set the x86 floating-point control word according to what
        //exceptions you want to trap.
        _clearfp(); //Always call _clearfp before setting the control
        //word
        //Because the second parameter in the following call is 0, it
        //only returns the floating-point control word
        unsigned int cw;
#if _MSC_VER<1400
        cw = _controlfp(0, 0); //Get the default control
#else
        _controlfp_s(&cw, 0, 0); //Get the default control
#endif
    //word
    //Set the exception masks off for exceptions that you want to
    //trap.  When a mask bit is set, the corresponding floating-point
    //exception is //blocked from being generating.
        cw &= ~(EM_OVERFLOW | EM_UNDERFLOW | EM_ZERODIVIDE |
            EM_DENORMAL | EM_INVALID);
        //For any bit in the second parameter (mask) that is 1, the
        //corresponding bit in the first parameter is used to update
        //the control word.
        unsigned int cwOriginal = 0;
#if _MSC_VER<1400
        cwOriginal = _controlfp(cw, MCW_EM); //Set it.
#else
        _controlfp_s(&cwOriginal, cw, MCW_EM); //Set it.
#endif
    //MCW_EM is defined in float.h.

    // Divide by zero

        float a = 1.0f;
        float b = 0.0f;
        float c = a / b;
        c;

        //Restore the original value when done:
        _controlfp_s(&cw, cwOriginal, MCW_EM);
    }

    // #pragma optimize("", on)

#define BIG_NUMBER 0x1fffffff
//#define BIG_NUMBER 0xf
#pragma warning(disable: 4717) // avoid C4717 warning
#pragma warning(disable: 4702)
    int RecurseAlloc()
    {
        int* pi = nullptr;
        for (;;)
            pi = new int[BIG_NUMBER];
        return 0;
    }

    // Vulnerable function
#pragma warning(disable : 4996)   // for strcpy use
#pragma warning(disable : 6255)   // warning C6255: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
#pragma warning(disable : 6204)   // warning C6204: Possible buffer overrun in call to 'strcpy': use of unchecked parameter 'str'
    void test_buffer_overrun()
    {
        int a[12];
        for (int i = 0; i < 16; i++)
        {
            a[i] = i;
        }

        // No longer crashes in VS2017
        // char* buffer = (char*)_alloca(10);
        // strcpy(buffer, str); // overrun buffer !!!

        // use a secure CRT function to help prevent buffer overruns
        // truncate string to fit a 10 byte buffer
        // strncpy_s(buffer, _countof(buffer), str, _TRUNCATE);
    }
#pragma warning(default : 4996)
#pragma warning(default : 6255)
#pragma warning(default : 6204)

    // Stack overflow function
    struct DisableTailOptimization
    {
        ~DisableTailOptimization() {
            ++v;
        }
        static int v;
    };

    int DisableTailOptimization::v = 0;

    static void CauseStackOverflow()
    {
        DisableTailOptimization v;
        CauseStackOverflow();
    }
}

CRASHRPT_API(int) crTestCrash(unsigned uTestCrash) noexcept(false)
{
    crSetLastError(_T("Unspecified error."));

    switch (uTestCrash)
    {
    case CR_TEST_CRASH_SEH:
    {
        // Access violation
        int* p = 0;
#pragma warning(disable : 6011)   // warning C6011: Dereferencing nullptr pointer 'p'
        * p = 0;
#pragma warning(default : 6011)
    }
    break;
    case CR_TEST_CRASH_TERMINATE_CALL:
    {
        // Call terminate
        terminate();
    }
    break;
    case CR_TEST_CRASH_UNEXPECTED_CALL:
    {
        // Call unexpected
        unexpected();
    }
    break;
    case CR_TEST_CRASH_CPP_PURE:
    {
        // pure virtual method call
        purecall_test();
    }
    break;
    case CR_TEST_CRASH_SECURITY:
    {
        // Cause buffer overrun (/GS compiler option)

        // declare buffer that is bigger than expected
        // char large_buffer[] = "This string is longer than 10 characters!!";
        // test_buffer_overrun(large_buffer);
        test_buffer_overrun();
    }
    break;
    case CR_TEST_CRASH_INVALID_PARAMETER:
    {
        char* formatString;
        // Call printf_s with invalid parameters.
        formatString = nullptr;
#pragma warning(disable : 6387)   // warning C6387: 'argument 1' might be '0': this does not adhere to the specification for the function 'printf'
        printf(formatString);
#pragma warning(default : 6387)

    }
    break;
    case CR_TEST_CRASH_CPP_NEW_OPERATOR:
    {
        // Cause memory allocation error
        RecurseAlloc();
    }
    break;
    case CR_TEST_CRASH_SIGABRT:
    {
        // Call abort
        abort();
    }
    break;
    case CR_TEST_CRASH_SIGFPE:
    {
        // floating point exception ( /fp:except compiler option)
        sigfpe_test();
        return 1;
    }
    case CR_TEST_CRASH_SIGILL:
    {
        int result = raise(SIGILL);
        ATLASSERT(result == 0);
        crSetLastError(_T("Error raising SIGILL."));
        return result;
    }
    case CR_TEST_CRASH_SIGINT:
    {
        int result = raise(SIGINT);
        ATLASSERT(result == 0);
        crSetLastError(_T("Error raising SIGINT."));
        return result;
    }
    case CR_TEST_CRASH_SIGSEGV:
    {
        int result = raise(SIGSEGV);
        ATLASSERT(result == 0);
        crSetLastError(_T("Error raising SIGSEGV."));
        return result;
    }
    case CR_TEST_CRASH_SIGTERM:
    {
        int result = raise(SIGTERM);
        crSetLastError(_T("Error raising SIGTERM."));
        ATLASSERT(result == 0);
        return result;
    }
    case CR_TEST_CRASH_NONCONTINUABLE:
    {
        // Raise noncontinuable software exception
        ::RaiseException(123, EXCEPTION_NONCONTINUABLE, 0, nullptr);
    }
    break;
    case CR_TEST_CRASH_CPP_THROW:
    {
        _crCrashedByCppThrow();
        // Throw typed C++ exception.
    }
    break;
    case CR_TEST_CRASH_STACK_OVERFLOW:
    {
        // Infinite recursion and stack overflow.
        CauseStackOverflow();
    }
    default:
    {
        crSetLastError(_T("Unknown exception type specified."));
    }
    break;
    }

    return 1;
}

#ifndef CRASHRPT_LIB
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID /*lpReserved*/)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        // Save handle to the CrashRpt.dll module.
        g_hModuleCrashRpt = hModule;
    }
    else if (dwReason == DLL_THREAD_ATTACH)
    {
    }
    else if (dwReason == DLL_THREAD_DETACH)
    {
    }

    return TRUE;
}
#endif
