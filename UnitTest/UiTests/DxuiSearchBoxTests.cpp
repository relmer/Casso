#include "Pch.h"

#include "Widgets/DxuiSearchBox.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  SearchBoxTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SearchBoxTests)
{
public:

    static constexpr int  s_kBoxW = 200;
    static constexpr int  s_kBoxH = 28;


    static DxuiSearchBox MakeSearchBox ()
    {
        DxuiSearchBox  s;
        RECT           rc = { 0, 0, s_kBoxW, s_kBoxH };

        s.SetDpi (96);
        s.SetRect (rc);
        return s;
    }


    // Mirrors the search box's own pad / glyph-slot geometry so the test
    // can click the trailing clear glyph. Kept in sync with the constants
    // in DxuiSearchBox.cpp (pad 6, slot 20 at 96 DPI).
    static POINT ClearGlyphCenter ()
    {
        constexpr int  kPad  = 6;
        constexpr int  kSlot = 20;
        POINT          pt    = { s_kBoxW - kPad - kSlot / 2, s_kBoxH / 2 };

        return pt;
    }

    TEST_METHOD (OnChar_FiresChange_WithTypedText)
    {
        DxuiSearchBox  s = MakeSearchBox();
        std::wstring   last;

        s.SetOnChange ([&] (const std::wstring & v) { last = v; });
        s.SetFocused (true);

        s.OnChar (L'a');
        s.OnChar (L'b');

        Assert::AreEqual (std::wstring (L"ab"), s.Text());
        Assert::AreEqual (std::wstring (L"ab"), last);
    }


    TEST_METHOD (SetText_DoesNotFireChange)
    {
        DxuiSearchBox  s = MakeSearchBox();
        bool           fired = false;

        s.SetOnChange ([&] (const std::wstring &) { fired = true; });

        s.SetText (L"hello");

        Assert::AreEqual (std::wstring (L"hello"), s.Text());
        Assert::IsFalse (fired);
    }


    TEST_METHOD (Clear_EmptiesAndFiresChange)
    {
        DxuiSearchBox  s = MakeSearchBox();
        std::wstring   last = L"sentinel";

        s.SetText (L"abc");
        s.SetOnChange ([&] (const std::wstring & v) { last = v; });

        s.Clear();

        Assert::IsTrue (s.Text().empty());
        Assert::AreEqual (std::wstring (L""), last);
    }


    TEST_METHOD (ClearGlyphClick_WhenFocusedWithText_Clears)
    {
        DxuiSearchBox  s    = MakeSearchBox();
        POINT          pt   = ClearGlyphCenter();
        std::wstring   last = L"sentinel";
        bool           consumed = false;

        s.SetFocused (true);
        s.SetText (L"abc");
        s.SetOnChange ([&] (const std::wstring & v) { last = v; });

        consumed = s.OnLButtonDown (pt.x, pt.y);

        Assert::IsTrue (consumed);
        Assert::IsTrue (s.Text().empty());
        Assert::AreEqual (std::wstring (L""), last);
    }


    TEST_METHOD (ClearGlyphClick_WhenEmpty_DoesNotClearOrFire)
    {
        DxuiSearchBox  s     = MakeSearchBox();
        POINT          pt    = ClearGlyphCenter();
        bool           fired = false;

        s.SetFocused (true);
        s.SetOnChange ([&] (const std::wstring &) { fired = true; });

        // Empty + focused: the clear glyph is not shown, so a click in
        // that region routes to the input rather than clearing.
        s.OnLButtonDown (pt.x, pt.y);

        Assert::IsTrue (s.Text().empty());
        Assert::IsFalse (fired);
    }


    TEST_METHOD (ClearGlyphClick_WhenUnfocused_DoesNotClear)
    {
        DxuiSearchBox  s = MakeSearchBox();
        POINT          pt = ClearGlyphCenter();

        s.SetText (L"abc");   // text present but field unfocused

        s.OnLButtonDown (pt.x, pt.y);

        // The clear glyph only shows while focused, so an unfocused click
        // must not wipe the text.
        Assert::AreEqual (std::wstring (L"abc"), s.Text());
    }
};
