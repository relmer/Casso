#include "Pch.h"
#include "FixtureProvider.h"

#ifndef CASSO_FIXTURES_DIR
    #define CASSO_FIXTURES_DIR ""
#endif





////////////////////////////////////////////////////////////////////////////////
//
//  ResolveFixturesRoot
//
//  Locates `UnitTest/Fixtures` relative to the running test DLL by
//  walking up the executable's directory tree. The CASSO_FIXTURES_DIR
//  preprocessor define wins when set; otherwise we search ancestor
//  directories. Returns "" on failure (callers fail OpenFixture with
//  E_FAIL, matching the documented contract).
//
////////////////////////////////////////////////////////////////////////////////

std::string FixtureProvider::ResolveFixturesRoot()
{
    constexpr int     kMaxAncestorWalk = 8;



    std::string      baked     = CASSO_FIXTURES_DIR;
    std::string      root;
    fs::path         cursor;
    fs::path         candidate;
    std::error_code  ec;
    int              steps     = 0;
    bool             atRoot    = false;



    // The baked-in define wins outright; the ancestor walk is the fallback
    // for a test DLL run from somewhere the build system did not predict.
    if (!baked.empty())
    {
        root = baked;
    }
    else
    {
        cursor = fs::current_path (ec);

        for (steps = 0; !ec && root.empty() && !atRoot && steps < kMaxAncestorWalk; steps++)
        {
            // Two shapes per level: the repo layout (UnitTest/Fixtures) and a
            // deployed layout where Fixtures sits beside the binary.
            candidate = cursor / "UnitTest" / "Fixtures";

            if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
            {
                root = candidate.string();
            }

            if (root.empty())
            {
                candidate = cursor / "Fixtures";

                if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
                {
                    root = candidate.string();
                }
            }

            // A path that is its own parent is the drive root: nowhere left
            // to climb.
            atRoot = !cursor.has_parent_path() || cursor == cursor.parent_path();

            if (root.empty() && !atRoot)
            {
                cursor = cursor.parent_path();
            }
        }
    }

    return root;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::FixtureProvider
//
////////////////////////////////////////////////////////////////////////////////

FixtureProvider::FixtureProvider()
    : m_root (ResolveFixturesRoot())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::FixtureProvider (rootOverride)
//
////////////////////////////////////////////////////////////////////////////////

FixtureProvider::FixtureProvider (const std::string & rootOverride)
    : m_root (rootOverride)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::IsRejectedPath
//
//  Returns true if relativePath must be rejected (contains "..", a drive
//  letter, or an absolute path root).
//
////////////////////////////////////////////////////////////////////////////////

bool FixtureProvider::IsRejectedPath (const std::string & relativePath)
{
    // Everything that could escape the fixtures root, in one predicate. The
    // empty test comes first because the indexed reads below need a character.
    return relativePath.empty()
        || relativePath.find ("..") != std::string::npos          // climb-out
        || relativePath[0] == '/' || relativePath[0] == '\\'      // rooted
        || (relativePath.size() >= 2 && relativePath[1] == ':');  // drive letter
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::OpenFixture
//
//  Locates a test fixture by name, searching the same paths the product uses.
//
//  Searching rather than a fixed relative path, because the test binary runs
//  from several places -- the build output, a test-explorer working directory,
//  a CI agent -- and a hard-coded path works in exactly one of them.
//
//  A missing fixture fails with the NAME and the paths searched, since that is
//  the difference between a real failure and a fixture that simply was not
//  deployed, and the two look identical from the assertion alone.
//
//  Opened read-only and never copied: fixtures are inputs, so a test that
//  modified one would corrupt the suite for every later run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FixtureProvider::OpenFixture (
    const std::string          &  relativePath,
    std::vector<uint8_t>       &  outBytes)
{
    HRESULT             hr          = S_OK;
    fs::path            full;
    std::ifstream       stream;
    std::streamsize     size;
    bool                pathIsSafe  = false;
    bool                hasRoot     = false;
    bool                isOpen      = false;

    outBytes.clear();

    pathIsSafe = !IsRejectedPath (relativePath);
    CBRAEx (pathIsSafe, E_INVALIDARG);

    hasRoot = !m_root.empty();
    CBR (hasRoot);

    full = fs::path (m_root) / relativePath;

    stream.open (full, std::ios::binary | std::ios::ate);

    isOpen = stream.is_open();
    CBR (isOpen);

    size = stream.tellg();
    stream.seekg (0, std::ios::beg);

    outBytes.resize (static_cast<size_t> (size));
    if (size > 0)
    {
        stream.read (reinterpret_cast<char *> (outBytes.data()), size);
    }

Error:
    return hr;
}
