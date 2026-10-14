#include "Pch.h"

#include "Seams/Win32PackagedCommands.h"
#include "Core/TextEncoding.h"
#include "Core/ThreadName.h"

#pragma comment(lib, "xmllite.lib")


static constexpr const wchar_t *  s_kpszPackagesKey  = L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
static constexpr const wchar_t *  s_kpszManifestName = L"AppxManifest.xml";
static constexpr const wchar_t *  s_kpszCommandStoreKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\CommandStore\\shell";
static constexpr const wchar_t *  s_kpszSendToPhone     = L"Windows.SendToMyPhone";
static constexpr const wchar_t *  s_kpszCompressTo      = L"Windows.CompressTo";
static constexpr std::wstring_view  s_kMsResourceScheme = L"ms-resource:";

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
//  Win32PackagedCommands::~Win32PackagedCommands
//
////////////////////////////////////////////////////////////////////////////////

Win32PackagedCommands::~Win32PackagedCommands()
{
    if (m_reader.joinable())
    {
        m_reader.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::Start()
{
    if (m_reader.joinable())
    {
        return;
    }

    m_reader = std::thread ([this]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_MULTITHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"Package manifests");



        IGNORE_RETURN_VALUE (hrName, S_OK);

        ReadAll();

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::ReadAll
//
//  Read every installed package's manifest, listed by full name under the
//  AppModel package repository key. The registered manifest is in the
//  package's install folder, a sparse package's included; a version no
//  longer installed has no install folder and is skipped.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::ReadAll()
{
    std::vector<Registration>  found;
    HKEY                       packages  = nullptr;
    wchar_t                    name[512] = {};
    DWORD                      length    = 0;
    LSTATUS                    status    = ERROR_SUCCESS;



    status = RegOpenKeyExW (HKEY_CLASSES_ROOT, s_kpszPackagesKey, 0, KEY_READ, &packages);

    for (DWORD index = 0; status == ERROR_SUCCESS; index++)
    {
        Microsoft::WRL::ComPtr<IStream>  manifest;
        HRESULT                          hr = S_OK;

        length = (DWORD) std::size (name);
        status = RegEnumKeyExW (packages, index, name, &length, nullptr, nullptr, nullptr, nullptr);

        if (status != ERROR_SUCCESS)
        {
            break;
        }

        hr = OpenManifest (name, &manifest);

        if (SUCCEEDED (hr))
        {
            hr = ReadManifest (manifest.Get(), name, found);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }

    if (packages != nullptr)
    {
        RegCloseKey (packages);
    }

    //  File Explorer lists apps' commands by the app's name.
    std::stable_sort (found.begin(), found.end(), [] (const Registration & a, const Registration & b)
    {
        const std::wstring &  nameA = a.appName.empty() ? a.package : a.appName;
        const std::wstring &  nameB = b.appName.empty() ? b.package : b.appName;

        return _wcsicmp (nameA.c_str(), nameB.c_str()) < 0;
    });

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_registrations = std::move (found);
        m_read          = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::OpenManifest
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::OpenManifest (const wchar_t * fullName, IStream ** outStream)
{
    HRESULT       hr               = S_OK;
    wchar_t       folder[MAX_PATH] = {};
    UINT32        length           = (UINT32) std::size (folder);
    LONG          status           = GetPackagePathByFullName2 (fullName, PackagePathType_Install, &length, folder);
    std::wstring  manifest;



    *outStream = nullptr;

    CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));

    manifest = std::wstring (folder) + L"\\" + s_kpszManifestName;

    hr = SHCreateStreamOnFileEx (manifest.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, outStream);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::ReadManifest
//
//  Collect each Verb's class under an ItemType, inside a
//  FileExplorerContextMenus extension, with any namespace prefix, and the
//  package's display name from its Properties, which come first.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::ReadManifest (IStream * stream, const wchar_t * fullName, std::vector<Registration> & outRegistrations)
{
    HRESULT                             hr           = S_OK;
    Microsoft::WRL::ComPtr<IXmlReader>  reader;
    XmlNodeType                         node         = XmlNodeType_None;
    const wchar_t                     * local        = nullptr;
    const wchar_t                     * text         = nullptr;
    bool                                inMenus      = false;
    bool                                inProperties = false;
    bool                                inAppName    = false;
    std::wstring                        itemType;
    std::wstring                        value;
    Registration                        registration;



    registration.package = fullName;

    hr = CreateXmlReader (__uuidof (IXmlReader), (void **) reader.GetAddressOf(), nullptr);
    CHR (hr);

    hr = reader->SetProperty (XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
    CHR (hr);

    hr = reader->SetInput (stream);
    CHR (hr);

    for (;;)
    {
        bool  element = false;

        hr = reader->Read (&node);
        CHR (hr);
        BAIL_OUT_IF (hr == S_FALSE, S_OK);

        if (node == XmlNodeType_Text && inAppName)
        {
            hr = reader->GetValue (&text, nullptr);
            CHR (hr);

            registration.appName = LoadAppName (fullName, text);
            continue;
        }

        if (node != XmlNodeType_Element && node != XmlNodeType_EndElement)
        {
            continue;
        }

        hr = reader->GetLocalName (&local, nullptr);
        CHR (hr);

        element   = node == XmlNodeType_Element;
        inAppName = element && inProperties && wcscmp (local, L"DisplayName") == 0;

        if (wcscmp (local, L"Properties") == 0)
        {
            inProperties = element;
            continue;
        }

        if (wcscmp (local, L"FileExplorerContextMenus") == 0)
        {
            inMenus = element;
            continue;
        }

        if (!inMenus || !element)
        {
            continue;
        }

        if (wcscmp (local, L"ItemType") == 0)
        {
            hr = GetAttribute (reader.Get(), L"Type", itemType);
            itemType = SUCCEEDED (hr) ? itemType : std::wstring();
        }
        else if (wcscmp (local, L"Verb") == 0)
        {
            hr = GetAttribute (reader.Get(), L"Clsid", value);

            if (SUCCEEDED (hr))
            {
                hr = ParseClsid (value, registration.clsid);
            }

            if (SUCCEEDED (hr))
            {
                registration.itemType = itemType;
                AddRegistration (registration, outRegistrations);
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::LoadAppName
//
//  A display name given as a resource, "ms-resource:AppName", is looked up
//  in the package's resources; a plain one is used as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32PackagedCommands::LoadAppName (const wchar_t * fullName, const std::wstring & displayName)
{
    std::wstring  name      = fullName;
    std::wstring  resource  = displayName.substr ((std::min) (displayName.size(), s_kMsResourceScheme.size()));
    std::wstring  source;
    wchar_t       text[256] = {};
    HRESULT       hr        = S_OK;



    if (displayName.compare (0, s_kMsResourceScheme.size(), s_kMsResourceScheme) != 0)
    {
        return displayName;
    }

    name   = name.substr (0, name.find (L'_'));
    source = (resource.compare (0, 2, L"//") == 0) ? std::wstring (s_kMsResourceScheme) + resource
           : (resource.compare (0, 1, L"/") == 0)  ? std::wstring (s_kMsResourceScheme) + L"//" + name + resource
                                                   : std::wstring (s_kMsResourceScheme) + L"//" + name + L"/Resources/" + resource;

    hr = SHLoadIndirectString ((L"@{" + std::wstring (fullName) + L"?" + source + L"}").c_str(), text, (UINT) std::size (text), nullptr);

    return SUCCEEDED (hr) ? std::wstring (text) : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::LoadIndirectString
//
//  A "@file,-id" resource string read from its file; any other text is
//  returned as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32PackagedCommands::LoadIndirectString (const std::wstring & text)
{
    wchar_t  loaded[512] = {};
    bool     indirect    = !text.empty() && text[0] == L'@';
    HRESULT  hr          = indirect ? SHLoadIndirectString (text.c_str(), loaded, (UINT) std::size (loaded), nullptr) : E_INVALIDARG;



    return SUCCEEDED (hr) ? std::wstring (loaded) : text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetAttribute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::GetAttribute (IXmlReader * reader, const wchar_t * attribute, std::wstring & outValue)
{
    HRESULT          hr    = S_OK;
    const wchar_t  * value = nullptr;



    outValue.clear();

    hr = reader->MoveToAttributeByName (attribute, nullptr);
    CHR (hr);
    CBREx (hr == S_OK, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = reader->GetValue (&value, nullptr);
    CHR (hr);

    outValue = value;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::ParseClsid
//
//  Manifests write a class id with or without its braces.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::ParseClsid (const std::wstring & text, CLSID & outClsid)
{
    HRESULT       hr     = S_OK;
    bool          braced = !text.empty() && text.front() == L'{';
    std::wstring  id     = braced ? text : L"{" + text + L"}";



    hr = CLSIDFromString (id.c_str(), &outClsid);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddRegistration
//
//  A class registered more than once for the same item type is added once.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddRegistration (const Registration & registration, std::vector<Registration> & outRegistrations)
{
    for (const Registration & known : outRegistrations)
    {
        if (known.clsid == registration.clsid && _wcsicmp (known.itemType.c_str(), registration.itemType.c_str()) == 0)
        {
            return;
        }
    }

    outRegistrations.push_back (registration);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Applies
//
//  A file is matched by its extension or by "*"; a folder, the background of
//  one and a drive each by their own type. Every item must match.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::Applies (const Registration & registration, const std::vector<std::wstring> & paths, ItemKind kind)
{
    const wchar_t  * type = registration.itemType.c_str();



    switch (kind)
    {
        case ItemKind::Folder:      return _wcsicmp (type, L"Directory") == 0 || _wcsicmp (type, L"Folder") == 0;
        case ItemKind::Background:  return _wcsicmp (type, L"Directory\\Background") == 0;
        case ItemKind::Drive:       return _wcsicmp (type, L"Drive") == 0;
        case ItemKind::File:
        default:
            break;
    }

    if (wcscmp (type, L"*") == 0)
    {
        return true;
    }

    return type[0] == L'.' && !paths.empty() && std::all_of (paths.begin(), paths.end(), [type] (const std::wstring & path)
    {
        std::wstring  extension = fs::path (path).extension().wstring();

        return _wcsicmp (extension.c_str(), type) == 0;
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::MakeItems
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::MakeItems (const std::vector<std::wstring> & paths, IShellItemArray ** outItems)
{
    HRESULT                        hr  = S_OK;
    std::vector<PIDLIST_ABSOLUTE>  ids;



    *outItems = nullptr;

    for (const std::wstring & path : paths)
    {
        PIDLIST_ABSOLUTE  id = nullptr;

        hr = SHParseDisplayName (path.c_str(), nullptr, &id, 0, nullptr);
        CHR (hr);

        ids.push_back (id);
    }

    hr = SHCreateShellItemArrayFromIDLists ((UINT) ids.size(), (PCIDLIST_ABSOLUTE_ARRAY) ids.data(), outItems);
    CHR (hr);

Error:
    for (PIDLIST_ABSOLUTE id : ids)
    {
        CoTaskMemFree (id);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32PackagedCommands::Entry> Win32PackagedCommands::GetEntries (const std::vector<std::wstring> & paths, ItemKind kind)
{
    std::vector<Entry>                         entries;
    std::vector<Registration>                  registrations;
    Microsoft::WRL::ComPtr<IShellItemArray>    items;
    HRESULT                                    hr = S_OK;



    {
        std::lock_guard<std::mutex>  guard (m_lock);

        BAIL_OUT_IF (!m_read, S_OK);
        registrations = m_registrations;
    }

    BAIL_OUT_IF (paths.empty(), S_OK);

    hr = MakeItems (paths, &items);
    CHR (hr);

    //  A package's registrations are read together, so each package's are a
    //  run here.
    for (size_t first = 0, next = 0; first < registrations.size(); first = next)
    {
        std::vector<Entry>  own;
        size_t              applies = 0;

        for (next = first; next < registrations.size() && registrations[next].package == registrations[first].package; next++)
        {
            Microsoft::WRL::ComPtr<IExplorerCommand>  command;
            HRESULT                                   hrMade = S_OK;

            if (!Applies (registrations[next], paths, kind))
            {
                continue;
            }

            applies++;
            hrMade = CoCreateInstance (registrations[next].clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));

            if (SUCCEEDED (hrMade))
            {
                AddEntry (command.Get(), items.Get(), 0, L"", L"", own);
            }
        }

        AddPackageEntries (registrations[first].appName, applies, own, entries);
    }

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddPackageEntries
//
//  An app with more than one command for the items has them in a submenu
//  under its name, as File Explorer groups them; one with a single command
//  has it on the menu itself.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddPackageEntries (const std::wstring & appName, size_t applies, std::vector<Entry> & own, std::vector<Entry> & outEntries)
{
    Entry  group;



    if (own.empty())
    {
        return;
    }

    if (applies < 2 || appName.empty())
    {
        std::move (own.begin(), own.end(), std::back_inserter (outEntries));
        return;
    }

    group.title    = appName;
    group.icon     = own.front().icon;
    group.children = std::move (own);

    outEntries.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddEntry
//
//  Skip a command hidden for these items and gray a disabled one. After
//  E_PENDING from a quick GetState, call it again with okToBeSlow set, as
//  File Explorer does. A command with no title or icon of its
//  own takes the fallbacks, and one with subcommands becomes a submenu.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddEntry (IExplorerCommand * command, IShellItemArray * items, int depth, const std::wstring & fallbackTitle,
                                      const std::wstring & fallbackIcon, std::vector<Entry> & outEntries)
{
    EXPCMDSTATE                                    state  = ECS_ENABLED;
    EXPCMDFLAGS                                    flags  = ECF_DEFAULT;
    PWSTR                                          title  = nullptr;
    PWSTR                                          icon   = nullptr;
    Entry                                          entry;
    Microsoft::WRL::ComPtr<IEnumExplorerCommand>   children;
    HRESULT                                        hr     = S_OK;



    hr = command->GetState (items, FALSE, &state);

    if (hr == E_PENDING)
    {
        hr = command->GetState (items, TRUE, &state);
    }

    state = SUCCEEDED (hr) ? state : ECS_ENABLED;

    hr = command->GetFlags (&flags);
    flags = SUCCEEDED (hr) ? flags : ECF_DEFAULT;

    if ((state & ECS_HIDDEN) != 0 || (flags & ECF_ISSEPARATOR) != 0)
    {
        return;
    }

    hr = command->GetTitle (items, &title);

    entry.title = (SUCCEEDED (hr) && title != nullptr) ? std::wstring (title) : fallbackTitle;
    CoTaskMemFree (title);

    if (entry.title.empty())
    {
        return;
    }

    entry.enabled = (state & ECS_DISABLED) == 0;
    entry.command = command;

    hr = command->GetIcon (items, &icon);

    entry.icon = (SUCCEEDED (hr) && icon != nullptr) ? std::wstring (icon) : fallbackIcon;
    CoTaskMemFree (icon);

    if ((flags & ECF_HASSUBCOMMANDS) != 0 && depth < s_kMaxDepth)
    {
        hr = command->EnumSubCommands (&children);

        while (SUCCEEDED (hr) && children)
        {
            Microsoft::WRL::ComPtr<IExplorerCommand>  child;
            ULONG                                     fetched = 0;

            hr = children->Next (1, &child, &fetched);

            if (hr != S_OK || fetched == 0)
            {
                break;
            }

            AddEntry (child.Get(), items, depth + 1, L"", L"", entry.children);
        }
    }

    outEntries.push_back (std::move (entry));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetWindowsEntries
//
//  File Explorer's order for a file: Send to My Phone, the type's own Windows
//  verbs with Add to Favorites last among them, then Compress to. A folder
//  has only Compress to here; its pin and tab rows are the window's own.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32PackagedCommands::Entry> Win32PackagedCommands::GetWindowsEntries (const std::vector<std::wstring> & paths, ItemKind kind)
{
    std::vector<Entry>                         entries;
    Microsoft::WRL::ComPtr<IShellItemArray>    items;
    HRESULT                                    hr     = S_OK;
    bool                                       isFile = kind == ItemKind::File;



    BAIL_OUT_IF (paths.empty() || (kind != ItemKind::File && kind != ItemKind::Folder), S_OK);

    hr = MakeItems (paths, &items);
    CHR (hr);

    if (isFile)
    {
        AddStoreEntry  (s_kpszSendToPhone, L"windows.sendtomyphone", items.Get(), entries);
        AddStaticVerbs (paths[0], items.Get(), entries);
    }

    AddStoreEntry (s_kpszCompressTo, L"Windows.Zip.Action", items.Get(), entries);

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddStoreEntry
//
//  A command from the shell's command store, created from its handler class
//  in this process, with the store's title and icon for a handler that has
//  none of its own.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddStoreEntry (const wchar_t * name, const wchar_t * explorerIcon, IShellItemArray * items, std::vector<Entry> & outEntries)
{
    HRESULT                                    hr      = S_OK;
    std::wstring                               key     = std::wstring (s_kpszCommandStoreKey) + L"\\" + name;
    std::wstring                               handler;
    std::wstring                               title;
    std::wstring                               icon;
    CLSID                                      clsid   = {};
    Microsoft::WRL::ComPtr<IExplorerCommand>   command;



    hr = ReadString (HKEY_LOCAL_MACHINE, key, L"ExplorerCommandHandler", handler);
    CHR (hr);

    hr = CLSIDFromString (handler.c_str(), &clsid);
    CHR (hr);

    hr    = ReadString (HKEY_LOCAL_MACHINE, key, L"MUIVerb", title);
    title = SUCCEEDED (hr) ? LoadIndirectString (title) : std::wstring();

    hr = ReadString (HKEY_LOCAL_MACHINE, key, L"Icon", icon);
    IGNORE_RETURN_VALUE (hr, S_OK);

    hr = CoCreateInstance (clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));
    CHR (hr);

    AddEntry (command.Get(), items, 0, title, icon, outEntries);

    if (!outEntries.empty() && outEntries.back().command.Get() == command.Get())
    {
        outEntries.back().explorerIcon = explorerIcon;
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddStaticVerbs
//
//  Add the file's first-level static verbs, from the keys the shell reads a
//  file's verbs from: its program, its extension's and its perceived type's
//  system associations, then every file's.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddStaticVerbs (const std::wstring & path, IShellItemArray * items, std::vector<Entry> & outEntries)
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
            bool  added = std::any_of (outEntries.begin(), outEntries.end(), [verb] (const Entry & known)
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddStaticVerb
//
//  Skip a verb marked LegacyDisable, ProgrammaticAccessOnly or Extended,
//  which no first-level menu lists, and one its command state handler hides.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::AddStaticVerb (const std::wstring & classKey, const wchar_t * verb, IShellItemArray * items, std::vector<Entry> & outEntries)
{
    HRESULT                                         hr        = S_OK;
    std::wstring                                    key       = classKey + L"\\shell\\" + verb;
    std::wstring                                    title;
    std::wstring                                    handler;
    CLSID                                           clsid     = {};
    EXPCMDSTATE                                     state     = ECS_ENABLED;
    Microsoft::WRL::ComPtr<IExplorerCommandState>   stateHandler;
    Entry                                           entry;
    bool                                            exists    = HasValue (key, nullptr);
    bool                                            hidden    = HasValue (key, L"LegacyDisable") || HasValue (key, L"ProgrammaticAccessOnly") || HasValue (key, L"Extended");



    CBREx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    BAIL_OUT_IF (hidden, S_OK);

    hr = ReadString (HKEY_CLASSES_ROOT, key, L"MUIVerb", title);

    if (FAILED (hr) || title.empty())
    {
        hr = ReadString (HKEY_CLASSES_ROOT, key, nullptr, title);
        CHR (hr);
    }

    title = LoadIndirectString (title);

    hr = ReadString (HKEY_CLASSES_ROOT, key, L"CommandStateHandler", handler);

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

    hr = ReadString (HKEY_CLASSES_ROOT, key, L"Icon", entry.icon);
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





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::ReadString
//
//  A null value reads the key's default.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::ReadString (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText)
{
    HRESULT  hr         = S_OK;
    wchar_t  text[1024] = {};
    DWORD    bytes      = (DWORD) sizeof (text);
    LSTATUS  status     = RegGetValueW (root, key.c_str(), value, RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, text, &bytes);



    outText.clear();

    CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));

    outText = text;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::HasValue
//
//  A null value tests for the key itself.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::HasValue (const std::wstring & key, const wchar_t * value)
{
    HKEY     opened = nullptr;
    LSTATUS  status = RegOpenKeyExW (HKEY_CLASSES_ROOT, key.c_str(), 0, KEY_READ, &opened);
    bool     found  = status == ERROR_SUCCESS;



    if (found && value != nullptr)
    {
        status = RegQueryValueExW (opened, value, nullptr, nullptr, nullptr, nullptr);
        found  = status == ERROR_SUCCESS;
    }

    if (opened != nullptr)
    {
        RegCloseKey (opened);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Invoke
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::Invoke (HWND owner, const Entry & entry, const std::vector<std::wstring> & paths)
{
    HRESULT                                    hr = S_OK;
    Microsoft::WRL::ComPtr<IShellItemArray>    items;



    UNREFERENCED_PARAMETER (owner);

    CBREx (entry.command != nullptr, E_INVALIDARG);

    hr = MakeItems (paths, &items);
    CHR (hr);

    hr = entry.command->Invoke (items.Get(), nullptr);
    CHR (hr);

Error:
    return hr;
}
