#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PackBytes
//
//  The IIgs toolbox's PackBytes compression. Each run starts with a flag
//  byte: its high two bits pick the run and its low six bits are a count
//  less one. 00 is that many literal bytes; 01 one byte repeated; 10 four
//  bytes repeated; 11 one byte repeated four times the count.
//
////////////////////////////////////////////////////////////////////////////////

class PackBytes
{
public:
    //  Expands until the input ends or `limit` bytes are out; returns how
    //  many input bytes were used.
    static size_t  Unpack (std::span<const Byte> in, size_t limit, std::vector<Byte> & outBytes);
};
