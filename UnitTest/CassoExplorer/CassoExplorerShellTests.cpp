#include "Pch.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"
#include "CassoExplorer/Model/TabTransfer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoExplorerShellTests
//
//  The command line the emulator writes, read back; the refusals; the palette
//  a preference selects; and the caption. The window itself is exercised by
//  launching the executable, not here.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoExplorerShellTests)
{
public:

    //  Splits the way the C runtime splits a command line, for the simple
    //  quoting the emulator writes.
    static std::vector<std::wstring>  Split (const std::wstring & commandLine)
    {
        int                        count = 0;
        LPWSTR *                   parts = CommandLineToArgvW ((L"CassoExplorer.exe " + commandLine).c_str(), &count);
        std::vector<std::wstring>  out;

        for (int i = 1; i < count; i++)
        {
            out.push_back (parts[i]);
        }

        LocalFree (parts);
        return out;
    }


    TEST_METHOD (ArgumentsTheEmulatorWrites_ParseBack)
    {
        CassoExplorerLaunchOptions  options;
        HWND                        owner   = (HWND) (UINT_PTR) 0x1A2B3C;

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments (Split (LaunchCommand::MakeCassoExplorerArguments (owner, L"033 explorer")), options));

        Assert::IsTrue   (options.hasOwner);
        Assert::IsTrue   (options.owner == owner);
        Assert::AreEqual (std::wstring (L"033 explorer"), options.titlePrefix);
    }


    TEST_METHOD (TearOffArguments_ParseBack)
    {
        // A torn-off tab crosses on the command line as JSON, quotes and
        // backslashes and all, and must come out of the C runtime's split
        // exactly as it went in.
        CassoExplorerLaunchOptions  options;
        BrowserModel::Tab           tab;
        BrowserModel::Tab           received;
        std::wstring                text;
        RECT                        window = { -1200, 40, -400, 640 };

        tab.location  = Location::MakeDiskDirectory (L"C:\\My Disks\\game \"one\".po", "GAMES");
        tab.back      = { Location::MakeHostFolder (L"C:\\My Disks\\") };
        tab.selection = { L"A \"quoted\" name" };
        text          = TabTransfer::Encode (tab);

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments (Split (LaunchCommand::MakeTearOffArguments (text, window, L"033 explorer")), options));
        Assert::IsTrue   (options.newWindow, L"A torn-off tab opens a window of its own");
        Assert::AreEqual (text, options.tab);
        Assert::IsTrue   (options.hasPlacement);
        Assert::AreEqual (-1200L, options.placement.left, L"on any monitor, left of the primary one included");
        Assert::AreEqual (40L,    options.placement.top);
        Assert::AreEqual (-400L,  options.placement.right);
        Assert::AreEqual (640L,   options.placement.bottom);
        Assert::AreEqual (std::wstring (L"033 explorer"), options.titlePrefix);

        Assert::IsTrue   (TabTransfer::TryDecode (options.tab, received));
        Assert::IsTrue   (received.location == tab.location);
        Assert::IsTrue   (received.back[0] == tab.back[0], L"A trailing backslash survives the quoting");
        Assert::AreEqual (tab.selection[0], received.selection[0]);
    }


    TEST_METHOD (WindowRect_TakesFourIntegersWithAPositiveSize)
    {
        CassoExplorerLaunchOptions  options;
        RECT                        rect    = {};

        Assert::IsTrue   (CassoExplorerShell::TryParseRect (L"-10,20,300,400", rect));
        Assert::AreEqual (-10L, rect.left);
        Assert::AreEqual (290L, rect.right);
        Assert::AreEqual (420L, rect.bottom);

        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"",           rect));
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,2,3",      rect));
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,2,3,4,5",  rect));
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,2,0,4",    rect), L"no width");
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,2,3,-4",   rect), L"no height");
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"a,2,3,4",    rect));
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,,3,4",     rect));
        Assert::IsFalse  (CassoExplorerShell::TryParseRect (L"1,2,3,4x",   rect));

        Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments ({ L"--at", L"1,2" }, options));
        Assert::IsFalse  (options.refusal.empty(), L"and the refusal gives the form it takes");
    }


    TEST_METHOD (NoArguments_IsAPlainLaunch)
    {
        CassoExplorerLaunchOptions  options;

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments ({}, options));
        Assert::IsFalse  (options.hasOwner);
        Assert::IsTrue   (options.titlePrefix.empty());
    }


    TEST_METHOD (BadOwner_IsRefusedWithAReason)
    {
        const wchar_t *  bad[] = { L"--owner abc", L"--owner 12x", L"--owner 0", L"--owner", L"--owner -5" };

        for (const wchar_t * commandLine : bad)
        {
            CassoExplorerLaunchOptions  options;

            Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments (Split (commandLine), options), commandLine);
            Assert::IsFalse  (options.refusal.empty(), commandLine);
        }
    }


    TEST_METHOD (UnknownOrRepeatedArgument_IsRefused)
    {
        CassoExplorerLaunchOptions  options;

        Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments (Split (L"--disk1 a.dsk"), options));
        Assert::AreNotEqual (std::wstring::npos, options.refusal.find (L"--disk1"));

        Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments (Split (L"--owner 5 --owner 6"), options));
    }


    TEST_METHOD (Path_IsOpenedOnceAlongsideTheSwitches)
    {
        CassoExplorerLaunchOptions  options;

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments ({ L"C:\\Disks\\a b", L"--owner", L"5" }, options));
        Assert::AreEqual (std::wstring (L"C:\\Disks\\a b"), options.openPath);
        Assert::IsTrue   (options.hasOwner);

        Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments ({ L"C:\\One", L"C:\\Two" }, options));
        Assert::AreNotEqual (std::wstring::npos, options.refusal.find (L"C:\\Two"));
    }


    TEST_METHOD (NewWindowArguments_ParseBack)
    {
        CassoExplorerLaunchOptions  options;
        const wchar_t *             paths[] = { L"C:\\", L"C:\\My Disks\\", L"D:\\a b\\game.dsk" };

        for (const wchar_t * path : paths)
        {
            Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments (Split (LaunchCommand::MakeNewWindowArguments (path, L"033 explorer")), options), path);
            Assert::IsTrue   (options.newWindow, path);
            Assert::IsFalse  (options.hasOwner, path);
            Assert::AreEqual (std::wstring (path), options.openPath);
            Assert::AreEqual (std::wstring (L"033 explorer"), options.titlePrefix);
        }

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments ({ L"C:\\One" }, options));
        Assert::IsFalse  (options.newWindow);
        Assert::AreEqual (E_INVALIDARG, CassoExplorerShell::ParseArguments ({ L"--new-window", L"--new-window" }, options));
    }


    TEST_METHOD (ChooseTheme_FollowsPreferenceThenSystem)
    {
        DxuiLightTheme  light;
        DxuiDarkTheme   dark;

        Assert::IsTrue (&CassoExplorerShell::ChooseTheme (CassoExplorerPrefs::kThemeLight,        true,  light, dark) == &light);
        Assert::IsTrue (&CassoExplorerShell::ChooseTheme (CassoExplorerPrefs::kThemeDark,         false, light, dark) == &dark);
        Assert::IsTrue (&CassoExplorerShell::ChooseTheme (CassoExplorerPrefs::kThemeFollowSystem, true,  light, dark) == &dark);
        Assert::IsTrue (&CassoExplorerShell::ChooseTheme (CassoExplorerPrefs::kThemeFollowSystem, false, light, dark) == &light);
        Assert::IsTrue (&CassoExplorerShell::ChooseTheme (CassoExplorerPrefs::kThemeDarkModern,   false, light, dark) == &light);
    }


    TEST_METHOD (Palettes_KeepBodyTextReadable)
    {
        DxuiLightTheme  light;
        DxuiDarkTheme   dark;

        Assert::IsTrue (ContrastRatio (light.Foreground(), light.Background()) >= 4.5);
        Assert::IsTrue (ContrastRatio (dark.Foreground(),  dark.Background())  >= 4.5);
        Assert::IsTrue (ContrastRatio (light.ForegroundMuted(), light.BackgroundElevated()) >= 4.5);
        Assert::IsTrue (ContrastRatio (dark.ForegroundMuted(),  dark.BackgroundElevated())  >= 4.5);
    }


    TEST_METHOD (ComposeTitle_PutsTheLabelFirst)
    {
        Assert::AreEqual (std::wstring (L"Casso Explorer"),               CassoExplorerShell::ComposeTitle (L"", L""));
        Assert::AreEqual (std::wstring (L"worktree - Casso Explorer"),    CassoExplorerShell::ComposeTitle (L"worktree", L""));
    }


    TEST_METHOD (ComposeTitle_DebugBuildAppendsItsIdentity)
    {
        Assert::AreEqual (std::wstring (L"Casso Explorer [Debug] - v1.0.0 x64 (Sep 14 2026 12:00:00)"),
                          CassoExplorerShell::ComposeTitle (L"", L"v1.0.0 x64 (Sep 14 2026 12:00:00)"));
        Assert::AreEqual (std::wstring (L"worktree - Casso Explorer [Debug] - v1.0.0 x64 (Sep 14 2026 12:00:00)"),
                          CassoExplorerShell::ComposeTitle (L"worktree", L"v1.0.0 x64 (Sep 14 2026 12:00:00)"));
    }


    static double  ContrastRatio (uint32_t a, uint32_t b)
    {
        double  la = RelativeLuminance (a);
        double  lb = RelativeLuminance (b);

        return ((std::max) (la, lb) + 0.05) / ((std::min) (la, lb) + 0.05);
    }


    static double  RelativeLuminance (uint32_t argb)
    {
        auto  channel = [] (uint32_t value)
        {
            double  c = value / 255.0;

            return (c <= 0.03928) ? c / 12.92 : std::pow ((c + 0.055) / 1.055, 2.4);
        };

        return 0.2126 * channel ((argb >> 16) & 0xFF) + 0.7152 * channel ((argb >> 8) & 0xFF) + 0.0722 * channel (argb & 0xFF);
    }
};
