#include "Pch.h"

#include "Seams/Win32PackagedCommands.h"
#include "Core/ThreadName.h"

#pragma comment(lib, "xmllite.lib")


static constexpr const wchar_t *  s_kpszPackagesKey  = L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
static constexpr const wchar_t *  s_kpszManifestName = L"AppxManifest.xml";
static constexpr const wchar_t *  s_kpszCloudItemType = L"cloud";
static constexpr std::wstring_view  s_kMsResourceScheme = L"ms-resource:";





////////////////////////////////////////////////////////////////////////////////
//
//  CommandSite
//
//  What a command asks its host for: the window to own what it shows.
//
////////////////////////////////////////////////////////////////////////////////

class CommandSite : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IServiceProvider, IOleWindow>
{
public:
    explicit CommandSite (HWND owner) : m_owner (owner) {}

    IFACEMETHODIMP QueryService (REFGUID, REFIID riid, void ** ppv) override
    {
        *ppv = nullptr;

        return (riid == __uuidof (IOleWindow) || riid == __uuidof (IServiceProvider)) ? QueryInterface (riid, ppv) : E_NOINTERFACE;
    }

    IFACEMETHODIMP GetWindow (HWND * outWindow) override { *outWindow = m_owner; return (m_owner != nullptr) ? S_OK : E_FAIL; }
    IFACEMETHODIMP ContextSensitiveHelp (BOOL) override { return E_NOTIMPL; }

private:
    HWND  m_owner = nullptr;
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

void Win32PackagedCommands::Start (HWND notify, UINT assocMessage)
{
    if (m_reader.joinable())
    {
        return;
    }

    m_reader =std::thread ([this]()
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

    m_watcher.Start (notify, assocMessage, [this]() { ReadAll(); });
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

    //  File Explorer lists apps' commands by the app's name, the one on its
    //  tile: "Photos" for the package displayed as "Microsoft Photos".
    std::stable_sort (found.begin(), found.end(), [] (const Registration & a, const Registration & b)
    {
        const std::wstring &  nameA = !a.sortName.empty() ? a.sortName : a.appName.empty() ? a.package : a.appName;
        const std::wstring &  nameB = !b.sortName.empty() ? b.sortName : b.appName.empty() ? b.package : b.appName;

        return _wcsicmp (nameA.c_str(), nameB.c_str()) < 0;
    });

    {
        std::lock_guard<std::mutex>  guard (m_lock);

        m_registrations = found;
        m_read          = true;
        m_generation++;
    }

    PrepareCommands (found);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::PrepareCommands
//
//  Create each command once and release it, which loads its DLL or starts
//  its server, so the first menu that lists it opens quickly.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::PrepareCommands (const std::vector<Registration> & registrations)
{
    std::vector<CLSID>  made;



    for (const Registration & registration : registrations)
    {
        Microsoft::WRL::ComPtr<IExplorerCommand>  command;
        HRESULT                                   hr = S_OK;

        if (std::any_of (made.begin(), made.end(), [&registration] (const CLSID & clsid) { return IsEqualCLSID (clsid, registration.clsid); }))
        {
            continue;
        }

        made.push_back (registration.clsid);

        hr = CoCreateInstance (registration.clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));
        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Refresh
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::Refresh()
{
    m_watcher.Refresh();
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
        CBR (local != nullptr);

        element   = node == XmlNodeType_Element;
        inAppName = element && inProperties && wcscmp (local, L"DisplayName") == 0;

        //  The first application's small logo, the icon of an app's group, and
        //  its display name, which orders the app's commands. A name that does
        //  not resolve comes back as the package's, and is not used.
        if (element && registration.logo.empty() && wcscmp (local, L"VisualElements") == 0)
        {
            std::wstring  package = std::wstring (fullName).substr (0, std::wstring (fullName).find (L'_'));

            hr                = GetAttribute (reader.Get(), L"Square44x44Logo", value);
            registration.logo = SUCCEEDED (hr) ? FindLogoFile (fullName, value) : std::wstring();

            hr                    = GetAttribute (reader.Get(), L"DisplayName", value);
            registration.sortName = SUCCEEDED (hr) ? LoadAppName (fullName, value) : std::wstring();
            registration.sortName = (registration.sortName == package) ? std::wstring() : registration.sortName;
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

        //  A sync client's commands for what is in its sync root, as OneDrive's.
        if (wcscmp (local, L"CloudFilesContextMenus") == 0)
        {
            inMenus  = element;
            itemType = s_kpszCloudItemType;
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
    CBR (value != nullptr);

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
//  Win32PackagedCommands::IsInSyncRootOf
//
//  Whether the path is under a sync root the package's provider registered.
//  A root's key begins with its provider's name, as "OneDrive!...".
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::IsInSyncRootOf (const std::wstring & package, const std::wstring & path)
{
    static constexpr const wchar_t *  s_kpszRootsKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SyncRootManager";



    HKEY  roots = nullptr;
    bool  found = false;



    if (RegOpenKeyExW (HKEY_LOCAL_MACHINE, s_kpszRootsKey, 0, KEY_READ, &roots) != ERROR_SUCCESS)
    {
        return false;
    }

    for (DWORD index = 0; !found; index++)
    {
        wchar_t       name[256] = {};
        DWORD         length    = (DWORD) std::size (name);
        HKEY          users     = nullptr;
        LSTATUS       opened    = ERROR_SUCCESS;
        std::wstring  provider;

        if (RegEnumKeyExW (roots, index, name, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        {
            break;
        }

        provider = std::wstring (name).substr (0, std::wstring (name).find (L'!'));

        if (StrStrIW (package.c_str(), provider.c_str()) == nullptr)
        {
            continue;
        }

        opened = RegOpenKeyExW (roots, (std::wstring (name) + L"\\UserSyncRoots").c_str(), 0, KEY_READ, &users);

        if (opened != ERROR_SUCCESS || users == nullptr)
        {
            continue;
        }

        for (DWORD value = 0; !found; value++)
        {
            wchar_t  user[256]        = {};
            wchar_t  root[MAX_PATH]   = {};
            DWORD    userLength       = (DWORD) std::size (user);
            DWORD    rootBytes        = (DWORD) sizeof (root);

            if (RegEnumValueW (users, value, user, &userLength, nullptr, nullptr, (BYTE *) root, &rootBytes) != ERROR_SUCCESS)
            {
                break;
            }

            found = root[0] != L'\0' && _wcsnicmp (path.c_str(), root, wcslen (root)) == 0;
        }

        RegCloseKey (users);
    }

    RegCloseKey (roots);

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Applies
//
//  A file is matched by its extension or by "*"; a folder, the background of
//  one and a drive each by their own type. Every item must match.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::Applies (const Registration & registration, const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind)
{
    const wchar_t  * type = registration.itemType.c_str();



    //  A sync client's commands are for what is in its own sync root.
    if (registration.itemType == s_kpszCloudItemType)
    {
        return (kind == Win32ExplorerCommandEntries::ItemKind::File || kind == Win32ExplorerCommandEntries::ItemKind::Folder) && !paths.empty() && IsInSyncRootOf (registration.package, paths[0]);
    }

    switch (kind)
    {
        case Win32ExplorerCommandEntries::ItemKind::Folder:      return _wcsicmp (type, L"Directory") == 0 || _wcsicmp (type, L"Folder") == 0;
        case Win32ExplorerCommandEntries::ItemKind::Background:  return _wcsicmp (type, L"Directory\\Background") == 0;
        case Win32ExplorerCommandEntries::ItemKind::Drive:       return _wcsicmp (type, L"Drive") == 0;
        case Win32ExplorerCommandEntries::ItemKind::File:
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
//  Win32PackagedCommands::GetEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Win32ExplorerCommandEntries::Entry> Win32PackagedCommands::GetEntries (const std::vector<std::wstring> & paths, Win32ExplorerCommandEntries::ItemKind kind)
{
    std::vector<Win32ExplorerCommandEntries::Entry>  entries;
    std::vector<Registration>                        registrations;
    Microsoft::WRL::ComPtr<IShellItemArray>          items;
    HRESULT                                          hr            = S_OK;



    {
        std::lock_guard<std::mutex>  guard (m_lock);

        BAIL_OUT_IF (!m_read, S_OK);
        registrations = m_registrations;

        //  A new read of the manifests releases the commands made from the old one.
        if (m_commandsGeneration != m_generation)
        {
            m_commands.clear();
            m_commandsGeneration = m_generation;
        }
    }

    BAIL_OUT_IF (paths.empty() && kind != Win32ExplorerCommandEntries::ItemKind::Background, S_OK);

    if (!paths.empty())
    {
        hr = Win32ExplorerCommandEntries::MakeItems (m_itemIds.empty() ? paths : m_itemIds, &items);
        CHR (hr);
    }

    //  A package's registrations are read together, so each package's are a
    //  run here.
    for (size_t first = 0, next = 0; first < registrations.size(); first = next)
    {
        std::vector<Win32ExplorerCommandEntries::Entry>  own;
        size_t                                           applies = 0;

        for (next = first; next < registrations.size() && registrations[next].package == registrations[first].package; next++)
        {
            Microsoft::WRL::ComPtr<IExplorerCommand>  command;
            HRESULT                                   hrMade = S_OK;

            if (!Applies (registrations[next], paths, kind))
            {
                continue;
            }

            applies++;
            hrMade = GetCommand (registrations[next].clsid, &command);

            if (SUCCEEDED (hrMade))
            {
                Win32ExplorerCommandEntries::AddEntry (command.Get(), items.Get(), 0, L"", L"", own);
            }
        }

        AddPackageEntries (registrations[first], applies, own, entries);
    }

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::GetCommand
//
//  Reuse the command made for an earlier menu. A local server's command
//  stops working when its server exits, so a disconnected one is made again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::GetCommand (const CLSID & clsid, IExplorerCommand ** outCommand)
{
    HRESULT                                   hr      = S_OK;
    EXPCMDFLAGS                               flags   = ECF_DEFAULT;
    Microsoft::WRL::ComPtr<IExplorerCommand>  command;
    auto                                      cached  = std::find_if (m_commands.begin(), m_commands.end(), [&clsid] (const CachedCommand & entry)
    {
        return IsEqualCLSID (entry.clsid, clsid);
    });



    *outCommand = nullptr;

    if (cached != m_commands.end() && IsDisconnected (cached->command->GetFlags (&flags)))
    {
        m_commands.erase (cached);
        cached = m_commands.end();
    }

    if (cached != m_commands.end())
    {
        command = cached->command;
    }
    else
    {
        hr = CoCreateInstance (clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));
        CHR (hr);

        m_commands.push_back ({ clsid, command });
    }

    *outCommand = command.Detach();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::IsDisconnected
//
////////////////////////////////////////////////////////////////////////////////

bool Win32PackagedCommands::IsDisconnected (HRESULT hr)
{
    return hr == RPC_E_DISCONNECTED || hr == CO_E_OBJNOTCONNECTED || hr == RPC_E_SERVER_DIED || hr == RPC_E_SERVER_DIED_DNE ||
           hr == HRESULT_FROM_WIN32 (RPC_S_SERVER_UNAVAILABLE) || hr == HRESULT_FROM_WIN32 (RPC_S_CALL_FAILED);
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

void Win32PackagedCommands::AddPackageEntries (const Registration & package, size_t applies, std::vector<Win32ExplorerCommandEntries::Entry> & own, std::vector<Win32ExplorerCommandEntries::Entry> & outEntries)
{
    Win32ExplorerCommandEntries::Entry  group;



    if (own.empty())
    {
        return;
    }

    if (applies < 2 || package.appName.empty())
    {
        std::move (own.begin(), own.end(), std::back_inserter (outEntries));
        return;
    }

    group.cloud    = package.itemType == s_kpszCloudItemType;
    group.title    = package.appName;
    group.icon     = package.logo.empty() ? own.front().icon : package.logo;
    group.children = std::move (own);

    outEntries.push_back (std::move (group));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::Invoke
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::Invoke (HWND owner, const Win32ExplorerCommandEntries::Entry & entry, const std::vector<std::wstring> & paths)
{
    HRESULT                                       hr       = S_OK;
    Microsoft::WRL::ComPtr<IShellItemArray>       items;
    Microsoft::WRL::ComPtr<IObjectWithSite>       sited;
    Microsoft::WRL::ComPtr<IObjectWithSelection>  selected;
    Microsoft::WRL::ComPtr<CommandSite>           site     = Microsoft::WRL::Make<CommandSite> (owner);
    HRESULT                                       hrSite   = S_OK;



    CBREx (entry.command != nullptr, E_INVALIDARG);

    hr = Win32ExplorerCommandEntries::MakeItems (paths, &items);
    CHR (hr);

    //  As File Explorer hosts a command: some look for their window through the
    //  site, or take the items from the selection.
    hrSite = entry.command.As (&selected);

    if (SUCCEEDED (hrSite))
    {
        hrSite = selected->SetSelection (items.Get());
        IGNORE_RETURN_VALUE (hrSite, S_OK);
    }

    hrSite = entry.command.As (&sited);

    if (SUCCEEDED (hrSite) && site != nullptr)
    {
        hrSite = SetSiteGuarded (sited.Get(), static_cast<IServiceProvider *> (site.Get()));
    }

    hr = SUCCEEDED (hrSite) || hrSite == E_NOINTERFACE ? InvokeGuarded (entry.command.Get(), items.Get()) : hrSite;

    if (SUCCEEDED (hrSite) && sited != nullptr)
    {
        hrSite = SetSiteGuarded (sited.Get(), nullptr);
        IGNORE_RETURN_VALUE (hrSite, S_OK);
    }

    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::SetSiteGuarded
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::SetSiteGuarded (IObjectWithSite * sited, IUnknown * site)
{
    __try
    {
        return sited->SetSite (site);
    }

    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return E_UNEXPECTED;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::InvokeGuarded
//
//  A command is another program's code in this process. One that faults, as
//  Windows' Send to my phone has, fails rather than closing every tab.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::InvokeGuarded (IExplorerCommand * command, IShellItemArray * items)
{
    __try
    {
        return command->Invoke (items, nullptr);
    }

    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return E_UNEXPECTED;
    }
}
