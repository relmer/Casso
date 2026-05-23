#pragma once

#include "Pch.h"

#include "IRegistrySettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistrySettings
//
//  Production `IRegistrySettings` adapter that delegates to the legacy
//  `RegistrySettings` static API. Holds no state; one instance per
//  process is enough.
//
////////////////////////////////////////////////////////////////////////////////

class Win32RegistrySettings : public IRegistrySettings
{
public:
    HRESULT ReadString  (LPCWSTR              subkey,
                         LPCWSTR              valueName,
                         std::wstring       & outValue) override;
    HRESULT WriteString (LPCWSTR              subkey,
                         LPCWSTR              valueName,
                         const std::wstring & value) override;

    HRESULT ReadDword   (LPCWSTR              subkey,
                         LPCWSTR              valueName,
                         DWORD              & outValue) override;
    HRESULT WriteDword  (LPCWSTR              subkey,
                         LPCWSTR              valueName,
                         DWORD                value) override;
};
