#include "Pch.h"

#include "Ui/Chrome/InputClusterEntry.h"
#include "Ui/Chrome/PrinterStatusLed.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeToolbarPartsTests
//
//  The two emulator parts the toolbar hosts, driven on their own: the
//  printer light's status-to-color rule, and the input cluster's segments
//  as the toolbar sees them through IDxuiToolbarCustomEntry.
//
//  The cluster is laid out at 96 dpi with no text renderer, so the label
//  measures by the character fallback: 5 glyphs at 7.5 px, plus the 3 px
//  slack, then the 8 px gap, then segments of 5 + 7 + 4 + 19 + 5 = 40 px,
//  2 px apart.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ChromeToolbarPartsTests)
{
public:

    static constexpr int  s_kLabelPx = 37 + 3 + 8;   // "Input" + slack + gap
    static constexpr int  s_kSegPx   = 40;
    static constexpr int  s_kSegGap  = 2;

    static POINT  SegmentCenter (int index)
    {
        return POINT { s_kLabelPx + index * (s_kSegPx + s_kSegGap) + s_kSegPx / 2, 21 };
    }


    TEST_METHOD (PrinterLed_ColorPerStatus_NothingWhileIdle)
    {
        Assert::AreEqual (0u,          PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Idle));
        Assert::AreEqual (0xFF4CE96Au, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Receiving));
        Assert::AreEqual (0xFFFFB938u, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Pending));
        Assert::AreEqual (0xFFFF5257u, PrinterStatusLed::GetStatusCoreColor (PrinterStatus::Error));
    }


    TEST_METHOD (InputCluster_SegmentClickReportsItsMode)
    {
        InputClusterEntry  cluster;
        DxuiDpiScaler      scaler;
        InputMappingMode   reported = InputMappingMode::Off;
        int                reports  = 0;
        POINT              p        = {};


        cluster.SetSink ([&] (InputMappingMode mode) { reported = mode; reports++; });
        cluster.SetInputState (false, InputMappingMode::Off, true);      // three segments
        cluster.Layout (RECT { 0, 0, 400, 42 }, true, scaler);

        Assert::IsTrue (cluster.IsExpanded());
        Assert::AreEqual (3, cluster.GetSegmentCount());

        p = SegmentCenter (0);
        Assert::IsTrue (cluster.OnLButtonDown (p.x, p.y));
        Assert::IsTrue (cluster.OnClick (p.x, p.y));
        Assert::IsTrue (reported == InputMappingMode::Joystick);

        p = SegmentCenter (1);
        Assert::IsTrue (cluster.OnLButtonDown (p.x, p.y));
        Assert::IsTrue (cluster.OnClick (p.x, p.y));
        Assert::IsTrue (reported == InputMappingMode::Paddle);

        p = SegmentCenter (2);
        Assert::IsTrue (cluster.OnLButtonDown (p.x, p.y));
        Assert::IsTrue (cluster.OnClick (p.x, p.y));
        Assert::IsTrue (reported == InputMappingMode::Mouse);

        Assert::AreEqual (3, reports);

        // A click on the label lands on no segment: not taken, not consumed.
        Assert::IsFalse (cluster.OnLButtonDown (5, 21));
        Assert::IsFalse (cluster.OnClick (5, 21));
        Assert::AreEqual (3, reports);
    }


    TEST_METHOD (InputCluster_ClickNotConsumedWhileCollapsed)
    {
        InputClusterEntry  cluster;
        DxuiDpiScaler      scaler;
        int                reports  = 0;


        cluster.SetSink ([&] (InputMappingMode) { reports++; });
        cluster.SetInputState (false, InputMappingMode::Off, true);
        cluster.Layout (RECT { 0, 0, 35, 42 }, false, scaler);

        Assert::IsFalse (cluster.IsExpanded());
        Assert::IsFalse (cluster.OnLButtonDown (17, 21));
        Assert::IsFalse (cluster.OnClick (17, 21));
        Assert::AreEqual (0, reports);
    }


    TEST_METHOD (InputCluster_TooltipInsideSegmentOnly)
    {
        InputClusterEntry  cluster;
        DxuiDpiScaler      scaler;
        RECT               anchor = {};
        POINT              p      = {};


        cluster.SetInputState (false, InputMappingMode::Off, false);     // two segments
        cluster.Layout (RECT { 0, 0, 400, 42 }, true, scaler);

        Assert::AreEqual (2, cluster.GetSegmentCount());

        p = SegmentCenter (1);
        Assert::IsNotNull (cluster.GetTooltipAt (p.x, p.y, anchor));
        Assert::AreEqual  ((LONG) s_kLabelPx + s_kSegPx + s_kSegGap, anchor.left);

        p = SegmentCenter (2);                                            // no third segment
        Assert::IsNull (cluster.GetTooltipAt (p.x, p.y, anchor));
        Assert::IsNull (cluster.GetTooltipAt (5, 21, anchor));           // the label
    }


    TEST_METHOD (InputCluster_PickerItemsFollowSegmentCountAndState)
    {
        InputClusterEntry  cluster;


        Assert::IsFalse  (cluster.SetInputState (true, InputMappingMode::Paddle, false));
        Assert::AreEqual ((size_t) 2, cluster.GetPickerItems().size());
        Assert::IsTrue   (cluster.GetPickerItems()[0].command->IsChecked());     // joystick
        Assert::IsTrue   (cluster.GetPickerItems()[1].command->IsChecked());     // paddle

        Assert::IsTrue   (cluster.SetInputState (false, InputMappingMode::Mouse, true));
        Assert::AreEqual ((size_t) 3, cluster.GetPickerItems().size());
        Assert::IsFalse  (cluster.GetPickerItems()[0].command->IsChecked());
        Assert::IsTrue   (cluster.GetPickerItems()[2].command->IsChecked());
    }
};
