#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"

class UserConfigStore;
class IFileSystem;





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

    // Persist several $cassoUiPrefs keys for one machine in a single
    // read-modify-write, for a setting that spans more than one key.
    static HRESULT  WriteSavedUiPrefs    (UserConfigStore    & store,
                                          IFileSystem        & fs,
                                          const std::wstring & machineName,
                                          const std::vector<std::pair<std::string, JsonValue>> & values);

private:
    static HRESULT       LoadMachineDefaultJson (const std::wstring & machineName, JsonValue & outDefault);
    static std::string   WideToUtf8             (const std::wstring & w);
    static std::wstring  Utf8ToWide             (const std::string & s);
};
