#include <windows.h>
#include "crashrpt/crashrpt.h"

void onMiniDumpCreated(void* param, const wchar_t* dumpOutDirPath)
{
    // Add your code here...
}

int WINAPI wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hInstPrev, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argc == 2)
    {
        LPCWSTR crashGUID = argv[1];
        return crCreateMiniDump(crashGUID, &onMiniDumpCreated);
    }
    return 1;
}
