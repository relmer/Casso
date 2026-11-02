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



            view.Layout  (RECT { 0, 0, 140, 40 }, Scaler96());
            view.Tick    (1000);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.Tick    (1000);
            view.SetHead ({ 40, 139, 0x01, true, 0 });

            Assert::AreEqual (0.0f, view.GetHeadX(), L"a seek starts where the head was");

            view.Tick (1000 + (int64_t) (DiskHeadView::kMsPerQuarter * 10));
            Assert::AreEqual (10.0f, view.GetHeadX(), 0.01f, L"ten quarter tracks along");

            view.Tick (5000);
            Assert::AreEqual (40.0f, view.GetHeadX(), L"and arrives");
        }


        //  A change of drive is a different head, so it moves at once.
        TEST_METHOD (ADriveChangeMovesTheHeadAtOnce)
        {
            DiskHeadView  view;



            view.Layout  (RECT { 0, 0, 140, 40 }, Scaler96());
            view.Tick    (1000);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.SetHead ({ 40, 139, 0x01, true, 1 });

            Assert::AreEqual (40.0f, view.GetHeadX());
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



            view.Layout  (RECT { 0, 0, 140, 40 }, Scaler96());
            view.Tick    (kStart);
            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.SetHead ({ 4, 139, 0x01, true, 0 });
            view.Tick    (arrive);
            view.Paint   (painter, text, theme);

            fills = GetRulerFills (painter, 4.0f);
            Assert::IsFalse (fills.empty());
            Assert::AreNotEqual (theme.Accent(), fills.back(), L"a head just arrived is brighter than the accent");

            fills = GetRulerFills (painter, 2.0f);
            Assert::IsFalse (fills.empty(), L"the track left behind still shows");
            Assert::IsTrue  ((fills.back() >> 24) < 0xFF, L"and is fading");

            painter.Reset();
            view.Tick  (arrive + DiskHeadView::kSettleMs);
            view.Paint (painter, text, theme);
            Assert::AreEqual (theme.Accent(), GetRulerFills (painter, 4.0f).back(), L"settled");

            painter.Reset();
            view.Tick  (arrive + DiskHeadView::kFadeMs + 1);
            view.Paint (painter, text, theme);
            Assert::IsTrue (GetRulerFills (painter, 2.0f).empty(), L"the trail is gone");
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


        //  The label beside the lamps starts clear of the Motor caption's box.
        TEST_METHOD (TheLabelBesideTheLampsIsClearOfMotor)
        {
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            float                 motorEnd = 0.0f;
            float                 labelX   = 0.0f;



            view.SetHead ({ 0, 139, 0x01, true, 0 });
            view.Layout  (RECT { 0, 0, 900, 60 }, Scaler96());
            view.Paint   (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"Motor")
                {
                    motorEnd = call.x + call.width;
                }
                else if (call.text.starts_with (L"Drive"))
                {
                    labelX = call.x;
                }
            }

            Assert::IsTrue (motorEnd > 0.0f && labelX >= motorEnd + (float) DiskHeadView::kLabelGapDip, L"a gap between Motor and the label");
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