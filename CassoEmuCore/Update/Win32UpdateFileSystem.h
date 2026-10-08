#pragma once

#include "Pch.h"

#include "Update/IUpdateFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32UpdateFileSystem
//
//  IUpdateFileSystem on the real file system.
//
////////////////////////////////////////////////////////////////////////////////

class Win32UpdateFileSystem : public IUpdateFileSystem
{
public:
    bool    Exists              (const std::wstring & path) override;
    HRESULT CreateDirectoryTree (const std::wstring & path) override;
    HRESULT WriteAllBytes       (const std::wstring & path, std::span<const Byte> bytes) override;
    HRESULT RemoveFile          (const std::wstring & path) override;
    HRESULT RenameFile          (const std::wstring & from, const std::wstring & to) override;
    HRESULT RemoveDirectoryTree (const std::wstring & path) override;
    HRESULT GetFileVersion      (const std::wstring & path, ReleaseVersion & outVersion) override;
};
