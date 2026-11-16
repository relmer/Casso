#include "Pch.h"

#include "CassoExplorer/Model/HostFolderNames.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderNames::GetDriveRoots
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::wstring> HostFolderNames::GetDriveRoots()
{
    std::vector<std::wstring>  roots;
    wchar_t                    buffer[512] = {};
    DWORD                      length      = GetLogicalDriveStringsW (ARRAYSIZE (buffer), buffer);
    const wchar_t *            root        = buffer;



    if (length == 0 || length > ARRAYSIZE (buffer))
    {
        return roots;
    }

    while (*root != L'\0')
    {
        roots.push_back (root);
        root += wcslen (root) + 1;
    }

    return roots;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderNames::GetLocalizedFolderName
//
//  SHGetLocalizedName returns the module and string id the desktop.ini
//  refers to; the string is loaded from that module as data, never run. A
//  desktop.ini whose name is plain text, as iCloud's is, has no module, and
//  its text is the name.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFolderNames::GetLocalizedFolderName (const std::wstring & path)
{
    HRESULT       hr                 = S_OK;
    wchar_t       module[MAX_PATH]   = {};
    wchar_t       expanded[MAX_PATH] = {};
    wchar_t       text[MAX_PATH]     = {};
    int           id                 = 0;
    HMODULE       library            = nullptr;
    int           length             = 0;
    std::wstring  name;



    hr = SHGetLocalizedName (path.c_str(), module, ARRAYSIZE (module), &id);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    BAIL_OUT_IF (ExpandEnvironmentStringsW (module, expanded, ARRAYSIZE (expanded)) == 0, S_OK);

    library = LoadLibraryExW (expanded, nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    BAIL_OUT_IF (library == nullptr, S_OK);

    length = LoadStringW (library, (UINT) id, text, ARRAYSIZE (text));

    if (length > 0)
    {
        name.assign (text, (size_t) length);
    }

Error:
    if (library != nullptr)
    {
        FreeLibrary (library);
    }

    return name.empty() ? GetPlainFolderName (path) : name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HostFolderNames::GetPlainFolderName
//
//  The LocalizedResourceName of a folder's desktop.ini when it is text rather
//  than a reference to a module's string, which starts with an at sign.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring HostFolderNames::GetPlainFolderName (const std::wstring & path)
{
    wchar_t       text[MAX_PATH] = {};
    std::wstring  ini            = path + L"\\desktop.ini";
    DWORD         length         = GetPrivateProfileStringW (L".ShellClassInfo", L"LocalizedResourceName", L"", text, ARRAYSIZE (text), ini.c_str());



    return (length > 0 && text[0] != L'@') ? std::wstring (text, length) : std::wstring();
}
