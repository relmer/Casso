#pragma once

#include "Pch.h"
#include "IFileSystem.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FileSystem
//
//  Production IFileSystem implementation. All Win32 surface
//  (CreateFileW, FindFirstFileW, MoveFileExW, GetLastError) is
//  confined to Win32FileSystem.cpp. WriteAllText is implemented
//  via a "write temp, then MoveFileExW(MOVEFILE_REPLACE_EXISTING)"
//  pattern so any reader of `path` observes either the prior file
//  or the new file in full.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FileSystem : public IFileSystem
{
public:
    Win32FileSystem() = default;

    HRESULT ReadAllText          (const std::wstring & path,
                                  std::string        & outContent) override;
    HRESULT WriteAllText         (const std::wstring & path,
                                  const std::string  & content) override;
    bool    Exists               (const std::wstring & path) override;
    HRESULT Delete               (const std::wstring & path) override;
    HRESULT EnumerateFiles       (const std::wstring        & directory,
                                  std::vector<std::wstring> & outFilenames) override;
    HRESULT EnumerateDirectories (const std::wstring        & directory,
                                  std::vector<std::wstring> & outDirNames) override;
};
