#include "Pch.h"

#include "Widgets/DxuiAddressBar.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiAddressBarTests
//
//  The segments, the switch into a typed path and back, and the segments
//  dropped from the start of a narrow bar. The mock renderer's glyphs are 7
//  pixels wide, so at 96 DPI the segments C:, Disks and work.po are 26, 47
//  and 61 pixels with their padding, each followed by an 18-pixel separator,
//  starting 8 pixels in: C: spans 8-34, Disks 52-99, work.po 117-178.
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

        void  Click (int x)
        {
            DxuiMouseEvent  ev;

            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { x, 10 };

            ev.kind = DxuiMouseEventKind::Down;
            bar.OnMouse (ev);
            ev.kind = DxuiMouseEventKind::Up;
            bar.OnMouse (ev);
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

        f.Click (60);

        Assert::AreEqual (1, clicked, L"A click on Disks reports the second segment");
        Assert::IsFalse  (f.bar.IsEditing());
    }


    TEST_METHOD (SeparatorClick_DoesNothing)
    {
        Fixture  f;
        int      clicked = -1;

        f.LayOut (400);
        f.bar.SetOnSegment ([&] (int index) { clicked = index; });

        f.Click (40);

        Assert::AreEqual (-1, clicked);
        Assert::IsFalse  (f.bar.IsEditing());
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
