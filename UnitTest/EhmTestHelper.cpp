#include "Pch.h"

#include "EhmTestHelper.h"

#include <crtdbg.h>
#include <cstdlib>





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace UnitTestHelpers
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  EhmBreakpointHandler
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void EhmBreakpointHandler ()
    {
        Assert::Fail (L"EHM assertion failure detected in unit test");
    }





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SuppressCrtAssertDialogs
    //
    ////////////////////////////////////////////////////////////////////////////////

    static void SuppressCrtAssertDialogs ()
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

    void SetupForUnitTests ()
    {
        SetBreakpointFunction (EhmBreakpointHandler);
        SuppressCrtAssertDialogs ();
    }
}
