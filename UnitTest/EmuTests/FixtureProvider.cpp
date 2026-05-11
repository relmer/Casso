#include "../CassoEmuCore/Pch.h"

#include "FixtureProvider.h"

#ifndef CASSO_FIXTURES_DIR
    #define CASSO_FIXTURES_DIR ""
#endif


namespace
{
    static constexpr int   kMaxAncestorWalk = 8;

    ////////////////////////////////////////////////////////////////////////
    //
    //  ResolveFixturesRoot
    //
    //  Locates `UnitTest/Fixtures` relative to the running test DLL by
    //  walking up the executable's directory tree. The CASSO_FIXTURES_DIR
    //  preprocessor define wins when set; otherwise we search ancestor
    //  directories. Returns "" on failure (callers fail OpenFixture with
    //  E_FAIL, matching the documented contract).
    //
    ////////////////////////////////////////////////////////////////////////

    std::string ResolveFixturesRoot ()
    {
        std::string   baked = CASSO_FIXTURES_DIR;
        fs::path      cursor;
        fs::path      candidate;
        int           steps;

        if (!baked.empty ())
        {
            return baked;
        }

        std::error_code   ec;

        cursor = fs::current_path (ec);
        if (ec)
        {
            return std::string ();
        }

        for (steps = 0; steps < kMaxAncestorWalk; steps++)
        {
            candidate = cursor / "UnitTest" / "Fixtures";

            if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
            {
                return candidate.string ();
            }

            candidate = cursor / "Fixtures";

            if (fs::exists (candidate, ec) && fs::is_directory (candidate, ec))
            {
                return candidate.string ();
            }

            if (!cursor.has_parent_path () || cursor == cursor.parent_path ())
            {
                break;
            }

            cursor = cursor.parent_path ();
        }

        return std::string ();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::FixtureProvider
//
////////////////////////////////////////////////////////////////////////////////

FixtureProvider::FixtureProvider ()
    : m_root (ResolveFixturesRoot ())
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
    if (relativePath.empty ())
    {
        return true;
    }

    if (relativePath.find ("..") != std::string::npos)
    {
        return true;
    }

    if (relativePath[0] == '/' || relativePath[0] == '\\')
    {
        return true;
    }

    if (relativePath.size () >= 2 && relativePath[1] == ':')
    {
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider::OpenFixture
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FixtureProvider::OpenFixture (
    const std::string          &  relativePath,
    std::vector<uint8_t>       &  outBytes)
{
    HRESULT             hr = S_OK;
    fs::path            full;
    std::ifstream       stream;
    std::streamsize     size;

    outBytes.clear ();

    if (IsRejectedPath (relativePath))
    {
        hr = E_INVALIDARG;
        goto Error;
    }

    if (m_root.empty ())
    {
        hr = E_FAIL;
        goto Error;
    }

    full = fs::path (m_root) / relativePath;

    stream.open (full, std::ios::binary | std::ios::ate);
    if (!stream.is_open ())
    {
        hr = E_FAIL;
        goto Error;
    }

    size = stream.tellg ();
    stream.seekg (0, std::ios::beg);

    outBytes.resize (static_cast<size_t> (size));
    if (size > 0)
    {
        stream.read (reinterpret_cast<char *> (outBytes.data ()), size);
    }

Error:
    return hr;
}
