#pragma once

#include "Pch.h"

class UserConfigStore;
class IFileSystem;
class JsonValue;


////////////////////////////////////////////////////////////////////////////////
//
//  DiskSettings
//
//  Per-machine remembered state, backed by the per-machine $cassoUiPrefs
//  block in the unified prefs JSON via UserConfigStore: the disk1Path /
//  disk2Path mount paths (stored exe-relative via
//  PathResolver::MakeExeRelativePath so the casso.exe + Disks/ tree stays
//  portable), plus arbitrary boolean UI flags such as the //c case-switch
//  latches (eightyColumnSwitch / keyboardDvorak).
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

    // Persist a single boolean under $cassoUiPrefs.<key> for one machine.
    // Used for the //c case-switch latches so they survive across runs.
    static HRESULT  WriteSavedUiPrefBool (UserConfigStore    & store,
                                          IFileSystem        & fs,
                                          const std::string  & key,
                                          const std::wstring & machineName,
                                          bool                 value);

private:
    static HRESULT       LoadMachineDefaultJson (const std::wstring & machineName, JsonValue & outDefault);
    static std::string   WideToUtf8             (const std::wstring & w);
    static std::wstring  Utf8ToWide             (const std::string & s);
};
