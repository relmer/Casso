#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Config/GlobalUserPrefs.h"
#include "Debugger/Source/SourcePathList.h"
#include "UiTests/InMemoryFileSystem.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerClosedPanesSettingTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SettingHost
    //
    //  A host that keeps the layout and the closed panes the window saves, each
    //  in its own setting, as the preferences do.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SettingHost : public IDebuggerWindowHost
    {
    public:
        void  RunDebuggerCommand       (const std::string &)                     override {}
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
        void  SetDebuggerLayout        (const std::string & text)                override { layout = text; }
        void  SetDebuggerClosedPanes   (const std::string & text)                override { closedPanes = text; }
        void  SetDebuggerOpenViews     (const std::string &)                     override {}
        void  SetDebuggerPlacement     (const RECT &)                            override {}
        void  SetDebuggerMemoryWindow  (int, std::optional<Word>)                override {}
        void  RunDebuggerCommandInMode (const std::string &, CommandMode)        override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return layout; }
        std::string     GetDebuggerClosedPanes()          override { return closedPanes; }
        std::string     GetDebuggerOpenViews()            override { return {}; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::string  layout;
        std::string  closedPanes;

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SettingWindow
    //
    //  The debugger window with its controls built and no HWND.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SettingWindow : public DebuggerWindow
    {
    public:
        SettingWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme = &theme;
            m_host  = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::Layout;
        using DebuggerWindow::ClosePane;
        using DebuggerWindow::IsPaneShown;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerClosedPanesSettingTests
    //
    //  The fixed panes the user closed are saved in a setting of their own, not
    //  on a line inside the layout text, and an old layout that carries that
    //  line has it moved to the setting.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerClosedPanesSettingTests)
    {
    public:

        TEST_METHOD (AClosedPaneIsSavedInItsOwnSetting)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            SettingHost    host;
            DxuiDpiScaler  scaler;
            SettingWindow  window (theme, host);



            scaler.SetDpi (96);

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.ClosePane (DebuggerLayout::kRegisters);

            Assert::AreEqual (SourcePathList::WideToUtf8 (DebuggerLayout::kRegisters), host.closedPanes);
            Assert::IsFalse  (host.layout.starts_with ("closed"), L"the layout text carries no closed line");
        }


        TEST_METHOD (AnOldLayoutHasItsClosedLineMovedToTheSetting)
        {
            CassoTheme     theme  = CassoTheme::MakeSkeuomorphic();
            SettingHost    host;
            DxuiDpiScaler  scaler;
            SettingWindow  window (theme, host);
            std::string    layout = SourcePathList::WideToUtf8 (DebuggerLayout::MakeDefault().ToText());



            scaler.SetDpi (96);
            host.layout = "closed " + SourcePathList::WideToUtf8 (DebuggerLayout::kWatches) + "\n" + layout;

            window.OnCreate();
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            Assert::IsFalse  (window.IsPaneShown (DebuggerLayout::kWatches), L"the pane the old line listed stays closed");
            Assert::AreEqual (SourcePathList::WideToUtf8 (DebuggerLayout::kWatches), host.closedPanes);
            Assert::IsFalse  (host.layout.starts_with ("closed"), L"the line is gone from the layout");
        }


        TEST_METHOD (ThePreferenceRoundTrips)
        {
            InMemoryFileSystem  fs;
            GlobalUserPrefs     saved;
            GlobalUserPrefs     loaded;
            HRESULT             hr     = S_OK;



            Assert::IsTrue (saved.debuggerClosedPanes.empty(), L"empty until a pane closes");

            saved.debuggerClosedPanes = "registers watches";

            hr = saved.Save (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            hr = loaded.Load (L"C:\\Casso", fs);
            Assert::IsTrue (SUCCEEDED (hr));

            Assert::AreEqual (std::string ("registers watches"), loaded.debuggerClosedPanes);
        }
    };
}
