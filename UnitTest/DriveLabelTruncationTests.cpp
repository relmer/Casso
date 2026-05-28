#include "Pch.h"

#include "../Casso/Ui/Chrome/DriveLabelTruncation.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DriveLabelTruncationTests
{
    static constexpr float  s_kCharWidthPx = 8.0f;



    static auto FixedWidthMeasure ()
    {
        return [] (std::wstring_view v) { return (float) v.size() * s_kCharWidthPx; };
    }



    TEST_CLASS (DriveLabelTruncationTests)
    {
    public:

        TEST_METHOD (Fits_ReturnsUntruncated)
        {
            // 8 chars * 8 px = 64 px
            auto  out = TruncateToWidth (L"game.dsk", 100.0f, FixedWidthMeasure());
            Assert::AreEqual (std::wstring (L"game.dsk"), out);
        }


        TEST_METHOD (Truncates_WithSingleCharEllipsis)
        {
            // 64 px budget = 8 chars (the ellipsis counts as 1 char).
            auto  out = TruncateToWidth (L"superlongname.dsk", 64.0f, FixedWidthMeasure());
            Assert::IsTrue (out.size() <= 8);
            Assert::AreEqual (L'\u2026', out.back());
            Assert::AreEqual ((size_t) 0, out.find (L's'));   // starts with prefix of "super…"
        }


        TEST_METHOD (NoExtension_Preserved)
        {
            auto  out = TruncateToWidth (L"README", 200.0f, FixedWidthMeasure());
            Assert::AreEqual (std::wstring (L"README"), out);
        }


        TEST_METHOD (MultipleDots_NotStripped)
        {
            auto  out = TruncateToWidth (L"my.backup.dsk", 200.0f, FixedWidthMeasure());
            Assert::AreEqual (std::wstring (L"my.backup.dsk"), out);
        }


        TEST_METHOD (Degenerate_NarrowerThanEllipsis_ReturnsEllipsis)
        {
            // 4 px budget cannot even fit the ellipsis (8 px); function returns just ellipsis.
            auto  out = TruncateToWidth (L"name.dsk", 4.0f, FixedWidthMeasure());
            Assert::AreEqual ((size_t) 1, out.size());
            Assert::AreEqual (L'\u2026', out[0]);
        }


        TEST_METHOD (Empty_ReturnsEmpty)
        {
            auto  out = TruncateToWidth (L"", 100.0f, FixedWidthMeasure());
            Assert::IsTrue (out.empty());
        }


        TEST_METHOD (NullMeasure_ReturnsLiteral)
        {
            std::function<float (std::wstring_view)>  nullMeasure;
            auto  out = TruncateToWidth (L"anything.dsk", 1.0f, nullMeasure);
            Assert::AreEqual (std::wstring (L"anything.dsk"), out);
        }
    };
}
