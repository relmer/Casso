#pragma once

#include "Pch.h"

class UserConfigStore;
class IFileSystem;


////////////////////////////////////////////////////////////////////////////////
//
//  DiskSettings
//
//  Per-machine remembered disk-mount paths. Backed by the per-machine
//  $cassoUiPrefs.disk1Path / disk2Path keys in the unified prefs JSON
//  via UserConfigStore. Paths are stored exe-relative when possible
//  (PathResolver::MakeExeRelativePath) so the casso.exe + Disks/ tree
//  stays portable across moves.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSettings
{
public:

    static HRESULT  ReadSavedDiskPath  (UserConfigStore     & store,
                                        IFileSystem         & fs,
                                        int                   drive,
                                        const std::wstring  & machineName,
                                        std::wstring        & outPath);

    static HRESULT  WriteSavedDiskPath (UserConfigStore     & store,
                                        IFileSystem         & fs,
                                        int                   drive,
                                        const std::wstring  & machineName,
                                        const std::wstring  & path);
};
