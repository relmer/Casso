#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  HRESULT assertions
//
//  Replaces `Assert::IsTrue (SUCCEEDED (Foo()))`, which had two problems.
//
//  It wraps a call in a macro, which is what CS0011 exists to stop everywhere
//  else -- the macro hides the operation, and where an out param landed.
//
//  And it throws away the only useful piece of evidence. `Assert::IsTrue` sees
//  a bool, so a failure reports "Expected: 1, Actual: 0" and never the code.
//  These print the HRESULT, so a broken fixture path reads as 0x80070002
//  instead of a bare false.
//
//  Global rather than namespaced, and inline rather than a .cpp, so the 190-odd
//  call sites need no qualification and no new translation unit. Test-only --
//  this header is reachable from UnitTest/Pch.h and nowhere else.
//
////////////////////////////////////////////////////////////////////////////////

inline void AssertSucceeded (HRESULT hr, const wchar_t * what = nullptr)
{
    wchar_t  msg[256] = {};

    if (FAILED (hr))
    {
        swprintf_s (msg, L"expected success, got HRESULT 0x%08X%ls%ls",
                    (unsigned int) hr,
                    (what != nullptr) ? L" -- " : L"",
                    (what != nullptr) ? what    : L"");

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (msg);
    }
}


inline void AssertFailed (HRESULT hr, const wchar_t * what = nullptr)
{
    wchar_t  msg[256] = {};

    if (SUCCEEDED (hr))
    {
        swprintf_s (msg, L"expected failure, got HRESULT 0x%08X%ls%ls",
                    (unsigned int) hr,
                    (what != nullptr) ? L" -- " : L"",
                    (what != nullptr) ? what    : L"");

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::Fail (msg);
    }
}
