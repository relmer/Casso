#include "Pch.h"

#include "Devices/Printer/PrintFileNaming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrintFileNamingTests
//
//  Timestamped naming and collision suffixing, with a synthetic "taken" set --
//  no filesystem involved.
//
//  The policy is shared by printouts and screenshots, so the base name and the
//  extension are exercised as parameters rather than assumed: a generalization
//  only one caller ever varies is a generalization nothing proves.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrintFileNamingTests
{
    static SYSTEMTIME FixedTime()
    {
        SYSTEMTIME   st = {};
        st.wYear   = 2026;
        st.wMonth  = 7;
        st.wDay    = 8;
        st.wHour   = 14;
        st.wMinute = 30;
        st.wSecond = 5;
        return st;
    }


    static bool NothingTaken (const fs::path &)
    {
        return false;
    }




    TEST_CLASS (PrintFileNamingTests)
    {
    public:

        TEST_METHOD (ComposesTimestampedName)
        {
            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Prints", L"Casso Print", L".png", FixedTime(), NothingTaken);

            Assert::AreEqual (wstring (L"Casso Print 2026-07-08 143005.png"),
                              path.filename().wstring());
            Assert::AreEqual (wstring (L"C:\\Prints"), path.parent_path().wstring());
        }


        TEST_METHOD (AppendsSuffixOnCollision)
        {
            wstring   firstTaken = L"Casso Print 2026-07-08 143005.png";

            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Prints", L"Casso Print", L".png", FixedTime(),
                [&] (const fs::path & p) { return p.filename().wstring() == firstTaken; });

            Assert::AreEqual (wstring (L"Casso Print 2026-07-08 143005 (2).png"),
                              path.filename().wstring());
        }


        TEST_METHOD (SkipsMultipleCollisions)
        {
            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Prints", L"Casso Print", L".png", FixedTime(),
                [] (const fs::path & p)
                {
                    wstring   name = p.filename().wstring();
                    return name == L"Casso Print 2026-07-08 143005.png"
                        || name == L"Casso Print 2026-07-08 143005 (2).png"
                        || name == L"Casso Print 2026-07-08 143005 (3).png";
                });

            Assert::AreEqual (wstring (L"Casso Print 2026-07-08 143005 (4).png"),
                              path.filename().wstring());
        }


        // The screenshot base name. "Screenshot" is deliberately absent -- the
        // folder supplies it -- while "Casso" is kept, because the folder is
        // not the context that travels when the file is dragged into an issue.
        TEST_METHOD (ComposesScreenshotName)
        {
            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Shots", L"Casso", L".png", FixedTime(), NothingTaken);

            Assert::AreEqual (wstring (L"Casso 2026-07-08 143005.png"),
                              path.filename().wstring());
            Assert::AreEqual (wstring (L"C:\\Shots"), path.parent_path().wstring());
        }


        TEST_METHOD (ScreenshotNameTakesTheSuffixToo)
        {
            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Shots", L"Casso", L".png", FixedTime(),
                [] (const fs::path & p)
                {
                    return p.filename().wstring() == L"Casso 2026-07-08 143005.png";
                });

            Assert::AreEqual (wstring (L"Casso 2026-07-08 143005 (2).png"),
                              path.filename().wstring());
        }


        // The extension is a parameter, and the suffix goes BEFORE it -- a
        // name ending " (2)" after the dot would not open.
        TEST_METHOD (HonorsANonPngExtension)
        {
            fs::path   plain = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Out", L"Casso", L".txt", FixedTime(), NothingTaken);

            fs::path   suffixed = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Out", L"Casso", L".txt", FixedTime(),
                [] (const fs::path & p)
                {
                    return p.filename().wstring() == L"Casso 2026-07-08 143005.txt";
                });

            Assert::AreEqual (wstring (L"Casso 2026-07-08 143005.txt"),
                              plain.filename().wstring());
            Assert::AreEqual (wstring (L"Casso 2026-07-08 143005 (2).txt"),
                              suffixed.filename().wstring());
        }


        // Single-digit fields pad, so names sort lexicographically in the
        // shell's file listing -- which is the order a user looks for them in.
        TEST_METHOD (PadsEveryTimestampField)
        {
            SYSTEMTIME   early = {};
            early.wYear   = 2026;
            early.wMonth  = 1;
            early.wDay    = 2;
            early.wHour   = 3;
            early.wMinute = 4;
            early.wSecond = 5;

            fs::path   path = PrintFileNaming::ComposeTimestampedPath (
                L"C:\\Shots", L"Casso", L".png", early, NothingTaken);

            Assert::AreEqual (wstring (L"Casso 2026-01-02 030405.png"),
                              path.filename().wstring());
        }
    };
}
