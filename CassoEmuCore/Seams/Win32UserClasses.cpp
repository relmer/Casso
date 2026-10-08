#include "Pch.h"

#include "Seams/Win32UserClasses.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::GetFullKey
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Win32UserClasses::GetFullKey (const std::wstring & key)
{
    return L"Software\\Classes\\" + key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::GetValue
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UserClasses::GetValue (const std::wstring & key, const std::wstring & name, std::wstring & outValue) const
{
    DWORD    bytes  = 0;
    LSTATUS  status = RegGetValueW (HKEY_CURRENT_USER, GetFullKey (key).c_str(), name.empty() ? nullptr : name.c_str(),
                                    RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, nullptr, &bytes);



    outValue.clear();

    if (status != ERROR_SUCCESS)
    {
        return false;
    }

    outValue.resize (bytes / sizeof (wchar_t));
    status = RegGetValueW (HKEY_CURRENT_USER, GetFullKey (key).c_str(), name.empty() ? nullptr : name.c_str(),
                           RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND, nullptr, outValue.data(), &bytes);

    if (status != ERROR_SUCCESS)
    {
        outValue.clear();
        return false;
    }

    outValue.resize (wcsnlen (outValue.c_str(), outValue.size()));
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::SetValue
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UserClasses::SetValue (const std::wstring & key, const std::wstring & name, const std::wstring & value)
{
    HRESULT  hr     = S_OK;
    LSTATUS  status = RegSetKeyValueW (HKEY_CURRENT_USER, GetFullKey (key).c_str(), name.empty() ? nullptr : name.c_str(), REG_SZ,
                                       value.c_str(), (DWORD) ((value.size() + 1) * sizeof (wchar_t)));



    CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::DeleteValue
//
//  A value already absent is not a failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UserClasses::DeleteValue (const std::wstring & key, const std::wstring & name)
{
    HRESULT  hr     = S_OK;
    LSTATUS  status = RegDeleteKeyValueW (HKEY_CURRENT_USER, GetFullKey (key).c_str(), name.empty() ? nullptr : name.c_str());



    CBREx (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND, HRESULT_FROM_WIN32 (status));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::DeleteKey
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32UserClasses::DeleteKey (const std::wstring & key)
{
    HRESULT  hr     = S_OK;
    LSTATUS  status = RegDeleteTreeW (HKEY_CURRENT_USER, GetFullKey (key).c_str());



    CBREx (status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND, HRESULT_FROM_WIN32 (status));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::IsEmpty
//
////////////////////////////////////////////////////////////////////////////////

bool Win32UserClasses::IsEmpty (const std::wstring & key) const
{
    HKEY     handle  = nullptr;
    DWORD    subkeys = 0;
    DWORD    values  = 0;
    LSTATUS  status  = RegOpenKeyExW (HKEY_CURRENT_USER, GetFullKey (key).c_str(), 0, KEY_READ, &handle);



    if (status != ERROR_SUCCESS)
    {
        return true;
    }

    status = RegQueryInfoKeyW (handle, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey (handle);

    return status == ERROR_SUCCESS && subkeys == 0 && values == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses::NotifyChanged
//
////////////////////////////////////////////////////////////////////////////////

void Win32UserClasses::NotifyChanged()
{
    SHChangeNotify (SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}
