#include "Pch.h"

#include "Ui/Chrome/TapeDeckWidget.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TapeDeckWidgetTests
//
//  Where each control of the flat recorder is, when each one acts, and what
//  the readout says.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TapeDeckWidgetTests)
{
public:

    static constexpr TapeDeckRegion  s_kButtons[] = { TapeDeckRegion::Rewind, TapeDeckRegion::Play, TapeDeckRegion::Stop,
                                                      TapeDeckRegion::Record, TapeDeckRegion::Eject };


    static void LayOut (TapeDeckWidget & widget)
    {
        DxuiDpiScaler  scaler;
        RECT           anchor = { 100, 200, 100, 200 };



        scaler.SetDpi (96);
        widget.Layout (anchor, scaler);
    }


    static TapeDeckView MakeView (TapeTransport transport, bool isWritable = true, bool isArmed = false)
    {
        TapeDeckView  view;



        view.path            = L"C:\\Tapes\\adventure.wav";
        view.transport       = transport;
        view.isWritable      = isWritable;
        view.isRecordArmed   = isArmed;
        view.positionSeconds = 83.6;
        view.lengthSeconds   = 296.0;

        return view;
    }


    TEST_METHOD (EachButtonCenterHitsItsOwnRegion)
    {
        TapeDeckWidget  widget;
        size_t          checked = 0;



        LayOut (widget);

        for (TapeDeckRegion region : s_kButtons)
        {
            RECT  box = widget.GetButtonRect (region);

            Assert::IsTrue (box.right > box.left);
            Assert::IsTrue (widget.HitTest ((box.left + box.right) / 2, (box.top + box.bottom) / 2) == region);
            checked++;
        }

        Assert::AreEqual (TapeDeckWidget::kButtonCount, checked);
    }


    TEST_METHOD (ButtonsDoNotOverlapAndSitRightOfTheName)
    {
        TapeDeckWidget  widget;
        RECT            name     = {};
        int             previous = 0;



        LayOut (widget);
        name     = widget.GetNameRect();
        previous = name.right;

        for (TapeDeckRegion region : s_kButtons)
        {
            RECT  box = widget.GetButtonRect (region);

            Assert::IsTrue (box.left >= previous);
            previous = box.right;
        }
    }


    TEST_METHOD (NameAndCaptionOpenThePicker)
    {
        TapeDeckWidget  widget;
        RECT            name  = {};
        RECT            outer = {};



        LayOut (widget);
        name  = widget.GetNameRect();
        outer = widget.GetOuterRect();

        Assert::IsTrue (widget.HitTest ((name.left + name.right) / 2, (name.top + name.bottom) / 2) == TapeDeckRegion::Name);
        Assert::IsTrue (widget.HitTest (outer.left, name.top) == TapeDeckRegion::Name, L"the caption is part of the control, as on a drive");
        Assert::IsTrue (widget.HitTest (outer.left - 1, name.top) == TapeDeckRegion::None);
    }


    TEST_METHOD (HiddenWidgetMissesEverywhere)
    {
        TapeDeckWidget  widget;
        RECT            name = {};



        LayOut (widget);
        name = widget.GetNameRect();
        widget.Hide();

        Assert::IsTrue (widget.IsHidden());
        Assert::IsTrue (widget.HitTest ((name.left + name.right) / 2, (name.top + name.bottom) / 2) == TapeDeckRegion::None);
    }


    TEST_METHOD (EmptyDeckOffersOnlyThePicker)
    {
        TapeDeckView  view;



        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Name, view));

        for (TapeDeckRegion region : s_kButtons)
        {
            Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (region, view));
        }
    }


    TEST_METHOD (StoppedDeckOffersPlayRewindRecordEject)
    {
        TapeDeckView  view = MakeView (TapeTransport::Stopped);



        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Rewind, view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Eject,  view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Stop,   view));
    }


    TEST_METHOD (MovingDeckOffersStopNotPlayOrRecord)
    {
        constexpr TapeTransport  kMoving[] = { TapeTransport::Playing, TapeTransport::Recording };



        for (TapeTransport transport : kMoving)
        {
            TapeDeckView  view = MakeView (transport);



            Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Stop,   view));
            Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
            Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        }
    }


    TEST_METHOD (ProtectedTapeCannotRecord)
    {
        TapeDeckView  view = MakeView (TapeTransport::Stopped, false);



        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
    }


    TEST_METHOD (ReadoutShowsElapsedOverLength)
    {
        Assert::AreEqual (std::wstring (L"1:23 / 4:56"), TapeDeckWidget::FormatReadout (MakeView (TapeTransport::Playing)));
        Assert::AreEqual (std::wstring (L"0:00"),        TapeDeckWidget::FormatTime (0.0));
        Assert::AreEqual (std::wstring (L"10:05"),       TapeDeckWidget::FormatTime (605.9));
        Assert::IsTrue   (TapeDeckWidget::FormatReadout (TapeDeckView()).empty());
    }


    TEST_METHOD (ProgressIsTheFractionPlayedAndClamped)
    {
        TapeDeckView  view = MakeView (TapeTransport::Playing);



        Assert::AreEqual (83.6 / 296.0, (double) TapeDeckWidget::GetProgress (view), 1e-6);

        view.positionSeconds = 400.0;
        Assert::AreEqual (1.0, (double) TapeDeckWidget::GetProgress (view), 1e-6);

        view.lengthSeconds = 0.0;
        Assert::AreEqual (0.0, (double) TapeDeckWidget::GetProgress (view), 1e-6);
    }


    TEST_METHOD (NameIsTheFileNameOrEmpty)
    {
        Assert::AreEqual (std::wstring (L"adventure.wav"), TapeDeckWidget::GetDisplayName (MakeView (TapeTransport::Stopped)));
        Assert::AreEqual (std::wstring (L"(empty)"),       TapeDeckWidget::GetDisplayName (TapeDeckView()));
    }

    TEST_METHOD (LoadingOffersOnlyThePickerAndEject)
    {
        TapeDeckView  view = MakeView (TapeTransport::Stopped);



        view.loadingPath = L"C:\\Tapes\\long.mp3";

        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Name,   view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Eject,  view), L"eject cancels the load");
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Rewind, view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        Assert::AreEqual (std::wstring (L"Loading long.mp3\x2026"), TapeDeckWidget::GetDisplayName (view));
    }
};
