#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Bzip2Decoder
//
//  Expands a bzip2 stream, NuFX thread format 7, through the reference
//  libbzip2 in External/bzip2, built without its stdio functions.
//
////////////////////////////////////////////////////////////////////////////////

class Bzip2Decoder
{
public:
    static HRESULT  Expand (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError);
};
