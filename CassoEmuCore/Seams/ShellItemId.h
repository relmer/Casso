#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemId
//
//  An id for a shell item whose parsing name is only its file name, such as a
//  cloud item on Home, which cannot be parsed back. The id contains the item's
//  ID list in hex and ends with the file name, so its extension is the item's.
//
////////////////////////////////////////////////////////////////////////////////

class ShellItemId
{
public:
    static constexpr const wchar_t *  kPrefix = L"shell-idlist:";

    static bool          IsEncoded (const std::wstring & id) { return id.starts_with (kPrefix); }
    static std::wstring  Encode    (PCIDLIST_ABSOLUTE pidl, const std::wstring & name);
    static HRESULT       ToPidl    (const std::wstring & id, PIDLIST_ABSOLUTE * outPidl);
    static HRESULT       ToItem    (const std::wstring & id, REFIID iid, void ** outItem);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemId::Encode
//
////////////////////////////////////////////////////////////////////////////////

inline std::wstring ShellItemId::Encode (PCIDLIST_ABSOLUTE pidl, const std::wstring & name)
{
    static constexpr const wchar_t *  s_kHex = L"0123456789ABCDEF";

    const BYTE *  bytes = (const BYTE *) pidl;
    UINT          size  = ILGetSize (pidl);
    std::wstring  id    = kPrefix;



    for (UINT at = 0; at < size; at++)
    {
        id += s_kHex[bytes[at] >> 4];
        id += s_kHex[bytes[at] & 0xF];
    }

    return id + L"\\" + name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemId::ToPidl
//
//  Returns an ID list to free with CoTaskMemFree, from an encoded id or any
//  other parsing name.
//
////////////////////////////////////////////////////////////////////////////////

inline HRESULT ShellItemId::ToPidl (const std::wstring & id, PIDLIST_ABSOLUTE * outPidl)
{
    size_t  start = wcslen (kPrefix);
    size_t  end   = id.find (L'\\');
    BYTE *  bytes = nullptr;



    if (!IsEncoded (id))
    {
        return SHParseDisplayName (id.c_str(), nullptr, outPidl, 0, nullptr);
    }

    if (end == std::wstring::npos || (end - start) % 2 != 0 || end == start)
    {
        return HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    }

    bytes = (BYTE *) CoTaskMemAlloc ((end - start) / 2);

    if (bytes == nullptr)
    {
        return E_OUTOFMEMORY;
    }

    for (size_t at = start; at < end; at += 2)
    {
        bytes[(at - start) / 2] = (BYTE) std::stoi (id.substr (at, 2), nullptr, 16);
    }

    *outPidl = (PIDLIST_ABSOLUTE) bytes;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellItemId::ToItem
//
////////////////////////////////////////////////////////////////////////////////

inline HRESULT ShellItemId::ToItem (const std::wstring & id, REFIID iid, void ** outItem)
{
    PIDLIST_ABSOLUTE  pidl = nullptr;
    HRESULT           hr   = S_OK;



    if (!IsEncoded (id))
    {
        return SHCreateItemFromParsingName (id.c_str(), nullptr, iid, outItem);
    }

    hr = ToPidl (id, &pidl);

    if (SUCCEEDED (hr))
    {
        hr = SHCreateItemFromIDList (pidl, iid, outItem);
        CoTaskMemFree (pidl);
    }

    return hr;
}
