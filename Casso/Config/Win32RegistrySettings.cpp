#include "Pch.h"

#include "Win32RegistrySettings.h"

#include "../RegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistrySettings::ReadString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RegistrySettings::ReadString (
    LPCWSTR              subkey,
    LPCWSTR              valueName,
    std::wstring       & outValue)
{
    return RegistrySettings::ReadString (subkey, valueName, outValue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistrySettings::WriteString
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RegistrySettings::WriteString (
    LPCWSTR              subkey,
    LPCWSTR              valueName,
    const std::wstring & value)
{
    return RegistrySettings::WriteString (subkey, valueName, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistrySettings::ReadDword
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RegistrySettings::ReadDword (
    LPCWSTR              subkey,
    LPCWSTR              valueName,
    DWORD              & outValue)
{
    return RegistrySettings::ReadDword (subkey, valueName, outValue);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistrySettings::WriteDword
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32RegistrySettings::WriteDword (
    LPCWSTR              subkey,
    LPCWSTR              valueName,
    DWORD                value)
{
    return RegistrySettings::WriteDword (subkey, valueName, value);
}
