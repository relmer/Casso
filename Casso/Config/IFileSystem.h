#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFileSystem
//
//  Test-friendly abstraction over the small subset of file-system
//  operations Casso needs for its JSON config / preferences plumbing.
//  Per spec 007-ui-overhaul P2-T1: the interface speaks only in
//  std::wstring paths and primitive byte payloads. Win32 types
//  (HANDLE, DWORD, GetLastError state) MUST NOT appear on the
//  interface surface — the production implementation hides them in
//  Win32FileSystem.cpp; unit tests substitute InMemoryFileSystem
//  (UnitTest/UiTests/InMemoryFileSystem.h, introduced in P2-T6).
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
    virtual ~IFileSystem () = default;

    // Reads the entire file content as bytes (no encoding conversion).
    // Returns HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) if `path`
    // does not exist; other failures surface their underlying Win32
    // error wrapped via HRESULT_FROM_WIN32.
    virtual HRESULT ReadAllText (
        const std::wstring  & path,
        std::string         & outContent) = 0;

    // Atomically replaces `path` with `content`. Creates any missing
    // parent directories. Implementation must ensure that an
    // observer reading `path` at any instant sees either the prior
    // content or the new content in full — never a partial write.
    virtual HRESULT WriteAllText (
        const std::wstring  & path,
        const std::string   & content) = 0;

    // True iff `path` exists and is a regular file.
    virtual bool    Exists (const std::wstring & path) = 0;

    // Deletes `path` if it exists. Succeeds (S_OK) if the file did
    // not exist to begin with — i.e. delete is idempotent.
    virtual HRESULT Delete (const std::wstring & path) = 0;

    // Non-recursive directory listing. Returns the bare filenames
    // (no directory prefix). Subdirectories are skipped. Returns an
    // empty list and S_OK if `directory` exists but is empty;
    // returns HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND) if the
    // directory itself does not exist.
    virtual HRESULT EnumerateFiles (
        const std::wstring         & directory,
        std::vector<std::wstring>  & outFilenames) = 0;
};
