#pragma once

#include "Pch.h"

#include "StateReader.h"
#include "StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateFloat
//
//  Saves and loads floating-point state as its exact bit pattern, so a sound
//  chip's filter history and fractional accumulators come back bit-identical
//  and replay from a snapshot produces the same samples. A float is four
//  bytes, a double eight. Reads follow the reader's sticky-error rule: a
//  failed read yields zero bits, which is 0.0.
//
////////////////////////////////////////////////////////////////////////////////

class StateFloat
{
public:
    static void Write (StateWriter & writer, float value)
    {
        uint32_t  bits = 0;



        memcpy (&bits, &value, sizeof (bits));
        writer.WriteUInt32 (bits);
    }

    static void Write (StateWriter & writer, double value)
    {
        uint64_t  bits = 0;



        memcpy (&bits, &value, sizeof (bits));
        writer.WriteUInt64 (bits);
    }

    static void Read (StateReader & reader, float & out)
    {
        uint32_t  bits = 0;



        reader.ReadUInt32 (bits);
        memcpy (&out, &bits, sizeof (out));
    }

    static void Read (StateReader & reader, double & out)
    {
        uint64_t  bits = 0;



        reader.ReadUInt64 (bits);
        memcpy (&out, &bits, sizeof (out));
    }
};
