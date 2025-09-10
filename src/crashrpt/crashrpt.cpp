#include "stdafx.h"
#include "crashrpt/crashrpt.h"
#include "CrashRptApp.h"
#include "ErrorStack.h"
#include "TestCrash.h"
#include "MiniDumpCreator.h"

CRASHRPT_API(int) crInstall(const CR_INSTALL_INFO* info)
{
    ErrorStack::clear();
    if (info == nullptr || info->cb != sizeof(CR_INSTALL_INFO))
    {
        ErrorStack::push(L"Invalid parameter");
        return 1;
    }

    CrashRptApp* app = CrashRptApp::instance();
    if (!app)
    {
        app = new CrashRptApp();
    }

    if (!app)
    {
        ErrorStack::push(L"Out of memory");
        return 1;
    }

    int ret = app->install(info);
    if (ret != 0)
    {
        delete app;
    }
    return ret;
}

CRASHRPT_API(int) crUninstall()
{
    ErrorStack::clear();
    CrashRptApp* app = CrashRptApp::instance();
    if (!app)
    {
        ErrorStack::push(L"Not installed yet");
        return 1;
    }
    delete app;
    return 0;
}

CRASHRPT_API(int) crAddProperty(const wchar_t* name, const wchar_t* value)
{
    ErrorStack::clear();
    if (!name || !value)
    {
        ErrorStack::push(L"Invalid parameter");
        return 1;
    }

    CrashRptApp* app = CrashRptApp::instance();
    if (!app)
    {
        ErrorStack::push(L"Not installed yet");
        return 1;
    }

    return app->addProperty(name, value);
}

CRASHRPT_API(int) crGetLastError(wchar_t* buffer, int len)
{
    auto ret = ErrorStack::dump();
    if (buffer && len > 0)
    {
        auto cpyLen = std::min(len - 1, (int)ret.length());
        wcsncpy_s(buffer, len, ret.c_str(), cpyLen);
        return cpyLen;
    }
    return (int)ret.length();
}

CRASHRPT_API(int) crTestCrash(unsigned long crashType) noexcept(false)
{
    TestCrash::test(crashType);
    return 0;
}

CRASHRPT_API(int) crCreateMiniDump(const wchar_t* crashGUID, CR_CREATEMINIDUMP_CALLBACK callback, void* param)
{
    ErrorStack::clear();
    if (!crashGUID)
    {
        ErrorStack::push(L"Invalid parameter");
        return 1;
    }
    MiniDumpCreator creator;
    creator.setCallback(callback, param);
    return creator.createMiniDump(crashGUID);
}
