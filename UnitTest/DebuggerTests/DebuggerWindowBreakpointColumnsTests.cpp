#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerBreakpointColumnsTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColumnsHost
    //
    //  A host that keeps the open views it was given and offers saved ones.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColumnsHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
        void  PauseDebugger           ()                                         override {}
        void  SetDebuggerCodeLines    (int, int)                                 override {}
        void  SetDebuggerCodeAddress  (std::optional<Word>, int)                 override {}
        void  SetDebuggerCodeTop      (Word, int)                                override {}
        void  SetDebuggerFollowView   (int)                                      override {}
        void  CloseDebuggerCodeView   (int)                                      override {}
        void  SetDebuggerTraceTop     (std::optional<uint64_t>)                  override {}
        void  GoToDebuggerMemory      (int, const std::string &)                 override {}
        void  ScrollDebuggerCode      (int, int)                                 override {}
        void  OnDebuggerWindowClosed  ()                                         override {}
        void  SetDebuggerKeyScheme    (const std::string &)                      override {}
        void  SetDebuggerLayout       (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  SetDebuggerMemoryWindow (int window, std::optional<Word> address) override
        {
            if (!address.has_value())
            {
                closedMemory.push_back (window);
            }
        }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return saved; }

        void  SetDebuggerOpenViews (const std::string & text) override { written = text; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<int>  closedMemory;
        std::string       saved;
        std::string       written;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ColumnsWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ColumnsWindow : public DebuggerWindow
    {
    public:
        ColumnsWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyBreakpoints;
        using DebuggerWindow::GetBreakpointOfRow;
        using DebuggerWindow::GetBreakpointList;
        using DebuggerWindow::SetSnapshotForTest;
        using DebuggerWindow::ToggleBreakpointColumn;
        using DebuggerWindow::SortBreakpoints;
        using DebuggerWindow::KeepOpenViews;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowFloatCloseTests
    //
    //  The close button on a floating pane's window closes the pane, which
    //  keeps its floating place to open in again; a pane nothing can reopen
    //  docks back instead.
    //
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowBreakpointColumnsTests
    //
    //  The breakpoints pane shows the columns of FR-117, Name, Condition and
    //  Hit count by default (FR-118); a column chosen from the pane's menu is
    //  kept with the open views and comes back on reopen; and a heading sorts
    //  the rows, each row still acting on its own breakpoint.
    //
    ////////////////////////////////////////////////////////////////////////////////

    using Column = BreakpointColumns::Column;



    static std::shared_ptr<DebuggerViewSnapshot> MakeSnapshot()
    {
        std::shared_ptr<DebuggerViewSnapshot>  snapshot = std::make_shared<DebuggerViewSnapshot>();
        uint32_t                               hits[]   = { 10, 2, 7 };



        for (int i = 0; i < 3; i++)
        {
            DebuggerViewSnapshot::BreakpointLine  bp;

            bp.id           = i + 1;
            bp.address      = (Word) (0x0300 + i);
            bp.info.id      = bp.id;
            bp.info.address = bp.address;
            bp.info.hits    = hits[i];
            snapshot->breakpoints.push_back (bp);
        }

        return snapshot;
    }



    TEST_CLASS (DebuggerWindowBreakpointColumnsTests)
    {
    public:

        TEST_METHOD (NameConditionAndHitCountShowByDefault)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            ColumnsHost    host;
            ColumnsWindow  window (theme, host);



            window.OnCreate();

            Assert::AreEqual (BreakpointColumns::kCount, window.GetBreakpointList()->GetColumnCount());
            Assert::IsTrue   (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Name));
            Assert::IsTrue   (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Condition));
            Assert::IsTrue   (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::HitCount));
            Assert::IsFalse  (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Data));
        }


        TEST_METHOD (AColumnChosenIsSavedAndNameAlwaysShows)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            ColumnsHost    host;
            ColumnsWindow  window (theme, host);



            window.OnCreate();
            window.SetSnapshotForTest (MakeSnapshot());
            window.KeepOpenViews();

            window.ToggleBreakpointColumn (Column::Data);
            window.ToggleBreakpointColumn (Column::Name);

            Assert::IsTrue (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Data));
            Assert::IsTrue (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Name), L"Name cannot be hidden");
            Assert::IsTrue (BreakpointColumns::ParseShown (host.written)[(size_t) Column::Data], L"the choice is saved");
        }


        TEST_METHOD (TheSavedColumnsComeBackOnReopen)
        {
            CassoTheme                theme  = CassoTheme::MakeSkeuomorphic();
            ColumnsHost               host;
            ColumnsWindow             window (theme, host);
            BreakpointColumns::Shown  shown  = BreakpointColumns::GetDefaultShown();



            shown[(size_t) Column::Condition] = false;
            shown[(size_t) Column::Address]   = true;
            host.saved                        = "follow=1" + BreakpointColumns::FormatShown (shown);

            window.OnCreate();
            window.SetSnapshotForTest (MakeSnapshot());
            window.KeepOpenViews();

            Assert::IsFalse (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Condition));
            Assert::IsTrue  (window.GetBreakpointList()->IsColumnVisible ((size_t) Column::Address));
        }


        TEST_METHOD (AHeadingSortsTheRowsAndEachRowKeepsItsBreakpoint)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            ColumnsHost    host;
            ColumnsWindow  window (theme, host);



            window.OnCreate();
            window.SetSnapshotForTest (MakeSnapshot());
            window.ApplyBreakpoints();

            Assert::AreEqual (1, window.GetBreakpointOfRow (0)->id, L"the engine's order before any sort");

            window.SortBreakpoints (Column::HitCount);
            Assert::AreEqual (2, window.GetBreakpointOfRow (0)->id, L"fewest hits first");
            Assert::AreEqual (1, window.GetBreakpointOfRow (2)->id);

            window.SortBreakpoints (Column::HitCount);
            Assert::AreEqual (1, window.GetBreakpointOfRow (0)->id, L"a second click turns it around");
        }
    };
}
