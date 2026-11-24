#include "Pch.h"

#include "Seams/IRegistryReader.h"
#include "Seams/Win32WindowsCommands.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeRegistryReader
//
//  Registry keys in memory: each key's string values by name, the default
//  under the empty name. A key exists while it is in the map, and the keys
//  of HKEY_LOCAL_MACHINE and HKEY_CLASSES_ROOT are separate.
//
////////////////////////////////////////////////////////////////////////////////

class FakeRegistryReader : public IRegistryReader
{
public:
    using Values = std::map<std::wstring, std::wstring>;

    HRESULT ReadString (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText) const override
    {
        auto  found = keys.find (GetPath (root, key));
        bool  has   = found != keys.end() && found->second.contains (value != nullptr ? value : L"");

        outText = has ? found->second.at (value != nullptr ? value : L"") : std::wstring();
        return has ? S_OK : HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    }

    bool HasValue (HKEY root, const std::wstring & key, const wchar_t * value) const override
    {
        auto  found = keys.find (GetPath (root, key));

        return found != keys.end() && (value == nullptr || found->second.contains (value));
    }

    void SetValue (HKEY root, const std::wstring & key, const std::wstring & name, const std::wstring & value)
    {
        keys[GetPath (root, key)][name] = value;
    }

    static std::wstring GetPath (HKEY root, const std::wstring & key)
    {
        return ((root == HKEY_CLASSES_ROOT) ? L"HKCR\\" : L"HKLM\\") + key;
    }

    std::map<std::wstring, Values>  keys;
};





TEST_CLASS (Win32WindowsCommandsTests)
{
public:

    //  A file with no extension has no program or perceived type, so its
    //  verbs come from every file's keys alone.
    static constexpr const wchar_t *  kpszPath = L"C:\\Disks\\README";

    static std::vector<Win32ExplorerCommandEntries::Entry> GetStaticVerbs (FakeRegistryReader & registry)
    {
        Win32WindowsCommands                             commands (registry);
        std::vector<Win32ExplorerCommandEntries::Entry>  entries;

        commands.AddStaticVerbs (kpszPath, nullptr, entries);
        return entries;
    }



    //  The keys are read in the shell's order, every file's before every
    //  file system object's, and the verbs in each key in File Explorer's.
    TEST_METHOD (AddStaticVerbs_ListsTheFirstLevelVerbsInOrder)
    {
        FakeRegistryReader                               registry;
        std::vector<Win32ExplorerCommandEntries::Entry>  entries;

        registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\pintohomefile",                             L"MUIVerb", L"Add to Favorites");
        registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\setdesktopwallpaper",                       L"MUIVerb", L"Set as desktop background");
        registry.SetValue (HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shell\\Windows.PowerShell.Run", L"",        L"Run with PowerShell");

        entries = GetStaticVerbs (registry);

        Assert::AreEqual ((size_t) 3, entries.size());
        Assert::AreEqual (std::string ("setdesktopwallpaper"),    entries[0].verb);
        Assert::AreEqual (std::string ("pintohomefile"),          entries[1].verb);
        Assert::AreEqual (std::string ("Windows.PowerShell.Run"), entries[2].verb);

        Assert::AreEqual (std::wstring (L"Set as desktop background"), entries[0].title);
        Assert::AreEqual (std::wstring (L"Run with PowerShell"),       entries[2].title, L"The key's default is the title without a MUIVerb");

        Assert::AreEqual (std::wstring (L"windows.setdesktopwallpaper"), entries[0].explorerIcon);
        Assert::AreEqual (std::wstring (L"Windows.PowerShell.Run"),      entries[2].explorerIcon);
        Assert::IsTrue   (entries[0].enabled);
    }



    TEST_METHOD (AddStaticVerbs_SkipsAVerbFileExplorerHides)
    {
        FakeRegistryReader  registry;

        for (const wchar_t * hide : { L"LegacyDisable", L"ProgrammaticAccessOnly", L"Extended" })
        {
            registry.keys.clear();
            registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\pintohomefile", L"MUIVerb", L"Add to Favorites");
            registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\pintohomefile", hide,       L"");

            Assert::IsTrue (GetStaticVerbs (registry).empty(), hide);
        }
    }



    //  Only Windows' first-level verbs are listed, each once, and one with
    //  no title is left out.
    TEST_METHOD (AddStaticVerbs_ListsEachFirstLevelVerbOnce)
    {
        FakeRegistryReader                               registry;
        std::vector<Win32ExplorerCommandEntries::Entry>  entries;

        registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\open",                                   L"",        L"Open");
        registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\pintohomefile",                          L"MUIVerb", L"Add to Favorites");
        registry.SetValue (HKEY_CLASSES_ROOT, L"*\\shell\\pintohomefile",                          L"Icon",    L"shell32.dll,-1");
        registry.SetValue (HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shell\\pintohomefile",       L"MUIVerb", L"Pin again");
        registry.SetValue (HKEY_CLASSES_ROOT, L"AllFilesystemObjects\\shell\\setdesktopwallpaper", L"Icon",    L"imageres.dll,-2");

        entries = GetStaticVerbs (registry);

        Assert::AreEqual ((size_t) 1, entries.size());
        Assert::AreEqual (std::wstring (L"Add to Favorites"), entries[0].title);
        Assert::AreEqual (std::wstring (L"shell32.dll,-1"),   entries[0].icon);
    }
};
