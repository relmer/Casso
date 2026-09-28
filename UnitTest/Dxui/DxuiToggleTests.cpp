#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToggleTests
//
//  The toggle's orientation. Right is the horizontal pill it has always
//  been; Up and Down stand it on end, with the thumb at the top or the
//  bottom while it is checked. The geometry is a pure function of the pill's
//  box, so it is checked here without a painter, and one paint test checks
//  the thumb is drawn where the geometry puts it.
//
//  THE HORIZONTAL PILL MUST NOT MOVE. The expected values for Right are the
//  formulas the paint used before the orientation existed: end caps of half
//  the pill's height, a thumb inset a sixth of the height from the edge.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToggleTests)
{
public:

    static constexpr float  kTolerance = 0.001f;

    //  A 36 by 18 pill, the toggle's size at 96 DPI, at (10, 20).
    static D2D1_RECT_F MakePill()
    {
        return D2D1::RectF (10.0f, 20.0f, 46.0f, 38.0f);
    }


    static void AssertRect (const D2D1_RECT_F & expected, const D2D1_RECT_F & actual, const wchar_t * message)
    {
        Assert::AreEqual (expected.left,   actual.left,   kTolerance, message);
        Assert::AreEqual (expected.top,    actual.top,    kTolerance, message);
        Assert::AreEqual (expected.right,  actual.right,  kTolerance, message);
        Assert::AreEqual (expected.bottom, actual.bottom, kTolerance, message);
    }


    TEST_METHOD (OnDirection_DefaultsToRight)
    {
        DxuiToggle  toggle;



        Assert::IsTrue (toggle.GetOnDirection() == DxuiToggle::OnDirection::Right);

        toggle.SetOnDirection (DxuiToggle::OnDirection::Down);
        Assert::IsTrue (toggle.GetOnDirection() == DxuiToggle::OnDirection::Down);
    }


    TEST_METHOD (Right_IsTodaysHorizontalPill)
    {
        D2D1_RECT_F                pill      = MakePill();
        float                      capR      = (pill.bottom - pill.top) * 0.5f;
        float                      thumbR    = capR - (pill.bottom - pill.top) / 6.0f;
        DxuiToggle::TrackAndThumb  unchecked = DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Right, false);
        DxuiToggle::TrackAndThumb  checked   = DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Right, true);



        AssertRect (pill, unchecked.track, L"the track is the pill");
        Assert::AreEqual (capR,               unchecked.capRadius,     kTolerance);
        Assert::AreEqual (thumbR,             unchecked.thumbRadius,   kTolerance, L"six at 96 DPI, as the fixed inset gave");
        Assert::AreEqual (pill.left + capR,   unchecked.thumbCenter.x, kTolerance, L"off: the thumb at the left end");
        Assert::AreEqual (pill.top + capR,    unchecked.thumbCenter.y, kTolerance);
        Assert::AreEqual (pill.right - capR,  checked.thumbCenter.x,   kTolerance, L"on: the thumb at the right end");
        Assert::AreEqual (pill.top + capR,    checked.thumbCenter.y,   kTolerance);
    }


    TEST_METHOD (UpAndDown_StandThePillOnEnd)
    {
        D2D1_RECT_F  pill     = MakePill();
        D2D1_RECT_F  upright  = D2D1::RectF (pill.left, pill.top, pill.left + 18.0f, pill.top + 36.0f);



        for (DxuiToggle::OnDirection direction : { DxuiToggle::OnDirection::Up, DxuiToggle::OnDirection::Down })
        {
            for (bool checked : { false, true })
            {
                DxuiToggle::TrackAndThumb  geometry = DxuiToggle::ComputeTrackAndThumb (pill, direction, checked);
                float                      width    = geometry.track.right  - geometry.track.left;
                float                      height   = geometry.track.bottom - geometry.track.top;

                Assert::IsTrue (height > width, L"a track taller than wide");
                AssertRect (upright, geometry.track, L"the long side runs down from the pill's corner");
                AssertRect (upright, DxuiToggle::ComputeTrackAndThumb (upright, direction, checked).track, L"and an upright box stays upright");
                Assert::AreEqual (pill.left + 9.0f, geometry.thumbCenter.x, kTolerance, L"the thumb on the track's middle line");
                Assert::AreEqual (6.0f,             geometry.thumbRadius,   kTolerance);
            }
        }
    }


    TEST_METHOD (UpAndDown_PutTheThumbAtTheirOwnEndWhileChecked)
    {
        D2D1_RECT_F  pill   = MakePill();
        float        top    = pill.top + 9.0f;
        float        bottom = pill.top + 36.0f - 9.0f;



        Assert::AreEqual (top,    DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Up,   true).thumbCenter.y,  kTolerance, L"Up: on at the top");
        Assert::AreEqual (bottom, DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Up,   false).thumbCenter.y, kTolerance, L"Up: off at the bottom");
        Assert::AreEqual (bottom, DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Down, true).thumbCenter.y,  kTolerance, L"Down: on at the bottom");
        Assert::AreEqual (top,    DxuiToggle::ComputeTrackAndThumb (pill, DxuiToggle::OnDirection::Down, false).thumbCenter.y, kTolerance, L"Down: off at the top");
    }


    //  The orientation changes only the drawing: a click and Space flip the
    //  toggle and raise the change the same way in every direction.
    TEST_METHOD (ClickAndSpace_FlipItInEveryDirection)
    {
        const DxuiToggle::OnDirection  directions[] = { DxuiToggle::OnDirection::Right, DxuiToggle::OnDirection::Up, DxuiToggle::OnDirection::Down };
        size_t                         swept        = 0;



        for (DxuiToggle::OnDirection direction : directions)
        {
            DxuiToggle  toggle;
            int         changes = 0;

            toggle.SetRect        ({ 0, 0, 200, 40 });
            toggle.SetOnDirection (direction);
            toggle.SetOnChange    ([&changes] (bool) { changes++; });

            Assert::IsTrue (toggle.OnLButtonDown (5, 20));
            Assert::IsTrue (toggle.OnLButtonUp   (5, 20));
            Assert::IsTrue (toggle.IsChecked(), L"a click turns it on");

            toggle.SetFocused (true);
            Assert::IsTrue  (toggle.OnKey ((WPARAM) VK_SPACE));
            Assert::IsFalse (toggle.IsChecked(), L"and Space turns it off");
            Assert::AreEqual (2, changes);
            swept++;
        }

        Assert::AreEqual (std::size (directions), swept);
    }


    //  The paint takes the thumb from the geometry: on end in its bounds,
    //  centered top to bottom, the thumb below the middle while a Down
    //  toggle is checked and above it while not.
    TEST_METHOD (Paint_DrawsTheThumbWhereTheGeometryPutsIt)
    {
        constexpr uint32_t  kThumbArgb = 0xFFFFFFFF;
        constexpr float     kMiddleY   = 20.0f;



        for (bool checked : { false, true })
        {
            DxuiToggle                 toggle;
            MockDxuiPainter            painter;
            MockDxuiTextRenderer       text;
            const RecordedPaintCall  * thumb = nullptr;

            toggle.SetRect        ({ 0, 0, 200, 40 });
            toggle.SetOnDirection (DxuiToggle::OnDirection::Down);
            toggle.SetChecked     (checked);
            toggle.Paint          (painter, text);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillCircle && call.argb == kThumbArgb)
                {
                    thumb = &call;
                }
            }

            Assert::IsNotNull (thumb, L"the thumb is painted");
            Assert::AreEqual  (9.0f, thumb->x, kTolerance, L"on the middle line of an 18-wide track at the left of the bounds");
            Assert::IsTrue    (checked ? thumb->y > kMiddleY : thumb->y < kMiddleY, checked ? L"on: toward the bottom" : L"off: toward the top");
        }
    }


    //  GetTrackAndThumb gives the pill where the paint puts it, so a layout
    //  can place labels against either end of it: the thumb it gives for
    //  each position is the circle painted in that position.
    TEST_METHOD (GetTrackAndThumb_IsWhereThePaintPutsIt)
    {
        constexpr uint32_t  kThumbArgb = 0xFFFFFFFF;



        for (bool checked : { false, true })
        {
            DxuiToggle                 toggle;
            MockDxuiPainter            painter;
            MockDxuiTextRenderer       text;
            const RecordedPaintCall  * thumb = nullptr;
            DxuiToggle::TrackAndThumb  geometry;

            toggle.SetRect        ({ 30, 50, 230, 94 });
            toggle.SetOnDirection (DxuiToggle::OnDirection::Down);
            toggle.SetChecked     (checked);
            toggle.Paint          (painter, text);

            geometry = toggle.GetTrackAndThumb (checked);

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillCircle && call.argb == kThumbArgb)
                {
                    thumb = &call;
                }
            }

            Assert::IsNotNull (thumb, L"the thumb is painted");
            Assert::AreEqual  (geometry.thumbCenter.x, thumb->x, kTolerance, L"across");
            Assert::AreEqual  (geometry.thumbCenter.y, thumb->y, kTolerance, checked ? L"down: the painted thumb" : L"up: the painted thumb");
        }
    }


    //  A hidden label paints no text at all, not even the On / Off an
    //  unlabeled toggle narrates, and stays the accessible name.
    TEST_METHOD (HiddenLabel_PaintsNoText)
    {
        DxuiToggle            toggle;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;



        toggle.SetRect         ({ 0, 0, 200, 40 });
        toggle.SetOnDirection  (DxuiToggle::OnDirection::Down);
        toggle.SetLabel        (L"Joyport");
        toggle.Paint           (painter, text);

        Assert::AreEqual (size_t (1), text.Calls().size(), L"shown: the label is painted");

        text.Reset();
        toggle.SetLabelVisible (false);
        toggle.Paint           (painter, text);

        Assert::IsTrue   (text.Calls().empty(),                                      L"hidden: nothing is painted");
        Assert::AreEqual (std::wstring (L"Joyport"), toggle.GetAccessibleName(), L"hidden: still the accessible name");
    }};
