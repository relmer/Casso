#include "Pch.h"

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
            Assert::IsTrue   (HasText (text, L"aux") && HasText (text, L"ROM"), L"the key names the sources in use");
            Assert::IsFalse  (HasText (text, L"slot ROM"),                      L"and only those");
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
            Assert::IsTrue   (HasText (text, L"Drive 1  track 17.25"));

            //  Phases 0 and 2 lit, 1 and 3 dark, then the motor.
            Assert::IsTrue (HasFill (painter, 0.0f,                              23.0f, 12.0f, theme.Accent()));
            Assert::IsTrue (HasFill (painter, (float) DiskHeadView::kLampStepDip,     23.0f, 12.0f, theme.ControlBackground()));
            Assert::IsTrue (HasFill (painter, (float) DiskHeadView::kLampStepDip * 2, 23.0f, 12.0f, theme.Accent()));
            Assert::IsTrue (HasFill (painter, (float) DiskHeadView::kLampStepDip * 4, 23.0f, 12.0f, theme.Accent()), L"the motor");

            painter.Reset();
            view.SetHead ({ 0, 139, 0x00, false, 1 });
            view.Paint   (painter, text, theme);
            Assert::IsTrue (HasFill (painter, 0.0f, 0.0f, 1.0f, theme.ForegroundMuted()), L"a resting head is muted");

            //  140 pixels is too narrow for the drive and track beside the
            //  lamps, so they take a row of their own rather than being cut.
            Assert::AreEqual (view.GetPreferredHeightPx (1000, Scaler96()) + DiskHeadView::kRowDip,
                              view.GetPreferredHeightPx (140, Scaler96()));
        }


        //  In a narrow pane the key wraps rather than running its names into
        //  one another or leaving sources out, and the bar asks for the rows.
        TEST_METHOD (TheMemoryMapKeyWrapsAndNamesEverySource)
        {
            constexpr int         kNarrow = 120;
            MemoryMapBar          bar;
            DiagnosticsMemoryMap  map;
            MockDxuiPainter       painter;
            MockDxuiTextRenderer  text;
            MockDxuiTheme         theme;
            const MemorySource    sources[] = { MemorySource::Main, MemorySource::Aux, MemorySource::LcBank1,
                                                MemorySource::Rom,  MemorySource::SlotRom, MemorySource::Io };



            for (size_t page = 0; page < DiagnosticsMemoryMap::kPageCount; page++)
            {
                map.pages[page] = { sources[page % std::size (sources)], MemorySource::Main };
            }

            bar.SetMap (map);
            bar.Layout (RECT { 0, 0, kNarrow, 200 }, Scaler96());
            bar.Paint  (painter, text, theme);

            for (MemorySource source : sources)
            {
                Assert::IsTrue (HasText (text, MemoryMapBar::GetSourceName (source)), MemoryMapBar::GetSourceName (source));
            }

            Assert::IsTrue (bar.GetPreferredHeightPx (kNarrow, Scaler96()) > bar.GetPreferredHeightPx (2000, Scaler96()),
                            L"the narrow bar's key takes more rows");
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
