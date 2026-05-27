#include "Pch.h"

#include "CppUnitTest.h"

#include "../Casso/Ui/Win11DwmHelpers.h"

#include <fstream>
#include <sstream>
#include <string>


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Win11DwmHelpersTests
//
//  Two flavours of test:
//
//    1. Behavioural: every Apply* method is a safe no-op when given a
//       null HWND (proving the version gate path doesn't dereference
//       the handle first). These run on any OS.
//
//    2. Structural: read the .cpp source and verify each Apply method
//       contains the IsWindows11OrGreater() / IsWindows10_1809OrGreater()
//       gate call before any DwmSetWindowAttribute / DwmExtendFrameIntoClientArea
//       invocation. Cheap, fast, and catches regressions where a future
//       Apply method forgets the gate (T106a / FR-042).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    // Locate Win11DwmHelpers.cpp relative to the test DLL's source tree.
    // The test runs from x64/Debug/ so the project root is three levels
    // up; the source file lives at Casso/Ui/Win11DwmHelpers.cpp.
    std::string LoadHelpersSource()
    {
        const char *  s_kCandidates[] = {
            "../Casso/Ui/Win11DwmHelpers.cpp",
            "../../Casso/Ui/Win11DwmHelpers.cpp",
            "../../../Casso/Ui/Win11DwmHelpers.cpp",
        };
        std::ifstream  in;
        size_t         i = 0;

        for (i = 0; i < sizeof (s_kCandidates) / sizeof (s_kCandidates[0]); i++)
        {
            in.open (s_kCandidates[i]);
            if (in.is_open())
            {
                break;
            }
        }

        if (!in.is_open())
        {
            return std::string();
        }

        std::stringstream  ss;
        ss << in.rdbuf();
        return ss.str();
    }


    // Extracts the body of a `void Win11DwmHelpers::<name> (...)` function
    // -- everything between its opening `{` and matching closing `}`.
    std::string ExtractFunctionBody (const std::string & src, const std::string & qualifiedName)
    {
        size_t  pos    = src.find (qualifiedName);
        size_t  braceL = 0;
        size_t  i      = 0;
        int     depth  = 0;

        if (pos == std::string::npos)
        {
            return std::string();
        }

        braceL = src.find ('{', pos);
        if (braceL == std::string::npos)
        {
            return std::string();
        }

        depth = 1;
        for (i = braceL + 1; i < src.size() && depth > 0; i++)
        {
            if (src[i] == '{') { depth++; }
            if (src[i] == '}') { depth--; }
            if (depth == 0)
            {
                return src.substr (braceL + 1, i - braceL - 1);
            }
        }

        return std::string();
    }
}


