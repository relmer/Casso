#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IRegistryReader
//
//  String values and keys read from the registry: a key by its path below a
//  root, and a value by its name in the key, a null name for the key's
//  default.
//
//  A SEAM BECAUSE A TEST MAY NOT READ THE REGISTRY. The verbs File Explorer
//  lists for a file type are tested against a fake that has the keys in
//  memory.
//
////////////////////////////////////////////////////////////////////////////////

class IRegistryReader
{
public:
    virtual ~IRegistryReader () = default;

    //  Fails when the key or the value is absent.
    virtual HRESULT  ReadString (HKEY root, const std::wstring & key, const wchar_t * value, std::wstring & outText) const = 0;

    //  A null value tests for the key itself.
    virtual bool     HasValue   (HKEY root, const std::wstring & key, const wchar_t * value) const = 0;
};
