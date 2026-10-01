#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerThemePreviewTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PreviewHost
    //
    //  A host that keeps the theme it was told to save.
    //
    ////////////////////////////////////////////////////////////////////////////////
    class PreviewHost : public IDebuggerWindowHost
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
        void  SetDebuggerTheme        (const std::string & name)                 override { saved = name; }

        void  RunDebuggerCommandInMode (const std::string &, CommandMode) override {}

        bool  TakeDebuggerUpdate (std::shared_ptr<const DebuggerViewSnapshot> &, std::vector<std::string> &) override { return false; }
        bool  TryGetDebuggerPlacement (RECT &)                                   override { return false; }

        IHostDialogs &  GetHostDialogs()         noexcept override { return m_dialogs; }
        std::string     GetDebuggerKeyScheme()            override { return {}; }
        std::string     GetDebuggerLayout()               override { return {}; }
        std::string     GetDebuggerOpenViews()            override { return {}; }
        std::string     GetDebuggerTheme()                override { return saved; }

        SourceLookup  FindDebuggerSource         (const DebugSourceFile &, const std::wstring &, const std::string &)                     override { return {}; }
        SourceLookup  MatchDroppedDebuggerSource (const std::vector<DebugSourceFile> &, const std::wstring &, const std::string &, int &) override { return {}; }

        std::string  saved = "unset";

    private:
        FakeHostDialogs  m_dialogs;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  PreviewWindow
    //
    //  The debugger window with its controls built and no HWND, over the
    //  emulator's theme as Create gives it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class PreviewWindow : public DebuggerWindow
    {
    public:
        PreviewWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme         = &theme;
            m_emulatorTheme = &theme;
            m_host          = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::ApplyTheme;
        using DebuggerWindow::GetThemeName;
        using DebuggerWindow::GetMenuBar;
        using DebuggerWindow::EndThemePreview;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowThemePreviewTests
    //
    //  Walking the rows of Tools > Theme shows each theme at once; closing the
    //  menu without a choice puts back the theme it opened over.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowThemePreviewTests)
    {
    public:

        static constexpr int  kToolsMenu = 5;


        //  Tools by keyboard, down to Theme, into its rows, and down one to
        //  the first Casso theme.
        static void  WalkToSecondTheme (PreviewWindow & window)
        {
            DxuiMenuBar  * bar = window.GetMenuBar();



            bar->Open      (kToolsMenu, true);
            bar->HandleKey (VK_DOWN);
            bar->HandleKey (VK_RIGHT);
            bar->HandleKey (VK_DOWN);
        }


        TEST_METHOD (HighlightingAThemeRowShowsThatTheme)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            PreviewHost    host;
            PreviewWindow  window (theme, host);



            window.OnCreate();
            window.ApplyTheme ("");

            WalkToSecondTheme (window);

            Assert::AreEqual (std::string (DebuggerThemes::GetChoices()[1].name), window.GetThemeName(), L"the highlighted theme is not in force");
            Assert::AreEqual (std::string ("unset"), host.saved, L"a preview is not saved");
        }


        TEST_METHOD (ClosingWithoutAChoiceRestoresTheTheme)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            PreviewHost    host;
            PreviewWindow  window (theme, host);



            window.OnCreate();
            window.ApplyTheme ("SystemDark");

            WalkToSecondTheme (window);
            window.GetMenuBar()->CloseAll();
            window.EndThemePreview();

            Assert::AreEqual (std::string ("SystemDark"), window.GetThemeName());
        }


        TEST_METHOD (ChoosingTheHighlightedThemeKeepsIt)
        {
            CassoTheme     theme = CassoTheme::MakeSkeuomorphic();
            PreviewHost    host;
            PreviewWindow  window (theme, host);



            window.OnCreate();
            window.ApplyTheme ("SystemDark");

            WalkToSecondTheme (window);
            window.GetMenuBar()->HandleKey (VK_RETURN);
            window.EndThemePreview();

            Assert::IsFalse  (window.GetMenuBar()->IsOpen());
            Assert::AreEqual (std::string (DebuggerThemes::GetChoices()[1].name), window.GetThemeName());
            Assert::AreEqual (std::string (DebuggerThemes::GetChoices()[1].name), host.saved);
        }
    };
}