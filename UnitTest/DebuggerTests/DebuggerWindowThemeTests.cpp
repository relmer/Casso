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
    //  emulator's theme as Create gives it. Its system themes take their
    //  accent and list colors from the test, not from the machine.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class ThemeWindow : public DebuggerWindow
    {
    public:
        //  The dark list fill the test gives, one step off the #191919 the
        //  visual style gives, so a window that read the machine's shows
        //  something else.
        static constexpr uint32_t  kSystemContentDark = 0xFF1A1A1A;

        ThemeWindow (const CassoTheme & theme, IDebuggerWindowHost & host)
        {
            m_theme         = &theme;
            m_emulatorTheme = &theme;
            m_host          = &host;

            SetSystemColorsForTest (MakeSystemColors());
        }

        using DebuggerWindow::OnCreate;
        using DebuggerWindow::ApplyTheme;
        using DebuggerWindow::GetMenuCommands;
        using DebuggerWindow::GetThemeName;
        using DebuggerWindow::GetTooltip;

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

        //  Windows' default blue accent ramp, the light list fill its visual
        //  style gives, and the test's own dark one.
        static DxuiWindowsThemeColors::SystemColors MakeSystemColors()
        {
            static constexpr uint32_t             kAccentLight3 = 0xFF99EBFF;
            static constexpr uint32_t             kAccentLight2 = 0xFF4CC2FF;
            static constexpr uint32_t             kAccentDark1  = 0xFF0067C0;
            static constexpr uint32_t             kAccentDark2  = 0xFF003E92;
            static constexpr uint32_t             kContentLight = 0xFFFFFFFF;
            DxuiWindowsThemeColors::SystemColors  colors;



            colors.hasAccent    = true;
            colors.accentLight3 = kAccentLight3;
            colors.accentLight2 = kAccentLight2;
            colors.accentDark1  = kAccentDark1;
            colors.accentDark2  = kAccentDark2;
            colors.hasSurfaces  = true;
            colors.contentLight = kContentLight;
            colors.contentDark  = kSystemContentDark;

            return colors;
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


        //  The system themes the window chooses are its own, with Visual
        //  Studio's light pane colors, and its tip takes their tooltip
        //  colors: Visual Studio's light tooltip in light, and in dark a fill
        //  of the window's background with a border at half its brightness,
        //  each in Visual Studio's frame.
        TEST_METHOD (TheSystemThemesTakeTheDebuggersOwnColors)
        {
            CassoTheme   theme = CassoTheme::MakeSkeuomorphic();
            ThemeHost    host;
            ThemeWindow  window (theme, host);



            window.OnCreate();
            window.ApplyTheme ("SystemLight");

            Assert::AreEqual (0xFFF9F9F9u, window.GetTheme()->ContentBackground(),  L"light lists and titles");
            Assert::AreEqual (0xFFFFFFFFu, window.GetTheme()->TextViewBackground(), L"light text views");
            Assert::AreEqual (0xFFADADADu, window.GetTheme()->Border(),             L"a light pane's outline");
            Assert::AreEqual (0xFFF9F9F9u, window.GetTooltip().GetBackgroundArgb(), L"the light tip's fill, Visual Studio's");
            Assert::AreEqual (0xFFDDDDDDu, window.GetTooltip().GetBorderArgb(),     L"and its border");
            Assert::AreEqual (0xFF212121u, window.GetTooltip().GetTextArgb(),       L"and its text");

            window.ApplyTheme ("SystemDark");

            Assert::AreEqual (ThemeWindow::kSystemContentDark, window.GetTheme()->ContentBackground(), L"dark lists in the system's list fill, as the test gave it");

            Assert::AreEqual (0xFF272727u, window.GetTooltip().GetBackgroundArgb(), L"the dark tip's fill, the window's background");
            Assert::AreEqual (0xFF131313u, window.GetTooltip().GetBorderArgb(),     L"and its border, at half the brightness");
            Assert::AreEqual (0xFFFFFFFFu, window.GetTooltip().GetTextArgb(),       L"and its text");
            Assert::AreEqual (6.0f,        window.GetTooltip().GetCornerRadiusPx(), L"Visual Studio's 6.4-DIP corner at 96 DPI");
            Assert::AreEqual (1.0f,        window.GetTooltip().GetBorderPx(),       L"on a whole-pixel border");
        }
    };
}
