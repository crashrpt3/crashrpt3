#pragma once

struct IPCMessage
{
    DWORD crashrptVersion = 0;
    std::string appExePath;
    std::string dumpOutDirectory;
    DWORD processId = 0;
    DWORD threadId = 0;
    DWORD crashType = 0;
    MINIDUMP_TYPE minidumpType = MiniDumpNormal;
    DWORD exceptionCode = 0;
    INT64 exceptionPtrsAddr = 0;
    INT32 fpeSubCode = 0;
    std::string invalidParamExpr;
    std::string invalidParamFunc;
    std::string invalidParamFile;
    DWORD invalidParamLine = 0;
    std::map<std::string, std::string> properties;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(IPCMessage,
        crashrptVersion,
        appExePath,
        dumpOutDirectory,
        processId,
        threadId,
        crashType,
        minidumpType,
        exceptionCode,
        exceptionPtrsAddr,
        fpeSubCode,
        invalidParamExpr,
        invalidParamFunc,
        invalidParamFile,
        invalidParamLine,
        properties);
};
