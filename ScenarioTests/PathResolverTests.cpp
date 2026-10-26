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
//  This file has the case that creates %LOCALAPPDATA%\Casso. The rest are in
//  the unit suite's PathResolverTests.
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

private:

};