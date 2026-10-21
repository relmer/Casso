#include "Pch.h"
#include "CassoExplorer/CassoExplorerShell.h"
#include "CassoExplorer/Model/LaunchCommand.h"

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


    TEST_METHOD (ForwardedArguments_ParseBack)
    {
        // A second launch sends its command line to the running app, which
        // parses it as the launch would have: quotes, spaces and a trailing
        // backslash in the path included.
        CassoExplorerLaunchOptions  sent;
        CassoExplorerLaunchOptions  received;

        sent.owner       = (HWND) (UINT_PTR) 0x1A2B3C;
        sent.hasOwner    = true;
        sent.titlePrefix = L"033 \"explorer\"";
        sent.newWindow   = true;
        sent.openPath    = L"C:\\My Disks\\";

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments (CassoExplorerShell::SplitArguments (CassoExplorerShell::MakeForwardedArguments (sent)), received));
        Assert::IsTrue   (received.hasOwner);
        Assert::IsTrue   (received.owner == sent.owner);
        Assert::AreEqual (sent.titlePrefix, received.titlePrefix);
        Assert::IsTrue   (received.newWindow);
        Assert::AreEqual (sent.openPath, received.openPath);

        Assert::AreEqual (S_OK, CassoExplorerShell::ParseArguments (CassoExplorerShell::SplitArguments (CassoExplorerShell::MakeForwardedArguments (CassoExplorerLaunchOptions())), received));
        Assert::IsFalse  (received.hasOwner, L"A plain launch forwards as one");
        Assert::IsTrue   (received.openPath.empty());
    }


    static CassoExplorerSessionWindow  MakeSessionWindow (const wchar_t * path, int x)
    {
        CassoExplorerSessionWindow  window;
        BrowserModel::Tab           tab;

        tab.location           = Location::MakeHostFolder (path);
        window.tabs            = { tab, tab };
        window.activeTab       = 1;
        window.placement.x     = x;
        window.placement.w     = 800;
        window.placement.h     = 600;
        window.placement.valid = true;

        return window;
    }


    TEST_METHOD (FirstLaunch_RestoresEverySessionWindow)
    {
        CassoExplorerLaunchOptions               options;
        CassoExplorerPrefs                       prefs;
        std::vector<CassoExplorerWindowRequest>  requests;

        options.openPath = L"C:\\Opened";
        options.owner    = (HWND) (UINT_PTR) 0x1234;
        prefs.tabs       = { Location::MakeHostFolder (L"C:\\Old") };

        requests = CassoExplorerShell::PlanFirstLaunch (options, { MakeSessionWindow (L"C:\\A", 10), MakeSessionWindow (L"C:\\B", 900) }, prefs);

        Assert::AreEqual ((size_t) 2, requests.size(), L"One window for each in the session");
        Assert::AreEqual ((size_t) 2, requests[1].tabs.size());
        Assert::AreEqual ((size_t) 1, requests[1].activeTab);
        Assert::AreEqual (900,        requests[1].placement.x, L"each where it was");
        Assert::IsFalse  (requests[1].cascade);
        Assert::AreEqual (std::wstring (L"C:\\Opened"), requests[0].openPath, L"The launch's path opens in the first");
        Assert::IsTrue   (requests[0].owner == options.owner, L"which serves the launch's emulator");
        Assert::IsTrue   (requests[1].openPath.empty());
        Assert::IsTrue   (requests[1].owner == nullptr);
    }


    TEST_METHOD (ClosedRun_HasTheWindowsClosedShortlyBeforeTheLast)
    {
        std::vector<CassoExplorerShell::ClosedWindow>  closed;
        std::vector<CassoExplorerSessionWindow>        run;

        closed.push_back ({ 1000,  MakeSessionWindow (L"C:\\Alone",  10) });
        closed.push_back ({ 12000, MakeSessionWindow (L"C:\\First",  20) });
        closed.push_back ({ 20000, MakeSessionWindow (L"C:\\Second", 30) });

        //  Second closed 9 s before the last, First 8 s before Second; Alone
        //  11 s before First, which breaks the run.
        run = CassoExplorerShell::GetClosedRun (closed, 29000);

        Assert::AreEqual ((size_t) 2, run.size());
        Assert::AreEqual (20, run[0].placement.x, L"Oldest first");
        Assert::AreEqual (30, run[1].placement.x);

        //  A last window closed long after the others closes alone.
        Assert::IsTrue (CassoExplorerShell::GetClosedRun (closed, 31000).empty());
        Assert::IsTrue (CassoExplorerShell::GetClosedRun ({}, 29000).empty());
    }


    TEST_METHOD (FirstLaunch_WithNoSession_OpensThePreferencesTabs)
    {
        CassoExplorerLaunchOptions               options;
        CassoExplorerPrefs                       prefs;
        std::vector<CassoExplorerWindowRequest>  requests;

        prefs.tabs            = { Location::MakeHostFolder (L"C:\\Old"), Location::MakeDiskImage (L"C:\\Old\\a.dsk") };
        prefs.placement.x     = 42;
        prefs.placement.valid = true;

        requests = CassoExplorerShell::PlanFirstLaunch (options, {}, prefs);

        Assert::AreEqual ((size_t) 1, requests.size());
        Assert::AreEqual ((size_t) 2, requests[0].tabs.size(), L"The tabs from a build before sessions");
        Assert::IsTrue   (requests[0].tabs[1].location == Location::MakeDiskImage (L"C:\\Old\\a.dsk"));
        Assert::AreEqual (42, requests[0].placement.x);

        requests = CassoExplorerShell::PlanFirstLaunch (options, {}, CassoExplorerPrefs());
        Assert::AreEqual ((size_t) 1, requests.size(), L"With nothing saved, one window");
        Assert::IsTrue   (requests[0].tabs.empty(), L"on Home");
    }


    TEST_METHOD (FirstLaunch_NewWindowOpensItsPathAlone)
    {
        CassoExplorerLaunchOptions               options;
        CassoExplorerPrefs                       prefs;
        std::vector<CassoExplorerWindowRequest>  requests;

        options.newWindow = true;
        options.openPath  = L"C:\\Only";
        prefs.tabs        = { Location::MakeHostFolder (L"C:\\Old") };

        requests = CassoExplorerShell::PlanFirstLaunch (options, { MakeSessionWindow (L"C:\\A", 10) }, prefs);

        Assert::AreEqual ((size_t) 1, requests.size());
        Assert::IsTrue   (requests[0].tabs.empty(), L"Neither the session nor the preferences' tabs");
        Assert::AreEqual (std::wstring (L"C:\\Only"), requests[0].openPath);
    }


    TEST_METHOD (ForwardedLaunch_StepsFromTheWindowInFront)
    {
        CassoExplorerLaunchOptions     options;
        CassoExplorerPrefs::Placement  from;
        CassoExplorerWindowRequest     request;

        options.openPath = L"C:\\Next";
        from.x           = 100;
        from.valid       = true;

        request = CassoExplorerShell::PlanForwardedLaunch (options, from);

        Assert::AreEqual (std::wstring (L"C:\\Next"), request.openPath);
        Assert::AreEqual (100, request.placement.x);
        Assert::IsTrue   (request.cascade, L"stepped down and right from it");
        Assert::IsTrue   (request.tabs.empty(), L"on its path, not on the session");

        request = CassoExplorerShell::PlanForwardedLaunch (options, CassoExplorerPrefs::Placement());
        Assert::IsFalse  (request.cascade, L"With no window to step from, at the default place");
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
