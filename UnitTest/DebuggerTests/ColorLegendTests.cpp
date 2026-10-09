#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/ColorLegend.h"
#include "Ui/Debugger/ColorKeyPopup.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/CallStackPane.h"
#include "Ui/Debugger/Panes/DiskHeadView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace ColorLegendTests
{
    using Meaning = ColorLegend::Meaning;



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LegendHost
    //
    //  A host that does nothing; the tests read only the window's own state.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LegendHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                     override {}
        void  RunDebuggerAction        (const DebuggerAction &)                  override {}
        void  PauseDebugger            ()                                        override {}
        void  SetDebuggerCodeLines     (int, int)                                override {}
        void  SetDebuggerCodeAddress   (std::optional<Word>, int)                override {}
        void  SetDebuggerCodeTop       (Word, int)                               override {}
        void  SetDebuggerFollowView    (int)                                     override {}
        void  CloseDebuggerCodeView    (int)                                     override {}
        void  SetDebuggerTraceTop      (std::optional<uint64_t>)                 override {}
        void  GoToDebuggerMemory       (int, const std::string &)                override {}
        void  ScrollDebuggerCode       (int, int)                                override {}
        void  OnDebuggerWindowClosed   ()                                        override {}
        void  SetDebuggerKeyScheme     (const std::string &)                     override {}
        void  SetDebuggerLayout        (const std::string &)                     override {}
        void  SetDebuggerOpenViews     (const std::string &)                     override {}
        void  SetDebuggerPlacement     (const RECT &)                            override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  LegendWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class LegendWindow : public DebuggerWindow
    {
    public:
        LegendWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::TryGetCellTip;
        using DebuggerWindow::GetColorPalette;
        using DebuggerWindow::GetMenuBarItems;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColorLegendTests
    //
    //  Every color that means something says so: each meaning has its sentence
    //  and a line in the legend, in a color the theme gives; the tip over the
    //  PC's branch says why it is or is not taken; and the cells, the call
    //  stack's dimmed rows and the disk head carry their tips.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ColorLegendTests)
    {
    public:

        static ColorLegend::Palette  MakeDistinctPalette()
        {
            ColorLegend::Palette  palette;



            palette.text       = DebuggerTextColors::MakeFor (CassoTheme::MakeSkeuomorphic());
            palette.background = 0xFF000001;
            palette.pcMarker   = 0xFF000002;
            palette.breakpoint = 0xFF000003;
            palette.muted      = 0xFF000004;
            palette.disabled   = 0xFF000005;
            palette.accent     = 0xFF000006;
            palette.flash      = 0xFF000007;
            palette.meterEmpty = 0xFF000008;
            palette.meterFull  = 0xFF000009;

            return palette;
        }


        //  LDA $10 at $0300 holding the PC and reading $10, with a result,
        //  then a disabled breakpoint on RTS at $0302.
        static std::shared_ptr<const DebuggerViewSnapshot>  MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  rts;



            lda.address       = 0x0300;
            lda.bytes         = "A5 10";
            lda.instruction   = "LDA $10";
            lda.annotation    = "$0010=41";
            lda.effect        = "A=41";
            lda.isCurrent     = true;
            rts.address       = 0x0302;
            rts.bytes         = "60";
            rts.instruction   = "RTS";
            rts.hasBreakpoint = true;
            rts.isEnabled     = false;

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            return snapshot;
        }


        static std::wstring  GetTipOverCell (const LegendWindow & window, const DxuiListView & list, int row, size_t column)
        {
            RECT          cell   = {};
            RECT          anchor = {};
            RECT          bounds = list.GetBounds();
            std::wstring  text;

            //  The gutter's dot is an icon, with no text to aim at.
            if (column == 0 || !list.GetCellTextRectPx (row, column, cell))
            {
                cell = RECT { 2, list.GetHeaderHeightPx() + row * list.GetRowHeightDip(), 4, list.GetHeaderHeightPx() + (row + 1) * list.GetRowHeightDip() };
            }

            (void) window.TryGetCellTip (POINT { bounds.left + (cell.left + cell.right) / 2, bounds.top + (cell.top + cell.bottom) / 2 }, anchor, text);
            return text;
        }


        TEST_METHOD (EveryMeaningHasASentenceAndALineInTheLegend)
        {
            for (int each = 0; each < (int) Meaning::Count; each++)
            {
                Meaning       meaning = (Meaning) each;
                std::wstring  text    = ColorLegend::GetText (meaning);
                bool          listed  = std::ranges::any_of (ColorLegend::GetEntries(), [meaning] (const ColorLegend::Entry & entry) { return entry.meaning == meaning; });

                Assert::IsFalse (text.empty(), std::format (L"meaning {} says nothing", each).c_str());
                Assert::IsTrue  (iswupper (text[0]) != 0, text.c_str());
                Assert::IsTrue  (listed, std::format (L"{} is not in the legend", text).c_str());
            }
        }


        TEST_METHOD (EachPanesLinesSitTogether)
        {
            std::vector<ColorLegend::Pane>  seen;



            for (const ColorLegend::Entry & entry : ColorLegend::GetEntries())
            {
                if (seen.empty() || seen.back() != entry.pane)
                {
                    Assert::IsTrue (std::ranges::find (seen, entry.pane) == seen.end(), std::format (L"{} is split", ColorLegend::GetPaneTitle (entry.pane)).c_str());
                    seen.push_back (entry.pane);
                }
            }

            Assert::AreEqual ((size_t) ColorLegend::Pane::Count, seen.size(), L"every pane has a key");
        }


        TEST_METHOD (EveryLineHasAColorInEveryTheme)
        {
            ColorLegend::Palette  palette = MakeDistinctPalette();



            for (const ColorLegend::Entry & entry : ColorLegend::GetEntries())
            {
                Assert::AreNotEqual (0u, ColorLegend::GetArgb (entry.meaning, palette), ColorLegend::GetText (entry.meaning));
            }

            Assert::AreEqual (palette.pcMarker,    ColorLegend::GetArgb (Meaning::BranchTaken,    palette));
            Assert::AreEqual (palette.disabled,    ColorLegend::GetArgb (Meaning::BranchNotTaken, palette));
            Assert::AreEqual (palette.text.pcRow,  ColorLegend::GetArgb (Meaning::PcRow,          palette));
            Assert::AreEqual (palette.text.rom,    ColorLegend::GetArgb (Meaning::RomByte,        palette));
        }


        TEST_METHOD (TheThemesRomIoAndMapColorsReachThePanes)
        {
            CassoTheme               theme = CassoTheme::MakeSkeuomorphic();
            DebuggerTextColors::Set  set;



            theme.romText    = 0xFF1E90FF;
            theme.ioText     = 0xFFC0C0C0;
            theme.mapAuxRam  = 0xFF123456;
            theme.mapRom     = 0xFF654321;

            set = DebuggerTextColors::MakeFor (theme);

            Assert::AreEqual (DebuggerTextColors::GetReadable (theme.romText, theme.ContentBackground()), set.rom, L"the theme's ROM color was dropped");
            Assert::AreEqual (DebuggerTextColors::GetReadable (theme.ioText,  theme.ContentBackground()), set.io,  L"the theme's I/O color was dropped");
            Assert::AreEqual (theme.mapAuxRam, set.mapAux);
            Assert::AreEqual (theme.mapRom,    set.mapRom);
            Assert::AreEqual (MemoryMapBar::GetSourceColor (MemorySource::Io), set.mapIo, L"a map color the theme leaves out keeps its mid-tone");
        }


        //  A pane's key lists that pane's colors alone, each swatch drawn as
        //  the pane draws the color, in the palette's colors.
        TEST_METHOD (APanesKeyShowsItsColorsAloneAsTheyAreDrawn)
        {
            ColorLegend::Palette             palette = MakeDistinctPalette();
            std::vector<ColorKeyPopup::Row>  rows    = ColorKeyPopup::MakeRows (ColorLegend::Pane::Disassembly, palette);
            std::vector<ColorKeyPopup::Row>  memory  = ColorKeyPopup::MakeRows (ColorLegend::Pane::Memory, palette);



            Assert::AreEqual (ColorLegend::GetEntriesFor (ColorLegend::Pane::Disassembly).size(), rows.size());
            Assert::IsFalse  (std::ranges::any_of (rows, [] (const ColorKeyPopup::Row & row) { return row.text == ColorLegend::GetText (Meaning::RomByte); }),
                              L"the memory window's ROM in the disassembly's key");
            Assert::IsTrue   (std::ranges::any_of (memory, [] (const ColorKeyPopup::Row & row) { return row.text == ColorLegend::GetText (Meaning::RomByte); }));

            for (const ColorKeyPopup::Row & row : rows)
            {
                if (row.text == ColorLegend::GetText (Meaning::PcRow))
                {
                    Assert::IsTrue   (row.swatch == ColorLegend::Swatch::Row);
                    Assert::AreEqual (palette.text.pcRow, row.argb, L"the PC's row is not shown on its fill");
                }
                else if (row.text == ColorLegend::GetText (Meaning::Annotation))
                {
                    Assert::AreEqual (palette.text.annotation, row.argb, L"a text color is not shown in its color");
                }
                else if (row.text == ColorLegend::GetText (Meaning::BreakpointDisabled))
                {
                    Assert::IsNotNull (row.icon.get(), L"a ring is not drawn");
                }
            }
        }


        TEST_METHOD (SwatchShapesCoverWhatTheySay)
        {
            constexpr size_t  kCenter = (size_t) ((ColorLegend::kSwatchPx / 2) * ColorLegend::kSwatchPx + ColorLegend::kSwatchPx / 2);
            constexpr size_t  kCorner = 0;
            auto              alpha   = [] (const std::shared_ptr<DxuiIconImage> & image, size_t at) { return image->bgraPremul[at] >> 24; };



            Assert::AreEqual (255u, alpha (ColorLegend::MakeSwatchIcon (ColorLegend::Swatch::Dot,     0xFFFF0000), kCenter));
            Assert::AreEqual (0u,   alpha (ColorLegend::MakeSwatchIcon (ColorLegend::Swatch::Dot,     0xFFFF0000), kCorner));
            Assert::AreEqual (0u,   alpha (ColorLegend::MakeSwatchIcon (ColorLegend::Swatch::Ring,    0xFFFF0000), kCenter), L"a ring is filled");
            Assert::AreEqual (0u,   alpha (ColorLegend::MakeSwatchIcon (ColorLegend::Swatch::Outline, 0xFFFF0000), kCenter), L"an outline is filled");
            Assert::AreEqual (255u, alpha (ColorLegend::MakeSwatchIcon (ColorLegend::Swatch::Fill,    0xFFFF0000), kCenter));
        }

        TEST_METHOD (TheBranchTipSaysWhichFlagAndHowItStands)
        {
            Assert::AreEqual (std::wstring (L"Branch taken: BNE branches while Z is clear, and Z is clear"),
                              ColorLegend::GetBranchTip ("BNE $0310", true, (Byte) 0x00));
            Assert::AreEqual (std::wstring (L"Branch not taken: BEQ branches while Z is set, and Z is clear"),
                              ColorLegend::GetBranchTip ("beq $0310", false, (Byte) 0x00));
            Assert::AreEqual (std::wstring (L"Branch not taken: BCC branches while C is clear, and C is set"),
                              ColorLegend::GetBranchTip ("BCC LOOP", false, (Byte) 0x01));
            Assert::AreEqual (std::wstring (L"Branch taken: BBS3 branches while bit 3 of its byte is set"),
                              ColorLegend::GetBranchTip ("BBS3 $10,$0310", true, (Byte) 0x00));
            Assert::AreEqual (std::wstring (L"Call: JSR always calls its target"),
                              ColorLegend::GetBranchTip ("JSR $FDED", true, std::nullopt));
        }


        TEST_METHOD (ADimmedCallStackRowSaysWhy)
        {
            CallStackPane::Row  unverified;
            CallStackPane::Row  returned;
            CallStackPane::Row  live;
            CallStackBreak      txs;



            unverified.isDim  = true;
            unverified.tip    = CallStackPane::GetUnverifiedTip (std::nullopt);
            returned.isDim    = true;
            returned.isReturn = true;
            txs.kind          = CallBreakKind::Txs;
            txs.pc            = 0xD68C;

            Assert::AreEqual (unverified.tip, CallStackPane::GetCells (unverified, {})[0].tip);
            Assert::IsTrue   (unverified.tip.starts_with (L"Unverified: this call may already be over."), unverified.tip.c_str());
            Assert::IsTrue   (CallStackPane::GetUnverifiedTip (txs).find (L"$D68C above set the stack pointer directly") != std::wstring::npos, L"the break above, and what it did");
            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::LastReturn)),      CallStackPane::GetCells (returned,   {})[1].tip);
            Assert::IsTrue   (CallStackPane::GetCells (live, {})[0].tip.empty(), L"a live frame has a tip");
        }


        TEST_METHOD (TheDiskHeadsTipFollowsItsColor)
        {
            DiskHeadView         view;
            DxuiDpiScaler        scaler;
            DiagnosticsDiskHead  head;
            POINT                ruler = { 50, 4 };
            POINT                lamps = { 50, 30 };



            scaler.SetDpi (96);
            view.Layout (RECT { 0, 0, 300, 60 }, scaler);

            head.maxQuarterTrack = 139;
            view.SetHead (head);

            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::HeadMotorOff)), view.GetColorTipAt (ruler));
            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::LampLit)),      view.GetColorTipAt (lamps));

            head.motorOn = true;
            view.SetHead (head);

            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::HeadSettled)), view.GetColorTipAt (ruler));

            head.quarterTrack = 68;
            view.SetHead (head);

            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::HeadMoving)), view.GetColorTipAt (ruler));
            Assert::IsTrue   (view.GetColorTipAt (POINT { 400, 4 }).empty(), L"a tip off the view");
        }


        TEST_METHOD (TheDisassemblysCellsSayWhatTheirColorsMean)
        {
            CassoTheme              theme = CassoTheme::MakeSkeuomorphic();
            LegendHost              host;
            LegendWindow            window (theme, host);
            DxuiDpiScaler           scaler;
            DxuiListView          * list  = nullptr;
            std::wstring            operand;
            MockDxuiPainter         painter;
            MockDxuiTextRenderer    text;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            list    = window.GetCodeList (0);

            //  A paint fits the columns to their text, which places the cells.
            list->Paint (painter, text, theme);

            operand = GetTipOverCell (window, *list, 0, 5);

            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::PcRow)),              GetTipOverCell (window, *list, 0, 1));
            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::PcMarker)),           GetTipOverCell (window, *list, 0, 0), L"the arrow is in the glyph margin");
            Assert::AreEqual (std::wstring (ColorLegend::GetText (Meaning::BreakpointDisabled)), GetTipOverCell (window, *list, 1, 0));
            Assert::IsTrue   (operand.find (ColorLegend::GetText (Meaning::Annotation)) != std::wstring::npos, operand.c_str());
            Assert::IsTrue   (operand.find (ColorLegend::GetText (Meaning::Result))     != std::wstring::npos, operand.c_str());
            Assert::IsTrue   (GetTipOverCell (window, *list, 1, 1).empty(), L"a plain row has a tip");
        }
    };
}
