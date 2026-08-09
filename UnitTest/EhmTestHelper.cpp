#include "Pch.h"

#include "EhmTestHelper.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UnitTestHelpers
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EhmBreakpointHandler
    //
    ////////////////////////////////////////////////////////////////////////////////

    // Non-null while an ExpectedEhmAssert scope is open. Assertions raised
    // inside that scope are counted instead of failing the test.
    static ExpectedEhmAssert  * s_pExpecting    = nullptr;
    static int                  s_expectedCount = 0;


    static void EhmBreakpointHandler (const wchar_t * message)
    {
        std::wstring  detail = L"EHM assertion failure detected in unit test";

        if (s_pExpecting != nullptr)
        {
            s_expectedCount++;
            return;
        }

        if (message != nullptr && message[0] != L'\0')
        {
            detail += L": ";
            detail += message;
        }

        Assert::Fail (detail.c_str());
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SuppressCrtAssertDialogs
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void SuppressCrtAssertDialogs()
    {
        // Redirect CRT assert/error/warning to stderr instead of showing dialog boxes
        _CrtSetReportMode (_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile (_CRT_ASSERT, _CRTDBG_FILE_STDERR);

        _CrtSetReportMode (_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile (_CRT_ERROR, _CRTDBG_FILE_STDERR);

        _CrtSetReportMode (_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile (_CRT_WARN, _CRTDBG_FILE_STDERR);

        // Also suppress the invalid parameter handler dialog
        _set_invalid_parameter_handler ([] (const wchar_t *, const wchar_t *, const wchar_t *, unsigned int, uintptr_t) {});
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SetupForUnitTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    void SetupForUnitTests()
    {
        SetBreakpointFunction (EhmBreakpointHandler);
        SuppressCrtAssertDialogs();
    }




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ExpectedEhmAssert
    //
    ////////////////////////////////////////////////////////////////////////////////

    ExpectedEhmAssert::ExpectedEhmAssert()
    {
        Assert::IsNull (s_pExpecting,
            L"ExpectedEhmAssert scopes do not nest -- the inner one would "
            L"swallow assertions the outer scope meant to account for");

        s_pExpecting    = this;
        s_expectedCount = 0;
    }


    int ExpectedEhmAssert::Count() const
    {
        // The tally lives in the static the handler can reach; read it live so
        // a caller inside the scope sees the running total rather than 0.
        return (s_pExpecting == this) ? s_expectedCount : m_count;
    }


    void ExpectedEhmAssert::RequireCount (int expected) const
    {
#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
        Assert::AreEqual (expected, Count(),
            L"ExpectedEhmAssert fired a different number of times than the "
            L"test expects: a guard was added, removed, or stopped asserting");
#else
        (void) expected;
#endif
    }


    ExpectedEhmAssert::~ExpectedEhmAssert()
    {
        m_count      = s_expectedCount;
        s_pExpecting = nullptr;

        // A validation guard that stops firing is a silent regression: the
        // test would still pass on the return value while the assert -- the
        // thing that tells a developer they have a bug -- had quietly gone.
        //
        // Two conditions on saying so, both learned the hard way.
        //
        // Release compiles EHM_BREAKPOINT to ((void) 0), so no assertion can
        // arrive and a zero count means nothing. Checking anyway failed every
        // one of these scopes in Release.
        //
        // And Assert::IsTrue reports by throwing. Throwing from a destructor
        // while another exception is already unwinding calls std::terminate --
        // a __fastfail, which kills the whole test host rather than failing one
        // test. That is what turned the above into a 0xC0000409 that ended the
        // Release run at test 1368 of 2804, with no summary and no named
        // failure. Never report from here while an exception is in flight.
#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
        if (std::uncaught_exceptions() == 0)
        {
            Assert::IsTrue (m_count > 0,
                L"ExpectedEhmAssert saw no assertion: the guard it was written "
                L"for is gone or no longer asserts");
        }
#endif
    }
}
