#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateHash
//
//  A fast 64-bit hash for machine state and ROM images, for telling whether
//  two runs of bytes match. Not cryptographic.
//
////////////////////////////////////////////////////////////////////////////////

class StateHash
{
public:
    static constexpr uint64_t  kSeed = 0xCBF29CE484222325ULL;

    static uint64_t  Hash (const Byte * data, size_t size, uint64_t seed = kSeed);
};