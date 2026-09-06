#include "Pch.h"

#include "Capture/ScreenshotMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotModeTests
//
//  The persisted spelling of a capture mode. The tokens are a storage format,
//  so what matters here is that they round-trip exactly and that an
//  unrecognized value degrades to the default instead of failing -- a prefs
//  file written by a newer build is the expected case, not an error.
//
////////////////////////////////////////////////////////////////////////////////

namespace ScreenshotModeTests
{
    TEST_CLASS (ScreenshotModeTests)
    {
    public:

        TEST_METHOD (FormatsEachModeToItsToken)
        {
            Assert::AreEqual ("scene", ScreenshotModeToken::Format (ScreenshotMode::Scene));
            Assert::AreEqual ("crt",   ScreenshotModeToken::Format (ScreenshotMode::Crt));
            Assert::AreEqual ("raw",   ScreenshotModeToken::Format (ScreenshotMode::Raw));
        }


        TEST_METHOD (ParsesEachTokenToItsMode)
        {
            Assert::IsTrue (ScreenshotModeToken::Parse ("scene") == ScreenshotMode::Scene);
            Assert::IsTrue (ScreenshotModeToken::Parse ("crt")   == ScreenshotMode::Crt);
            Assert::IsTrue (ScreenshotModeToken::Parse ("raw")   == ScreenshotMode::Raw);
        }


        TEST_METHOD (RoundTripsEveryMode)
        {
            ScreenshotMode   modes[] = { ScreenshotMode::Scene,
                                         ScreenshotMode::Crt,
                                         ScreenshotMode::Raw };
            size_t           i       = 0;

            for (i = 0; i < std::size (modes); i++)
            {
                string   token = ScreenshotModeToken::Format (modes[i]);

                Assert::IsTrue (ScreenshotModeToken::Parse (token) == modes[i]);
            }
        }


        TEST_METHOD (UnknownTokenFallsBackToScene)
        {
            Assert::IsTrue (ScreenshotModeToken::Parse ("window")  == ScreenshotMode::Scene);
            Assert::IsTrue (ScreenshotModeToken::Parse ("picture") == ScreenshotMode::Scene);
        }


        TEST_METHOD (EmptyTokenFallsBackToScene)
        {
            Assert::IsTrue (ScreenshotModeToken::Parse ("") == ScreenshotMode::Scene);
        }


        // The tokens are case-sensitive on purpose: Casso is the only writer,
        // so a differing case means the value came from somewhere else and the
        // default is the safer reading.
        TEST_METHOD (TokenMatchingIsCaseSensitive)
        {
            Assert::IsTrue (ScreenshotModeToken::Parse ("Raw") == ScreenshotMode::Scene);
            Assert::IsTrue (ScreenshotModeToken::Parse ("CRT") == ScreenshotMode::Scene);
        }


        TEST_METHOD (DefaultIsScene)
        {
            Assert::IsTrue (ScreenshotModeToken::kDefault == ScreenshotMode::Scene);
        }
    };
}
