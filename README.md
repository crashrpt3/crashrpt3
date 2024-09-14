A crash reporting system for Windows applications

----

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

