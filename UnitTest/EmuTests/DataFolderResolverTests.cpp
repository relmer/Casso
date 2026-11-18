#include "Pch.h"

#include "Core/DataFolderResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DataFolderResolverTests
//
//  One test per row of the data folder rules. The resolver is pure, so each
//  test supplies a fake set of folders that have a .git entry and reads
//  nothing from the machine running it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DataFolderResolverTests)
{
public:

    static constexpr const wchar_t * kpszLocalAppData = L"C:\\Users\\dev\\AppData\\Local";
    static constexpr const wchar_t * kpszOfficial     = L"C:\\Users\\dev\\AppData\\Local\\Casso";



    ////////////////////////////////////////////////////////////////////////////
    //
    //  MakeInputs
    //
    //  `gitFolders` lists the folders that have a .git entry, whether a folder
    //  (a clone) or a file (a worktree).
    //
    ////////////////////////////////////////////////////////////////////////////

    static DataFolderInputs MakeInputs (
        InstallType                installType,
        const wchar_t            * modulePath,
        std::vector<fs::path>      gitFolders)
    {
        DataFolderInputs  inputs;



        inputs.installType  = installType;
        inputs.modulePath   = modulePath;
        inputs.localAppData = kpszLocalAppData;
        inputs.hasGitEntry  = [gitFolders] (const fs::path & folder) -> bool
        {
            return std::find (gitFolders.begin(), gitFolders.end(), folder) != gitFolders.end();
        };

        return inputs;
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rule 1
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Resolve_ProfileOverride_IsReturnedForAnOfficialBuild)
    {
        DataFolderInputs  inputs = MakeInputs (InstallType::Zip, L"C:\\Casso\\Casso.exe", {});
        DataFolderResult  result;



        inputs.profileOverride = L"D:\\Scratch\\Profile";
        result                 = DataFolderResolver::Resolve (inputs);

        Assert::AreEqual (std::wstring (L"D:\\Scratch\\Profile"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::ProfileOverride);
    }


    TEST_METHOD (Resolve_ProfileOverride_IsReturnedForADeveloperBuild)
    {
        DataFolderInputs  inputs = MakeInputs (InstallType::Developer,
                                               L"C:\\src\\Casso\\x64\\Debug\\Casso.exe",
                                               { L"C:\\src\\Casso" });
        DataFolderResult  result;



        inputs.profileOverride = L"D:\\Scratch\\Profile";
        result                 = DataFolderResolver::Resolve (inputs);

        Assert::AreEqual (std::wstring (L"D:\\Scratch\\Profile"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::ProfileOverride);
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rule 2
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Resolve_Msix_UsesLocalAppData)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Msix, L"C:\\Program Files\\WindowsApps\\Casso\\Casso.exe", {}));



        Assert::AreEqual (std::wstring (kpszOfficial), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::Official);
    }


    TEST_METHOD (Resolve_Zip_UsesLocalAppData)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Zip, L"C:\\Tools\\Casso\\Casso.exe", {}));



        Assert::AreEqual (std::wstring (kpszOfficial), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::Official);
    }


    TEST_METHOD (Resolve_Zip_InsideARepo_StillUsesLocalAppData)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Zip, L"C:\\src\\Casso\\x64\\Release\\Casso.exe", { L"C:\\src\\Casso" }));



        Assert::AreEqual (std::wstring (kpszOfficial), result.folder.wstring());
    }


    TEST_METHOD (Resolve_Official_WithNoLocalAppData_ReturnsAnEmptyFolder)
    {
        DataFolderInputs  inputs = MakeInputs (InstallType::Zip, L"C:\\Tools\\Casso\\Casso.exe", {});
        DataFolderResult  result;



        inputs.localAppData = fs::path();
        result              = DataFolderResolver::Resolve (inputs);

        Assert::IsTrue (result.folder.empty());
        Assert::IsTrue (result.rule == DataFolderRule::Official);
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rule 3
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Resolve_Developer_InAClone_UsesTheRepoRoot)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer,
                        L"C:\\src\\Casso\\x64\\Debug\\Casso.exe",
                        { L"C:\\src\\Casso" }));



        Assert::AreEqual (std::wstring (L"C:\\src\\Casso\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::RepoRoot);
    }


    TEST_METHOD (Resolve_Unknown_InAClone_UsesTheRepoRoot)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Unknown,
                        L"C:\\src\\Casso\\x64\\Debug\\Casso.exe",
                        { L"C:\\src\\Casso" }));



        Assert::AreEqual (std::wstring (L"C:\\src\\Casso\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::RepoRoot);
    }


    TEST_METHOD (Resolve_Developer_InAWorktree_UsesTheWorktreeFolder)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer,
                        L"C:\\src\\Casso-worktrees\\feature\\x64\\Debug\\Casso.exe",
                        { L"C:\\src\\Casso-worktrees\\feature" }));



        Assert::AreEqual (std::wstring (L"C:\\src\\Casso-worktrees\\feature\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::RepoRoot);
    }


    TEST_METHOD (Resolve_Developer_ChecksTheModuleFolderItself)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer,
                        L"C:\\src\\Casso\\Casso.exe",
                        { L"C:\\src\\Casso", L"C:\\src" }));



        Assert::AreEqual (std::wstring (L"C:\\src\\Casso\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::RepoRoot);
    }


    TEST_METHOD (Resolve_Developer_NearestGitEntryWinsOverAFartherOne)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer,
                        L"C:\\src\\Casso\\wt\\feature\\x64\\Debug\\Casso.exe",
                        { L"C:\\src\\Casso", L"C:\\src\\Casso\\wt\\feature" }));



        Assert::AreEqual (std::wstring (L"C:\\src\\Casso\\wt\\feature\\CassoData"), result.folder.wstring());
    }


    TEST_METHOD (Resolve_Developer_FindsAGitEntryAtTheDriveRoot)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer,
                        L"C:\\build\\Casso.exe",
                        { L"C:\\" }));



        Assert::AreEqual (std::wstring (L"C:\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::RepoRoot);
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Rule 4
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Resolve_Developer_WithNoGitEntry_UsesTheModuleFolder)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Developer, L"E:\\Stick\\Casso\\Casso.exe", {}));



        Assert::AreEqual (std::wstring (L"E:\\Stick\\Casso\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::BesideModule);
    }


    TEST_METHOD (Resolve_Unknown_WithNoGitEntry_UsesTheModuleFolder)
    {
        DataFolderResult  result = DataFolderResolver::Resolve (
            MakeInputs (InstallType::Unknown, L"E:\\Stick\\Casso\\Casso.exe", {}));



        Assert::AreEqual (std::wstring (L"E:\\Stick\\Casso\\CassoData"), result.folder.wstring());
        Assert::IsTrue   (result.rule == DataFolderRule::BesideModule);
    }


    TEST_METHOD (Resolve_Developer_WithNoLocalAppData_StillResolves)
    {
        DataFolderInputs  inputs = MakeInputs (InstallType::Developer,
                                               L"C:\\src\\Casso\\x64\\Debug\\Casso.exe",
                                               { L"C:\\src\\Casso" });
        DataFolderResult  result;



        inputs.localAppData = fs::path();
        result              = DataFolderResolver::Resolve (inputs);

        Assert::AreEqual (std::wstring (L"C:\\src\\Casso\\CassoData"), result.folder.wstring());
    }





    ////////////////////////////////////////////////////////////////////////////
    //
    //  Guarantee: a developer build never lands in the shipped folder
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Resolve_DeveloperRules_NeverReturnTheShippedFolder)
    {
        InstallType                   types[]     = { InstallType::Unknown, InstallType::Developer };
        std::vector<fs::path>         gitSets[]   = { {}, { L"C:\\src\\Casso" } };
        DataFolderResult              result;
        std::wstring                  folder;
        std::wstring                  shipped     = kpszOfficial;
        bool                          isInside    = false;



        for (InstallType type : types)
        {
            for (const std::vector<fs::path> & gitFolders : gitSets)
            {
                result   = DataFolderResolver::Resolve (MakeInputs (type, L"C:\\src\\Casso\\x64\\Debug\\Casso.exe", gitFolders));
                folder   = result.folder.wstring();
                isInside = folder.starts_with (shipped);

                Assert::IsFalse (isInside, L"a developer build resolved under the shipped folder");
            }
        }
    }
};
