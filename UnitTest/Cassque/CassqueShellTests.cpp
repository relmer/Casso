#include "Pch.h"
#include "Cassque/CassqueShell.h"
#include "Cassque/Model/LaunchCommand.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassqueShellTests
//
//  The command line the emulator writes, read back; the refusals; the palette
//  a preference selects; and the caption. The window itself is exercised by
//  launching the executable, not here.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassqueShellTests)
{
public:

    //  Splits the way the C runtime splits a command line, for the simple
    //  quoting the emulator writes.
    static std::vector<std::wstring>  Split (const std::wstring & commandLine)
    {
        int                        count = 0;
        LPWSTR *                   parts = CommandLineToArgvW ((L"Cassque.exe " + commandLine).c_str(), &count);
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
        CassqueLaunchOptions  options;
        HWND                  owner = (HWND) (UINT_PTR) 0x1A2B3C;

        Assert::AreEqual (S_OK, CassqueShell::ParseArguments (Split (LaunchCommand::MakeCassqueArguments (owner, L"033 cassque")), options));

        Assert::IsTrue   (options.hasOwner);
        Assert::IsTrue   (options.owner == owner);
        Assert::AreEqual (std::wstring (L"033 cassque"), options.titlePrefix);
    }


    TEST_METHOD (NoArguments_IsAPlainLaunch)
    {
        CassqueLaunchOptions  options;

        Assert::AreEqual (S_OK, CassqueShell::ParseArguments ({}, options));
        Assert::IsFalse  (options.hasOwner);
        Assert::IsTrue   (options.titlePrefix.empty());
    }


    TEST_METHOD (BadOwner_IsRefusedWithAReason)
    {
        const wchar_t *  bad[] = { L"--owner abc", L"--owner 12x", L"--owner 0", L"--owner", L"--owner -5" };

        for (const wchar_t * commandLine : bad)
        {
            CassqueLaunchOptions  options;

            Assert::AreEqual (E_INVALIDARG, CassqueShell::ParseArguments (Split (commandLine), options), commandLine);
            Assert::IsFalse  (options.refusal.empty(), commandLine);
        }
    }


    TEST_METHOD (UnknownOrRepeatedArgument_IsRefused)
    {
        CassqueLaunchOptions  options;

        Assert::AreEqual (E_INVALIDARG, CassqueShell::ParseArguments (Split (L"--disk1 a.dsk"), options));
        Assert::AreNotEqual (std::wstring::npos, options.refusal.find (L"--disk1"));

        Assert::AreEqual (E_INVALIDARG, CassqueShell::ParseArguments (Split (L"--owner 5 --owner 6"), options));
    }


    TEST_METHOD (ChooseTheme_FollowsPreferenceThenSystem)
    {
        DxuiLightTheme  light;
        DxuiDarkTheme   dark;

        Assert::IsTrue (&CassqueShell::ChooseTheme (CassquePrefs::kThemeLight,        true,  light, dark) == &light);
        Assert::IsTrue (&CassqueShell::ChooseTheme (CassquePrefs::kThemeDark,         false, light, dark) == &dark);
        Assert::IsTrue (&CassqueShell::ChooseTheme (CassquePrefs::kThemeFollowSystem, true,  light, dark) == &dark);
        Assert::IsTrue (&CassqueShell::ChooseTheme (CassquePrefs::kThemeFollowSystem, false, light, dark) == &light);
        Assert::IsTrue (&CassqueShell::ChooseTheme (CassquePrefs::kThemeDarkModern,   false, light, dark) == &light);
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
        Assert::AreEqual (std::wstring (L"Cassque"),               CassqueShell::ComposeTitle (L""));
        Assert::AreEqual (std::wstring (L"worktree - Cassque"),    CassqueShell::ComposeTitle (L"worktree"));
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
