#pragma once

#include "../../CassoEmuCore/Pch.h"

#include "IFixtureProvider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FixtureProvider
//
//  Concrete IFixtureProvider implementation. Resolves paths relative to
//  CASSO_FIXTURES_DIR (set by the UnitTest project at build time). Rejects
//  any relativePath containing "..", drive letters, or absolute roots —
//  returns E_INVALIDARG. The fixtures directory is resolved once at
//  construction and cached.
//
////////////////////////////////////////////////////////////////////////////////

class FixtureProvider : public IFixtureProvider
{
public:
                            FixtureProvider ();
    explicit                FixtureProvider (const std::string & rootOverride);

    HRESULT                  OpenFixture (
        const std::string          &  relativePath,
        std::vector<uint8_t>       &  outBytes) override;

    const std::string &      GetRoot () const { return m_root; }

private:
    std::string              m_root;

    static bool              IsRejectedPath (const std::string & relativePath);
};
