#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Debugger/SymbolTable.h"
#include "Theme/DxuiColor.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerTextColors.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyOptions.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DisassemblyViewingTests
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
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DisassemblyViewingTests
    //
    //  The disassembly's viewing options as they start, their tips, dragging
    //  the PC's arrow, and its text colors on a light page.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DisassemblyViewingTests)
    {
    public:
        //  LDA #$41 at $0300 holding the PC, RTS at $0302 and a data byte at
        //  $0303.
        static std::shared_ptr<const DebuggerViewSnapshot> MakeSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  lda;
            DebuggerViewSnapshot::CodeLine  rts;
            DebuggerViewSnapshot::CodeLine  data;



            lda.address      = 0x0300;
            lda.bytes        = "A9 41";
            lda.instruction  = "LDA #$41";
            lda.isCurrent    = true;
            rts.address      = 0x0302;
            rts.bytes        = "60";
            rts.instruction  = "RTS";
            data.address     = 0x0303;
            data.bytes       = "FF";
            data.instruction = "DB $FF";

            snapshot->pc           = 0x0300;
            snapshot->isPaused     = true;
            snapshot->codeViews[0] = { lda, rts, data };
            snapshot->code         = snapshot->codeViews[0];
            snapshot->codeOpen[0]  = true;

            return snapshot;
        }



        //  A left button event at the gutter of a row, at 96 DPI.
        static DxuiMouseEvent MakeGutterEvent (const DxuiListView & list, int row, DxuiMouseEventKind kind)
        {
            DxuiMouseEvent  ev;



            ev.kind        = kind;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { list.GetBounds().left + 2,
                                     list.GetBounds().top + list.GetHeaderHeightPx() + (row - list.GetTopRow()) * list.GetRowHeightDip() + list.GetRowHeightDip() / 2 };
            return ev;
        }



        TEST_METHOD (EveryViewingOptionStartsChecked)
        {
            DisassemblyOptions  options;



            for (int i = 0; i < DisassemblyOptions::kOptionCount; i++)
            {
                Assert::IsTrue (options.IsOn ((Option) i), DisassemblyOptions::GetLabel ((Option) i));
            }
        }



        TEST_METHOD (TheSourceTipSaysHowToGetSource)
        {
            std::wstring  tip = DisassemblyOptions::GetSourceTip (false);



            Assert::IsTrue (tip.find (L"debug file")  != std::wstring::npos, tip.c_str());
            Assert::IsTrue (tip.find (L"source file") != std::wstring::npos, tip.c_str());
        }



        TEST_METHOD (TheSymbolsTipListsEachSourceOnALine)
        {
            SymbolTable               symbols;
            std::vector<std::string>  sources;
            std::wstring              tip;



            symbols.Add       (SymbolTableId::Main, "COUT", 0xFDED);
            symbols.Add       (SymbolTableId::User, "START", 0x0300);
            symbols.AddOrigin (SymbolTableId::User, "game.sym");

            sources = DebuggerViewState::DescribeSymbolSources (symbols);
            tip     = DisassemblyOptions::GetSymbolsTip (sources);

            Assert::AreEqual ((size_t) 2, sources.size());
            Assert::AreEqual (std::string ("Built-in MAIN table, 1 symbols"), sources[0]);
            Assert::AreEqual (std::string ("game.sym, in the USER table"),     sources[1]);
            Assert::AreEqual (std::wstring (L"Show symbol names\nSymbols loaded from:\nBuilt-in MAIN table, 1 symbols\ngame.sym, in the USER table"), tip);

            symbols.Clear (SymbolTableId::User);
            Assert::IsTrue (symbols.GetOrigins (SymbolTableId::User).empty(), L"clearing a table forgets its file");
        }



        TEST_METHOD (DraggingThePcArrowToAnotherLineSetsThePc)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            ViewingHost    host;
            DxuiDpiScaler  scaler;
            ViewingWindow  window (theme, host);
            DxuiListView * list  = nullptr;



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ApplyCodeSnapshot (MakeSnapshot(), 0);

            list = window.GetCodeList (0);

            Assert::IsTrue (window.ClickGutter  (MakeGutterEvent (*list, 0, DxuiMouseEventKind::Down)));
            Assert::IsTrue (host.actions.empty(), L"a press on the PC's arrow waits for the release");
            Assert::IsTrue (window.DropPcMarker (MakeGutterEvent (*list, 1, DxuiMouseEventKind::Up)));

            Assert::AreEqual ((size_t) 1, host.actions.size());
            Assert::IsTrue   (host.actions[0].command.verb == DebugVerb::SetRegister);
            Assert::IsTrue   (host.actions[0].echo.find ("PC 0302") != std::string::npos, L"the PC moves to the RTS");

            Assert::IsTrue  (window.ClickGutter  (MakeGutterEvent (*list, 0, DxuiMouseEventKind::Down)));
            Assert::IsTrue  (window.DropPcMarker (MakeGutterEvent (*list, 2, DxuiMouseEventKind::Up)));
            Assert::AreEqual ((size_t) 1, host.actions.size(), L"a data byte cannot be the next statement");

            Assert::IsTrue   (window.ClickGutter  (MakeGutterEvent (*list, 0, DxuiMouseEventKind::Down)));
            Assert::IsTrue   (window.DropPcMarker (MakeGutterEvent (*list, 0, DxuiMouseEventKind::Up)));
            Assert::AreEqual ((size_t) 2, host.actions.size(), L"released where pressed, it is a click");
            Assert::IsFalse  (host.actions[1].command.verb == DebugVerb::SetRegister, L"a click sets a breakpoint");
        }



        TEST_METHOD (LightTextReadsOnASelectedRowAndIsNotStark)
        {
            constexpr uint32_t       kWhite     = 0xFFFFFFFF;
            constexpr uint32_t       kSelection = 0xFFCCE4F7;
            DebuggerTextColors::Set  set        = DebuggerTextColors::Make (kWhite, 0xFF000000, 0xFF5D5D5D, 0xFF00727D);
            const uint32_t           colors[]   = { set.syntax.mnemonic, set.syntax.directive, set.syntax.symbol, set.syntax.number,
                                                    set.syntax.string, set.syntax.address, set.syntax.bytes, set.annotation,
                                                    set.changed, set.result };



            for (uint32_t argb : colors)
            {
                Assert::IsTrue (DxuiColor::ComputeContrastRatio (argb, kSelection) >= DebuggerTextColors::s_kMinTextContrast,
                                std::format (L"{:08X} reads on the selection", argb).c_str());
            }

            Assert::IsTrue (DxuiColor::ComputeContrastRatio (set.syntax.address, kWhite) < 15.0f, L"plain text is not stark black");
        }
    };
}
