#pragma once

#include "Pch.h"

#include "Update/ReleaseVersion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IUpdateFileSystem
//
//  The file operations a zip update performs on the install folder, behind
//  one seam so a test can fail any single step and check what is put back.
//  Paths are absolute. RenameFile never replaces an existing file. The
//  names avoid DeleteFile and MoveFile, which windows.h turns into macros.
//
////////////////////////////////////////////////////////////////////////////////

class IUpdateFileSystem
{
public:
    virtual ~IUpdateFileSystem() = default;

    virtual bool    Exists              (const std::wstring & path) = 0;
    virtual HRESULT CreateDirectoryTree (const std::wstring & path) = 0;
    virtual HRESULT WriteAllBytes       (const std::wstring & path, std::span<const Byte> bytes) = 0;
    virtual HRESULT RemoveFile          (const std::wstring & path) = 0;
    virtual HRESULT RenameFile          (const std::wstring & from, const std::wstring & to) = 0;
    virtual HRESULT RemoveDirectoryTree (const std::wstring & path) = 0;
    virtual HRESULT GetFileVersion      (const std::wstring & path, ReleaseVersion & outVersion) = 0;
};
