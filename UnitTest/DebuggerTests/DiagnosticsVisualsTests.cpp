#include "Pch.h"
#include "Ui/Debugger/ColorLegend.h"

#include "Ui/Debugger/Panes/DiskHeadView.h"
#include "Ui/Debugger/Panes/MemoryMapBar.h"
#include "Ui/Debugger/Panes/MeterBar.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"
#include "Ui/Chrome/CassoTheme.h"

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



        //  A map of a few runs per strip: aux pages 0 and 1 (only page 0
        //  written there), main RAM, the I/O page, main RAM again, then ROM
        //  that writes go nowhere from.
        static DiagnosticsMemoryMap MakeMap()
        {
            DiagnosticsMemoryMap  map;



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

            return map;
        }



        //  A run of pages of one source is one fill from its first page to
        //  the next run, the read strip's left of the write strip's.
        TEST_METHOD (TheMemoryMapColorsEachPageBySource)
        {
            constexpr float       kRead     = (float) MemoryMapBar::kBoundsDip;
            constexpr float       kWrite    = (float) (MemoryMapBar::kBoundsDip + MemoryMapBar::kStripDip + MemoryMapBar::kStripGapDip);
            constexpr float       kStrip    = (float) MemoryMapBar::kStripDip;
            MemoryMapBar          bar;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            size_t                readFills = 0;



            bar.SetMap (MakeMap());
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 256 }, Scaler96());
            bar.Paint  (painter, text, theme);

            Assert::IsTrue (HasFill (painter, kRead,  bar.GetPageY (0x00, false), kStrip, MemoryMapBar::GetSourceColor (MemorySource::Aux)),  L"pages 0 and 1 read aux");
            Assert::IsTrue (HasFill (painter, kRead,  bar.GetPageY (0x02, false), kStrip, MemoryMapBar::GetSourceColor (MemorySource::Main)), L"then main to $BF");
            Assert::IsTrue (HasFill (painter, kRead,  bar.GetPageY (0xC0, false), kStrip, MemoryMapBar::GetSourceColor (MemorySource::Io)));
            Assert::IsTrue (HasFill (painter, kWrite, bar.GetPageY (0x00, true),  kStrip, MemoryMapBar::GetSourceColor (MemorySource::Aux)),  L"only page 0 writes aux");
            Assert::IsTrue (HasFill (painter, kWrite, bar.GetPageY (0xD0, true),  kStrip, theme.Divider()), L"a write that goes nowhere");

            for (const RecordedPaintCall & call : painter.Calls())
            {
                readFills += (call.kind == RecordedPaintKind::FillRect && call.x == kRead) ? 1 : 0;
            }

            Assert::AreEqual ((size_t) 5, readFills, L"aux, main, I/O, main, ROM");
            Assert::IsTrue   (HasText (text, L"Main RAM"), L"the tall main run has no name");
            Assert::IsTrue   (HasText (text, L"$0000") && HasText (text, L"$FFFF"), L"the strips' ends have no labels");
        }


        //  A strip shorter than one label line has its first label at its
        //  top, not over the heading above it.
        TEST_METHOD (AMemoryMapShorterThanALabelHasItsLabelAtItsTop)
        {
            MemoryMapBar          bar;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            CassoTheme            theme   = CassoTheme::MakeSkeuomorphic();     // a font with a size, so a label has a height
            auto                  first   = text.Calls().end();



            bar.SetMap (MakeMap());
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 4 }, Scaler96());
            bar.Paint  (painter, text, theme);

            first = std::ranges::find_if (text.Calls(), [] (const RecordedTextCall & call) { return call.text == L"$0000"; });

            Assert::IsTrue (first != text.Calls().end(), L"the first label");
            Assert::IsTrue (first->y >= bar.GetPageY (0, false) - 0.01f, std::format (L"at {}, above the strip's top at {}", first->y, bar.GetPageY (0, false)).c_str());
        }


        //  A run of one page is as tall as the minimum, taken from the larger
        //  runs, and the runs fill the strip from its heading to its foot.
        TEST_METHOD (EveryMemoryMapRunIsAtLeastTheMinimumTall)
        {
            constexpr float  kTop    = (float) MemoryMapBar::kHeadingDip;
            constexpr float  kMin    = (float) MemoryMapBar::kMinRunDip;
            MemoryMapBar     bar;



            bar.SetMap (MakeMap());
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 256 }, Scaler96());

            for (bool isWrite : { false, true })
            {
                Assert::AreEqual (kTop,         bar.GetPageY (0x00, isWrite), 0.01f, L"the strip does not start below its heading");
                Assert::AreEqual (kTop + 256.0f, bar.GetPageY ((int) DiagnosticsMemoryMap::kPageCount, isWrite), 0.01f, L"the runs do not fill the strip");
                Assert::IsTrue   (bar.GetPageY (0xC1, isWrite) - bar.GetPageY (0xC0, isWrite) >= kMin - 0.01f, L"the I/O page is shorter than the minimum");
            }

            Assert::IsTrue (bar.GetPageY (0x02, false) - bar.GetPageY (0x00, false) >= kMin - 0.01f, L"the two aux pages are shorter than the minimum");
            Assert::IsTrue (bar.GetPageY (0x01, true)  - bar.GetPageY (0x00, true)  >= kMin - 0.01f, L"the one aux page written is shorter than the minimum");
        }


        //  A run's tip has its source and its addresses, and nothing over the
        //  bounds' labels or the headings.
        //  The read and write strips share one address axis, so an address is
        //  at the same height in both, though the two strips' runs differ.
        TEST_METHOD (TheMemoryMapStripsShareOneAddressAxis)
        {
            MemoryMapBar  bar;



            bar.SetMap (MakeMap());
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 256 }, Scaler96());

            for (int page = 0; page <= (int) DiagnosticsMemoryMap::kPageCount; page++)
            {
                Assert::AreEqual (bar.GetPageY (page, false), bar.GetPageY (page, true), 0.01f, std::format (L"page ${:02X}", page).c_str());
            }
        }


        TEST_METHOD (AMemoryMapRunsTipHasItsSourceAndAddresses)
        {
            constexpr LONG        kRead  = MemoryMapBar::kBoundsDip;
            constexpr LONG        kWrite = MemoryMapBar::kBoundsDip + MemoryMapBar::kStripDip + MemoryMapBar::kStripGapDip;
            MemoryMapBar          bar;
            std::wstring          tip;



            bar.SetMap (MakeMap());
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 256 }, Scaler96());

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kRead + 1, (LONG) bar.GetPageY (0x00, false) + 1 }, tip));
            Assert::AreEqual (std::wstring (L"Aux RAM  $0000-$01FF"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kRead + 1, (LONG) bar.GetPageY (0x40, false) }, tip));
            Assert::AreEqual (std::wstring (L"Main RAM  $0200-$BFFF"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kWrite + 1, (LONG) bar.GetPageY (0xE0, true) }, tip));
            Assert::AreEqual (std::wstring (L"Not written  $D000-$FFFF"), tip);

            Assert::IsFalse  (bar.TryGetTipAt (POINT { 2, (LONG) bar.GetPageY (0x02, false) }, tip), L"none over the bounds' labels");
            Assert::IsFalse  (bar.TryGetTipAt (POINT { kRead + 1, 2 }, tip), L"none over the headings");
        }


        //  The selected card's expansion ROM is a run apart from the slots'
        //  pages, with the slot in its tip.
        TEST_METHOD (TheExpansionRomRunHasItsSlot)
        {
            constexpr LONG        kRead = MemoryMapBar::kBoundsDip;
            DiagnosticsMemoryMap  map   = MakeMap();
            MemoryMapBar          bar;
            std::wstring          tip;



            for (size_t page = 0xC1; page <= 0xCE; page++)
            {
                map.pages[page] = { MemorySource::SlotRom, MemorySource::None };
            }

            map.pages[0xCF]   = { MemorySource::None, MemorySource::None };
            map.expansionSlot = 2;

            bar.SetMap (map);
            bar.Layout (RECT { 0, 0, MemoryMapBar::GetPreferredWidthPx (Scaler96()), MemoryMapBar::kHeadingDip + 512 }, Scaler96());

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kRead + 1, (LONG) bar.GetPageY (0xC1, false) + 1 }, tip));
            Assert::AreEqual (std::wstring (L"Slot ROM  $C100-$C7FF"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kRead + 1, (LONG) bar.GetPageY (0xC8, false) + 1 }, tip));
            Assert::AreEqual (std::wstring (L"Slot 2 ROM  $C800-$CEFF"), tip);

            Assert::IsTrue   (bar.TryGetTipAt (POINT { kRead + 1, (LONG) bar.GetPageY (0xCF, false) + 1 }, tip));
            Assert::AreEqual (std::wstring (L"Unused (floating bus)  $CF00-$CFFF"), tip);
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


        //  140 quarter tracks across a 140-pixel ruler past the drive's name:
        //  the head sits at its quarter track, lit while the motor turns.
        TEST_METHOD (TheDiskHeadSitsAtItsQuarterTrack)
        {
            constexpr float       kLeft  = (float) DiskHeadView::kDriveDip;
            constexpr float       kRight = 1000.0f;
            constexpr float       kStep  = (float) DiskHeadView::kLampStepDip;
            DiskHeadView          view;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;



            view.SetHead ({ 69, 139, 0x05, true, 0 });
            view.Layout  (RECT { 0, 0, 140 + DiskHeadView::kDriveDip, 40 }, Scaler96());
            view.Paint   (painter, text, theme);

            Assert::AreEqual (kLeft + 69.0f, view.GetHeadX());
            Assert::AreEqual (1.0f,  view.GetHeadWidth());
            Assert::IsTrue   (HasFill (painter, kLeft + 69.0f, 0.0f, 1.0f, theme.Accent()), L"the head, lit");
            Assert::IsTrue   (HasText (text, L"Drive 1") && HasText (text, L"Track 17.25"));

            //  Phases 3 2 1 0 at full size, ending at the ruler's right edge:
            //  0 and 2 lit, 1 and 3 dark.
            painter.Reset();
            view.Layout (RECT { 0, 0, (LONG) kRight, 40 }, Scaler96());
            view.Paint  (painter, text, theme);

            Assert::IsTrue (HasFill (painter, kRight - kStep * 3 - 12.0f, 23.0f, 12.0f, theme.ControlBackground()), L"phase 3");
            Assert::IsTrue (HasFill (painter, kRight - kStep * 2 - 12.0f, 23.0f, 12.0f, theme.Accent()),            L"phase 2");
            Assert::IsTrue (HasFill (painter, kRight - kStep * 1 - 12.0f, 23.0f, 12.0f, theme.ControlBackground()), L"phase 1");
            Assert::IsTrue (HasFill (painter, kRight - 12.0f,     23.0f, 12.0f, theme.Accent()),            L"phase 0");

            painter.Reset();
            view.Layout  (RECT { 0, 0, 140 + DiskHeadView::kDriveDip, 40 }, Scaler96());
            view.SetHead ({ 0, 139, 0x00, false, 1 });
            view.Paint   (painter, text, theme);
            Assert::IsTrue (HasFill (painter, kLeft, 0.0f, 1.0f, theme.ForegroundMuted()), L"a resting head is muted");

            //  The row under the ruler shrinks rather than wrapping, so the
            //  height is the same at any width.
            Assert::AreEqual (view.GetPreferredHeightPx (1000, Scaler96()), view.GetPreferredHeightPx (140, Scaler96()));
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
