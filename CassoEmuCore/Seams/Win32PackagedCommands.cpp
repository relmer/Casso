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


//  The command store's commands that run a verb of the selection's own menu,
//  in File Explorer's order: Rotate right, then Rotate left.
static constexpr const wchar_t *  s_kpszStoreVerbs[]    = { L"Windows.rotate90", L"Windows.rotate270" };
static constexpr std::wstring_view  s_kMsResourceScheme = L"ms-resource:";

//  The shell's handler for Start's Pin and Unpin on a folder or a program.
static constexpr CLSID  s_kStartPinHandler = { 0x470C0EBD, 0x5D73, 0x4D58, { 0x9C, 0xED, 0xE9, 0x1E, 0x22, 0xE2, 0x32, 0x82 } };

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

    if (m_newReader.joinable())
    {
        m_newReader.join();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Start
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::Start (HWND notify, UINT message)
{
    if (m_reader.joinable())
    {
        return;
    }

    m_notify        = notify;
    m_notifyMessage = message;

    //  The shell's New menu handler runs in a single-threaded apartment.
    m_newReader = std::thread ([this]()
    {
        HRESULT  hrCom  = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
        HRESULT  hrName = ThreadName::SetForCurrentThread (L"New menu");



        IGNORE_RETURN_VALUE (hrName, S_OK);

        ReadNewChoices();

        if (SUCCEEDED (hrCom))
        {
            CoUninitialize();
        }
    });

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
//  Win32PackagedCommands::ReadNewChoices
//
//  The choices do not depend on the folder, so they are read against the
//  temporary folder, and nothing is created there.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::ReadNewChoices()
{
    HRESULT                                 hr                      = S_OK;
    wchar_t                                 temporary[MAX_PATH + 1] = {};
    DWORD                                   length                  = GetTempPathW ((DWORD) std::size (temporary), temporary);
    bool                                    located                 = length != 0 && length < (DWORD) std::size (temporary);
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    HMENU                                   popup                   = nullptr;
    HMENU                                   choices                 = nullptr;
    std::vector<NewChoice>                  found;
    int                                     count                   = 0;



    CBR (located);

    hr = OpenNewMenu (temporary, &menu, popup, choices);
    CHR (hr);

    count = GetMenuItemCount (choices);

    for (int i = 0; i < count; i++)
    {
        wchar_t        text[256] = {};
        MENUITEMINFOW  info      = { sizeof (info) };
        NewChoice      choice;

        info.fMask      = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        info.dwTypeData = text;
        info.cch        = (UINT) std::size (text) - 1;

        if (!GetMenuItemInfoW (choices, (UINT) i, TRUE, &info) || (info.fType & MFT_SEPARATOR) != 0)
        {
            continue;
        }

        choice.title = text;
        choice.verb  = GetMenuVerb (menu.Get(), info.wID);

        if (!choice.title.empty() && !choice.verb.empty())
        {
            found.push_back (std::move (choice));
        }
    }

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_newChoices = std::move (found);
    }

    if (m_notify != nullptr)
    {
        PostMessageW (m_notify, m_notifyMessage, 0, 0);
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
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

        //  The first application's small logo, the icon of an app's group.
        if (element && registration.logo.empty() && wcscmp (local, L"VisualElements") == 0)
        {
            hr                = GetAttribute (reader.Get(), L"Square44x44Logo", value);
            registration.logo = SUCCEEDED (hr) ? FindLogoFile (fullName, value) : std::wstring();
            continue;
        }

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
//  Win32PackagedCommands::FindLogoFile
//
//  The logo file a menu draws: the manifest's logo path has no resource
//  qualifiers, and the package has a file for each size and plate.
//  The unplated 32-pixel art first, as File Explorer's menu draws an app;
//  then the nearest sizes and scales; then the name as given. The files of
//  a sparse package are in its effective folder, not its install folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32PackagedCommands::FindLogoFile (const wchar_t * fullName, const std::wstring & logo)
{
    static constexpr const wchar_t *  s_kpszQualifiers[] =
    {
        L".altform-unplated_targetsize-32", L".altform-unplated_targetsize-48", L".altform-unplated_targetsize-24",
        L".targetsize-32", L".targetsize-48", L".targetsize-24",
        L".scale-200", L".scale-150", L".scale-100", L""
    };
    wchar_t          folder[MAX_PATH] = {};
    UINT32           length           = (UINT32) std::size (folder);
    LONG             status           = GetPackagePathByFullName2 (fullName, PackagePathType_Effective, &length, folder);
    fs::path         file             = fs::path (folder) / logo;
    std::error_code  error;



    if (status != ERROR_SUCCESS || logo.empty())
    {
        return std::wstring();
    }

    for (const wchar_t * qualifier : s_kpszQualifiers)
    {
        fs::path  candidate = file.parent_path() / (file.stem().wstring() + qualifier + file.extension().wstring());

        if (fs::exists (candidate, error))
        {
            return candidate.wstring();
        }
    }

    return std::wstring();
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

        AddPackageEntries (registrations[first], applies, own, entries);
    }

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddPackageEntries
//
//  An app with more than one command for the items has them in a submenu
//  under its name and logo, as File Explorer groups them; one with a single
//  command has it on the menu itself.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddPackageEntries (const Registration & package, size_t applies, std::vector<Entry> & own, std::vector<Entry> & outEntries)
{
    Entry  group;



    if (own.empty())
    {
        return;
    }

    if (applies < 2 || package.appName.empty())
    {
        std::move (own.begin(), own.end(), std::back_inserter (outEntries));
        return;
    }

    group.title    = package.appName;
    group.icon     = package.logo.empty() ? own.front().icon : package.logo;
    group.children = std::move (own);

    outEntries.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddEntry
//
//  Skip a command hidden for these items and gray a disabled one. After
//  E_PENDING from a quick GetState, call it again with okToBeSlow set, as
//  File Explorer does, and list the command whatever the slow answer hides,
//  as File Explorer lists Beyond Compare's. A command with no title or icon of its
//  own takes the fallbacks, and one with subcommands becomes a submenu.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddEntry (IExplorerCommand * command, IShellItemArray * items, int depth, const std::wstring & fallbackTitle,
                                      const std::wstring & fallbackIcon, std::vector<Entry> & outEntries)
{
    EXPCMDSTATE                                   state    = ECS_ENABLED;
    EXPCMDFLAGS                                   flags    = ECF_DEFAULT;
    PWSTR                                         title    = nullptr;
    PWSTR                                         icon     = nullptr;
    Entry                                         entry;
    Microsoft::WRL::ComPtr<IEnumExplorerCommand>  children;
    HRESULT                                       hr       = S_OK;
    bool                                          pending  = false;



    hr      = command->GetState (items, FALSE, &state);
    pending = hr == E_PENDING;

    if (pending)
    {
        hr = command->GetState (items, TRUE, &state);
    }

    state = SUCCEEDED (hr) ? state : ECS_ENABLED;
    state = pending ? (state & ~ECS_HIDDEN) : state;

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
//  File Explorer's Share with is left out: its handler, in ntshrui.dll,
//  faults when created outside File Explorer.
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

    //  Rotate goes before Add to Favorites, which ends Windows' own verbs.
    {
        std::vector<Entry>  rotate;
        auto                favorites = std::find_if (outEntries.begin(), outEntries.end(), [] (const Entry & known) { return _stricmp (known.verb.c_str(), "pintohomefile") == 0; });
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
//  Win32PackagedCommands::AddStoreVerb
//
//  A command store command with no handler of its own, which runs the verb
//  of its name on the selection's menu: its title and icon from the store,
//  and whether it applies from its command state handler.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::AddStoreVerb (const wchar_t * name, IShellItemArray * items, std::vector<Entry> & outEntries)
{
    HRESULT                                         hr      = S_OK;
    std::wstring                                    key     = std::wstring (s_kpszCommandStoreKey) + L"\\" + name;
    std::wstring                                    handler;
    std::wstring                                    title;
    CLSID                                           clsid   = {};
    EXPCMDSTATE                                     state   = ECS_HIDDEN;
    Microsoft::WRL::ComPtr<IExplorerCommandState>   stateHandler;
    Entry                                           entry;
    std::wstring                                    verb    = name;



    hr = ReadString (HKEY_LOCAL_MACHINE, key, L"CommandStateHandler", handler);
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

    hr = ReadString (HKEY_LOCAL_MACHINE, key, L"MUIVerb", title);
    CHR (hr);

    hr = ReadString (HKEY_LOCAL_MACHINE, key, L"Icon", entry.icon);
    IGNORE_RETURN_VALUE (hr, S_OK);

    verb               = verb.substr (verb.find (L'.') + 1);
    entry.title        = LoadIndirectString (title);
    entry.verb         = TextEncoding::WideToNarrow (verb);
    entry.explorerIcon = std::wstring (L"windows.") + verb;
    hr                 = S_OK;

    outEntries.push_back (std::move (entry));

Error:
    return hr;
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
//  Win32PackagedCommands::GetStartPinEntry
//
//  The handler is created alone, rather than with the item's whole classic
//  menu, which takes far longer. Its one row is titled Pin or Unpin by
//  whether the item is pinned; choosing it runs the row's verb through the
//  item's menu.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::GetStartPinEntry (const std::wstring & path, Entry & outEntry)
{
    HRESULT                                 hr      = S_OK;
    Microsoft::WRL::ComPtr<IShellItem>      item;
    Microsoft::WRL::ComPtr<IDataObject>     data;
    Microsoft::WRL::ComPtr<IShellExtInit>   init;
    Microsoft::WRL::ComPtr<IContextMenu>    menu;
    HMENU                                   popup   = CreatePopupMenu();
    int                                     count   = 0;
    bool                                    found   = false;
    std::wstring                            title;
    std::wstring                            verb;



    CWR (popup != nullptr);

    hr = SHCreateItemFromParsingName (path.c_str(), nullptr, IID_PPV_ARGS (&item));
    CHR (hr);

    hr = item->BindToHandler (nullptr, BHID_DataObject, IID_PPV_ARGS (&data));
    CHR (hr);

    hr = CoCreateInstance (s_kStartPinHandler, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&init));
    CHR (hr);

    hr = init->Initialize (nullptr, data.Get(), nullptr);
    CHR (hr);

    hr = init.As (&menu);
    CHR (hr);

    hr = menu->QueryContextMenu (popup, 0, s_kFirstNewCommandId, s_kLastNewCommandId, CMF_NORMAL);
    CHR (hr);

    count = GetMenuItemCount (popup);

    for (int i = 0; i < count && !found; i++)
    {
        wchar_t        text[256] = {};
        MENUITEMINFOW  info      = { sizeof (info) };

        info.fMask      = MIIM_STRING | MIIM_ID | MIIM_FTYPE;
        info.dwTypeData = text;
        info.cch        = (UINT) std::size (text) - 1;

        if (GetMenuItemInfoW (popup, (UINT) i, TRUE, &info) && (info.fType & MFT_SEPARATOR) == 0)
        {
            title = text;
            verb  = GetMenuVerb (menu.Get(), info.wID);
            found = !title.empty() && !verb.empty();
        }
    }

    if (found)
    {
        std::wstring  plain = title;

        std::erase (plain, L'&');

        outEntry              = Entry();
        outEntry.title        = title;
        outEntry.verb         = TextEncoding::WideToNarrow (verb);
        outEntry.explorerIcon = (_wcsnicmp (plain.c_str(), L"Unpin", 5) == 0) ? L"windows.unpinfromstartscreen" : L"PinToStartScreen";
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetNewChoices
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32PackagedCommands::NewChoice> Win32PackagedCommands::GetNewChoices()
{
    std::lock_guard<std::mutex>  guard (m_lock);



    return m_newChoices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::CreateNew
//
//  The new item's name comes from the handler, "New Text Document.txt" or the
//  like, so the item is found as the one entry the folder did not have
//  before. Shortcut's wizard runs in a process of its own and renames its
//  item when it finishes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::CreateNew (HWND owner, const std::wstring & folder, const std::wstring & verb, std::wstring & outPath)
{
    HRESULT                                 hr       = S_OK;
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    HMENU                                   popup    = nullptr;
    HMENU                                   choices  = nullptr;
    UINT                                    chosen   = 0;
    int                                     count    = 0;
    CMINVOKECOMMANDINFOEX                   invoke   = { sizeof (invoke) };
    std::unordered_set<std::wstring>        before;
    std::error_code                         error;



    outPath.clear();

    hr = OpenNewMenu (folder, &menu, popup, choices);
    CHR (hr);

    count = GetMenuItemCount (choices);

    for (int i = 0; i < count && chosen == 0; i++)
    {
        UINT  id = GetMenuItemID (choices, i);

        if (id != (UINT) -1 && _wcsicmp (GetMenuVerb (menu.Get(), id).c_str(), verb.c_str()) == 0)
        {
            chosen = id;
        }
    }

    CBREx (chosen != 0, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        before.insert (entry.path().filename().wstring());
    }

    invoke.fMask        = CMIC_MASK_UNICODE;
    invoke.hwnd         = owner;
    invoke.lpVerb       = MAKEINTRESOURCEA (chosen - s_kFirstNewCommandId);
    invoke.lpVerbW      = MAKEINTRESOURCEW (chosen - s_kFirstNewCommandId);
    invoke.lpDirectoryW = folder.c_str();
    invoke.nShow        = SW_SHOWNORMAL;

    hr = menu->InvokeCommand ((LPCMINVOKECOMMANDINFO) &invoke);
    CHR (hr);

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        if (outPath.empty() && !before.contains (entry.path().filename().wstring()))
        {
            outPath = entry.path().wstring();
        }
    }

Error:
    if (popup != nullptr)
    {
        DestroyMenu (popup);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::OpenNewMenu
//
//  The handler adds one row, New, and fills its submenu only as the submenu
//  opens, so the opening is sent to it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::OpenNewMenu (const std::wstring & folder, IContextMenu3 ** outMenu, HMENU & outPopup, HMENU & outChoices)
{
    HRESULT                                 hr     = S_OK;
    PIDLIST_ABSOLUTE                        pidl   = nullptr;
    Microsoft::WRL::ComPtr<IShellExtInit>   init;
    Microsoft::WRL::ComPtr<IContextMenu3>   menu;
    MENUITEMINFOW                           info   = { sizeof (info) };
    LRESULT                                 result = 0;
    bool                                    found  = false;



    *outMenu   = nullptr;
    outPopup   = nullptr;
    outChoices = nullptr;

    hr = SHParseDisplayName (folder.c_str(), nullptr, &pidl, 0, nullptr);
    CHR (hr);

    hr = CoCreateInstance (CLSID_NewMenu, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&init));
    CHR (hr);

    hr = init->Initialize (pidl, nullptr, nullptr);
    CHR (hr);

    hr = init.As (&menu);
    CHR (hr);

    outPopup = CreatePopupMenu();
    CWR (outPopup != nullptr);

    hr = menu->QueryContextMenu (outPopup, 0, s_kFirstNewCommandId, s_kLastNewCommandId, CMF_NORMAL);
    CHR (hr);

    info.fMask = MIIM_SUBMENU;
    found      = GetMenuItemInfoW (outPopup, 0, TRUE, &info) && info.hSubMenu != nullptr;

    CBREx (found, E_UNEXPECTED);

    hr = menu->HandleMenuMsg2 (WM_INITMENUPOPUP, (WPARAM) info.hSubMenu, 0, &result);
    CHR (hr);

    outChoices = info.hSubMenu;
    *outMenu   = menu.Detach();

Error:
    CoTaskMemFree (pidl);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetMenuVerb
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32PackagedCommands::GetMenuVerb (IContextMenu * menu, UINT id)
{
    wchar_t  verb[256] = {};
    HRESULT  hr        = menu->GetCommandString (id - s_kFirstNewCommandId, GCS_VERBW, nullptr, (LPSTR) verb, (UINT) std::size (verb) - 1);



    return SUCCEEDED (hr) ? std::wstring (verb) : std::wstring();
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
