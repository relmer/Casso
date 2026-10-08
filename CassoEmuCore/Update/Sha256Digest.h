#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Sha256Digest
//
//  SHA-256 over bytes in memory, through BCrypt.
//
////////////////////////////////////////////////////////////////////////////////

class Sha256Digest
{
public:
    static constexpr size_t  kDigestBytes = 32;

    static HRESULT  Compute (std::span<const Byte> bytes, std::vector<Byte> & outDigest);
};
