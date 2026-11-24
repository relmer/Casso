#include "Pch.h"

#include "Seams/Win32WindowsCommands.h"
#include "Seams/IRegistryReader.h"
#include "Core/TextEncoding.h"


static constexpr const wchar_t *  s_kpszCommandStoreKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CommandStore\\shell";
static constexpr const wchar_t *  s_kpszSendToPhone     = L"Windows.SendToMyPhone";
static constexpr const wchar_t *  s_kpszCompressTo      = L"Windows.CompressTo";


//  The command store's commands that run a verb of the selection's own menu,
//  in File Explorer's order: Rotate right, then Rotate left.
static constexpr const wchar_t *  s_kpszStoreVerbs[]    = { L"Windows.rotate90", L"Windows.rotate270" };


//  The static verbs on File Explorer's first level; every other static verb
//  appears only in the classic menu.
static constexpr const wchar_t *  s_kpszFirstLevelVerbs[] =
{
    L"Windows.PowerShell.Run",
    L"setdesktopwallpaper",
    L"pintohomefile",
};





////////////////////////////////////////////////////////////////////////////////
//
//  RegistryPropertyBag
//
//  A command store entry's values, as a command reads them to initialize.
//
////////////////////////////////////////////////////////////////////////////////

class RegistryPropertyBag : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IPropertyBag>
{
public:
    explicit RegistryPropertyBag (std::wstring key) : m_key (std::move (key)) {}

    IFACEMETHODIMP Read (LPCOLESTR name, VARIANT * value, IErrorLog *) override
    {
        DWORD    number     = 0;
        DWORD    bytes      = sizeof (number);
        wchar_t  text[1024] = {};
        LSTATUS  status     = RegGetValueW (HKEY_LOCAL_MACHINE, m_key.c_str(), name, RRF_RT_REG_DWORD, nullptr, &number, &bytes);

        if (status == ERROR_SUCCESS)
        {
            value->vt   = VT_UI4;
            value->ulVal = number;
            return S_OK;
        }

        bytes  = sizeof (text);
        status = RegGetValueW (HKEY_LOCAL_MACHINE, m_key.c_str(), name, RRF_RT_REG_SZ, nullptr, text, &bytes);

        if (status != ERROR_SUCCESS)
        {
            return E_INVALIDARG;
        }

        value->vt      = VT_BSTR;
        value->bstrVal = SysAllocString (text);

        return S_OK;
    }

    IFACEMETHODIMP Write (LPCOLESTR, VARIANT *) override { return E_NOTIMPL; }

private:
    std::wstring  m_key;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands::GetWindowsEntries
//
//  File Explorer's order for a file: Send to My Phone, the type's own Windows
//  verbs with Add to Favorites last among them, then Compress to. A folder
//  has only Compress to here; its pin and tab rows are the window's own.
//  File Explorer's Share with is left out: its handler, in ntshrui.dll,
//  faults when created outside File Explorer.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32ExplorerCommandEntries::Entry> Win32WindowsCommands::GetWindowsEntries (const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind)
{
    std::vector<Win32ExplorerCommandEntries::Entry>  entries;
    Microsoft::WRL::ComPtr<IShellItemArray>          items;
    HRESULT                                          hr      = S_OK;
    bool                                             isFile  = kind == Win32ExplorerCommandEntries::ItemKind::File;



    BAIL_OUT_IF (paths.empty() || (kind != Win32ExplorerCommandEntries::ItemKind::File && kind != Win32ExplorerCommandEntries::ItemKind::Folder), S_OK);

    hr = Win32ExplorerCommandEntries::MakeItems (m_itemIds.empty() ? paths : m_itemIds, &items);
    CHR (hr);

    if (isFile)
    {
        //  File Explorer leaves this off an item as Home lists it.
        if (m_itemIds.empty())
        {
            AddStoreEntry (s_kpszSendToPhone, L"windows.sendtomyphone", items.Get(), entries);
        }

        AddStaticVerbs (paths[0], items.Get(), entries);
    }

    AddStoreEntry (s_kpszCompressTo, L"Windows.Zip.Action", items.Get(), entries);

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands::AddStoreEntry
//
//  A command from the shell's command store, created from its handler class
//  in this process, with the store's title and icon for a handler that has
//  none of its own.
//
////////////////////////////////////////////////////////////////////////////////

void Win32WindowsCommands::AddStoreEntry (const wchar_t * name, const wchar_t * explorerIcon, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries)
{
    HRESULT                                      hr    = S_OK;
    std::wstring                                 key   = std::wstring (s_kpszCommandStoreKey) + L"\\" + name;
    std::wstring                                 handler;
    std::wstring                                 title;
    std::wstring                                 icon;
    CLSID                                        clsid = {};
    Microsoft::WRL::ComPtr<IExplorerCommand>     command;
    Microsoft::WRL::ComPtr<IInitializeCommand>   initialize;
    Microsoft::WRL::ComPtr<RegistryPropertyBag>  bag;



    hr = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"ExplorerCommandHandler", handler);
    CHR (hr);

    hr = CLSIDFromString (handler.c_str(), &clsid);
    CHR (hr);

    hr    = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"MUIVerb", title);
    title = SUCCEEDED (hr) ? Win32ExplorerCommandEntries::LoadIndirectString (title) : std::wstring();

