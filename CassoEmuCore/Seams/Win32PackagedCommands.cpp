#include "Pch.h"

#include "Seams/Win32PackagedCommands.h"
#include "Core/ThreadName.h"

#pragma comment(lib, "xmllite.lib")


static constexpr const wchar_t *  s_kpszPackagesKey  = L"Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\Repository\\Packages";
static constexpr const wchar_t *  s_kpszManifestName = L"AppxManifest.xml";
static constexpr const wchar_t *  s_kpszWindowsApps  = L"WindowsApps";





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
//  AppModel package repository key. A package's files are in its root
//  folder, or in its folder under WindowsApps when the key has no root.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::ReadAll()
{
    std::vector<Registration>  found;
    HKEY                       packages       = nullptr;
    PWSTR                      programFiles   = nullptr;
    std::wstring               windowsApps;
    wchar_t                    name[512]      = {};
    wchar_t                    root[MAX_PATH] = {};
    DWORD                      length         = 0;
    DWORD                      rootBytes      = 0;
    LSTATUS                    status         = ERROR_SUCCESS;
    HRESULT                    hr             = SHGetKnownFolderPath (FOLDERID_ProgramFiles, 0, nullptr, &programFiles);



    if (SUCCEEDED (hr))
    {
        windowsApps = std::wstring (programFiles) + L"\\" + s_kpszWindowsApps + L"\\";
    }

    CoTaskMemFree (programFiles);

    status = RegOpenKeyExW (HKEY_CLASSES_ROOT, s_kpszPackagesKey, 0, KEY_READ, &packages);

    for (DWORD index = 0; status == ERROR_SUCCESS; index++)
    {
        Microsoft::WRL::ComPtr<IStream>  manifest;
        std::wstring                     folder;
        LSTATUS                          rootStatus = ERROR_SUCCESS;

        length = (DWORD) std::size (name);
        status = RegEnumKeyExW (packages, index, name, &length, nullptr, nullptr, nullptr, nullptr);

        if (status != ERROR_SUCCESS)
        {
            break;
        }

        rootBytes  = (DWORD) sizeof (root);
        rootStatus = RegGetValueW (packages, name, L"PackageRootFolder", RRF_RT_REG_SZ, nullptr, root, &rootBytes);
        folder     = (rootStatus == ERROR_SUCCESS) ? std::wstring (root) : windowsApps + name;

        hr = OpenManifest (folder, &manifest);

        if (SUCCEEDED (hr))
        {
            ReadManifest (manifest.Get(), found);
        }
    }

    if (packages != nullptr)
    {
        RegCloseKey (packages);
    }

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
//  Open the manifest file in the folder, or else the manifest inside a
//  package file there. A sparse package, such as VS Code's, has only its
//  .appx file in that folder.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::OpenManifest (const std::wstring & folder, IStream ** outStream)
{
    HRESULT                                       hr         = S_OK;
    std::wstring                                  manifest   = folder + L"\\" + s_kpszManifestName;
    bool                                          hasPackage = false;
    std::error_code                               error;
    Microsoft::WRL::ComPtr<IAppxFactory>          factory;
    Microsoft::WRL::ComPtr<IStream>               file;
    Microsoft::WRL::ComPtr<IAppxPackageReader>    package;
    Microsoft::WRL::ComPtr<IAppxManifestReader>   reader;
    fs::path                                      packageFile;



    *outStream = nullptr;

    hr = SHCreateStreamOnFileEx (manifest.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, outStream);
    BAIL_OUT_IF (SUCCEEDED (hr), S_OK);

    for (const fs::directory_entry & entry : fs::directory_iterator (folder, error))
    {
        std::wstring  extension = entry.path().extension().wstring();

        if (_wcsicmp (extension.c_str(), L".appx") == 0 || _wcsicmp (extension.c_str(), L".msix") == 0)
        {
            packageFile = entry.path();
            hasPackage  = true;
            break;
        }
    }

    CBREx (hasPackage, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = CoCreateInstance (__uuidof (AppxFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&factory));
    CHR (hr);

    hr = SHCreateStreamOnFileEx (packageFile.c_str(), STGM_READ | STGM_SHARE_DENY_NONE, FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &file);
    CHR (hr);

    hr = factory->CreatePackageReader (file.Get(), &package);
    CHR (hr);

    hr = package->GetManifest (&reader);
    CHR (hr);

    hr = reader->GetStream (outStream);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::ReadManifest
//
//  Collect each Verb's class under an ItemType, inside a
//  FileExplorerContextMenus extension, with any namespace prefix.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32PackagedCommands::ReadManifest (IStream * stream, std::vector<Registration> & outRegistrations)
{
    HRESULT                             hr      = S_OK;
    Microsoft::WRL::ComPtr<IXmlReader>  reader;
    XmlNodeType                         node    = XmlNodeType_None;
    const wchar_t                     * local   = nullptr;
    bool                                inMenus = false;
    std::wstring                        itemType;
    std::wstring                        value;
    Registration                        registration;



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

        if (node != XmlNodeType_Element && node != XmlNodeType_EndElement)
        {
            continue;
        }

        hr = reader->GetLocalName (&local, nullptr);
        CHR (hr);

        element = node == XmlNodeType_Element;

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

    for (const Registration & registration : registrations)
    {
        Microsoft::WRL::ComPtr<IExplorerCommand>  command;
        HRESULT                                   hrMade = S_OK;

        if (!Applies (registration, paths, kind))
        {
            continue;
        }

        hrMade = CoCreateInstance (registration.clsid, nullptr, CLSCTX_LOCAL_SERVER | CLSCTX_INPROC_SERVER, IID_PPV_ARGS (&command));

        if (SUCCEEDED (hrMade))
        {
            AddEntry (command.Get(), items.Get(), 0, entries);
        }
    }

Error:
    return entries;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PackagedCommands::AddEntry
//
//  Skip a command hidden for these items and gray a disabled one. A command
//  with subcommands becomes a submenu of them.
//
////////////////////////////////////////////////////////////////////////////////

void Win32PackagedCommands::AddEntry (IExplorerCommand * command, IShellItemArray * items, int depth, std::vector<Entry> & outEntries)
{
    EXPCMDSTATE                                    state  = ECS_ENABLED;
    EXPCMDFLAGS                                    flags  = ECF_DEFAULT;
    PWSTR                                          title  = nullptr;
    PWSTR                                          icon   = nullptr;
    Entry                                          entry;
    Microsoft::WRL::ComPtr<IEnumExplorerCommand>   children;
    HRESULT                                        hr     = S_OK;



    hr = command->GetState (items, FALSE, &state);
    state = SUCCEEDED (hr) ? state : ECS_ENABLED;

    hr = command->GetFlags (&flags);
    flags = SUCCEEDED (hr) ? flags : ECF_DEFAULT;

    if ((state & ECS_HIDDEN) != 0 || (flags & ECF_ISSEPARATOR) != 0)
    {
        return;
    }

    hr = command->GetTitle (items, &title);

    if (FAILED (hr) || title == nullptr || title[0] == L'\0')
    {
        CoTaskMemFree (title);
        return;
    }

    entry.title   = title;
    entry.enabled = (state & ECS_DISABLED) == 0;
    entry.command = command;
    CoTaskMemFree (title);

    hr = command->GetIcon (items, &icon);

    if (SUCCEEDED (hr) && icon != nullptr)
    {
        entry.icon = icon;
    }

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

            AddEntry (child.Get(), items, depth + 1, entry.children);
        }
    }

    outEntries.push_back (std::move (entry));
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
