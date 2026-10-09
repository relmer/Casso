#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerFocusAccentTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AccentHost
    //
    //  A host that keeps nothing and returns nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class AccentHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand      (const std::string &)                      override {}
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

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AccentWindow
    //
    //  The debugger window with its controls built and no HWND, whose focus
    //  hook a test can run as the window's own focus messages would.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class AccentWindow : public DebuggerWindow
    {
    public:
        AccentWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::FocusControl;
        using DebuggerWindow::GetCommandBox;
        using DebuggerWindow::GetPaneOfFocus;
        using DebuggerWindow::OnWindowFocusChanged;

        DxuiDockSite * FindDockSite()
        {
            for (size_t i = 0; i < GetChildCount(); i++)
            {
                if (auto * site = dynamic_cast<DxuiDockSite *> (GetChild (i)))
                {
                    return site;
                }
            }

            return nullptr;
        }

        //  Whether the group holding the focused control shows the focused
        //  look, and no other group does.
        bool HasAccentOnFocusedPane()
        {
            DxuiDockSite  * site    = FindDockSite();
            IDxuiControl  * focused = GetFocused();
            bool            matches = site != nullptr && focused != nullptr && site->GetGroupCount() > 0;

            for (size_t i = 0; matches && i < site->GetGroupCount(); i++)
            {
                DxuiTabGroup  * group      = site->GetGroup (i);
                bool            holdsFocus = false;

                for (size_t t = 0; t < group->GetTabCount(); t++)
                {
                    holdsFocus = holdsFocus || site->GetPaneOf (group->GetContent ((int) t)) == GetPaneOfFocus();
                }

                matches = group->HasFocusedLook() == holdsFocus;
            }

            return matches;
        }

        //  Each group's panes, with a star on a group showing the focused
        //  look, for a failure message.
        std::wstring DescribeGroups()
        {
            DxuiDockSite  * site = FindDockSite();
            std::wstring    text = L"focus " + GetPaneOfFocus() + L":";

            for (size_t i = 0; site != nullptr && i < site->GetGroupCount(); i++)
            {
                text += site->GetGroup (i)->HasFocusedLook() ? L" *[" : L" [";

                for (size_t t = 0; t < site->GetGroup (i)->GetTabCount(); t++)
                {
                    text += site->GetPaneOf (site->GetGroup (i)->GetContent ((int) t)) + L" ";
                }

                text += L"]";
            }

            return text;
        }

        bool HasAnyAccent()
        {
            DxuiDockSite  * site  = FindDockSite();
            bool            found = false;

            for (size_t i = 0; site != nullptr && i < site->GetGroupCount(); i++)
            {
                found = found || site->GetGroup (i)->HasFocusedLook();
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerFocusAccentTests
    //
    //  The window taking the keyboard focus shows the accent on its focused
    //  pane at once, in the focus message, rather than at its next frame
    //  paint; losing the focus to another application changes nothing.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerFocusAccentTests)
    {
    public:

        TEST_METHOD (TakingTheFocusShowsTheAccentAtOnce)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            AccentHost     host;
            AccentWindow   window (theme, host);
            DxuiDpiScaler  scaler;



            scaler.SetDpi (96);
            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.FocusControl (window.GetCommandBox());

            Assert::IsNotNull (window.FindDockSite(),                                       L"the dock site is one of the window's controls");
            Assert::AreEqual  (std::wstring (DebuggerLayout::kConsole), window.GetPaneOfFocus(), L"the console has the focus");
            Assert::IsFalse   (window.HasAnyAccent(),                                       L"no frame has run, so no pane shows the accent yet");

            window.OnWindowFocusChanged (true);
            Assert::IsTrue (window.HasAccentOnFocusedPane(), (L"the focus message itself puts the accent on the console, " + window.DescribeGroups()).c_str());

            window.OnWindowFocusChanged (false);
            Assert::IsTrue (window.HasAccentOnFocusedPane(), (L"losing the focus to another application keeps it there, " + window.DescribeGroups()).c_str());
        }
    };
}