    hr = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"Icon", icon);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));
    CHR (hr);

    //  As File Explorer creates one: Send to My Phone builds what it runs on
    //  here, and faults on Invoke without it.
    hr = command.As (&initialize);

    if (SUCCEEDED (hr))
    {
        bag = Microsoft::WRL::Make<RegistryPropertyBag> (key);
        CBREx (bag != nullptr, E_OUTOFMEMORY);

        hr = initialize->Initialize (name, bag.Get());
        CHR (hr);
    }

    Win32ExplorerCommandEntries::AddEntry (command.Get(), items, 0, title, icon, outEntries);

    if (!outEntries.empty() && outEntries.back().command.Get() == command.Get())
    {
        outEntries.back().explorerIcon = explorerIcon;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands::AddStaticVerbs
//
//  Add the file's first-level static verbs, from the keys the shell reads a
//  file's verbs from: its program, its extension's and its perceived type's
//  system associations, then every file's.
//
////////////////////////////////////////////////////////////////////////////////

void Win32WindowsCommands::AddStaticVerbs (const std::wstring & path, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries)
{
    std::wstring               extension    = fs::path (path).extension().wstring();
    wchar_t                    program[256] = {};
    DWORD                      length       = (DWORD) std::size (program);
    PERCEIVED                  perceived    = PERCEIVED_TYPE_UNSPECIFIED;
    PERCEIVEDFLAG              flags        = 0;
    PWSTR                      typeName     = nullptr;
    std::vector<std::wstring>  classes;
    HRESULT                    hr           = S_OK;



    if (!extension.empty())
    {
        hr = AssocQueryStringW (ASSOCF_NONE, ASSOCSTR_PROGID, extension.c_str(), nullptr, program, &length);

        if (SUCCEEDED (hr))
        {
            classes.push_back (program);
        }

        classes.push_back (L"SystemFileAssociations\\" + extension);

        hr = AssocGetPerceivedType (extension.c_str(), &perceived, &flags, &typeName);

        if (SUCCEEDED (hr) && typeName != nullptr)
        {
            classes.push_back (std::wstring (L"SystemFileAssociations\\") + typeName);
        }

        CoTaskMemFree (typeName);
    }

    classes.push_back (L"*");
    classes.push_back (L"AllFilesystemObjects");

    for (const std::wstring & classKey : classes)
    {
        for (const wchar_t * verb : s_kpszFirstLevelVerbs)
        {
            bool  added = std::any_of (outEntries.begin(), outEntries.end(), [verb] (const Win32ExplorerCommandEntries::Entry & known)
            {
                return _wcsicmp (TextEncoding::NarrowToWide (known.verb).c_str(), verb) == 0;
            });

            if (!added)
            {
                hr = AddStaticVerb (classKey, verb, items, outEntries);
                IGNORE_RETURN_VALUE (hr, S_OK);
            }
        }
    }

    //  Rotate goes before Add to Favorites, which ends Windows' own verbs.
    {
        std::vector<Win32ExplorerCommandEntries::Entry>  rotate;
        auto                favorites = std::find_if (outEntries.begin(), outEntries.end(), [] (const Win32ExplorerCommandEntries::Entry & known) { return _stricmp (known.verb.c_str(), "pintohomefile") == 0; });
        ptrdiff_t           at        = favorites - outEntries.begin();

        for (const wchar_t * name : s_kpszStoreVerbs)
        {
            hr = AddStoreVerb (name, items, rotate);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        outEntries.insert (outEntries.begin() + at, std::make_move_iterator (rotate.begin()), std::make_move_iterator (rotate.end()));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands::AddStoreVerb
//
//  A command store command with no handler of its own, which runs the verb
//  of its name on the selection's menu: its title and icon from the store,
//  and whether it applies from its command state handler.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32WindowsCommands::AddStoreVerb (const wchar_t * name, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries)
{
    HRESULT                                        hr    = S_OK;
    std::wstring                                   key   = std::wstring (s_kpszCommandStoreKey) + L"\\" + name;
    std::wstring                                   handler;
    std::wstring                                   title;
    CLSID                                          clsid = {};
    EXPCMDSTATE                                    state = ECS_HIDDEN;
    Microsoft::WRL::ComPtr<IExplorerCommandState>  stateHandler;
    Win32ExplorerCommandEntries::Entry             entry;
    std::wstring                                   verb  = name;



    hr = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"CommandStateHandler", handler);
    CHR (hr);

    hr = CLSIDFromString (handler.c_str(), &clsid);
    CHR (hr);

    hr = CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&stateHandler));
    CHR (hr);

    //  A verb that does not apply is omitted, as in File Explorer: Rotate for
    //  a text file is reported as disabled rather than hidden.
    hr = stateHandler->GetState (items, TRUE, &state);
    CHR (hr);
    BAIL_OUT_IF ((state & (ECS_HIDDEN | ECS_DISABLED)) != 0, S_OK);

    hr = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"MUIVerb", title);
    CHR (hr);

    hr = m_registry.ReadString (HKEY_LOCAL_MACHINE, key, L"Icon", entry.icon);
    IGNORE_RETURN_VALUE (hr, S_OK);

    verb               = verb.substr (verb.find (L'.') + 1);
    entry.title        = Win32ExplorerCommandEntries::LoadIndirectString (title);
    entry.verb         = TextEncoding::WideToNarrow (verb);
    entry.explorerIcon = std::wstring (L"windows.") + verb;
    hr                 = S_OK;

    outEntries.push_back (std::move (entry));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32WindowsCommands::AddStaticVerb
//
//  Skip a verb marked LegacyDisable, ProgrammaticAccessOnly or Extended,
//  which no first-level menu lists, and one its command state handler hides.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32WindowsCommands::AddStaticVerb (const std::wstring & classKey, const wchar_t * verb, IShellItemArray * items, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries)
{
    HRESULT                                        hr     = S_OK;
    std::wstring                                   key    = classKey + L"\\shell\\" + verb;
    std::wstring                                   title;
    std::wstring                                   handler;
    CLSID                                          clsid  = {};
    EXPCMDSTATE                                    state  = ECS_ENABLED;
    Microsoft::WRL::ComPtr<IExplorerCommandState>  stateHandler;
    Win32ExplorerCommandEntries::Entry             entry;
    bool                                           exists = m_registry.HasValue (HKEY_CLASSES_ROOT, key, nullptr);
    bool                                           hidden = m_registry.HasValue (HKEY_CLASSES_ROOT, key, L"LegacyDisable") ||
                                                            m_registry.HasValue (HKEY_CLASSES_ROOT, key, L"ProgrammaticAccessOnly") ||
                                                            m_registry.HasValue (HKEY_CLASSES_ROOT, key, L"Extended");



    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    BAIL_OUT_IF (hidden, S_OK);

    hr = m_registry.ReadString (HKEY_CLASSES_ROOT, key, L"MUIVerb", title);

    if (FAILED (hr) || title.empty())
    {
        hr = m_registry.ReadString (HKEY_CLASSES_ROOT, key, nullptr, title);
        CHR (hr);
    }

    title = Win32ExplorerCommandEntries::LoadIndirectString (title);

    hr = m_registry.ReadString (HKEY_CLASSES_ROOT, key, L"CommandStateHandler", handler);

    if (SUCCEEDED (hr))
    {
        hr = CLSIDFromString (handler.c_str(), &clsid);
    }

    if (SUCCEEDED (hr))
    {
        hr = CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&stateHandler));
    }

    if (SUCCEEDED (hr))
    {
        hr    = stateHandler->GetState (items, TRUE, &state);
        state = SUCCEEDED (hr) ? state : ECS_ENABLED;
    }

    BAIL_OUT_IF ((state & ECS_HIDDEN) != 0, S_OK);

    hr = m_registry.ReadString (HKEY_CLASSES_ROOT, key, L"Icon", entry.icon);
    IGNORE_RETURN_VALUE (hr, S_OK);

    //  File Explorer's icon for a verb is the verb's name, under "windows.".
    entry.title        = title;
    entry.enabled      = (state & ECS_DISABLED) == 0;
    entry.verb         = TextEncoding::WideToNarrow (verb);
    entry.explorerIcon = (_wcsnicmp (verb, L"Windows.", 8) == 0) ? std::wstring (verb) : std::wstring (L"windows.") + verb;
    hr                 = S_OK;

    outEntries.push_back (std::move (entry));

Error:
    return hr;
}





