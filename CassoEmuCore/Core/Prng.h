#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Prng
//
//  SplitMix64 deterministic PRNG (Sebastiano Vigna, public domain). Pure —
//  no host state, no time source, no syscalls. Constructor takes the seed
//  explicitly.
//
//  Production seed source = static_cast<uint64_t>(time(nullptr)) ^
//                           static_cast<uint64_t>(GetCurrentProcessId())
//  Computed by callers; Prng itself stays pure.
//
//  Tests pin seed = 0xCA550001 so two runs reproduce byte-identical output.
//
////////////////////////////////////////////////////////////////////////////////

class Prng
{
public:
    explicit            Prng (uint64_t seed);

    uint64_t            Next64    ();
    uint8_t             NextByte  ();
    void                Fill      (uint8_t * dst, size_t count);

    uint64_t            GetState  () const { return m_state; }

private:
    uint64_t            m_state = 0;
};