namespace UiTests
{

TEST_CLASS (Win11DwmHelpersTests)
{
public:

    // -------- Behavioural: nullptr HWND is always a safe no-op. -----------

    TEST_METHOD (ApplyRoundedCorners_NullHwnd_NoCrash)
    {
        Win11DwmHelpers::ApplyRoundedCorners (nullptr, true);
        Win11DwmHelpers::ApplyRoundedCorners (nullptr, false);
    }

    TEST_METHOD (ApplyMicaBackdrop_NullHwnd_NoCrash)
    {
        Win11DwmHelpers::ApplyMicaBackdrop (nullptr, true);
        Win11DwmHelpers::ApplyMicaBackdrop (nullptr, false);
    }

    TEST_METHOD (ApplyImmersiveDarkMode_NullHwnd_NoCrash)
    {
        Win11DwmHelpers::ApplyImmersiveDarkMode (nullptr, true);
        Win11DwmHelpers::ApplyImmersiveDarkMode (nullptr, false);
    }

    TEST_METHOD (ExtendFrameIntoClientArea_NullHwnd_NoCrash)
    {
        Win11DwmHelpers::ExtendFrameIntoClientArea (nullptr, 1);
        Win11DwmHelpers::ExtendFrameIntoClientArea (nullptr, 0);
    }

    TEST_METHOD (VersionProbes_AreCallable)
    {
        // These read NTDLL!RtlGetVersion and should never crash; we
        // don't assert true/false because the result depends on the
        // host OS, only that the call completes.
        bool  w11   = Win11DwmHelpers::IsWindows11OrGreater();
        bool  w1809 = Win11DwmHelpers::IsWindows10_1809OrGreater();

        // Logical implication: Win11 implies 1809+.
        if (w11)
        {
            Assert::IsTrue (w1809, L"Win11 must also report 1809+");
        }
    }


    // -------- Structural: each Apply method contains its version gate. -----

    TEST_METHOD (ApplyRoundedCorners_GatedOnWin11)
    {
        std::string  src  = LoadHelpersSource();
        std::string  body = ExtractFunctionBody (src, "Win11DwmHelpers::ApplyRoundedCorners");

        Assert::IsFalse (body.empty(),
            L"Could not load Win11DwmHelpers.cpp body for ApplyRoundedCorners");
        Assert::IsTrue (body.find ("IsWindows11OrGreater") != std::string::npos,
            L"ApplyRoundedCorners must gate on IsWindows11OrGreater (FR-042)");
        Assert::IsTrue (body.find ("DwmSetWindowAttribute") != std::string::npos,
            L"ApplyRoundedCorners must call DwmSetWindowAttribute");
        Assert::IsTrue (body.find ("IsWindows11OrGreater") < body.find ("DwmSetWindowAttribute"),
            L"Version gate must run before the DWM API call");
    }

    TEST_METHOD (ApplyMicaBackdrop_GatedOnWin11)
    {
        std::string  src  = LoadHelpersSource();
        std::string  body = ExtractFunctionBody (src, "Win11DwmHelpers::ApplyMicaBackdrop");

        Assert::IsFalse (body.empty());
        Assert::IsTrue (body.find ("IsWindows11OrGreater") != std::string::npos,
            L"ApplyMicaBackdrop must gate on IsWindows11OrGreater (FR-042)");
        Assert::IsTrue (body.find ("DwmSetWindowAttribute") != std::string::npos);
        Assert::IsTrue (body.find ("IsWindows11OrGreater") < body.find ("DwmSetWindowAttribute"));
    }

    TEST_METHOD (ApplyImmersiveDarkMode_GatedOnWin10_1809)
    {
        std::string  src  = LoadHelpersSource();
        std::string  body = ExtractFunctionBody (src, "Win11DwmHelpers::ApplyImmersiveDarkMode");

        Assert::IsFalse (body.empty());
        Assert::IsTrue (body.find ("IsWindows10_1809OrGreater") != std::string::npos,
            L"ApplyImmersiveDarkMode must gate on IsWindows10_1809OrGreater (FR-042)");
        Assert::IsTrue (body.find ("DwmSetWindowAttribute") != std::string::npos);
        Assert::IsTrue (body.find ("IsWindows10_1809OrGreater") < body.find ("DwmSetWindowAttribute"));
    }

    TEST_METHOD (ExtendFrameIntoClientArea_AcceptsAllSupportedBuilds)
    {
        // DwmExtendFrameIntoClientArea is documented as available on
        // every Win10+ build, so no version gate is required -- only
        // a nullptr-hwnd check. Verify that contract holds.
        std::string  src  = LoadHelpersSource();
        std::string  body = ExtractFunctionBody (src, "Win11DwmHelpers::ExtendFrameIntoClientArea");

        Assert::IsFalse (body.empty());
        Assert::IsTrue (body.find ("hwnd == nullptr") != std::string::npos,
            L"ExtendFrameIntoClientArea must guard against null hwnd");
        Assert::IsTrue (body.find ("DwmExtendFrameIntoClientArea") != std::string::npos);
    }
};

}   // namespace UiTests
