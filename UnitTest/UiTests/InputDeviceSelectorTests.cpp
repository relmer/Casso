#include "Pch.h"

#include "Ui/Chrome/InputDeviceSelector.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  InputDeviceSelectorTests (T030d)
//
//  Pure-logic coverage for the segmented device selector: segment layout
//  and hit-testing across the 2- and 3-segment configurations, tooltip
//  state mapping, and the glyph painters emitting primitives confined to
//  their box (the mock painter records the quad/ellipse/line extensions
//  as bounding boxes, so containment is asserted across every primitive).
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    InputDeviceSelector MakeLaidOut (bool mouseAvailable)
    {
        InputDeviceSelector  sel;
        DxuiDpiScaler        scaler;
        scaler.SetDpi (96);

        sel.SetState (false, InputMappingMode::Off, mouseAvailable);
        RECT  anchor = { 200, 100, 200, 100 };   // center point
        sel.Layout (anchor, scaler);
        return sel;
    }
}


TEST_CLASS (InputDeviceSelectorTests)
{
public:

    TEST_METHOD (Layout_CentersOnAnchor_AndSizesPerSegmentCount)
    {
        InputDeviceSelector  two   = MakeLaidOut (false);
        InputDeviceSelector  three = MakeLaidOut (true);

        RECT  b2 = two.Bounds ();
        RECT  b3 = three.Bounds ();

        Assert::AreEqual (200L, (b2.left + b2.right) / 2,  L"2-seg centered X");
        Assert::AreEqual (100L, (b2.top + b2.bottom) / 2,  L"2-seg centered Y");
        Assert::IsTrue ((b3.right - b3.left) > (b2.right - b2.left),
            L"mouse-available adds a third segment (wider)");
    }


    TEST_METHOD (SegmentAt_MapsChipsAndRejectsGaps)
    {
        InputDeviceSelector  sel = MakeLaidOut (true);
        RECT  b = sel.Bounds ();

        // Walk the control horizontally at mid height; collect segments.
        int   midY = (b.top + b.bottom) / 2;
        bool  sawJ = false, sawP = false, sawM = false, sawGap = false;

        for (int x = b.left; x < b.right; ++x)
        {
            switch (sel.SegmentAt (x, midY))
            {
                case InputDeviceSelector::Segment::Joystick: sawJ = true; break;
                case InputDeviceSelector::Segment::Paddle:   sawP = true; break;
                case InputDeviceSelector::Segment::Mouse:    sawM = true; break;
                default:                                     sawGap = true; break;
            }
        }

        Assert::IsTrue (sawJ && sawP && sawM, L"all three segments hit-testable");
        Assert::IsTrue (sawGap,               L"group gap yields Segment::None");
        Assert::IsTrue (sel.SegmentAt (b.left - 5, midY) == InputDeviceSelector::Segment::None,
            L"outside bounds -> None");

        // Without the mouse, the third segment must be gone.
        InputDeviceSelector  two = MakeLaidOut (false);
        RECT  c = two.Bounds ();
        bool  sawM2 = false;
        for (int x = c.left; x < c.right; ++x)
        {
            if (two.SegmentAt (x, (c.top + c.bottom) / 2) == InputDeviceSelector::Segment::Mouse)
            {
                sawM2 = true;
            }
        }
        Assert::IsFalse (sawM2, L"no Mouse segment when unavailable");
    }


    TEST_METHOD (TooltipText_TracksSplitState)
    {
        InputDeviceSelector  sel;

        sel.SetState (false, InputMappingMode::Off, true);
        std::wstring  off = sel.TooltipText ();

        sel.SetState (true, InputMappingMode::Off, true);
        std::wstring  joy = sel.TooltipText ();

        sel.SetState (true, InputMappingMode::Mouse, true);
        std::wstring  mouse = sel.TooltipText ();

        sel.SetState (false, InputMappingMode::Paddle, true);
        std::wstring  paddle = sel.TooltipText ();

        Assert::IsTrue (off != joy && joy != mouse && mouse != paddle,
            L"each state maps to a distinct tooltip");
        Assert::IsTrue (mouse.find (L"pointer") != std::wstring::npos,
            L"mouse tooltip describes the pointer mapping");

        // Positional tooltips: each segment describes ITSELF, regardless of
        // the current state.
        InputDeviceSelector  laid = MakeLaidOut (true);
        RECT  b = laid.Bounds ();
        int   midY = (b.top + b.bottom) / 2;
        std::wstring  segTips[3];
        int           found = 0;
        InputDeviceSelector::Segment  last = InputDeviceSelector::Segment::None;
        for (int x = b.left; x < b.right && found < 3; ++x)
        {
            InputDeviceSelector::Segment  seg = laid.SegmentAt (x, midY);
            if (seg != InputDeviceSelector::Segment::None && seg != last)
            {
                segTips[found++] = laid.TooltipTextAt (x, midY);
                last = seg;
            }
        }
        Assert::AreEqual (3, found, L"three segment tooltips collected");
        Assert::IsTrue (segTips[0] != segTips[1] && segTips[1] != segTips[2],
            L"segment tooltips are independent");

        // Case-insensitive so the assertion tracks each segment's SUBJECT, not
        // the exact wording / capitalization of the tooltip copy.
        auto containsCI = [] (std::wstring hay, const wchar_t * needleLower)
        {
            for (wchar_t & c : hay) { if (c >= L'A' && c <= L'Z') { c = (wchar_t) (c - L'A' + L'a'); } }
            return hay.find (needleLower) != std::wstring::npos;
        };
        Assert::IsTrue (containsCI (segTips[0], L"joystick"), L"seg 0 = joystick tip");
        Assert::IsTrue (containsCI (segTips[2], L"mouse"),    L"seg 2 = mouse tip");
    }


    TEST_METHOD (GlyphPainters_EmitPrimitivesInsideTheBox)
    {
        RECT  box = { 100, 100, 148, 148 };   // 48px glyph

        auto check = [&] (void (*paint) (IDxuiPainter &, const RECT &, bool), bool skeuo, const wchar_t * name)
        {
            MockDxuiPainter  p;
            paint (p, box, skeuo);

            Assert::IsTrue (p.Calls ().size () > 4, name);

            for (const RecordedPaintCall & c : p.Calls ())
            {
                Assert::IsTrue (c.x >= box.left - 2 && c.y >= box.top - 2 &&
                                c.x + c.width  <= box.right + 2 &&
                                c.y + c.height <= box.bottom + 2,
                    L"glyph primitives stay inside the box");
            }
        };

        check (&InputDeviceSelector::PaintJoystickGlyph, false, L"joystick flat emits");
        check (&InputDeviceSelector::PaintJoystickGlyph, true,  L"joystick skeuo emits");
        check (&InputDeviceSelector::PaintPaddleGlyph,   false, L"paddle flat emits");
        check (&InputDeviceSelector::PaintPaddleGlyph,   true,  L"paddle skeuo emits");
        check (&InputDeviceSelector::PaintMouseGlyph,    false, L"mouse flat emits");
        check (&InputDeviceSelector::PaintMouseGlyph,    true,  L"mouse skeuo emits");
    }
};
