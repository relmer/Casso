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




    TEST_CLASS (PrintFileNamingTests)
    {
    public:

        TEST_METHOD (ComposesTimestampedName)
        {
            fs::path   path = PrintFileNaming::ComposePngPath (
                L"C:\\Prints", FixedTime(), [] (const fs::path &) { return false; });

            Assert::AreEqual (wstring (L"Casso Print 2026-07-08 143005.png"),
                              path.filename().wstring());
            Assert::AreEqual (wstring (L"C:\\Prints"), path.parent_path().wstring());
        }


        TEST_METHOD (AppendsSuffixOnCollision)
        {
            wstring   firstTaken = L"Casso Print 2026-07-08 143005.png";

            fs::path   path = PrintFileNaming::ComposePngPath (
                L"C:\\Prints", FixedTime(),
                [&] (const fs::path & p) { return p.filename().wstring() == firstTaken; });

            Assert::AreEqual (wstring (L"Casso Print 2026-07-08 143005 (2).png"),
                              path.filename().wstring());
        }


        TEST_METHOD (SkipsMultipleCollisions)
        {
            fs::path   path = PrintFileNaming::ComposePngPath (
                L"C:\\Prints", FixedTime(),
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
    };
}
