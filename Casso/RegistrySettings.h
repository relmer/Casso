#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegistrySettings
//
//  Simple read/write of string values under HKCU\Software\relmer\Casso.
//
////////////////////////////////////////////////////////////////////////////////

class RegistrySettings
{
public:
    static HRESULT ReadString  (LPCWSTR valueName, wstring & outValue);
    static HRESULT WriteString (LPCWSTR valueName, const wstring & value);

private:
    static constexpr LPCWSTR kRegistryKeyPath = L"Software\\relmer\\Casso";
};
