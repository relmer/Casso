#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "ControllerRig.h"
#include "Debugger/TraceLookahead.h"
#include "TestHelpers.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/Panes/TracePane.h"
#include "Widgets/DxuiListView.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceKeysHost
    //
    //  Records each line the window runs.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TraceKeysHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string & line)                 override { lines.push_back (line); }
        void  RunDebuggerAction       (const DebuggerAction & action)            override { lines.push_back (action.echo); }
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerMemoryWindow (int, std::optional<Word>)                 override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  RunDebuggerCommandInMode (const std::string & line, CommandMode) override { lines.push_back (line); }

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        FakeHostDialogs           dialogs;
        std::vector<std::string>  lines;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceKeysWindow
    //
    ////////////////////////////////////////////////////////////////////////////////

    class TraceKeysWindow : public DebuggerWindow
    {
    public:
        TraceKeysWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::OnKey;
        using DebuggerWindow::RouteMappedKey;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::TakeSnapshot;

        bool  Press (WPARAM vk)
        {
            DxuiKeyEvent  ev = { DxuiKeyEventKind::Down, vk, false, false, false, false };

            return OnKey (ev) || RouteMappedKey (ev);
        }

        std::vector<DxuiLabel *>  GetTraceLabels()
        {
            std::vector<DxuiLabel *>  labels;



            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (GetPaneOfControl (GetChild (i)) == DebuggerLayout::kTrace)
                {
                    labels.push_back (dynamic_cast<DxuiLabel *> (GetChild (i)));
                }
            }

            return labels;
        }

        DxuiListView *  GetTrace()
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (GetPaneOfControl (GetChild (i)) == DebuggerLayout::kTrace)
                {
                    if (DxuiListView * list = dynamic_cast<DxuiListView *> (GetChild (i)))
                    {
                        return list;
                    }
                }
            }

            return nullptr;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TracePaneKeysTests
    //
    //  The trace pane's key line and its keys, the instructions to run next
    //  below its rows, and its save.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TracePaneKeysTests)
    {
    public:

        static void  Open (TraceKeysWindow & window, DxuiListView *& trace)
        {
            DxuiDpiScaler  scaler;
            auto           snapshot = std::make_shared<DebuggerViewSnapshot>();



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            snapshot->isPaused    = true;
            snapshot->trace.isOn  = true;
            snapshot->trace.total = 10;
            snapshot->trace.entries.resize (10);
            window.TakeSnapshot (snapshot);
            window.Layout (RECT { 0, 0, 1100, 840 }, scaler);

            trace = window.GetTrace();
            Assert::IsNotNull (trace);
            window.FocusControl (trace);
        }



        TEST_METHOD (GetKeyAction_MapsTheSingleKeys)
        {
            using Key = TracePane::KeyAction;



            Assert::IsTrue (TracePane::GetKeyAction (VK_SPACE,  false, false, false) == Key::StepInto);
            Assert::IsTrue (TracePane::GetKeyAction ('O',       false, false, false) == Key::StepOver);
            Assert::IsTrue (TracePane::GetKeyAction ('R',       false, false, false) == Key::StepOut);
            Assert::IsTrue (TracePane::GetKeyAction (VK_RETURN, false, false, false) == Key::Run);
            Assert::IsTrue (TracePane::GetKeyAction ('T',       false, false, false) == Key::ToggleTrace);
            Assert::IsTrue (TracePane::GetKeyAction ('B',       false, false, false) == Key::ToggleBytes);
            Assert::IsTrue (TracePane::GetKeyAction ('S',       false, false, false) == Key::Save);
            Assert::IsTrue (TracePane::GetKeyAction ('O',       true,  false, false) == Key::None, L"a chord is left to the window");
            Assert::IsTrue (TracePane::GetKeyAction ('X',       false, false, false) == Key::None);
        }



        TEST_METHOD (KeyHint_IsAPartOfThePane)
        {
            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            TraceKeysHost    host;
            TraceKeysWindow  window   (theme, host);
            DxuiListView   * trace    = nullptr;
            bool             hasHint  = false;



            Open (window, trace);

            for (DxuiLabel * label : window.GetTraceLabels())
            {
                if (label != nullptr)
                {
                    hasHint = label->GetText() == TracePane::GetKeyHint();
                }
            }

            Assert::IsTrue (hasHint, L"the trace pane holds its key line");
        }



        TEST_METHOD (Keys_InTheTracePane_StepRunAndSave)
        {
            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            TraceKeysHost    host;
            TraceKeysWindow  window   (theme, host);
            DxuiListView   * trace    = nullptr;



            Open (window, trace);
            host.dialogs.path = L"C:\\out\\trace.txt";

            Assert::IsTrue (window.Press (VK_SPACE));
            Assert::IsTrue (window.Press ('O'));
            Assert::IsTrue (window.Press ('R'));
            Assert::IsTrue (window.Press (VK_RETURN));
            Assert::IsTrue (window.Press ('T'));
            Assert::IsTrue (window.Press ('S'));

            Assert::AreEqual ((size_t) 6, host.lines.size());
            Assert::AreEqual (std::string ("T"),                                  host.lines[0]);
            Assert::AreEqual (std::string ("P"),                                  host.lines[1]);
            Assert::AreEqual (std::string ("RTS"),                                host.lines[2]);
            Assert::AreEqual (std::string ("G"),                                  host.lines[3]);
            Assert::AreEqual (std::string ("HISTORY OFF"),                        host.lines[4]);
            Assert::AreEqual (std::string ("HISTORY SAVE \"C:\\out\\trace.txt\""), host.lines[5]);
            Assert::AreEqual (1, host.dialogs.saves);
        }



        TEST_METHOD (SaveKey_BackingOut_SavesNothing)
        {
            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            TraceKeysHost    host;
            TraceKeysWindow  window   (theme, host);
            DxuiListView   * trace    = nullptr;



            Open (window, trace);
            host.dialogs.picks = false;

            window.Press ('S');

            Assert::AreEqual (1, host.dialogs.saves);
            Assert::IsTrue   (host.lines.empty());
        }



        TEST_METHOD (BKey_HidesAndShowsTheBytesColumn)
        {
            static constexpr size_t  kBytes = 3;

            CassoTheme       theme    = CassoTheme::MakeSkeuomorphic();
            TraceKeysHost    host;
            TraceKeysWindow  window   (theme, host);
            DxuiListView   * trace    = nullptr;



            Open (window, trace);
            Assert::IsTrue  (trace->IsColumnVisible (kBytes));

            window.Press ('B');
            Assert::IsFalse (trace->IsColumnVisible (kBytes), L"B hides the bytes");

            window.Press ('B');
            Assert::IsTrue  (trace->IsColumnVisible (kBytes), L"and shows them again");
        }



        TEST_METHOD (NextRows_FollowTheLastEntry)
        {
            DxuiListView                          list;
            TracePane                             pane (&list, [] (std::optional<uint64_t>) {});
            DebuggerViewSnapshot::TraceState      trace;
            std::vector<DxuiListView::Cell>       cells;



            pane.Configure();

            trace.total = 2;
            trace.entries.resize (2);
            trace.next.resize (2);
            trace.next[0].pc          = 0x0300;
            trace.next[0].instruction = "LDA #$05";
            trace.next[1].pc          = 0x0302;
            pane.Apply (trace);

            Assert::AreEqual (4, list.GetRowCount(), L"two entries and two to run next");

            pane.ProvideRow (2, cells);
            Assert::AreEqual (std::wstring (L"next"),     cells[0].text);
            Assert::AreEqual (std::wstring (L"0300"),     cells[2].text);
            Assert::AreEqual (std::wstring (L"LDA #$05"), cells[5].text);
            Assert::IsTrue   (cells[6].text.empty(), L"no registers before it runs");

            pane.ProvideRow (3, cells);
            Assert::AreEqual (std::wstring (L"0302"), cells[2].text);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  TraceLookaheadTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (TraceLookaheadTests)
    {
    public:

        static constexpr Byte  kZero  = 0x02;
        static constexpr Byte  kCarry = 0x01;

        static std::vector<Word>  Walk (std::initializer_list<std::pair<Word, std::vector<Byte>>> code, Word pc, Byte p, size_t count = 8)
        {
            std::array<Byte, 0x10000>  memory = {};
            TestCpu                    cpu;
            std::vector<Word>          addresses;



            cpu.InitForTest();

            for (const auto & [at, bytes] : code)
            {
                std::copy (bytes.begin(), bytes.end(), memory.begin() + at);
            }

            for (const DisassembledInstruction & instruction :
                 TraceLookahead::FindNext (cpu.GetInstructionSet(), pc, p,
                                           [&memory] (Word address, Byte & value) { value = memory[address]; return true; }, count))
            {
                addresses.push_back (instruction.address);
            }

            return addresses;
        }



        TEST_METHOD (Snapshot_WhileStopped_HoldsTheNextInstructions)
        {
            ControllerRig         rig;
            DebuggerViewSnapshot  snapshot = rig.view.Build (rig.controller.GetSession());



            //  LDA #$41 / STA $0400 / RTS at $0300, the PC on the first.
            Assert::AreEqual ((size_t) 3,                snapshot.trace.next.size(), L"up to and including the RTS");
            Assert::AreEqual ((Word) 0x0300,             snapshot.trace.next[0].pc);
            Assert::AreEqual (std::string ("LDA #$41"),  snapshot.trace.next[0].instruction);
            Assert::AreEqual ((Word) 0x0302,             snapshot.trace.next[1].pc);
            Assert::AreEqual ((Word) 0x0305,             snapshot.trace.next[2].pc);
        }



        TEST_METHOD (Straight_StopsAtTheCount)
        {
            //  NOP x 10
            std::vector<Word>  walk = Walk ({ { 0x0300, std::vector<Byte> (10, 0xEA) } }, 0x0300, 0, 4);



            Assert::AreEqual ((size_t) 4, walk.size());
            Assert::AreEqual ((Word) 0x0303, walk[3]);
        }



        TEST_METHOD (Branch_FlagsKnown_IsFollowed)
        {
            //  $0300: BEQ $0310    $0302: NOP    $0310: RTS
            std::vector<Word>  taken    = Walk ({ { 0x0300, { 0xF0, 0x0E, 0xEA } }, { 0x0310, { 0x60 } } }, 0x0300, kZero);
            std::vector<Word>  notTaken = Walk ({ { 0x0300, { 0xF0, 0x0E, 0x60 } } },                       0x0300, 0);



            Assert::AreEqual ((size_t) 2, taken.size());
            Assert::AreEqual ((Word) 0x0310, taken[1], L"Z set: the branch is taken");

            Assert::AreEqual ((size_t) 2, notTaken.size());
            Assert::AreEqual ((Word) 0x0302, notTaken[1], L"Z clear: it falls through");
        }



        TEST_METHOD (Branch_AfterAFlagChange_EndsTheWalk)
        {
            //  $0300: LDA #$00    $0302: BEQ $0310    $0310: NOP
            std::vector<Word>  walk = Walk ({ { 0x0300, { 0xA9, 0x00, 0xF0, 0x0C } }, { 0x0310, { 0xEA } } }, 0x0300, 0);



            Assert::AreEqual ((size_t) 2, walk.size(), L"the branch is shown, and the walk ends at it");
            Assert::AreEqual ((Word) 0x0302, walk[1]);
        }



        TEST_METHOD (SecThenBcs_IsFollowed)
        {
            //  $0300: SEC    $0301: BCS $0310    $0310: RTS
            std::vector<Word>  walk = Walk ({ { 0x0300, { 0x38, 0xB0, 0x0D } }, { 0x0310, { 0x60 } } }, 0x0300, 0);



            Assert::AreEqual ((size_t) 3, walk.size());
            Assert::AreEqual ((Word) 0x0310, walk[2], L"SEC sets the carry the branch then reads");
            (void) kCarry;
        }



        TEST_METHOD (JsrAndJmp_AreFollowed)
        {
            //  $0300: JSR $0400    $0400: JMP $0500    $0500: RTS
            std::vector<Word>  walk = Walk ({ { 0x0300, { 0x20, 0x00, 0x04 } }, { 0x0400, { 0x4C, 0x00, 0x05 } }, { 0x0500, { 0x60 } } }, 0x0300, 0);



            Assert::AreEqual ((size_t) 3, walk.size());
            Assert::AreEqual ((Word) 0x0400, walk[1]);
            Assert::AreEqual ((Word) 0x0500, walk[2]);
        }
    };
}
