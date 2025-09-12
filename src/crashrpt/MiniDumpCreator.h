#pragma once
#include "IPCMessage.h"
#include "crashrpt/CrashRpt.h"

class MiniDumpCreator
{
public:
    void setCallback(CR_CREATEMINIDUMP_CALLBACK callback, void* param);
    int createMiniDump(const wchar_t* crashGUID);

private:
    BOOL onMiniDumpCallback(PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output);
    void readExceptionAddr();
    void createTextFile(LPCWSTR filePath);
    void launchCrashRptUI(LPCWSTR param, BOOL bWait);

private:
    static BOOL CALLBACK miniDumpCallback(PVOID param, PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT output);
    static BOOL setDumpPrivileges();
    static std::string formatTime(const CTime& ct);

private:
    CR_CREATEMINIDUMP_CALLBACK m_callback = nullptr;
    void* m_callbackParam = nullptr;
    std::shared_ptr<IPCMessage> m_ipcmsg;
    DWORD64 m_exceptionAddr = 0;
    std::string m_crashModulePath;
    std::string m_crashModuleVer;
    ULONG m_crashModuleTimestamp = 0;
};
