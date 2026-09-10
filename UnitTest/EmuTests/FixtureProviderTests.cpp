#include "Pch.h"
#include "../EhmTestHelper.h"

#include "FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProviderTests
//
//  The fixture provider serves files from under UnitTest/Fixtures and nowhere
//  else. A path that could climb out of that root is a caller bug, refused
//  with E_INVALIDARG and an assert, and this is the test that keeps both the
//  refusal and the assert wired.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FixtureProviderTests)
{
public:

    TEST_METHOD (PathsOutsideTheFixturesRootAreRejected)
    {
        FixtureProvider       fixtures;
        std::vector<uint8_t>  bytes;
        HRESULT               hr = S_OK;

        // Every rejection below trips the same asserting guard. The scope
        // both permits that and proves the assert is still there -- a
        // rejection that stopped asserting would otherwise pass silently on
        // the HRESULT alone.
        {
            UnitTestHelpers::ExpectedEhmAssert  expect;

            hr = fixtures.OpenFixture ("../escape.bin", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"'..' traversal must be rejected");

            hr = fixtures.OpenFixture ("subdir/../../escape.bin", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"embedded '..' must be rejected");

            hr = fixtures.OpenFixture ("/etc/passwd", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"absolute root must be rejected");

            hr = fixtures.OpenFixture ("\\windows\\system32", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"backslash root must be rejected");

            hr = fixtures.OpenFixture ("C:\\windows", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"drive letter must be rejected");

            hr = fixtures.OpenFixture ("", bytes);
            Assert::AreEqual (E_INVALIDARG, hr, L"empty path must be rejected");

            expect.RequireCount (6);
        }
    }
};
