#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IUserClasses
//
//  The current user's file types, as Windows keeps them under
//  HKEY_CURRENT_USER\Software\Classes: keys by path below that root, each
//  with string values, the unnamed one its default.
//
//  A SEAM BECAUSE A TEST MAY NOT CHANGE THE USER'S FILE TYPES. Registering and
//  unregistering are tested against a fake that holds the keys in memory.
//
////////////////////////////////////////////////////////////////////////////////

class IUserClasses
{
public:
    virtual ~IUserClasses () = default;

    //  False when the key or the value is absent. An empty name is the key's
    //  default value.
    virtual bool     GetValue    (const std::wstring & key, const std::wstring & name, std::wstring & outValue) const = 0;
    virtual HRESULT  SetValue    (const std::wstring & key, const std::wstring & name, const std::wstring & value) = 0;
    virtual HRESULT  DeleteValue (const std::wstring & key, const std::wstring & name) = 0;

    //  The key and everything below it.
    virtual HRESULT  DeleteKey   (const std::wstring & key) = 0;

    //  True when the key is absent or holds no values and no subkeys.
    virtual bool     IsEmpty     (const std::wstring & key) const = 0;

    //  Tells File Explorer the types changed, so it redraws their icons.
    virtual void     NotifyChanged () = 0;
};
