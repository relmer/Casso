#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ISignatureVerifier
//
//  Whether a file has a valid signature from Casso's publisher. A file
//  that is unsigned, signed by anyone else, or fails verification is simply
//  not official (S_OK, false); a failure HRESULT means the check itself could
//  not run.
//
////////////////////////////////////////////////////////////////////////////////

class ISignatureVerifier
{
public:
    virtual ~ISignatureVerifier() = default;

    virtual HRESULT IsOfficialFile (const std::wstring & path, bool & outIsOfficial) = 0;
};
