#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Debugger/SymbolTable.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/BranchArrow.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyOptions.h"
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

            Assert::IsFalse   (list->GetCellsOfRow (1)[1].text.empty(), L"the arrow shows on the row under the pointer");
            Assert::IsTrue    (list->GetCellsOfRow (0)[1].text.empty(),  L"and leaves the PC's row");
            Assert::AreEqual  (pcRow, list->GetCellsOfRow (1)[1].background, L"the PC's row color moves with it");
            Assert::AreNotEqual (pcRow, list->GetCellsOfRow (0)[1].background, L"and leaves the PC's row");

            up.kind        = DxuiMouseEventKind::Up;
            up.button      = DxuiMouseButton::Left;
            up.positionDip = POINT { -10, -10 };

            Assert::IsTrue  (window.DropPcMarker (up));
            Assert::IsFalse (list->GetCellsOfRow (0)[1].text.empty(), L"dropped off the view, the arrow returns to the PC");
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



        //  A breakpoint sits in the glyph margin as Visual Studio's does: its
        //  dot 0.7 of the 16-DIP icon, 11.2 DIP and so 14 pixels across at
        //  125%, centered 8.4 DIP in from the list's left, 10.5 pixels there,
        //  in a 17-DIP column.
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
            int                                    across   = 0;



            scaler.SetDpi    (96);
            scaler120.SetDpi (120);

            snapshot->codeViews[0][0].hasBreakpoint = true;
            snapshot->codeViews[0][0].isEnabled     = true;
            snapshot->code                          = snapshot->codeViews[0];

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (snapshot, 0);

            list   = window.GetCodeList (0);
            dot    = list->GetCellsOfRow (0)[0].icon;
            bounds = list->GetBounds();

            Assert::IsTrue   (dot != nullptr, L"the row has a breakpoint");
            Assert::AreEqual (kIconPx, dot->width);

            for (int x = 0; x < dot->width; x++)
            {
                across += ((dot->bgraPremul[(size_t) ((dot->height / 2) * dot->width + x)] >> 24) >= 0x80u) ? 1 : 0;
            }

            Assert::AreEqual (34, across, L"the dot is 0.7 of its image across, 33.6 of 48 pixels");

            list->Layout (bounds, scaler120);
            list->Paint  (painter, text, theme);

            Assert::IsFalse  (text.IconCalls().empty(), L"the dot is drawn");
            Assert::AreEqual ((float) bounds.left + 10.5f, text.IconCalls()[0].x + text.IconCalls()[0].width * 0.5f, 0.001f, L"centered 10.5 pixels in at 125%");
            Assert::AreEqual (20.0f, text.IconCalls()[0].width, L"in a 20-pixel icon, so the dot is 14 pixels across");
            Assert::IsTrue   (list->GetCellTextRectPx (0, 2, address), L"the address has a cell");
            Assert::AreEqual ((LONG) (scaler120.ToPx (17) + scaler120.ToPx (20) + scaler120.ToPx (4)), address.left,
                              L"the address after the 17-DIP glyph column, the 20-DIP marker column and its padding");
        }
    };
}