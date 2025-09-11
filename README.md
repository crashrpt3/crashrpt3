### The most powerful crash reporting system for Windows C++ applications only.

Since the author has not updated and maintained it for too long, this repository is used to collect newer patches and features.

For Windows C++ applications, the CrashRpt3 is more powerful than `crashpad`, CrashRpt3 can catch more crashes.



### APIs

Only 8 APIs, It's very easy to use.

```cpp
int crInstall(const CR_INSTALL_INFO* info);
int crUninstall();

int crInstallThisThread(unsigned long crashHandlers = 0);
int crUninstallThisThread();

int crAddProperty(const wchar_t* name, const wchar_t* value);
int crTestCrash(unsigned long crashType) noexcept(false);
int crCreateMiniDump(const wchar_t* crashGUID, CR_CREATEMINIDUMP_CALLBACK callback, void* param);

int crGetLastError(wchar_t* buffer, int sizeInWords);
```



### Demo

```cpp
int main()
{
    CR_INSTALL_INFO info = { 0 };
    info.cb = sizeof(CR_INSTALL_INFO);
    crInstall(&info); // Install crashrpt3
    
    // Add you code here...
}
```

For more details, you can see `/crashrpt3/src/demo/main.cpp`.



### Features

- Supports Visual C++ 2005, 2008, 2010, 2012 and Visual C++ Express1. Can be compiled for 32-bit and 64-bit platforms.
- Works in Windows XP/2003/Vista, Windows 7 and Windows 8.
- Handles exceptions in the main thread and/or in all worker threads of your user-mode program: SEH exceptions, unhandled C++ typed exceptions, signals and CRT errors.
- Generates error report including crash minidump, extensible crash description XML, application-defined files, desktop screenshots and screen capture videos.
- Presents UI allowing user to review the crash report. Supports Privacy Policy definition.
- Can display its UI using different languages, which makes it even more suitable for multi-lingual applications.
- Sends error reports in background after user has provided his/her consent. HTTP (or HTTPS), SMTP and Simple MAPI are available methods to transfer the report data over the Internet.
- Automatically restarts the application on crash (if user provides his/her consent).
- Small overhead to the size of the software - only 1,9 Mb of additional files: CrashRpt.dll, CrashSender.exe, crashrpt_lang.ini, dbghelp.dll.
- Automates error report processing on developer's side using command-line tool. This option becomes helpful when you receive lots of error reports from users of your software. Provides API for accessing error report properties and files programmatically.

----



### Additional Information

1. This repository is fork from https://sourceforge.net/p/crashrpt/code/ci/master/tree/ 

   ```bash
   git clone https://git.code.sf.net/p/crashrpt/code crashrpt-code
   ```

2. Merged all pull requests from https://sourceforge.net/p/crashrpt/code/merge-requests/
3. Merged from https://github.com/CrashRpt/crashrpt2
4. Merged all pull request from https://github.com/CrashRpt/crashrpt2/pulls

