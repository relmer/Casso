#include "Pch.h"

#include "Widgets/DxuiAddressBar.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBarTests
//
//  The segments and their separators, the switch into a typed path and back,
//  the hover card, and the segments dropped from the start of a narrow bar.
//  The mock renderer's glyphs are 7 pixels wide, so at 96 DPI the segments
//  C:, Disks and work.po are 30, 51 and 65 pixels with their padding, each
//  followed by a 28-pixel separator, starting 4 pixels in: C: spans 4-34 and
//  its separator 34-62, Disks 62-113 and 113-141, work.po 141-206 and 206-234.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiAddressBarTests)
{
public:

    struct Fixture
    {
        MockDxuiTextRenderer  text;
        DxuiAddressBar        bar;

        void  LayOut (LONG width)
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (96);
            bar.SetTextRenderer (&text);
            bar.Layout (RECT { 0, 0, width, 30 }, scaler);
            bar.SetSegments ({ L"C:", L"Disks", L"work.po" });
            bar.SetPath (L"C:\\Disks\\work.po");
        }

        void  Mouse (DxuiMouseEventKind kind, int x)
        {
            DxuiMouseEvent  ev;

            ev.kind        = kind;
            ev.button      = (kind == DxuiMouseEventKind::Move) ? DxuiMouseButton::None : DxuiMouseButton::Left;
            ev.positionDip = POINT { x, 10 };

            bar.OnMouse (ev);
        }

        void  Click (int x)
        {
            Mouse (DxuiMouseEventKind::Down, x);
            Mouse (DxuiMouseEventKind::Up,   x);
        }

        bool  Key (DxuiKeyEventKind kind, WPARAM vk)
        {
            DxuiKeyEvent  ev;

            ev.kind = kind;
            ev.vk   = vk;

            return bar.OnKey (ev);
        }
    };


    TEST_METHOD (SegmentClick_ReportsItsIndex)
    {
        Fixture  f;
        int      clicked = -1;

        f.LayOut (400);
        f.bar.SetOnSegment ([&] (int index) { clicked = index; });

        f.Click (80);

        Assert::AreEqual (1, clicked, L"A click on Disks reports the second segment");
        Assert::IsFalse  (f.bar.IsEditing());
    }


    TEST_METHOD (SeparatorClick_ReportsTheSegmentBeforeItAndItsRect)
    {
        Fixture  f;
        int      opened  = -1;
        int      clicked = -1;
        RECT     anchor  = {};

        f.LayOut (400);
        f.bar.SetOnSegment   ([&] (int index) { clicked = index; });
        f.bar.SetOnSeparator ([&] (int index, const RECT & rc) { opened = index; anchor = rc; });

        f.Click (50);

        Assert::AreEqual (0,  opened, L"The separator after C: opens C:'s menu");
        Assert::AreEqual (-1, clicked, L"and navigates nowhere itself");
        Assert::AreEqual (34L, anchor.left);
        Assert::AreEqual (62L, anchor.right);
        Assert::AreEqual (30L, anchor.bottom, L"The menu hangs from the bottom of the bar");
    }


    TEST_METHOD (Hover_IsARoundedCardInsetFromTheBarsEdges)
    {
        Fixture          f;
        MockDxuiPainter  painter;
        MockDxuiTheme    theme;
        bool             found = false;

        f.LayOut (400);
        f.Mouse (DxuiMouseEventKind::Move, 80);
        f.bar.Paint (painter, f.text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            found = found || (call.kind == RecordedPaintKind::FillRoundedRect
                              && call.x == 62.0f && call.width == 51.0f
                              && call.y == 4.0f  && call.height == 22.0f);
        }

        Assert::IsTrue (found, L"The hovered segment's card spans the segment and stops 4 pixels short of the top and bottom");
    }


    TEST_METHOD (ClickPastTheSegments_EditsThePathAllSelected)
    {
        Fixture  f;

        f.LayOut (400);
        f.Click (300);

        Assert::IsTrue   (f.bar.IsEditing());
        Assert::AreEqual (std::wstring (L"C:\\Disks\\work.po"), f.bar.GetEditText());

        f.Key (DxuiKeyEventKind::Char, L'D');
        Assert::AreEqual (std::wstring (L"D"), f.bar.GetEditText(), L"The path starts all selected, so typing replaces it");
    }


    TEST_METHOD (Enter_ReportsWhatWasTyped_EscapePutsTheSegmentsBack)
    {
        Fixture       f;
        std::wstring  submitted;

        f.LayOut (400);
        f.bar.SetOnSubmit ([&] (const std::wstring & text) { submitted = text; });

        Assert::IsTrue   (f.Key (DxuiKeyEventKind::Down, VK_F4), L"F4 opens the path for editing, as in Explorer");
        f.Key (DxuiKeyEventKind::Char, L'Q');

        Assert::IsTrue   (f.Key (DxuiKeyEventKind::Down, VK_RETURN));
        Assert::AreEqual (std::wstring (L"Q"), submitted);
        Assert::IsTrue   (f.bar.IsEditing(), L"The host ends the edit once it has gone there");

        Assert::IsTrue   (f.Key (DxuiKeyEventKind::Down, VK_ESCAPE));
        Assert::IsFalse  (f.bar.IsEditing());
        Assert::IsFalse  (f.Key (DxuiKeyEventKind::Down, VK_TAB), L"Tab is left to the host");
    }


    TEST_METHOD (LosingFocus_EndsTheEdit)
    {
        Fixture  f;

        f.LayOut (400);
        f.bar.BeginEdit();
        f.bar.OnFocusChanged (false);

        Assert::IsFalse (f.bar.IsEditing());
    }


    TEST_METHOD (NarrowBar_DropsSegmentsFromTheStart)
    {
        Fixture  f;
        int      clicked = -1;

        f.LayOut (120);
        f.bar.SetOnSegment ([&] (int index) { clicked = index; });

        Assert::AreEqual (2, f.bar.GetFirstShown(), L"Too narrow for all three, the bar keeps the location's own name");

        f.Click (20);
        Assert::AreEqual (2, clicked, L"and shows it first");
    }
};
