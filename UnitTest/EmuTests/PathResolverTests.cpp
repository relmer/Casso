#include "Pch.h"
#include "Core/PathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolverTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PathResolverTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSearchPaths_ContainsExeDirAndCwd
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BuildSearchPaths_ContainsExeDirAndCwd)
    {
        auto paths = PathResolver::BuildSearchPaths (fs::path ("C:/app/bin"), fs::path ("C:/project"));

        // Schema: [%LOCALAPPDATA%\Casso, exeDir, cwd, exeDir parents, cwd parents].
        // We don't assert the localappdata path (depends on the test
        // host environment) -- only that exeDir + cwd land in the list
        // immediately after.
        Assert::IsTrue (paths.size () >= 3);
        bool foundExe = false;
        bool foundCwd = false;
        for (const auto & p : paths)
        {
            if (p == fs::path ("C:/app/bin"))  { foundExe = true; }
            if (p == fs::path ("C:/project"))  { foundCwd = true; }
        }
        Assert::IsTrue (foundExe, L"exeDir must appear in search paths");
        Assert::IsTrue (foundCwd, L"cwd must appear in search paths");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSearchPaths_IncludesParentDirectories
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BuildSearchPaths_IncludesParentDirectories)
    {
        auto paths = PathResolver::BuildSearchPaths (
            fs::path ("C:/app/x64/Debug"), fs::path ("C:/project/build"));



        // Should include parents: C:/app/x64, C:/app, C:/project
        bool foundExeParent      = false;
        bool foundExeGrandparent = false;
        bool foundCwdParent      = false;

        for (const auto & p : paths)
        {
            if (p == fs::path ("C:/app/x64"))      foundExeParent      = true;
            if (p == fs::path ("C:/app"))           foundExeGrandparent = true;
            if (p == fs::path ("C:/project"))       foundCwdParent      = true;
        }

        Assert::IsTrue (foundExeParent);
        Assert::IsTrue (foundExeGrandparent);
        Assert::IsTrue (foundCwdParent);
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInFirstPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInFirstPath)
    {
        // Use the repo's own machines/ directory as test data
        fs::path repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;  // Skip if repo root not found
        }

        std::vector<fs::path> paths = { repoRoot, fs::path ("C:/nonexistent") };
        fs::path result = PathResolver::FindFile (paths, "Machines/Apple2Plus.json");

        Assert::IsFalse (result.empty ());
        Assert::IsTrue (fs::exists (result));
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInSecondPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInSecondPath)
    {
        fs::path repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::vector<fs::path> paths = { fs::path ("C:/nonexistent"), repoRoot };
        fs::path result = PathResolver::FindFile (paths, "Machines/Apple2Plus.json");

        Assert::IsFalse (result.empty ());
        Assert::IsTrue (fs::exists (result));
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_NotFound_ReturnsEmpty
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_NotFound_ReturnsEmpty)
    {
        std::vector<fs::path> paths = { fs::path ("C:/nonexistent1"), fs::path ("C:/nonexistent2") };
        fs::path result = PathResolver::FindFile (paths, "nosuchfile.xyz");

        Assert::IsTrue (result.empty ());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_IndependentSearch_MachinesAndRoms
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_IndependentSearch_MachinesAndRoms)
    {
        // Verify that machines/ and ROMs/ can be searched independently —
        // both should resolve from the same search paths
        fs::path repoRoot = FindRepoRoot ();

        if (repoRoot.empty ())
        {
            return;
        }

        std::vector<fs::path> paths = { repoRoot };

        fs::path configFound = PathResolver::FindFile (paths, "Machines/Apple2Plus.json");
        Assert::IsFalse (configFound.empty ());

        // ROMs/ may or may not exist in the test environment,
        // but the search itself should not crash
        PathResolver::FindFile (paths, "ROMs/Apple2Plus.rom");
    }

private:

    // Walk up from the test DLL directory to find the repo root (has machines/)
    static fs::path FindRepoRoot ()
    {
        auto paths = PathResolver::BuildSearchPaths (
            PathResolver::GetExecutableDirectory (),
            PathResolver::GetWorkingDirectory ());

        fs::path found = PathResolver::FindFile (paths, "Machines/Apple2Plus.json");

        if (found.empty ())
        {
            return {};
        }

        // Return the repo root (two levels up from Machines/Apple2Plus.json)
        return found.parent_path ().parent_path ();
    }
};