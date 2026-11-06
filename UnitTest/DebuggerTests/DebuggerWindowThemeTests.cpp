#include "Pch.h"

#include "CaptureTests/FakeHostDialogs.h"
#include "Ui/Chrome/CassoTheme.h"
#include "Ui/Debugger/DebuggerThemes.h"
#include "Ui/Debugger/DebuggerWindow.h"
#include "Ui/Debugger/DisassemblyCells.h"
#include "Ui/Debugger/Panes/SourceDocumentPanes.h"

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
    //  SourceThemeHost
    //
    //  The same, finding main.a65: a comment, then LDA #$41 at $0300.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class SourceThemeHost : public ThemeHost
    {
    public:
        SourceLookup  FindDebuggerSource (const DebugSourceFile &, const std::wstring &, const std::string &) override
        {
            SourceLookup  lookup;



            lookup.match = SourceMatch::Exact;
            lookup.path  = L"C:\\Work\\main.a65";
            lookup.text  = "; main\n        lda #$41\n";
            return lookup;
        }
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
        using DebuggerWindow::Layout;
        using DebuggerWindow::ApplyTheme;
        using DebuggerWindow::GetMenuCommands;
        using DebuggerWindow::GetThemeName;
        using DebuggerWindow::GetTooltip;
        using DebuggerWindow::TakeSnapshot;
        using DebuggerWindow::GetCodeList;
        using DebuggerWindow::GetSourceGroup;
        using DebuggerWindow::GetPaneHostForTest;

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


        //  Stopped at line 2 of main.a65, the LDA at $0300, which is the
        //  first view's one row.
        static std::shared_ptr<DebuggerViewSnapshot>  MakeSourceSnapshot()
        {
            auto                            snapshot = std::make_shared<DebuggerViewSnapshot>();
            DebuggerViewSnapshot::CodeLine  line;
            DebugSourceFile                 file;



            line.address     = 0x0300;
            line.bytes       = "A9 41";
            line.instruction = "LDA #$41";
            line.isCurrent   = true;

            file.id   = 0;
            file.name = "main.a65";

            snapshot->isPaused              = true;
            snapshot->codeOpen[0]           = true;
            snapshot->pc                    = 0x0300;
            snapshot->codeViews[0]          = { line };
            snapshot->code                  = snapshot->codeViews[0];
            snapshot->source                = DebuggerViewSnapshot::SourceState();
            snapshot->source->debugFilePath = L"C:\\Work\\main.dbg";
            snapshot->source->programKey    = "key";
            snapshot->source->fileId        = 0;
            snapshot->source->line          = 2;
            snapshot->source->lineAddresses = std::make_shared<const std::map<std::pair<int, int>, Word>> (
                                                  std::map<std::pair<int, int>, Word> { { { 0, 2 }, 0x0300 } });
            snapshot->source->files.push_back (file);
            return snapshot;
        }


        //  The PC's mark on a tab has at least a glyph's 3:1 contrast with
        //  the band behind the tabs, in every theme the window offers.
        TEST_METHOD (ThePcsTabMarkReadsOnTheTabBandInEveryTheme)
        {
            constexpr float  kGlyphContrast = 3.0f;
            CassoTheme       theme          = CassoTheme::MakeSkeuomorphic();
            ThemeHost        host;
            ThemeWindow      window         (theme, host);
            uint32_t         mark           = 0;
            float            ratio          = 0.0f;



            window.OnCreate();
            Assert::IsFalse (DebuggerThemes::GetChoices().empty(), L"the window offers themes");

            for (const DebuggerThemes::Choice & choice : DebuggerThemes::GetChoices())
            {
                window.ApplyTheme (choice.name);

                mark  = window.GetPaneHostForTest().GetPcTabMarkArgb();
                ratio = DxuiColor::ComputeContrastRatio (mark, window.GetTheme()->PaneBand());

                Assert::IsTrue (ratio >= kGlyphContrast, (std::wstring (choice.label) + L": " + std::to_wstring (ratio)).c_str());
            }
        }


        //  A theme chosen while the machine is stopped colors the code views
        //  and the source documents again from the last snapshot, so the PC's
        //  row is in the new theme's color with no new snapshot.
        TEST_METHOD (AThemeChangeRecolorsTheCodeViewsAndDocuments)
        {
            CassoTheme           theme    = CassoTheme::MakeSkeuomorphic();
            SourceThemeHost      host;
            ThemeWindow          window   (theme, host);
            DxuiDpiScaler        scaler;
            DxuiTextView       * view     = nullptr;
            uint32_t             before   = 0;



            scaler.SetDpi (96);
            window.OnCreate();
            window.ApplyTheme ("");
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);
            window.TakeSnapshot (MakeSourceSnapshot());
            window.GetSourceGroup().Activate (0);
            window.Layout (RECT { 0, 0, 1400, 900 }, scaler);

            view   = window.GetSourceGroup().GetDocument (0).GetView();
            before = window.GetPaneHostForTest().GetPcRowArgb();

            Assert::AreEqual (before, window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kAddressColumn].background, L"the code view's PC row");
            Assert::AreEqual (before, view->GetRows()[1].background, L"the document's PC line");

            window.ApplyTheme ("SystemLight");

            Assert::AreNotEqual (before, window.GetPaneHostForTest().GetPcRowArgb(), L"the light theme's PC row differs");
            Assert::AreEqual    (window.GetPaneHostForTest().GetPcRowArgb(), window.GetCodeList (0)->GetCellsOfRow (0)[DisassemblyCells::kAddressColumn].background,
                                 L"the code view's PC row in the new theme's color");
            Assert::AreEqual    (window.GetPaneHostForTest().GetPcRowArgb(), view->GetRows()[1].background, L"the document's PC line in it too");
        }
    };
}
