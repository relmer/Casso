#pragma once

#include "Pch.h"

#include "Seams/IUserClasses.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UserClasses
//
//  IUserClasses over the registry itself, below a root key of the current
//  user's: the file types, or File Explorer's record of each type's programs.
//
////////////////////////////////////////////////////////////////////////////////

class Win32UserClasses : public IUserClasses
{
public:
    static constexpr const wchar_t *  kClassesRoot  = L"Software\\Classes";
    static constexpr const wchar_t *  kFileExtsRoot = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts";

    explicit Win32UserClasses (std::wstring root = kClassesRoot) : m_root (std::move (root)) {}

    bool     GetValue      (const std::wstring & key, const std::wstring & name, std::wstring & outValue) const override;
    HRESULT  SetValue      (const std::wstring & key, const std::wstring & name, const std::wstring & value) override;
    HRESULT  DeleteValue   (const std::wstring & key, const std::wstring & name) override;
    HRESULT  DeleteKey     (const std::wstring & key) override;
    bool     IsEmpty       (const std::wstring & key) const override;
    void     NotifyChanged () override;

private:
    std::wstring  GetFullKey (const std::wstring & key) const;

    std::wstring  m_root;
};
