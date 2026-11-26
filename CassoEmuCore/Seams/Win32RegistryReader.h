#pragma once

#include "Pch.h"

#include "Seams/IRegistryReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32RegistryReader
//
//  IRegistryReader over the Windows registry.
//
////////////////////////////////////////////////////////////////////////////////

class Win32RegistryReader : public IRegistryReader
{
public:
    HRESULT  ReadString (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText) const override;
    bool     HasValue   (HKEY root, const std::wstring & key, const wchar_t * value) const override;
};
