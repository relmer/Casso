#pragma once

#include "Config/IFileSystem.h"

class HostAccessGuard;
enum class HostAccess;





////////////////////////////////////////////////////////////////////////////////
//
//  GuardedFileSystem
//
//  The session's file system while a debug channel client's line runs: each
//  call is checked by the client's HostAccessGuard first. One that fails
//  returns ERROR_ACCESS_DENIED, and the file system under it isn't called.
//  Exists and DirectoryExists return false for a path the client may not
//  read, and a directory is made only with write access.
//
////////////////////////////////////////////////////////////////////////////////

class GuardedFileSystem : public IFileSystem
{
public:
    GuardedFileSystem (IFileSystem & inner, HostAccessGuard & guard);

    HRESULT ReadAllText          (const std::wstring & path,
                                  std::string        & outContent) override;
    HRESULT WriteAllText         (const std::wstring & path,
                                  const std::string  & content) override;
    HRESULT AppendAllText        (const std::wstring & path,
                                  const std::string  & content) override;
    bool    Exists               (const std::wstring & path) override;
    bool    DirectoryExists      (const std::wstring & path) override;
    HRESULT MakeDirectory        (const std::wstring & path) override;
    HRESULT Delete               (const std::wstring & path) override;
    HRESULT EnumerateFiles       (const std::wstring        & directory,
                                  std::vector<std::wstring> & outFilenames) override;
    HRESULT EnumerateDirectories (const std::wstring        & directory,
                                  std::vector<std::wstring> & outDirNames) override;
    HRESULT EnumerateEntries     (const std::wstring           & directory,
                                  std::vector<FileSystemEntry> & outEntries) override;
    HRESULT GetReadOnlyAttribute (const std::wstring & path,
                                  bool               & outReadOnly) override;
    HRESULT SetReadOnlyAttribute (const std::wstring & path,
                                  bool                 readOnly) override;

private:
    HRESULT CheckAccess          (const std::wstring & path, HostAccess access);

    IFileSystem      & m_inner;
    HostAccessGuard  & m_guard;
};
