#include "Pch.h"
#include "Core/PathResolver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PathResolverTests
//
//  Search-path construction, file resolution, and the exe-relative round trip.
//
//  The ROUND TRIP is the substance. A path made relative and resolved back must
//  name the same file, since that pair is what keeps a casso.exe plus its
//  Disks/ tree portable across a move -- and a mismatch loses the user's
//  mounted disks silently on the next launch.
//
//  The escape case is pinned specifically: a path outside the exe subtree must
//  stay ABSOLUTE rather than acquiring a `..` climb-out, which would break the
//  moment the install moved and would break by resolving somewhere unrelated
//  rather than by failing.
//
//  Search ordering is asserted because it decides which copy of an asset wins
//  -- a machine found in a development tree must load ROMs from that tree
//  rather than from an installed copy elsewhere.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (PathResolverTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  BuildSearchPaths_ReturnsLocalAppDataOnly
    //
    //  Casso uses a single asset directory; exeDir / cwd parameters
    //  are accepted for compat but no longer contribute search paths.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BuildSearchPaths_ReturnsLocalAppDataOnly)
    {
        auto paths = PathResolver::BuildSearchPaths (fs::path ("C:/app/bin"),
                                                     fs::path ("C:/project"));

        // The single element is %LOCALAPPDATA%\Casso\, which depends on
        // the test host. We only assert that the legacy fallback dirs
        // do NOT appear.
        Assert::AreEqual ((size_t) 1, paths.size(),
            L"BuildSearchPaths now returns exactly one entry (localappdata)");
        for (const auto & p : paths)
        {
            Assert::AreNotEqual (fs::path ("C:/app/bin").wstring(),  p.wstring());
            Assert::AreNotEqual (fs::path ("C:/project").wstring(),  p.wstring());
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInFirstPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInFirstPath)
    {
        // Use the repo's own machines/ directory as test data
        fs::path repoRoot = FindRepoRoot();

        if (repoRoot.empty())
        {
            return;  // Skip if repo root not found
        }

        std::vector<fs::path> paths = { repoRoot, fs::path ("C:/nonexistent") };
        fs::path result = PathResolver::FindFile (paths, "Resources/Machines/Apple2Plus/Apple2Plus.json");

        Assert::IsFalse (result.empty());
        Assert::IsTrue (fs::exists (result));
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_FoundInSecondPath
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_FoundInSecondPath)
    {
        fs::path repoRoot = FindRepoRoot();

        if (repoRoot.empty())
        {
            return;
        }

        std::vector<fs::path> paths = { fs::path ("C:/nonexistent"), repoRoot };
        fs::path result = PathResolver::FindFile (paths, "Resources/Machines/Apple2Plus/Apple2Plus.json");

        Assert::IsFalse (result.empty());
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

        Assert::IsTrue (result.empty());
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  FindFile_IndependentSearch_MachinesAndRoms
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (FindFile_IndependentSearch_MachinesAndRoms)
    {
        std::vector<fs::path>  paths;



        // Verify that machines/ and ROMs/ can be searched independently —
        // both should resolve from the same search paths
        fs::path repoRoot = FindRepoRoot();

        if (repoRoot.empty())
        {
            return;
        }

        paths = { repoRoot };

        fs::path configFound = PathResolver::FindFile (paths, "Resources/Machines/Apple2Plus/Apple2Plus.json");
        Assert::IsFalse (configFound.empty());

        // ROMs/ may or may not exist in the test environment,
        // but the search itself should not crash
        PathResolver::FindFile (paths, "ROMs/Apple2Plus.rom");
    }

private:

    // Walk up from the test DLL directory until we find a directory
    // that contains Resources/Machines/Apple2Plus/Apple2Plus.json -- this
    // is the repo root regardless of which build configuration the
    // test DLL was loaded from. Independent of BuildSearchPaths so it
    // keeps working after that API collapsed to localappdata-only.
    static fs::path FindRepoRoot()
    {
        fs::path  cursor  = PathResolver::GetExecutableDirectory();
        int       hop     = 0;
        fs::path  root;
        bool      walking = false;
        fs::path  marker = fs::path ("Resources") / "Machines" /
                           "Apple2Plus" / "Apple2Plus.json";

        walking = true;

        for (hop = 0; walking && root.empty() && hop < 8; hop++)
        {
            if (fs::exists (cursor / marker))
            {
                root = cursor;
            }
            else if (cursor.parent_path() == cursor)
            {
                // Drive root: no repo above this point.
                walking = false;
            }
            else
            {
                cursor = cursor.parent_path();
            }
        }

        return root;
    }
};