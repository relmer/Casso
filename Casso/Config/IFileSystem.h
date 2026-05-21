#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFileSystem
//
//  Test-friendly abstraction over the small subset of file-system
//  operations Casso needs for its JSON config / preferences plumbing.
//  Overhaul : the interface speaks only in
//  std::wstring paths and primitive byte payloads. Win32 types
//  (HANDLE, DWORD, GetLastError state) MUST NOT appear on the
//  interface surface — the production implementation hides them in
//  Win32FileSystem.cpp; unit tests substitute InMemoryFileSystem
//  (UnitTest/UiTests/InMemoryFileSystem.h, introduced in ).
//
//  All operations follow Casso's EHM pattern: HRESULT return, no
//  thrown exceptions. WriteAllText is atomic on success — the
//  production implementation writes to a sibling temp file and
//  performs MoveFileExW with MOVEFILE_REPLACE_EXISTING so partial
//  writes can never leave a corrupt config in place. EnumerateFiles
//  is non-recursive and reports only files (not subdirectories);
//  the caller filters by extension as needed.
//
////////////////////////////////////////////////////////////////////////////////

class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    // Reads the entire file as raw bytes.
    virtual HRESULT ReadAllText          (const std::wstring & path,
                                          std::string        & outContent) = 0;

    // Atomically replaces `path` with `content`.
    virtual HRESULT WriteAllText         (const std::wstring & path,
                                          const std::string  & content) = 0;

    // True iff `path` exists and is a regular file.
    virtual bool    Exists               (const std::wstring & path) = 0;

    // Deletes `path` if it exists.
    virtual HRESULT Delete               (const std::wstring & path) = 0;

    // Non-recursive listing of bare filenames.
    virtual HRESULT EnumerateFiles       (const std::wstring        & directory,
                                          std::vector<std::wstring> & outFilenames) = 0;

    // Non-recursive listing of bare sub-directory names.
    virtual HRESULT EnumerateDirectories (const std::wstring        & directory,
                                          std::vector<std::wstring> & outDirNames) = 0;
};
