#include "Pch.h"

#include "Seams/Win32RegistryReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistryReader::ReadString
//
//  A null value reads the key's default.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RegistryReader::ReadString (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText) const
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
//  Win32RegistryReader::HasValue
//
//  A null value tests for the key itself.
//
////////////////////////////////////////////////////////////////////////////////

bool Win32RegistryReader::HasValue (HKEY root, const std::wstring & key, const wchar_t * value) const
{
    HKEY     opened = nullptr;
    LSTATUS  status = RegOpenKeyExW (root, key.c_str(), 0, KEY_READ, &opened);
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





