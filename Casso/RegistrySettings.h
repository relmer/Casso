#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RegistrySettings
//
//  Read/write of REG_SZ values under HKCU\Software\relmer\Casso[\subkey].
//  The optional subkey lets callers carve out per-feature namespaces
//  (e.g. L"Machines\\apple2e" for per-machine UI state). Subkeys are
//  created on first write; missing keys/values return S_FALSE on read.
//
////////////////////////////////////////////////////////////////////////////////

class RegistrySettings
{
public:
    // Subkey-relative variants. Pass an empty subkey to read/write
    // directly under the root Casso key.
    static HRESULT ReadString  (LPCWSTR subkey, LPCWSTR valueName, wstring & outValue);
    static HRESULT WriteString (LPCWSTR subkey, LPCWSTR valueName, const wstring & value);

    // Legacy root-level convenience (subkey = empty).
    static HRESULT ReadString  (LPCWSTR valueName, wstring & outValue)
    {
        return ReadString (L"", valueName, outValue);
    }
    static HRESULT WriteString (LPCWSTR valueName, const wstring & value)
    {
        return WriteString (L"", valueName, value);
    }

private:
    static constexpr LPCWSTR kRegistryKeyPath = L"Software\\relmer\\Casso";

    static wstring BuildKeyPath (LPCWSTR subkey);
};
