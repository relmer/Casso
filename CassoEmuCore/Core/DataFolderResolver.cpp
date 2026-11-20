#include "Pch.h"

#include "Core/DataFolderResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderResolver::Resolve
//
//  Applies the rules in the order they are listed in the header. The walk for
//  the repo root starts at the module's folder and ends at the drive root, so a
//  worktree nested under a clone resolves to the worktree.
//
////////////////////////////////////////////////////////////////////////////////

DataFolderResult DataFolderResolver::Resolve (const DataFolderInputs & inputs)
{
    DataFolderResult  result;
    fs::path          moduleFolder;
    fs::path          cursor;
    fs::path          parent;
    bool              isOfficial   = false;
    bool              isWalking    = false;
    bool              hasOverride  = false;
    bool              hasGit       = false;



    moduleFolder = inputs.modulePath.parent_path();
    hasOverride  = !inputs.profileOverride.empty();
    isOfficial   = inputs.installType == InstallType::Msix ||
                   inputs.installType == InstallType::Zip;

    // Rules 3 and 4 are the fallthrough, so start from rule 4 and let the walk
    // replace it when a repo root turns up.
    result.rule   = DataFolderRule::BesideModule;
    result.folder = moduleFolder / kpszDeveloperFolderName;

    if (hasOverride)
    {
        result.rule   = DataFolderRule::ProfileOverride;
        result.folder = fs::path (inputs.profileOverride);
    }
    else if (isOfficial)
    {
        result.rule   = DataFolderRule::Official;
        result.folder = inputs.localAppData.empty()
                            ? fs::path()
                            : inputs.localAppData / kpszOfficialFolderName;
    }
    else
    {
        cursor    = moduleFolder;
        isWalking = inputs.hasGitEntry != nullptr && !cursor.empty();

        while (isWalking)
        {
            hasGit = inputs.hasGitEntry (cursor);

            if (hasGit)
            {
                result.rule   = DataFolderRule::RepoRoot;
                result.folder = cursor / kpszDeveloperFolderName;
                isWalking     = false;
            }
            else
            {
                parent    = cursor.parent_path();
                isWalking = parent != cursor && !parent.empty();
                cursor    = parent;
            }
        }
    }

    return result;
}
