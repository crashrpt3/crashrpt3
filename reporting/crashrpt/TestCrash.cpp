#include "stdafx.h"
#include "TestCrash.h"
#include "CrashRpt.h"
#include "LastErrorThreaded.h"

namespace
{
    class CBase
    {
    public:
        CBase()
        {
            function2();
        }

        virtual void function(void) = 0;

        virtual void function2(void)
        {
            function();
        }
    };

    class Derived : public CBase
    {
    public:
        void function() {};
    };

    static void _testCrashSEH()
    {
        int* p = 0;
#pragma warning(disable : 6011)
        * p = 0;
#pragma warning(default : 6011)
    }

    static void _testCrashCppPure()
    {
        Derived d;
        d.function();
    }

    static void _testCrashSIGFPE()
    {
        // Code taken from http://www.devx.com/cplus/Article/34993/1954
        //Set the x86 floating-point control word according to what
        //exceptions you want to trap.
        _clearfp(); //Always call _clearfp before setting the control
        //word
        //Because the second parameter in the following call is 0, it
        //only returns the floating-point control word
        unsigned int cw;
#if _MSC_VER<1400
        cw = _controlfp(0, 0); //Get the default control
#else
        _controlfp_s(&cw, 0, 0); //Get the default control
#endif
    //word
    //Set the exception masks off for exceptions that you want to
    //trap.  When a mask bit is set, the corresponding floating-point
    //exception is //blocked from being generating.
        cw &= ~(EM_OVERFLOW | EM_UNDERFLOW | EM_ZERODIVIDE |
            EM_DENORMAL | EM_INVALID);
        //For any bit in the second parameter (mask) that is 1, the
        //corresponding bit in the first parameter is used to update
        //the control word.
        unsigned int cwOriginal = 0;
#if _MSC_VER<1400
        cwOriginal = _controlfp(cw, MCW_EM); //Set it.
#else
        _controlfp_s(&cwOriginal, cw, MCW_EM); //Set it.
#endif
    //MCW_EM is defined in float.h.

    // Divide by zero

        float a = 1.0f;
        float b = 0.0f;
        float c = a / b;
        c;

        //Restore the original value when done:
        _controlfp_s(&cw, cwOriginal, MCW_EM);
    }

#define BIG_NUMBER 0x1fffffff
#pragma warning(disable: 4717) // avoid C4717 warning
#pragma warning(disable: 4702)
    static int _testCrashCppNewOperator()
    {
        int* pi = nullptr;
        for (;;)
            pi = new int[BIG_NUMBER];
        return 0;
    }

    // Vulnerable function
#pragma warning(disable : 4996)   // for strcpy use
#pragma warning(disable : 6255)   // warning C6255: _alloca indicates failure by raising a stack overflow exception. Consider using _malloca instead
#pragma warning(disable : 6204)   // warning C6204: Possible buffer overrun in call to 'strcpy': use of unchecked parameter 'str'
    static void _testCrashSecurity()
    {
        int a[12];
        for (int i = 0; i < 16; i++)
        {
            a[i] = i;
        }

        // No longer crashes in VS2017
        // char* buffer = (char*)_alloca(10);
        // strcpy(buffer, str); // overrun buffer !!!

        // use a secure CRT function to help prevent buffer overruns
        // truncate string to fit a 10 byte buffer
        // strncpy_s(buffer, _countof(buffer), str, _TRUNCATE);
    }
#pragma warning(default : 4996)
#pragma warning(default : 6255)
#pragma warning(default : 6204)

    // Stack overflow function
    struct DisableTailOptimization
    {
        ~DisableTailOptimization() {
            ++v;
        }
        static int v;
    };

    int DisableTailOptimization::v = 0;

    static void _testCrashStackOverflow()
    {
        DisableTailOptimization v;
        _testCrashStackOverflow();
    }

    static void _testCrashInvalidParameter()
    {
        char* formatString;
        formatString = nullptr;
#pragma warning(disable : 6387)
        printf(formatString);
#pragma warning(default : 6387)
    }
} // namespace


void TestCrash::test(UINT32 uTestCrash)
{
    switch (uTestCrash)
    {
    case CR_TEST_CRASH_SEH:
        _testCrashSEH();
        break;
    case CR_TEST_CRASH_TERMINATE_CALL:
        terminate();
        break;
    case CR_TEST_CRASH_UNEXPECTED_CALL:
        unexpected();
        break;
    case CR_TEST_CRASH_CPP_PURE:
        _testCrashCppPure();
        break;
    case CR_TEST_CRASH_SECURITY:
        _testCrashSecurity();
        break;
    case CR_TEST_CRASH_INVALID_PARAMETER:
        _testCrashInvalidParameter();
        break;
    case CR_TEST_CRASH_CPP_NEW_OPERATOR:
        _testCrashCppNewOperator();
        break;
    case CR_TEST_CRASH_SIGABRT:
        abort();
        break;
    case CR_TEST_CRASH_SIGFPE:
        _testCrashSIGFPE();
        break;
    case CR_TEST_CRASH_SIGILL:
        raise(SIGILL);
        break;
    case CR_TEST_CRASH_SIGINT:
        raise(SIGINT);
        break;
    case CR_TEST_CRASH_SIGSEGV:
        raise(SIGSEGV);
        break;
    case CR_TEST_CRASH_SIGTERM:
        raise(SIGTERM);
        break;
    case CR_TEST_CRASH_NONCONTINUABLE:
        ::RaiseException(123, EXCEPTION_NONCONTINUABLE, 0, nullptr);
        break;
    case CR_TEST_CRASH_CPP_THROW:
        throw 13;
        break;
    case CR_TEST_CRASH_STACK_OVERFLOW:
        _testCrashStackOverflow();
        break;
    default:
        break;
    }
}
