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
        cluster.SetInputState (false, InputMappingMode::Off, true);      // the mouse segment
        cluster.Layout (RECT { 0, 0, 400, 42 }, true, scaler);

        Assert::IsTrue (cluster.IsExpanded());

        // The joystick and paddle segments moved onto the command bar's
        // paddle-source picker, which wears whichever of them is driving on
        // its face; only the mouse is still this entry's to show.
        Assert::AreEqual (1, cluster.GetSegmentCount());

        p = SegmentCenter (0);
        Assert::IsTrue (cluster.OnLButtonDown (p.x, p.y));
        Assert::IsTrue (cluster.OnClick (p.x, p.y));
        Assert::IsTrue (reported == InputMappingMode::Mouse);

        Assert::AreEqual (1, reports);

        // A click on the label lands on no segment: not taken, not consumed.
        Assert::IsFalse (cluster.OnLButtonDown (5, 21));
        Assert::IsFalse (cluster.OnClick (5, 21));
        Assert::AreEqual (1, reports);
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


        // No mouse on this machine, so the entry has nothing left to show at
        // all: the other two segments now live on the paddle-source picker.
        cluster.SetInputState (false, InputMappingMode::Off, false);
        cluster.Layout (RECT { 0, 0, 400, 42 }, true, scaler);

        Assert::AreEqual (0, cluster.GetSegmentCount());

        p = SegmentCenter (0);
        Assert::IsNull (cluster.GetTooltipAt (p.x, p.y, anchor));
        Assert::IsNull (cluster.GetTooltipAt (5, 21, anchor));           // the label

        cluster.SetInputState (false, InputMappingMode::Off, true);
        cluster.Layout (RECT { 0, 0, 400, 42 }, true, scaler);

        Assert::AreEqual (1, cluster.GetSegmentCount());

        p = SegmentCenter (0);
        Assert::IsNotNull (cluster.GetTooltipAt (p.x, p.y, anchor));
        Assert::AreEqual  ((LONG) s_kLabelPx, anchor.left);
    }


    TEST_METHOD (InputCluster_PickerItemsFollowSegmentCountAndState)
    {
        InputClusterEntry  cluster;


        // Arrows and paddle no longer appear here whatever they are set to:
        // the paddle-source picker on the command bar owns that answer.
        Assert::IsFalse  (cluster.SetInputState (true, InputMappingMode::Paddle, false));
        Assert::AreEqual ((size_t) 0, cluster.GetPickerItems().size());

        Assert::IsTrue   (cluster.SetInputState (false, InputMappingMode::Mouse, true));
        Assert::AreEqual ((size_t) 1, cluster.GetPickerItems().size());
        Assert::IsTrue   (cluster.GetPickerItems()[0].command->IsChecked(), L"the mouse is on");

        Assert::IsFalse  (cluster.SetInputState (false, InputMappingMode::Off, true));
        Assert::IsFalse  (cluster.GetPickerItems()[0].command->IsChecked(), L"and off when it is not");
    }
};
