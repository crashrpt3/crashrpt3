#include <windows.h>
#include <atlstr.h>

int APIENTRY wWinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hInstPrev, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    int argc = 0;
    LPWSTR* argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);
    if (argc != 2)
    {
        return 1;
    }

    LPCWSTR dumpOutDirPath = argv[1];
    CString msg;
    msg.Format(L"%s\\crashdmp.dmp", dumpOutDirPath);
    ::MessageBoxW(nullptr, msg, L"Application crashed !!!", MB_OK | MB_ICONWARNING);
    return 0;
}
