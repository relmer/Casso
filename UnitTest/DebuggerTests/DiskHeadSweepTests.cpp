#include "Pch.h"

#include "Ui/Debugger/Panes/DevicePanelController.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskHeadSweepTests
//
//  The head marker sweeps from track to track at the drive's stepping speed,
//  flashes as it arrives, and leaves a fading trail. 140 quarter tracks over
//  140 pixels at 96 DPI, so a quarter track is a pixel.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskHeadSweepTests)
    {
    public:

        static DxuiDpiScaler Scaler96()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            return scaler;
        }


        //  The fills on the ruler at a quarter track's pixel.
        static std::vector<uint32_t> GetRulerFills (const MockDxuiPainter & painter, float x)
        {
            constexpr float        kTolerance = 0.01f;
            std::vector<uint32_t>  fills;



            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.y == 0.0f && std::abs (call.x - x) < kTolerance &&
                    std::abs (call.width - 1.0f) < kTolerance)
                {
                    fills.push_back (call.argb);
                }
            }

            return fills;
        }



        TEST_METHOD (TheHeadSweepsAtTheSteppingSpeed)
        {
            DiskHeadView  view;



            view.Layout  (RECT { 0, 0, 140 + DiskHeadView::kDriveDip, 40 }, Scaler96());
            view.Tick    (1000);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.Tick    (1000);
            view.SetHead ({ 40, 139, 0x01, true, 0 });

            Assert::AreEqual ((float) DiskHeadView::kDriveDip, view.GetHeadX(), L"a seek starts where the head was");

            view.Tick (1000 + (int64_t) (DiskHeadView::kMsPerQuarter * 10));
            Assert::AreEqual (10.0f + DiskHeadView::kDriveDip, view.GetHeadX(), 0.01f, L"ten quarter tracks along");

            view.Tick (5000);
            Assert::AreEqual (40.0f + DiskHeadView::kDriveDip, view.GetHeadX(), L"and arrives");
        }


        //  A change of drive is a different head, so it moves at once.
        TEST_METHOD (ADriveChangeMovesTheHeadAtOnce)
        {
            DiskHeadView  view;



            view.Layout  (RECT { 0, 0, 140 + DiskHeadView::kDriveDip, 40 }, Scaler96());
            view.Tick    (1000);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.SetHead ({ 40, 139, 0x01, true, 1 });

            Assert::AreEqual (40.0f + DiskHeadView::kDriveDip, view.GetHeadX());
        }


        //  The head flashes as it arrives and settles to the accent; a track
        //  it passed fades out and is gone once the fade is over. Quarter track 2 is between track ticks.
        TEST_METHOD (TheHeadFlashesAndLeavesAFadingTrail)
        {
            constexpr int64_t     kStart = 1000;
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            std::vector<uint32_t> fills;
            int64_t               arrive = kStart + (int64_t) (DiskHeadView::kMsPerQuarter * 4) + 1;



            view.Layout  (RECT { 0, 0, 140 + DiskHeadView::kDriveDip, 40 }, Scaler96());
            view.Tick    (kStart);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.SetHead ({ 4, 139, 0x01, true, 0 });
            view.Tick    (arrive);
            view.Paint   (painter, text, theme);

            fills = GetRulerFills (painter, 4.0f + DiskHeadView::kDriveDip);
            Assert::IsFalse (fills.empty());
            Assert::AreNotEqual (theme.Accent(), fills.back(), L"a head just arrived is brighter than the accent");

            fills = GetRulerFills (painter, 2.0f + DiskHeadView::kDriveDip);
            Assert::IsFalse (fills.empty(), L"the track left behind still shows");
            Assert::IsTrue  ((fills.back() >> 24) < 0xFF, L"and is fading");

            painter.Reset();
            view.Tick  (arrive + DiskHeadView::kSettleMs);
            view.Paint (painter, text, theme);
            Assert::AreEqual (theme.Accent(), GetRulerFills (painter, 4.0f + DiskHeadView::kDriveDip).back(), L"settled");

            painter.Reset();
            view.Tick  (arrive + DiskHeadView::kFadeMs + 1);
            view.Paint (painter, text, theme);
            Assert::IsTrue (GetRulerFills (painter, 2.0f + DiskHeadView::kDriveDip).empty(), L"the trail is gone");
        }


        TEST_METHOD (ThePhaseLampsAreLabeled)
        {
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.Layout  (RECT { 0, 0, 600, 60 }, Scaler96());
            view.Paint   (painter, text, theme);

            for (const wchar_t * label : { L"Phases", L"0", L"3", L"Motor" })
            {
                Assert::IsTrue (std::any_of (text.Calls().begin(), text.Calls().end(),
                                             [&] (const RecordedTextCall & call) { return call.text == label; }), label);
            }
        }


        //  The drive's name is ahead of the ruler. Under the ruler, from its
        //  left edge: the track, the word Motor in the accent while the motor
        //  is on, then the phases 3 2 1 0, the last ending at the ruler's
        //  right edge.
        TEST_METHOD (TheRowUnderTheRulerIsTrackMotorAndPhases)
        {
            constexpr float       kRight  = 900.0f;
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            float                 driveX  = -1.0f;
            float                 driveY  = -1.0f;
            float                 trackX  = -1.0f;
            float                 motorX  = -1.0f;
            uint32_t              motor   = 0;
            std::vector<float>    digitX (4, -1.0f);
            float                 lampEnd = 0.0f;



            view.SetHead ({ 69, 139, 0x01, true, 0 });
            view.Layout  (RECT { 0, 0, (LONG) kRight, 60 }, Scaler96());
            view.Paint   (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"Drive 1")       { driveX = call.x; driveY = call.y; }
                if (call.text == L"Track 17.25")   { trackX = call.x; }
                if (call.text == L"Motor")         { motorX = call.x; motor = call.argb; }

                for (int phase = 0; phase < 4; phase++)
                {
                    digitX[(size_t) phase] = (call.text == std::format (L"{}", phase)) ? call.x : digitX[(size_t) phase];
                }
            }

            for (const RecordedPaintCall & call : painter.Calls())
            {
                lampEnd = (call.kind == RecordedPaintKind::FillRect && call.y > 18.0f) ? std::max (lampEnd, call.x + call.width) : lampEnd;
            }

            Assert::AreEqual (0.0f, driveX);
            Assert::AreEqual (0.0f, driveY, L"the drive's name is on the ruler's row");
            Assert::AreEqual ((float) DiskHeadView::kDriveDip, trackX, L"the track starts under the ruler's left edge");
            Assert::IsTrue   (motorX > trackX, L"then Motor");
            Assert::AreEqual (theme.Accent(), motor, L"lit while the motor is on");
            Assert::IsTrue   (digitX[3] > motorX && digitX[3] < digitX[2] && digitX[2] < digitX[1] && digitX[1] < digitX[0], L"the phases 3 2 1 0");
            Assert::IsTrue   (digitX[0] < kRight && kRight - digitX[0] <= (float) DiskHeadView::kLampStepDip, L"ending at the ruler's right edge");
            Assert::IsTrue   (lampEnd <= kRight);
        }



        //  With both drives chosen, drive 2's head is drawn beside drive 1's
        //  where the width has room for two, and under it where it does not.
        //  The drive not selected has its track, with its phases and motor off.
        TEST_METHOD (BothDrivesAreDrawnSideBySideOrStacked)
        {
            DiskHeadView         view;
            DiagnosticsDiskHead  payload { 40, 139, 0x03, true, 0, { 40, 68 } };
            int                  wide    = (DiskHeadView::kSideBySideDip + DiskHeadView::kLabelGapDip) * 2;
            int                  single  = 0;



            view.SetHead   (payload);
            view.SetChoice (DiskHeadView::DriveChoice::Both);

            Assert::IsNotNull (view.GetOther());
            Assert::AreEqual  (40, view.GetHead().quarterTrack);
            Assert::AreEqual  (68, view.GetOther()->GetHead().quarterTrack);
            Assert::AreEqual  (1,  view.GetOther()->GetHead().drive);
            Assert::IsFalse   (view.GetOther()->GetHead().motorOn, L"the motor is drive 1's");
            Assert::AreEqual  ((int) (Byte) 0, (int) view.GetOther()->GetHead().phases);

            view.Layout (RECT { 0, 0, wide, 100 }, Scaler96());
            Assert::IsTrue  (view.GetOther()->GetBounds().left > view.GetBounds().right, L"beside");
            Assert::AreEqual (view.GetBounds().top, view.GetOther()->GetBounds().top);

            view.Layout (RECT { 0, 0, 300, 100 }, Scaler96());
            Assert::IsTrue  (view.GetOther()->GetBounds().top > view.GetBounds().bottom, L"under");

            single = DiskHeadView().GetPreferredHeightPx (300, Scaler96());
            Assert::IsTrue  (view.GetPreferredHeightPx (300, Scaler96()) > single * 2, L"two stacked and a gap");

            view.SetChoice (DiskHeadView::DriveChoice::Drive2);
            Assert::AreEqual (68, view.GetHead().quarterTrack, L"drive 2 alone");
            Assert::AreEqual (single, view.GetPreferredHeightPx (300, Scaler96()));
        }



        //  The choice is saved as text and read back; anything else is both.
        TEST_METHOD (TheDriveChoiceIsSavedAsText)
        {
            for (DiskHeadView::DriveChoice choice : { DiskHeadView::DriveChoice::Drive1, DiskHeadView::DriveChoice::Drive2, DiskHeadView::DriveChoice::Both })
            {
                Assert::IsTrue (choice == DevicePanelController::FromText (DevicePanelController::ToText (choice)));
            }

            Assert::IsTrue (DiskHeadView::DriveChoice::Both == DevicePanelController::FromText (""));
        }
    };
}