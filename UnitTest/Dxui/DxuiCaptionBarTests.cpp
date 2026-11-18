#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBarTests
//
//  The title's tooltip: the full title when a paint had to cut it short, and
//  only over the title itself. A title too long for the room is cut in the
//  middle, and the unsaved-changes dot always has its room. What fits
//  follows from the mock renderer's fixed glyph width, so the widths here
//  are chosen to fit or not by a wide margin.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiCaptionBarTests)
{
public:

    struct Fixture
    {
        DxuiCaptionBar        caption;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;

        void  PaintAt (const std::wstring & title, int width, size_t markAt = std::wstring::npos)
        {
            caption.ConfigureButtons (DxuiCaptionBar::Buttons::MinMaxClose);
            caption.SetTitle (title, markAt);
            caption.Layout (RECT { 0, 0, width, DxuiCaptionBar::kCaptionHeightDip }, scaler);
            caption.Paint (painter, text, theme);
        }
    };


    TEST_METHOD (TitleThatFits_ShowsNoTip)
    {
        Fixture  f;
        RECT     anchor = {};

        f.PaintAt (L"Short", 800);

        Assert::IsNull (f.caption.GetTooltipAt (POINT { 30, 16 }, anchor));
    }


    TEST_METHOD (TitleCutShort_ShowsTheWholeTitle)
    {
        Fixture       f;
        RECT          anchor = {};
        std::wstring  title (200, L'x');

        f.PaintAt (title, 400);

        Assert::AreEqual (title.c_str(), f.caption.GetTooltipAt (POINT { 30, 16 }, anchor));
        Assert::IsTrue   (anchor.right > anchor.left, L"anchored on the title");
    }


    TEST_METHOD (TitleCutShort_NoTipOverTheButtons)
    {
        Fixture       f;
        RECT          anchor = {};
        std::wstring  title (200, L'x');

        f.PaintAt (title, 400);

        Assert::IsNull (f.caption.GetTooltipAt (POINT { 400 - 10, 16 }, anchor), L"the close button is not the title");
    }


    TEST_METHOD (UnsavedTitleThatFits_HasTheDotAfterTheMark)
    {
        Fixture       f;
        std::wstring  head = L"Disk inspector - C:\\Disks\\Scratch.dsk";
        std::wstring  tail = L" (mounted in Apple //e drive 1)";

        f.PaintAt (head + tail, 1600, head.size());

        Assert::AreEqual (1, CountCircles (f));
        Assert::IsTrue   (IsDrawn (f, head), L"the text before the dot, whole");
        Assert::IsTrue   (IsDrawn (f, tail), L"the text after the dot, whole");
    }


    TEST_METHOD (UnsavedTitleCutShort_KeepsTheDotAndCutsTheMiddle)
    {
        Fixture       f;
        std::wstring  head = L"Disk inspector - C:\\" + std::wstring (200, L'x') + L"\\Scratch.dsk";
        std::wstring  tail = L" (mounted)";

        f.PaintAt (head + tail, 500, head.size());

        Assert::AreEqual (1, CountCircles (f), L"the dot has its room");
        Assert::IsTrue   (IsDrawn (f, tail), L"the text after the dot fits whole");
        Assert::IsTrue   (std::any_of (f.text.Calls().begin(), f.text.Calls().end(), [] (const RecordedTextCall & call)
                          {
                              return call.text.starts_with (L"Disk") && call.text.find (L'\x2026') != std::wstring::npos && call.text.back() != L'\x2026';
                          }), L"the text before the dot is cut in the middle, not at the end");
    }


    TEST_METHOD (UnsavedTitleWithNoRoom_StillHasTheDot)
    {
        Fixture  f;

        f.PaintAt (std::wstring (200, L'x') + L" (mounted)", 150, 200);

        Assert::AreEqual (1, CountCircles (f));
    }


    TEST_METHOD (TitleCutShort_IsCutInTheMiddle)
    {
        Fixture       f;
        std::wstring  title = L"Start " + std::wstring (200, L'x') + L" end";

        f.PaintAt (title, 400);

        Assert::IsTrue (IsDrawnEnding (f, L"end"), L"the end of the title stays");
        Assert::AreEqual (0, CountCircles (f), L"no dot without unsaved changes");
    }


    static int  CountCircles (const Fixture & f)
    {
        return (int) std::count_if (f.painter.Calls().begin(), f.painter.Calls().end(), [] (const RecordedPaintCall & call) { return call.kind == RecordedPaintKind::FillCircle; });
    }


    static bool  IsDrawn (const Fixture & f, const std::wstring & value)
    {
        return std::any_of (f.text.Calls().begin(), f.text.Calls().end(), [&value] (const RecordedTextCall & call) { return call.text == value; });
    }


    static bool  IsDrawnEnding (const Fixture & f, const std::wstring & value)
    {
        return std::any_of (f.text.Calls().begin(), f.text.Calls().end(), [&value] (const RecordedTextCall & call) { return call.text.find (L'\x2026') != std::wstring::npos && call.text.ends_with (value); });
    }
};
