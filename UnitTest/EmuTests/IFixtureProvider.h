#pragma once

#include "../../CassoEmuCore/Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFixtureProvider
//
//  Restricted file access for tests. The ONLY sanctioned path through which
//  test code may read binary fixture data. Implementations resolve paths
//  relative to UnitTest/Fixtures/ and reject any path containing "..",
//  drive letters, or absolute roots (return E_INVALIDARG).
//
////////////////////////////////////////////////////////////////////////////////

class IFixtureProvider
{
public:
    virtual                 ~IFixtureProvider () = default;

    virtual HRESULT          OpenFixture (
        const std::string          &  relativePath,
        std::vector<uint8_t>       &  outBytes) = 0;
};
