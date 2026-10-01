#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerWindow.h"

#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerWindowThemeTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ThemeHost
    //
    //  A host that keeps the theme it was told to save.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ThemeHost : public IDebuggerWindowHost
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
    //  ThemeWindow
    //
    //  The debugger window with its controls built and no HWND, over the
    //  emulator's theme as Create gives it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ThemeWindow : public DebuggerWindow
    {
    public:
        ThemeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme         = &theme;
            m_emulatorTheme = &theme;
            m_host          = &host;
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::ApplyTheme;
        using DebuggerWindow::GetMenuCommands;
        using DebuggerWindow::GetThemeName;

        const DxuiTheme *  GetTheme() const { return m_theme; }

        std::shared_ptr<DxuiCommand>  FindRow (const std::wstring & label) const
        {
            std::shared_ptr<DxuiCommand>  found;



            for (const std::shared_ptr<DxuiCommand> & command : GetMenuCommands())
            {
                if (command->label == label)
                {
                    found = command;
                }
            }

            return found;
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebuggerWindowThemeTests
    //
    //  The debugger window takes a theme of its own, from Casso's themes and
    //  the system's light and dark ones, and the host keeps the choice.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebuggerWindowThemeTests)
    {
    public:

        TEST_METHOD (ChooseGivesEachThemeByItsName)
        {
            CassoTheme      emulator = CassoTheme::MakeSkeuomorphic();
            DxuiLightTheme  light;
            DxuiDarkTheme   dark;
            CassoTheme      own;



            Assert::IsTrue (&DebuggerThemes::Choose ("",            emulator, light, dark, own) == &emulator, L"empty follows the emulator");
            Assert::IsTrue (&DebuggerThemes::Choose ("Nonsense",    emulator, light, dark, own) == &emulator, L"an unknown name follows the emulator");
            Assert::IsTrue (&DebuggerThemes::Choose ("SystemLight", emulator, light, dark, own) == &light);
            Assert::IsTrue (&DebuggerThemes::Choose ("SystemDark",  emulator, light, dark, own) == &dark);

            const DxuiTheme &  retro = DebuggerThemes::Choose ("RetroTerminal", emulator, light, dark, own);

            Assert::IsTrue  (&retro == &own);
            Assert::AreEqual (CassoTheme::MakeRetroTerminal().navStrip, own.navStrip, L"the Casso theme by that name");
        }


        TEST_METHOD (ThemeMenuChoiceAppliesAndIsSaved)
        {
            CassoTheme   theme = CassoTheme::MakeSkeuomorphic();
            ThemeHost    host;
            ThemeWindow  window (theme, host);



            window.OnCreate();
            window.ApplyTheme ("");

            Assert::IsTrue (window.GetTheme() == &theme, L"the emulator's theme by default");

            std::shared_ptr<DxuiCommand>  dark = window.FindRow (L"System dark");

            Assert::IsTrue (dark != nullptr, L"the Theme drop-down lists System dark");
            Assert::IsFalse (dark->IsChecked());

            dark->dispatch();

            Assert::AreEqual (std::string ("SystemDark"), host.saved, L"the host keeps the choice");
            Assert::AreEqual (std::string ("SystemDark"), window.GetThemeName());
            Assert::IsTrue   (window.GetTheme() != &theme, L"the window left the emulator's theme");
            Assert::IsTrue   (dynamic_cast<const DxuiDarkTheme *> (window.GetTheme()) != nullptr, L"the system dark theme is in force");
            Assert::IsTrue   (window.FindRow (L"System dark")->IsChecked(), L"the rebuilt row is checked");
            Assert::IsFalse  (window.FindRow (L"Same as Casso")->IsChecked());

            window.FindRow (L"Same as Casso")->dispatch();

            Assert::AreEqual (std::string(), host.saved);
            Assert::IsTrue   (window.GetTheme() == &theme, L"back on the emulator's theme");
        }
    };
}
