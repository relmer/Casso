#pragma once

#include "Pch.h"

#include "Update/ISignatureVerifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeSignatureVerifier
//
////////////////////////////////////////////////////////////////////////////////

class FakeSignatureVerifier : public ISignatureVerifier
{
public:
    bool          isOfficial = false;
    HRESULT       result     = S_OK;
    std::wstring  lastPath;
    int           calls      = 0;

    HRESULT IsOfficialFile (const std::wstring & path, bool & outIsOfficial) override
    {
        calls++;
        lastPath      = path;
        outIsOfficial = isOfficial;
        return result;
    }
};
