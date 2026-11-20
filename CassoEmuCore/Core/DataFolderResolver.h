#pragma once

#include "Pch.h"

#include "Update/InstallTypeDetector.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderRule
//
//  Which rule of the data folder contract produced a result.
//
////////////////////////////////////////////////////////////////////////////////

enum class DataFolderRule
{
    ProfileOverride,
    Official,
    RepoRoot,
    BesideModule,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderInputs
//
//  Everything the data folder choice depends on, supplied by the caller so the
//  choice itself touches no environment variable, registry key or file.
//
//  profileOverride  CASSO_PROFILE_DIR, empty when it is not set.
//  installType      Unknown counts as Developer.
//  modulePath       Absolute path of the module that contains the resolver.
//  localAppData     The user's local application data folder. Empty when every
//                   way of finding it failed.
//  hasGitEntry      Indicates whether a folder contains a .git file or folder.
//
////////////////////////////////////////////////////////////////////////////////

struct DataFolderInputs
{
    std::wstring                           profileOverride;
    InstallType                            installType  = InstallType::Unknown;
    fs::path                               modulePath;
    fs::path                               localAppData;
    std::function<bool (const fs::path &)> hasGitEntry;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderResult
//
//  `folder` is empty only when the Official rule applied and `localAppData`
//  was empty.
//
////////////////////////////////////////////////////////////////////////////////

struct DataFolderResult
{
    fs::path        folder;
    DataFolderRule  rule = DataFolderRule::BesideModule;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderResolver
//
//  The rules that choose where one process keeps every file it reads or
//  writes. First match wins:
//
//   1. A profile override is the folder.
//   2. An official build (Msix or Zip) uses <localAppData>\Casso.
//   3. Otherwise the nearest folder from the module's folder upward that has
//      a .git entry is the repo root, and the folder is <repo root>\CassoData.
//   4. Otherwise the folder is <module folder>\CassoData.
//
//  Pure: the answer depends only on the inputs.
//
////////////////////////////////////////////////////////////////////////////////

class DataFolderResolver
{
public:
    static DataFolderResult  Resolve (const DataFolderInputs & inputs);

    static constexpr const wchar_t * kpszOfficialFolderName  = L"Casso";
    static constexpr const wchar_t * kpszDeveloperFolderName = L"CassoData";
};
