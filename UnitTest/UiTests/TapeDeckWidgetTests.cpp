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

    static constexpr TapeDeckRegion  s_kButtons[] = { TapeDeckRegion::Record, TapeDeckRegion::Rewind, TapeDeckRegion::FastForward,
                                                      TapeDeckRegion::Play,   TapeDeckRegion::Stop,   TapeDeckRegion::Eject };


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


    TEST_METHOD (ButtonsSitUnderTheNameWithoutOverlapping)
    {
        TapeDeckWidget  widget;
        RECT            name     = {};
        int             previous = 0;



        LayOut (widget);
        name     = widget.GetNameRect();
        previous = name.left;

        for (TapeDeckRegion region : s_kButtons)
        {
            RECT  box = widget.GetButtonRect (region);

            Assert::IsTrue (box.left  >= previous);
            Assert::IsTrue (box.top   >  name.bottom, L"the transport is under the rail");
            Assert::IsTrue (box.right <= name.right,  L"and within the name's width");
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



        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Name,  view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Eject, view), L"Eject opens the picker");

        for (TapeDeckRegion region : s_kButtons)
        {
            if (region != TapeDeckRegion::Eject)
            {
                Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (region, view));
            }
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
        constexpr TapeTransport  kMoving[] = { TapeTransport::Playing, TapeTransport::FastForwarding, TapeTransport::Rewinding };



        for (TapeTransport transport : kMoving)
        {
            TapeDeckView  view = MakeView (transport);



            Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Stop,   view));
            Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
            Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        }
    }


    TEST_METHOD (RecordingOffersRecordAndStopButNotPlayOrWinding)
    {
        TapeDeckView  view = MakeView (TapeTransport::Recording);



        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record,      view), L"Record again stops it");
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Stop,        view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,        view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Rewind,      view));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::FastForward, view));
    }


    TEST_METHOD (ProtectedTapeCannotRecord)
    {
        TapeDeckView  view = MakeView (TapeTransport::Stopped, false);



        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Record, view));
        Assert::IsTrue  (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Play,   view));
    }


    TEST_METHOD (MagnificationIsFullUnderThePointerAndFadesToNothing)
    {
        constexpr float  kReach = 40.0f;



        Assert::AreEqual (TapeDeckWidget::kMagnifyMax, TapeDeckWidget::GetMagnification (0.0f,    kReach), 0.0001f);
        Assert::AreEqual (1.0f,                        TapeDeckWidget::GetMagnification (kReach,  kReach), 0.0001f);
        Assert::AreEqual (1.0f,                        TapeDeckWidget::GetMagnification (100.0f,  kReach), 0.0001f);
        Assert::AreEqual (1.0f,                        TapeDeckWidget::GetMagnification (-80.0f,  kReach), 0.0001f,
                          L"far to the right of the pointer is as unmagnified as far to the left");
        Assert::AreEqual (TapeDeckWidget::GetMagnification (10.0f, kReach),
                          TapeDeckWidget::GetMagnification (-10.0f, kReach), 0.0001f, L"either side alike");
        Assert::IsTrue   (TapeDeckWidget::GetMagnification (10.0f, kReach) > TapeDeckWidget::GetMagnification (20.0f, kReach),
                          L"nearer is larger");
    }


    TEST_METHOD (MagnifiedRowIsTheSameWidthWhereverThePointerIs)
    {
        // The row does not slide as the pointer crosses it only because the
        // growth across evenly spaced controls sums to the same at every
        // pointer position, which holds for a reach of exactly two pitches.
        constexpr float  kPitch = 18.0f;
        constexpr float  kReach = 2.0f * kPitch;
        float            first  = 0.0f;



        for (float mouse = 100.0f; mouse < 100.0f + kPitch; mouse += 1.5f)
        {
            float  growth = 0.0f;

            for (int k = 0; k < 12; k++)
            {
                growth += TapeDeckWidget::GetMagnification (mouse - (float) k * kPitch, kReach) - 1.0f;
            }

            if (first == 0.0f)
            {
                first = growth;
            }

            Assert::AreEqual (first, growth, 0.001f, L"the row would slide");
        }
    }


    TEST_METHOD (AtRestTheControlsKeepTheirLayout)
    {
        TapeDeckWidget  widget;
        RECT            box = {};



        LayOut (widget);
        box = widget.GetButtonRect (TapeDeckRegion::Play);

        Assert::IsTrue (widget.HitTest ((box.left + box.right) / 2, (box.top + box.bottom) / 2) == TapeDeckRegion::Play,
                        L"with the pointer away, nothing is magnified");
        Assert::IsFalse (widget.IsMagnifying());
    }


    TEST_METHOD (CounterShowsThePosition)
    {
        Assert::AreEqual (std::wstring (L"1:23"),  TapeDeckWidget::FormatCounter (MakeView (TapeTransport::Playing)));
        Assert::AreEqual (std::wstring (L"0:00"),  TapeDeckWidget::FormatTime (0.0));
        Assert::AreEqual (std::wstring (L"10:05"), TapeDeckWidget::FormatTime (605.9));
        Assert::IsTrue   (TapeDeckWidget::FormatCounter (TapeDeckView()).empty());
    }


    TEST_METHOD (ParseTimeTakesSecondsMinutesAndHours)
    {
        double  seconds = -1.0;



        Assert::IsTrue   (TapeDeckWidget::ParseTime (L"90", seconds));
        Assert::AreEqual (90.0, seconds);
        Assert::IsTrue   (TapeDeckWidget::ParseTime (L" 1:30 ", seconds));
        Assert::AreEqual (90.0, seconds);
        Assert::IsTrue   (TapeDeckWidget::ParseTime (L"1:02:03", seconds));
        Assert::AreEqual (3723.0, seconds);
        Assert::IsTrue   (TapeDeckWidget::ParseTime (L"75:00", seconds), L"minutes lead, so they may run past 59");
        Assert::AreEqual (4500.0, seconds);
    }


    TEST_METHOD (ParseTimeRefusesWhatIsNotATime)
    {
        double  seconds = 42.0;



        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"",         seconds));
        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"1:75",     seconds), L"seconds stop at 59");
        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"1:",       seconds));
        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"a:30",     seconds));
        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"-5",       seconds));
        Assert::IsFalse  (TapeDeckWidget::ParseTime (L"1:2:3:4",  seconds));
        Assert::AreEqual (42.0, seconds, L"a refused time leaves the value alone");
    }


    TEST_METHOD (CounterSitsBesideTheButtonsUnderTheRail)
    {
        TapeDeckWidget  widget;
        RECT            name    = {};
        RECT            counter = {};



        LayOut (widget);
        name    = widget.GetNameRect();
        counter = widget.GetCounterRect();

        Assert::IsTrue (counter.left  >= widget.GetButtonRect (TapeDeckRegion::Eject).right);
        Assert::IsTrue (counter.right <= name.right, L"the controls are no wider than the name");
        Assert::IsTrue (widget.HitTest ((counter.left + counter.right) / 2, (counter.top + counter.bottom) / 2) == TapeDeckRegion::Counter);
        Assert::IsTrue (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Counter, MakeView (TapeTransport::Stopped)));
        Assert::IsFalse (TapeDeckWidget::IsRegionEnabled (TapeDeckRegion::Counter, TapeDeckView()), L"no tape, nothing to wind");
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

    TEST_METHOD (MarqueeRestsBeforeAndAfterItsScrollAndTravelsOnePeriod)
    {
        constexpr int64_t  kStart  = 10000;
        constexpr float    kPeriod = 90.0f;     // px: a name plus the gap
        constexpr float    kSpeed  = 45.0f;     // px a second, so two seconds a scroll



        Assert::AreEqual (0.0f,  TapeDeckWidget::GetMarqueeOffset (kStart - 1,    kStart, kPeriod, kSpeed), L"waiting out the hold");
        Assert::AreEqual (0.0f,  TapeDeckWidget::GetMarqueeOffset (kStart,        kStart, kPeriod, kSpeed));
        Assert::AreEqual (45.0f, TapeDeckWidget::GetMarqueeOffset (kStart + 1000, kStart, kPeriod, kSpeed), 0.01f, L"halfway after a second");
        Assert::AreEqual (0.0f,  TapeDeckWidget::GetMarqueeOffset (kStart + 2000, kStart, kPeriod, kSpeed), L"back at rest, seamlessly");
        Assert::AreEqual (0.0f,  TapeDeckWidget::GetMarqueeOffset (kStart + 500,  kStart, kPeriod, 0.0f),   L"no speed, no scroll");
    }
};