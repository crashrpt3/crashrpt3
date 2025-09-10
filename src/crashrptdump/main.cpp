#include "stdafx.h"
#include "crashrpt/crashrpt.h"

int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hInstPrev, _In_ LPSTR cmdline, _In_ int cmdshow)
{
    LPCWSTR szCmdLineW = ::GetCommandLineW();
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(szCmdLineW, &argc);
    if (argc != 2)
    {
        return 1;
    }
    return crCreateMiniDump(argv[1]);
}
