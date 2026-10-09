#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyOptions.h"
#include "Ui/Debugger/GutterGlyph.h"
#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DisassemblyPcDragTests
{
    using Option = DisassemblyOptions::Option;



    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewingHost
    //
    //  A host that keeps the actions the window runs.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ViewingHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                     override {}
        void  RunDebuggerAction        (const DebuggerAction & action)           override { actions.push_back (action); }
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
        void  SetDebuggerDisassemblyOptions (const std::string &)                override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }
        std::string     GetDebuggerDisassemblyOptions()   override { return {}; }

        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<DebuggerAction>  actions;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ViewingWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ViewingWindow : public DebuggerWindow
    {
    public:
        ViewingWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyCodeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::ClickGutter;
        using DebuggerWindow::DropPcMarker;
        using DebuggerWindow::DragPcMarker;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DisassemblyPcDragTests
    //
    //  Dragging the PC's arrow carries the arrow and the PC's row color with
    //  the pointer; the line numbers check box says how to get line numbers;
    //  a branch arrow takes a click from a few pixels off its line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DisassemblyPcDragTests)
    {
    public:
        //  LDA #$41 at $0300 holding the PC, then RTS at $0302.
        static std::shared_ptr<const DebuggerViewSnapshot> MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  rts;



            lda.address      = 0x0300;
            lda.bytes        = "A9 41";
            lda.instruction  = "LDA #$41";
            lda.isCurrent    = true;
            rts.address      = 0x0302;
            rts.bytes        = "60";
            rts.instruction  = "RTS";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            return snapshot;
        }



        //  A point in the gutter of a row, at 96 DPI.
        static POINT GetGutterPoint (const DxuiListView & list, int row)
        {
            return POINT { list.GetBounds().left + 2,
                           list.GetBounds().top + list.GetHeaderHeightPx() + (row - list.GetTopRow()) * list.GetRowHeightDip() + list.GetRowHeightDip() / 2 };
        }



        TEST_METHOD (TheDraggedArrowAndPcRowFollowThePointer)
        {
            CassoTheme      theme = CassoTheme::MakeSkeuomorphic();
            ViewingHost     host;
            DxuiDpiScaler   scaler;
            ViewingWindow   window (theme, host);
            DxuiListView  * list  = nullptr;
            DxuiMouseEvent  down;
            DxuiMouseEvent  up;
            uint32_t        pcRow = 0;



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            list  = window.GetCodeList (0);
            pcRow = list->GetCellsOfRow (0)[1].background;

            down.kind        = DxuiMouseEventKind::Down;
            down.button      = DxuiMouseButton::Left;
            down.positionDip = GetGutterPoint (*list, 0);

            Assert::IsTrue (window.ClickGutter (down));
            window.DragPcMarker (GetGutterPoint (*list, 1));

            Assert::IsTrue    (list->GetCellsOfRow (1)[0].icon != nullptr, L"the arrow shows on the row under the pointer");
            Assert::IsTrue    (list->GetCellsOfRow (0)[0].icon == nullptr, L"and leaves the PC's row");
            Assert::AreEqual  (pcRow, list->GetCellsOfRow (1)[1].background, L"the PC's row color moves with it");
            Assert::AreNotEqual (pcRow, list->GetCellsOfRow (0)[1].background, L"and leaves the PC's row");

            up.kind        = DxuiMouseEventKind::Up;
            up.button      = DxuiMouseButton::Left;
            up.positionDip = POINT { -10, -10 };

            Assert::IsTrue  (window.DropPcMarker (up));
            Assert::IsTrue  (list->GetCellsOfRow (0)[0].icon != nullptr, L"dropped off the view, the arrow returns to the PC");
        }



        TEST_METHOD (TheLineNumbersTipSaysHowToGetThem)
        {
            std::wstring  noFile   = DisassemblyOptions::GetLineNumbersTip (false, false);
            std::wstring  noSource = DisassemblyOptions::GetLineNumbersTip (true, false);



            Assert::IsTrue (noFile.find (L"debug file")         != std::wstring::npos, noFile.c_str());
            Assert::IsTrue (noFile.find (L"Show source code")   != std::wstring::npos, noFile.c_str());
            Assert::IsTrue (noSource.find (L"Show source code") != std::wstring::npos, noSource.c_str());
        }



        TEST_METHOD (ABranchArrowTakesAClickAFewPixelsOffItsLine)
        {
            BranchArrow::Input  input;
            float               upright = 0.0f;



            input.mnemonicX = 100.0f;
            input.sourceY   = 50.0f;
            input.targetY   = 150.0f;
            upright         = input.mnemonicX - input.marginPx - input.stubPx;

            Assert::IsTrue  (BranchArrow::HitTest (input, upright - 7.0f, 100.0f), L"left of the upright");
            Assert::IsTrue  (BranchArrow::HitTest (input, upright + 7.0f, 100.0f), L"right of the upright");
            Assert::IsTrue  (BranchArrow::HitTest (input, upright + 4.0f, 57.0f),  L"below the row it leaves");
            Assert::IsFalse (BranchArrow::HitTest (input, upright + 10.0f, 100.0f), L"not the space between");
        }



        //  The first and last columns of an image's middle row whose ink is
        //  at least half covered.
        static void  GetMiddleRowInk (const DxuiIconImage & image, int & first, int & last)
        {
            first = -1;
            last  = -1;

            for (int x = 0; x < image.width; x++)
            {
                if ((image.bgraPremul[(size_t) ((image.height / 2) * image.width + x)] >> 24) >= 0x80u)
                {
                    first = (first < 0) ? x : first;
                    last  = x;
                }
            }
        }


        //  A pixel of an image, premultiplied, at a column and row.
        static uint32_t  GetPixel (const DxuiIconImage & image, int x, int y)
        {
            return image.bgraPremul[(size_t) (y * image.width + x)];
        }


        //  The smallest rect holding every pixel of an image with any ink.
        static RECT  GetInkBounds (const DxuiIconImage & image)
        {
            RECT  bounds = { image.width, image.height, 0, 0 };



            for (int y = 0; y < image.height; y++)
            {
                for (int x = 0; x < image.width; x++)
                {
                    bool  isInk = (GetPixel (image, x, y) >> 24) != 0;

                    bounds.left   = isInk ? (std::min) (bounds.left,   (LONG) x)     : bounds.left;
                    bounds.top    = isInk ? (std::min) (bounds.top,    (LONG) y)     : bounds.top;
                    bounds.right  = isInk ? (std::max) (bounds.right,  (LONG) x + 1) : bounds.right;
                    bounds.bottom = isInk ? (std::max) (bounds.bottom, (LONG) y + 1) : bounds.bottom;
                }
            }

            return bounds;
        }



        //  A breakpoint sits in the glyph margin as Visual Studio's does: its
        //  dot 0.7 of the 16-DIP icon, 11.2 DIP and so 14 pixels across at
        //  125%, centered 8.4 DIP in from the list's left, 10.5 pixels there,
        //  so its left edge is 2.8 DIP inside the list, in a 17-DIP column
        //  the address follows.
        TEST_METHOD (ABreakpointSitsInTheGlyphMarginAsVisualStudiosDoes)
        {
            constexpr int                          kIconPx  = 48;    // the dot image's own size
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            ViewingHost                            host;
            DxuiDpiScaler                          scaler;
            DxuiDpiScaler                          scaler120;
            ViewingWindow                          window (theme, host);
            DxuiListView                         * list     = nullptr;
            MockDxuiPainter                        painter;
            MockDxuiTextRenderer                   text;
            auto                                   snapshot = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            std::shared_ptr<const DxuiIconImage>   dot;
            RECT                                   bounds   = {};
            RECT                                   address  = {};
            int                                    first    = -1;
            int                                    last     = -1;



            scaler.SetDpi    (96);
            scaler120.SetDpi (120);

            //  The RTS, off the PC's line, so the dot is alone.
            snapshot->codeViews[0][1].hasBreakpoint = true;
            snapshot->codeViews[0][1].isEnabled     = true;
            snapshot->code                          = snapshot->codeViews[0];

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            list   = window.GetCodeList (0);
            dot    = list->GetCellsOfRow (1)[0].icon;
            bounds = list->GetBounds();

            Assert::IsTrue   (dot != nullptr, L"the row has a breakpoint");
            Assert::AreEqual (kIconPx, dot->width);

            GetMiddleRowInk (*dot, first, last);

            Assert::AreEqual (34, last - first + 1, L"the dot is 0.7 of its image across, 33.6 of 48 pixels");
            Assert::AreEqual (7,  first,            L"from 7.2 pixels of 48, 2.4 DIP into its 16-DIP icon");

            list->Layout (bounds, scaler120);
            list->Paint  (painter, text, theme);

            Assert::AreEqual ((size_t) 2, text.IconCalls().size(), L"the dot and the PC's arrow are drawn");

            for (const RecordedTextCall & icon : text.IconCalls())
            {
                Assert::AreEqual ((float) bounds.left + 10.5f, icon.x + icon.width * 0.5f, 0.001f, L"each centered 10.5 pixels in at 125%");
                Assert::AreEqual (20.0f, icon.width, L"in a 20-pixel icon, so the dot is 14 pixels across");
            }

            Assert::IsTrue   (list->GetCellTextRectPx (0, 1, address), L"the address has a cell, the column after the margin");
            Assert::AreEqual ((LONG) (scaler120.ToPx (17) + scaler120.ToPx (4)), address.left,
                              L"the address after the 17-DIP glyph column and its padding");
        }



        //  The PC's arrow sits in the glyph margin with the breakpoints, as
        //  Visual Studio's does: drawn in the same icon at the same place,
        //  its left edge on the dot's, 2.8 DIP inside the list, and as tall as
        //  the dot. No column of its own lies between the margin and the
        //  address.
        TEST_METHOD (ThePcsArrowSitsInTheGlyphMarginWhereTheDotDoes)
        {
            constexpr uint32_t                     kAnyArgb = 0xFFFFFFFF;   // the dot's place, not its color, is compared
            CassoTheme                             theme    = CassoTheme::MakeSkeuomorphic();
            ViewingHost                            host;
            DxuiDpiScaler                          scaler;
            ViewingWindow                          window (theme, host);
            DxuiListView                         * list     = nullptr;
            std::shared_ptr<const DxuiIconImage>   arrow;
            std::shared_ptr<DxuiIconImage>         dot      = GutterGlyph::MakeDot (kAnyArgb, true);
            RECT                                   ink      = {};
            RECT                                   dotInk   = {};



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            list  = window.GetCodeList (0);
            arrow = list->GetCellsOfRow (0)[0].icon;

            Assert::AreEqual ((size_t) 6, list->GetColumnCount(), L"the margin, address, bytes, label, instruction and operand");
            Assert::AreEqual (std::wstring (L"Address"), list->GetColumnAt (1).title, L"the address follows the margin");
            Assert::IsTrue   (arrow != nullptr, L"the PC's line shows its arrow in the margin");
            Assert::IsTrue   (list->GetCellsOfRow (0)[0].text.empty(), L"as an image, not text");
            Assert::IsTrue   (list->GetCellsOfRow (1)[0].icon == nullptr, L"and no other line does");
            Assert::AreEqual (dot->width, arrow->width, L"an image the size of the dot's, drawn in the same icon");

            ink    = GetInkBounds (*arrow);
            dotInk = GetInkBounds (*dot);

            Assert::AreEqual (dotInk.left,   ink.left,   L"its left edge on the dot's");
            Assert::AreEqual (dotInk.top,    ink.top,    L"as tall as the dot");
            Assert::AreEqual (dotInk.bottom, ink.bottom, L"as tall as the dot");
            Assert::IsTrue   (ink.right < dotInk.right,  L"pointing right, its tip inside the dot's right edge");
        }



        //  A breakpoint on the PC's line shows the arrow over the dot, as
        //  Visual Studio does: the arrow's color inside the arrow, and the
        //  dot's color where the dot shows past its tip.
        TEST_METHOD (OnThePcsLineTheArrowIsDrawnOverTheBreakpoint)
        {
            constexpr int                          kInArrowX = 14;   // inside both, left of the tip
            constexpr int                          kPastTipX = 39;   // inside the dot, past the arrow's tip
            CassoTheme                             theme     = CassoTheme::MakeSkeuomorphic();
            ViewingHost                            host;
            DxuiDpiScaler                          scaler;
            ViewingWindow                          window (theme, host);
            DxuiListView                         * list      = nullptr;
            auto                                   both      = std::make_shared<DebuggerViewSnapshot> (*MakeSnapshot());
            std::shared_ptr<const DxuiIconImage>   marked;
            std::shared_ptr<const DxuiIconImage>   dot;
            std::shared_ptr<const DxuiIconImage>   arrow;
            int                                    middle    = GutterGlyph::kSizePx / 2;



            scaler.SetDpi (96);

            //  A breakpoint on the PC's line and on the line after it.
            for (DebuggerViewSnapshot::CodeLine & line : both->codeViews[0])
            {
                line.hasBreakpoint = true;
                line.isEnabled     = true;
            }

            both->code = both->codeViews[0];

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            list  = window.GetCodeList (0);
            arrow = list->GetCellsOfRow (0)[0].icon;

            window.ApplyCodeSnapshot (both, 0);

            marked = list->GetCellsOfRow (0)[0].icon;
            dot    = list->GetCellsOfRow (1)[0].icon;

            Assert::IsTrue (arrow != nullptr && marked != nullptr && dot != nullptr, L"each line shows its glyphs");
            Assert::IsTrue (list->GetCellsOfRow (0)[0].tip.find (ColorLegend::GetText (ColorLegend::Meaning::PcMarker))          != std::wstring::npos, L"the tip says what the arrow means");
            Assert::IsTrue (list->GetCellsOfRow (0)[0].tip.find (ColorLegend::GetText (ColorLegend::Meaning::BreakpointEnabled)) != std::wstring::npos, L"and what the dot means");

            Assert::AreEqual    (GetPixel (*arrow, kInArrowX, middle), GetPixel (*marked, kInArrowX, middle), L"inside the arrow, the arrow's color");
            Assert::AreNotEqual (GetPixel (*dot,   kInArrowX, middle), GetPixel (*marked, kInArrowX, middle), L"over the dot");
            Assert::AreEqual    (GetPixel (*dot,   kPastTipX, middle), GetPixel (*marked, kPastTipX, middle), L"past its tip, the dot's color");
            Assert::AreEqual    (0u, GetPixel (*arrow, kPastTipX, middle), L"where the arrow alone leaves nothing");
        }
    };
}