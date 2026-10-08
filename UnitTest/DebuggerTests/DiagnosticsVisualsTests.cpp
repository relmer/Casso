#include "Pch.h"
#include "Ui/Debugger/ColorLegend.h"

#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsVisualsTests
//
//  The three panel graphics, painted into a recording painter at 96 DPI so a
//  pixel is a DIP and each geometry can be checked by hand.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiagnosticsVisualsTests)
    {
    public:

        static DxuiDpiScaler Scaler96()
        {
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            return scaler;
        }


        static bool HasFill (const MockDxuiPainter & painter, float x, float y, float width, uint32_t argb)
        {
            constexpr float  kTolerance = 0.01f;



            return std::any_of (painter.Calls().begin(), painter.Calls().end(), [=] (const RecordedPaintCall & call)
            {
                return call.kind == RecordedPaintKind::FillRect && call.argb == argb &&
                       std::abs (call.x - x) < kTolerance && std::abs (call.y - y) < kTolerance && std::abs (call.width - width) < kTolerance;
            });
        }


        static bool HasText (const MockDxuiTextRenderer & text, const std::wstring & string)
        {
            return std::any_of (text.Calls().begin(), text.Calls().end(), [&] (const RecordedTextCall & call) { return call.text == string; });
        }



        //  A bar 256 pixels wide past its label gives each page one pixel; a run
        //  of pages of one source is one fill.
        TEST_METHOD (TheMemoryMapColorsEachPageBySource)
        {
            constexpr float       kLabel    = (float) MemoryMapBar::kLabelDip;
            constexpr float       kWrite    = (float) (MemoryMapBar::kStripDip + MemoryMapBar::kGapDip);
            MemoryMapBar          bar;
            DiagnosticsMemoryMap  map;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            size_t                readFills = 0;



            for (DiagnosticsMemoryMap::Page & page : map.pages)
            {
                page = { MemorySource::Main, MemorySource::Main };
            }

            map.pages[0x00] = { MemorySource::Aux, MemorySource::Aux };
            map.pages[0x01] = { MemorySource::Aux, MemorySource::Main };
            map.pages[0xC0] = { MemorySource::Io,  MemorySource::Io };

            for (size_t page = 0xD0; page < DiagnosticsMemoryMap::kPageCount; page++)
            {
                map.pages[page] = { MemorySource::Rom, MemorySource::None };
            }

            bar.SetMap (map);
            bar.Layout (RECT { 0, 0, (LONG) kLabel + 256, 60 }, Scaler96());
            bar.Paint  (painter, text, theme);

            Assert::IsTrue (HasFill (painter, kLabel,          0.0f,   2.0f,  MemoryMapBar::GetSourceColor (MemorySource::Aux)),  L"pages 0 and 1 read aux");
            Assert::IsTrue (HasFill (painter, kLabel + 2,      0.0f,   190.0f, MemoryMapBar::GetSourceColor (MemorySource::Main)), L"then main to $BF");
            Assert::IsTrue (HasFill (painter, kLabel + 0xC0,   0.0f,   1.0f,  MemoryMapBar::GetSourceColor (MemorySource::Io)));
            Assert::IsTrue (HasFill (painter, kLabel,          kWrite, 1.0f,  MemoryMapBar::GetSourceColor (MemorySource::Aux)),  L"only page 0 writes aux");
            Assert::IsTrue (HasFill (painter, kLabel + 0xD0,   kWrite, 48.0f, theme.Divider()), L"a write that goes nowhere");

            for (const RecordedPaintCall & call : painter.Calls())
            {
                readFills += (call.y == 0.0f) ? 1 : 0;
            }

            Assert::AreEqual ((size_t) 5, readFills, L"aux, main, I/O, main, ROM");
        }


        //  A run of pages from one source tells, in its tip, its color, what
        //  it is, which way the strip goes, and its addresses.
        TEST_METHOD (AMemoryMapRunsTipSaysItsColorWhatAndWhere)
        {
            constexpr LONG        kLabel = MemoryMapBar::kLabelDip;
            constexpr LONG        kWrite = MemoryMapBar::kStripDip + MemoryMapBar::kGapDip;
            MemoryMapBar          bar;
            DiagnosticsMemoryMap  map;
            std::wstring          tip;



            for (DiagnosticsMemoryMap::Page & page : map.pages)
            {
                page = { MemorySource::Main, MemorySource::Main };
            }

            map.pages[0x00] = { MemorySource::Aux, MemorySource::Aux };
            map.pages[0x01] = { MemorySource::Aux, MemorySource::Main };

            for (size_t page = 0xD0; page < DiagnosticsMemoryMap::kPageCount; page++)
            {
                map.pages[page] = { MemorySource::Rom, MemorySource::None };
            }

            bar.SetMap (map);
            bar.Layout (RECT { 0, 0, kLabel + 256, 60 }, Scaler96());

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kLabel + 1, 2 }, tip));
            Assert::AreEqual (std::wstring (L"Orange: reads come from Aux RAM ($0000-$01FF)"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kLabel + 0x40, 2 }, tip));
            Assert::AreEqual (std::wstring (L"Blue: reads come from Main RAM ($0200-$CFFF)"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kLabel + 0xE0, kWrite + 2 }, tip));
            Assert::AreEqual (std::wstring (L"Nothing is written here ($D000-$FFFF)"), tip);

            Assert::IsFalse  (bar.TryGetTipAt (POINT { 2, 2 }, tip), L"none over the R label");
            Assert::IsFalse  (bar.TryGetTipAt (POINT { kLabel + 1, 50 }, tip), L"none below the strips");
        }


        TEST_METHOD (EveryMemoryMapSourceHasAColorNameOfItsOwn)
        {
            std::set<std::wstring>  names;



            for (int source = (int) MemorySource::Main; source < (int) MemorySource::Count; source++)
            {
                names.insert (ColorLegend::GetColorName (MemoryMapBar::GetSourceColor ((MemorySource) source)));
            }

            Assert::AreEqual ((size_t) MemorySource::Count - 1, names.size(), L"two sources share a color name");
            Assert::AreEqual (std::wstring (L"Gray"), ColorLegend::GetColorName (0xFF808080));
        }


        //  140 quarter tracks across 140 pixels: the head sits at its quarter
        //  track, lit while the motor turns.
        TEST_METHOD (TheDiskHeadSitsAtItsQuarterTrack)
        {
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            view.SetHead ({ 69, 139, 0x05, true, 0 });
            view.Layout  (RECT { 0, 0, 140, 40 }, Scaler96());
            view.Paint   (painter, text, theme);

            Assert::AreEqual (69.0f, view.GetHeadX());
            Assert::AreEqual (1.0f,  view.GetHeadWidth());
            Assert::IsTrue   (HasFill (painter, 69.0f, 0.0f, 1.0f, theme.Accent()), L"the head, lit");
            Assert::IsTrue   (HasText (text, L"Drive 1") && HasText (text, L"track 17.25"));

            //  Phases 0 and 2 lit, 1 and 3 dark, then the motor, at full size
            //  in a pane wide enough for the row.
            painter.Reset();
            view.Layout (RECT { 0, 0, 1000, 40 }, Scaler96());
            view.Paint  (painter, text, theme);
            view.Layout (RECT { 0, 0, 140, 40 }, Scaler96());

            Assert::IsTrue (HasFill (painter, (float) DiskHeadView::kCaptionDip,                         23.0f, 12.0f, theme.Accent()));
            Assert::IsTrue (HasFill (painter, (float) (DiskHeadView::kCaptionDip + DiskHeadView::kLampStepDip),     23.0f, 12.0f, theme.ControlBackground()));
            Assert::IsTrue (HasFill (painter, (float) (DiskHeadView::kCaptionDip + DiskHeadView::kLampStepDip * 2), 23.0f, 12.0f, theme.Accent()));
            Assert::IsTrue (HasFill (painter, (float) (DiskHeadView::kCaptionDip + DiskHeadView::kLampStepDip * 4), 23.0f, 12.0f, theme.Accent()), L"the motor");

            painter.Reset();
            view.SetHead ({ 0, 139, 0x00, false, 1 });
            view.Paint   (painter, text, theme);
            Assert::IsTrue (HasFill (painter, 0.0f, 0.0f, 1.0f, theme.ForegroundMuted()), L"a resting head is muted");

            //  140 pixels is too narrow for the drive and track beside the
            //  lamps, or on one row of their own, so they take a row each
            //  rather than being cut.
            Assert::AreEqual (view.GetPreferredHeightPx (1000, Scaler96()) + DiskHeadView::kRowDip * 2,
                              view.GetPreferredHeightPx (140, Scaler96()));
        }


        //  A pane narrower than the lamp row scales the row down to fit, so
        //  the motor's lamp and its caption stay inside the pane and on one
        //  row with the phases.
        TEST_METHOD (TheDiskLampRowScalesDownToFitANarrowPane)
        {
            constexpr LONG        kNarrow = 140;
            constexpr float       kSlack  = 0.01f;
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            float                 phasesY = -1.0f;
            float                 motorY  = -2.0f;
            float                 motorX  = 0.0f;
            float                 motorW  = 0.0f;
            float                 motorPt = 0.0f;
            float                 lampEnd = 0.0f;



            view.SetHead ({ 69, 139, 0x05, true, 0 });
            view.Layout  (RECT { 0, 0, kNarrow, 60 }, Scaler96());
            view.Paint   (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.text == L"Phases")
                {
                    phasesY = call.y;
                }

                if (call.text == L"Motor")
                {
                    motorY  = call.y;
                    motorX  = call.x;
                    motorW  = call.width;
                    motorPt = call.fontSizeDip;
                }
            }

            for (const RecordedPaintCall & call : painter.Calls())
            {
                if (call.kind == RecordedPaintKind::FillRect && call.y > 0.0f)
                {
                    lampEnd = std::max (lampEnd, call.x + call.width);
                }
            }

            Assert::AreEqual (phasesY, motorY,                         L"the motor stays on the phases' row");
            Assert::IsTrue   (motorX + motorW <= kNarrow + kSlack,    L"the motor's caption is inside the pane");
            Assert::IsTrue   (lampEnd <= kNarrow + kSlack,            L"every lamp is inside the pane");
            Assert::IsTrue   (motorPt < theme.MonospaceFont().sizeDip, L"the captions shrink with the row");
        }


        //  The drive and track stay one string wherever it fits, and in a pane
        //  too narrow for it break only between the drive and the track, each
        //  on a row of its own at the same left edge.
        TEST_METHOD (TheDiskLabelBreaksOnlyBetweenDriveAndTrack)
        {
            constexpr LONG        kFits     = 160;      // past the label's 156 pixels at 13 dip
            constexpr LONG        kNarrow   = 140;
            constexpr LONG        kTiny     = 40;
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            float                 driveX    = -1.0f;
            float                 driveY    = -1.0f;
            float                 trackX    = -2.0f;
            float                 trackY    = -1.0f;



            view.SetHead ({ 69, 139, 0x05, true, 0 });

            for (LONG width : { 1000L, kFits })
            {
                text.Reset();
                view.Layout (RECT { 0, 0, width, 60 }, Scaler96());
                view.Paint  (painter, text, theme);
                Assert::IsTrue  (HasText (text, L"Drive 1  track 17.25"), L"one line where it fits");
                Assert::IsFalse (HasText (text, L"Drive 1"),              L"not broken where it fits");
            }

            for (LONG width : { kNarrow, kTiny })
            {
                text.Reset();
                view.Layout (RECT { 0, 0, width, 80 }, Scaler96());
                view.Paint  (painter, text, theme);
                Assert::IsFalse (HasText (text, L"Drive 1  track 17.25"), L"too wide for one line");

                for (const RecordedTextCall & call : text.Calls())
                {
                    if (call.text == L"Drive 1")
                    {
                        driveX = call.x;
                        driveY = call.y;
                    }

                    if (call.text == L"track 17.25")
                    {
                        trackX = call.x;
                        trackY = call.y;
                    }
                }

                Assert::AreEqual (driveX, trackX,                                  L"both halves at the left edge");
                Assert::AreEqual (driveY + (float) DiskHeadView::kRowDip, trackY,  L"the track on the row below the drive");
            }

            Assert::AreEqual (view.GetPreferredHeightPx (kFits, Scaler96()) + DiskHeadView::kRowDip,
                              view.GetPreferredHeightPx (kNarrow, Scaler96()), L"a row more for the break");
        }


        //  A bar 100 pixels past its label: each level fills its share, and a
        //  level outside 0 to 1 stops at the end.
        TEST_METHOD (MetersFillInProportion)
        {
            constexpr float       kLabel = (float) MeterBar::kLabelDip;
            constexpr float       kStep  = (float) (MeterBar::kRowDip + MeterBar::kGapDip);
            MeterBar              bar;
            DiagnosticsMeters     meters;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            meters.levels = { { "half", 0.5f }, { "over", 1.5f }, { "under", -1.0f } };
            bar.SetMeters (meters);
            bar.Layout    (RECT { 0, 0, (LONG) kLabel + 100, 100 }, Scaler96());
            bar.Paint     (painter, text, theme);

            Assert::AreEqual (bar.GetPreferredHeightPx (Scaler96()), (int) (kStep * 3));
            Assert::IsTrue   (HasFill (painter, kLabel, 0.0f,      50.0f,  theme.Accent()));
            Assert::IsTrue   (HasFill (painter, kLabel, kStep,     100.0f, theme.Accent()));
            Assert::IsTrue   (HasFill (painter, kLabel, kStep * 2, 0.0f,   theme.Accent()));
            Assert::IsTrue   (HasText (text, L"half") && HasText (text, L"under"));

            //  Rows that do not fit are left out.
            painter.Reset();
            text.Reset();
            bar.Layout (RECT { 0, 0, (LONG) kLabel + 100, (LONG) kStep }, Scaler96());
            bar.Paint  (painter, text, theme);
            Assert::IsTrue  (HasText (text, L"half"));
            Assert::IsFalse (HasText (text, L"over"));
        }
    };
}
