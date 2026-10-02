#include "Pch.h"

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
    };
}
