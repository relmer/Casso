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
    static ExpectedEhmAssert *   s_pExpecting = nullptr;
    static int                   s_expectedCount = 0;


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


    ExpectedEhmAssert::~ExpectedEhmAssert()
    {
        m_count      = s_expectedCount;
        s_pExpecting = nullptr;

        // A validation guard that stops firing is a silent regression: the
        // test would still pass on the return value while the assert -- the
        // thing that tells a developer they have a bug -- had quietly gone.
        Assert::IsTrue (m_count > 0,
            L"ExpectedEhmAssert saw no assertion: the guard it was written "
            L"for is gone or no longer asserts");
    }
}
