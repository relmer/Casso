#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses
//
//  IUserClasses over the registry itself.
//
////////////////////////////////////////////////////////////////////////////////

class Win32UserClasses : public IUserClasses
{
public:
    bool     GetValue      (const std::wstring & key, const std::wstring & name, std::wstring & outValue) const override;
    HRESULT  SetValue      (const std::wstring & key, const std::wstring & name, const std::wstring & value) override;
    HRESULT  DeleteValue   (const std::wstring & key, const std::wstring & name) override;
    HRESULT  DeleteKey     (const std::wstring & key) override;
    bool     IsEmpty       (const std::wstring & key) const override;
    void     NotifyChanged () override;

private:
    static std::wstring  GetFullKey (const std::wstring & key);
};
