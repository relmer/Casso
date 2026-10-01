#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerWindowClosedPaneTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ClosedPaneHost
    //
    //  A host that keeps the layout text the window saves, as the preferences do.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ClosedPaneHost : public IDebuggerWindowHost
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
        void  SetDebuggerLayout       (const std::string & text)                 override { layout = text; }
        void  SetDebuggerOpenViews    (const std::string &)                      override {}
        void  SetDebuggerPlacement    (const RECT &)                             override {}

        void  SetDebuggerMemoryWindow (int window, std::optional<Word> address) override
        {
            if (address.has_value())
            {
                openedMemory.push_back (window);
            }
        }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return layout; }
        std::string     GetDebuggerClosedPanes()          override { return closedPanes; }
        void            SetDebuggerClosedPanes (const std::string & text) override { closedPanes = text; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::vector<int>  openedMemory;
        std::string       layout;
        std::string       closedPanes;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ClosedPaneWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ClosedPaneWindow : public DebuggerWindow
    {
    public:
        ClosedPaneWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::CloseFloatingPane;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::ShowPane;
        using DebuggerWindow::IsPaneShown;
        using DebuggerWindow::GetViewMenuPanes;
        using DebuggerWindow::GetPaneLayout;
        using DebuggerWindow::EditPaneLayout;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowClosedPaneTests
    //
    //  A fixed pane the user closed is saved with the layout, so the next
    //  session opens with it closed, and the View menu shows it again.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowClosedPaneTests)
    {
    public:

        TEST_METHOD (AClosedFixedPaneStaysClosedInTheNextSession)
        {
            CassoTheme        theme  = CassoTheme::MakeSkeuomorphic();
            ClosedPaneHost    host;
            DxuiDpiScaler     scaler;
            ClosedPaneWindow  first  (theme, host);
            ClosedPaneWindow  second (theme, host);
            ClosedPaneWindow  third  (theme, host);



            scaler.SetDpi (96);

            first.OnCreate();
            first.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            first.ClosePane (DebuggerLayout::kRegisters);

            second.OnCreate();
            second.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            Assert::IsFalse (second.IsPaneShown (DebuggerLayout::kRegisters), L"the registers stay closed");
            Assert::IsTrue  (second.IsPaneShown (DebuggerLayout::kWatches),   L"the others still show");
            Assert::IsTrue  (second.GetPaneLayout().IsDocked (DebuggerLayout::kRegisters), L"it keeps its place");

            second.ShowPane (DebuggerLayout::kRegisters);
            Assert::IsTrue (second.IsPaneShown (DebuggerLayout::kRegisters), L"the View menu shows it again");

            third.OnCreate();
            third.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsTrue (third.IsPaneShown (DebuggerLayout::kRegisters), L"shown again is saved too");
        }


        TEST_METHOD (TheClosedPanesTextRoundTrips)
        {
            std::set<std::wstring>  closed = { DebuggerLayout::kTrace, DebuggerLayout::kStack };
            std::set<std::wstring>  read;



            DebuggerLayout::ReadClosedPanes (DebuggerLayout::ClosedPanesToText (closed), read);
            Assert::IsTrue (read == closed);

            DebuggerLayout::ReadClosedPanes (L"", read);
            Assert::IsTrue (read.empty());
        }


        TEST_METHOD (TheOldClosedLineIsTakenFromTheLayout)
        {
            std::set<std::wstring>  read;
            std::wstring            layout = DebuggerLayout::MakeDefault().ToText();
            std::wstring            text   = std::wstring (L"closed ") + DebuggerLayout::kTrace + L"\n" + layout;



            Assert::AreEqual (layout, DebuggerLayout::TakeClosedPanes (text, read));
            Assert::IsTrue   (read == std::set<std::wstring> { DebuggerLayout::kTrace });

            Assert::AreEqual (layout, DebuggerLayout::TakeClosedPanes (layout, read), L"a layout without the line has nothing closed");
            Assert::IsTrue   (read.empty());
        }
    };
}