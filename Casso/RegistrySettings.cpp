#include "Pch.h"

#include "RegistrySettings.h"
#include "Ehm.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BuildKeyPath
//
//  Joins the root Casso key path with an optional subkey path. An empty
//  subkey returns the root path unchanged so legacy callers stay
//  compatible.
//
////////////////////////////////////////////////////////////////////////////////

wstring RegistrySettings::BuildKeyPath (LPCWSTR subkey)
{
    wstring  path = kRegistryKeyPath;

    if (subkey != nullptr && *subkey != L'\0')
    {
        path += L"\\";
        path += subkey;
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadString
//
//  Returns S_OK if the value was read, S_FALSE if the key or value does not
//  exist (outValue is left unchanged), or an error HRESULT on failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RegistrySettings::ReadString (LPCWSTR subkey, LPCWSTR valueName, wstring & outValue)
{
    HRESULT hr      = S_OK;
    HKEY    hKey    = nullptr;
    LSTATUS lstat   = ERROR_SUCCESS;
    DWORD   dwType  = 0;
    DWORD   cbValue = 0;
    wstring keyPath = BuildKeyPath (subkey);



    lstat = RegOpenKeyExW (HKEY_CURRENT_USER, keyPath.c_str (), 0, KEY_READ, &hKey);

    BAIL_OUT_IF (lstat == ERROR_FILE_NOT_FOUND, S_FALSE);
    CBRA (lstat == ERROR_SUCCESS);

    // Query size first
    lstat = RegQueryValueExW (hKey, valueName, nullptr, &dwType, nullptr, &cbValue);

    BAIL_OUT_IF (lstat == ERROR_FILE_NOT_FOUND, S_FALSE);
    CBRA (lstat == ERROR_SUCCESS);
    CBRA (dwType == REG_SZ);
    CBRA (cbValue > 0);
    CBRA (cbValue <= 2048);

    {
        vector<wchar_t> buffer (cbValue / sizeof (wchar_t));

        lstat = RegQueryValueExW (hKey,
                                  valueName,
                                  nullptr,
                                  &dwType,
                                  reinterpret_cast<LPBYTE> (buffer.data()),
                                  &cbValue);
        CBRA (lstat == ERROR_SUCCESS);

        outValue = buffer.data();
    }


Error:
    if (hKey != nullptr)
    {
        RegCloseKey (hKey);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteString
//
//  Creates the registry key (and any missing parent subkeys) if it does
//  not exist.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RegistrySettings::WriteString (LPCWSTR subkey, LPCWSTR valueName, const wstring & value)
{
    HRESULT hr            = S_OK;
    HKEY    hKey          = nullptr;
    LSTATUS lstat         = ERROR_SUCCESS;
    DWORD   dwDisposition = 0;
    DWORD   cbValue       = 0;
    wstring keyPath       = BuildKeyPath (subkey);



    lstat = RegCreateKeyExW (HKEY_CURRENT_USER,
                             keyPath.c_str (),
                             0,
                             nullptr,
                             REG_OPTION_NON_VOLATILE,
                             KEY_WRITE,
                             nullptr,
                             &hKey,
                             &dwDisposition);
    CBRA (lstat == ERROR_SUCCESS);

    cbValue = static_cast<DWORD> ((value.length() + 1) * sizeof (wchar_t));

    lstat = RegSetValueExW (hKey,
                            valueName,
                            0,
                            REG_SZ,
                            reinterpret_cast<const BYTE *> (value.c_str()),
                            cbValue);
    CBRA (lstat == ERROR_SUCCESS);


Error:
    if (hKey != nullptr)
    {
        RegCloseKey (hKey);
    }

    return hr;
}
