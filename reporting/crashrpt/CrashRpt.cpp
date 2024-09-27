#include "stdafx.h"
#include "crashrpt.h"
#include "CrashHandler.h"
#include "LastErrorThreaded.h"
#include "TestCrash.h"


CRASHRPT_API(int) crInstall(const CrInstallInfo* pInfo)
{
    int nRet = 1;
    CCrashHandler* pCrashHandler = nullptr;

    do
    {
        crLastErrorClear();

        if (pInfo == nullptr || pInfo->cb != sizeof(CrInstallInfo))
        {
            crLastErrorAdd(L"pInfo is nullptr or pInfo->cb member is not valid.");
            break;
        }

        pCrashHandler = CCrashHandler::instance();
        if (pCrashHandler && pCrashHandler->isInstalled())
        {
            crLastErrorAdd(L"Can't install crash handler to the same process twice.");
            break;
        }

        if (!pCrashHandler)
        {
            pCrashHandler = new CCrashHandler();
            if (!pCrashHandler)
            {
                crLastErrorAdd(L"Error allocating memory for crash handler.");
                break;
            }
        }

        nRet = pCrashHandler->install(pInfo);
    } while (false);

    if (nRet != 0)
    {
        if (pCrashHandler && !pCrashHandler->isInstalled())
        {
            delete pCrashHandler;
        }
    }
    return nRet;
}

CRASHRPT_API(int) crUninstall()
{
    crLastErrorClear();

    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (!pCrashHandler)
    {
        crLastErrorAdd(L"Crash handler wasn't preiviously installed for this process.");
        return 1;
    }

    if (!pCrashHandler->isInstalled())
    {
        crLastErrorAdd(L"Crash handler wasn't preiviously installed for this process.");
        return 1;
    }

    if (0 != pCrashHandler->uninstall())
    {
        return 1;
    }

    delete pCrashHandler;
    return 0;
}

CRASHRPT_API(int) crAddFile(const wchar_t* pszFile, const wchar_t* pszDestFile, const wchar_t* pszDesc)
{
    crLastErrorClear();

    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (!pCrashHandler)
    {
        crLastErrorAdd(_T("Crash handler wasn't previously installed for current process."));
        return 1;
    }
    return pCrashHandler->addFile(pszFile, pszDestFile, pszDesc);
}

CRASHRPT_API(int) crAddProperty(const wchar_t* name, const wchar_t* value)
{
    crLastErrorClear();

    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (!pCrashHandler)
    {
        crLastErrorAdd(_T("Crash handler wasn't previously installed for current process."));
        return 1;
    }
    return pCrashHandler->addProperty(name, value);
}

CRASHRPT_API(int) crGenerateErrorReport(CR_EXCEPTION_INFO* pExceptionInfo)
{
    crLastErrorClear();

    if (pExceptionInfo == nullptr || pExceptionInfo->cb != sizeof(CR_EXCEPTION_INFO))
    {
        crLastErrorAdd(L"pExceptionInfo is nullptr or pExceptionInfo->cb member is not valid.");
        return 1;
    }

    CCrashHandler* pCrashHandler = CCrashHandler::instance();
    if (pCrashHandler == nullptr)
    {
        crLastErrorAdd(_T("Crash handler wasn't previously installed for current process."));
        return 1;
    }

    return pCrashHandler->generateErrorReport(pExceptionInfo);
}

CRASHRPT_API(int) crGetLastError(wchar_t* buffer, int len)
{
    if (buffer && len > 0)
    {
        CString msg = LastErrorThreaded::thisThread().toString();
        len = std::min(msg.GetLength(), len);
        wcsncpy_s(buffer, len, msg.GetString(), len);
        return len;
    }
    return 0;
}

CRASHRPT_API(int) crTestCrash(UINT32 uTestCrash) noexcept(false)
{
    crLastErrorClear();

    TestCrash::test(uTestCrash);
    return 0;
}
