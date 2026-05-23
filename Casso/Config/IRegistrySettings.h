#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IRegistrySettings
//
//  Test-friendly abstraction over the REG_SZ / REG_DWORD subset of the
//  Windows registry that Casso actually uses. All paths are subkeys
//  beneath the project's root key (HKCU\Software\relmer\Casso). The
//  production implementation lives in Win32RegistrySettings; unit tests
//  substitute InMemoryRegistry so registry state never leaks across
//  test cases.
//
//  Read semantics mirror the legacy static RegistrySettings: S_OK on
//  success, S_FALSE when the key or value is missing (outValue left
//  untouched), or an error HRESULT on hard failure (wrong type, IO
//  error). Write semantics create the subkey on demand and return
//  S_OK on success.
//
////////////////////////////////////////////////////////////////////////////////

class IRegistrySettings
{
public:
    virtual ~IRegistrySettings () = default;

    virtual HRESULT ReadString  (LPCWSTR              subkey,
                                 LPCWSTR              valueName,
                                 std::wstring       & outValue) = 0;
    virtual HRESULT WriteString (LPCWSTR              subkey,
                                 LPCWSTR              valueName,
                                 const std::wstring & value) = 0;

    virtual HRESULT ReadDword   (LPCWSTR              subkey,
                                 LPCWSTR              valueName,
                                 DWORD              & outValue) = 0;
    virtual HRESULT WriteDword  (LPCWSTR              subkey,
                                 LPCWSTR              valueName,
                                 DWORD                value) = 0;
};
