#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <windows.h>
#include <atlstr.h>

#include "crashrpt/crashrpt.h"

#define TEST_FLAG_INSTALL_FOR_THIS_THREAD_YES 1
#define TEST_FLAG_INSTALL_FOR_THIS_THREAD_NO 2

void test(std::int64_t flag)
{
    std::unique_ptr<crashrpt::CrInstallThisThreadGurad> crashGuard;
    if (flag == TEST_FLAG_INSTALL_FOR_THIS_THREAD_YES)
    {
        crashGuard = std::make_unique<crashrpt::CrInstallThisThreadGurad>();
    }

    std::vector<std::pair<UINT32, std::string>> vec{
        {CR_CRASH_TYPE_SEH,               "SEH exception"},
        {CR_CRASH_TYPE_TERMINATE_CALL,    "C++ terminate() call"},
        {CR_CRASH_TYPE_UNEXPECTED_CALL,   "C++ unexpected() call"},
        {CR_CRASH_TYPE_CPP_PURE,          "C++ pure virtual function call (VS .NET and later)"},
        {CR_CRASH_TYPE_CPP_NEW_OPERATOR,  "C++ new operator fault (VS .NET and later)"},
        {CR_CRASH_TYPE_SECURITY,          "Buffer overrun error (VS .NET only. Can't catch any crashes since VS2017)"},
        {CR_CRASH_TYPE_INVALID_PARAMETER, "Invalid parameter exception (VS 2005 and later)"},
        {CR_CRASH_TYPE_SIGABRT,           "C++ SIGABRT signal (abort)"},
        {CR_CRASH_TYPE_SIGFPE,            "C++ SIGFPE signal (flotating point exception)"},
        {CR_CRASH_TYPE_SIGILL,            "C++ SIGILL signal (illegal instruction. Must call crInstallThisThread() if in threads)"},
        {CR_CRASH_TYPE_SIGINT,            "C++ SIGINT signal (CTRL+C)"},
        {CR_CRASH_TYPE_SIGSEGV,           "C++ SIGSEGV signal (invalid storage access. Must call crInstallThisThread() if in threads)"},
        {CR_CRASH_TYPE_SIGTERM,           "C++ SIGTERM signal (termination request)"},
        {CR_CRASH_TYPE_NONCONTINUABLE,    "Non continuable sofware exception"},
        {CR_CRASH_TYPE_CPP_THROW,         "Throw C++ typed exception (Windows API created threads only)"},
        {CR_CRASH_TYPE_STACK_OVERFLOW,    "Stack overflow"},
    };

    std::cout << "=================================\n";
    for (auto& kv : vec)
    {
        std::cout << kv.first << "\t: " << kv.second << std::endl;
    }
    std::cout << "=================================\n";

    std::cout << "Input crash test number:";
    UINT32 num = 0;
    std::cin >> num;
    crTestCrash(num);
}

unsigned __stdcall winThreadEntry(LPVOID pParam)
{
    test(reinterpret_cast<std::int64_t>(pParam));
    return 0;
}

std::int64_t askThreadChoice()
{
    std::cout << "================================" << std::endl;
    std::cout << "1. Test crash in main thread" << std::endl;
    std::cout << "2. Test crash in win-api thread" << std::endl;
    std::cout << "3. Test crash in std::thread" << std::endl;
    std::cout << "Please input your choice:";
    std::int64_t choice = 0;
    std::cin >> choice;
    return choice;
}

std::int64_t askTestFlag()
{
    std::cout << "================================" << std::endl;
    std::cout << "1. Call crInstallThisThread() for this thread" << std::endl;
    std::cout << "2. Don't call crInstallThisThread() for this thread" << std::endl;
    std::cout << "Please input your choice:";
    std::int64_t flag = TEST_FLAG_INSTALL_FOR_THIS_THREAD_YES;
    std::cin >> flag;
    return flag;
}

int main()
{
    // Install crashrpt
    CR_INSTALL_INFO installInfo = { 0 };
    installInfo.cb = sizeof(CR_INSTALL_INFO);
    crashrpt::CrInstallGurad crashGuard(&installInfo);
    if (!crashGuard.isInstalled())
    {
        std::wcout << "Install failed: " << crashGuard.getLastError();
        return 1;
    }

    // Add custom properties
    crashGuard.addProperty(L"productName", L"crashrpt3-demo");
    crashGuard.addProperty(L"productVersion", L"3.1.0");
    crashGuard.addProperty(L"author", L"siren186");

    auto choice = askThreadChoice();
    if (choice == 1)
    {
        test(TEST_FLAG_INSTALL_FOR_THIS_THREAD_NO);
    }
    else if (choice == 2)
    {
        auto flag = askTestFlag();
        auto hThread = (HANDLE)_beginthreadex(nullptr, 0, &winThreadEntry, reinterpret_cast<void*>(flag), 0, nullptr);
        if (hThread)
        {
            ::WaitForSingleObject(hThread, INFINITE);
            ::CloseHandle(hThread);
            hThread = nullptr;
        }
    }
    else if (choice == 3)
    {
        auto flag = askTestFlag();
        std::thread thd(&test, flag);
        thd.join();
    }
}
