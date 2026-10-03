#include "Pch.h"

#include "StateHash.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Hash
//
//  64-bit FNV-1a eight bytes at a time, folding the high half down after
//  each word so a change in any bit reaches every later bit. The words are
//  dealt round-robin to four lanes, each seeded differently, so the
//  multiplies overlap instead of waiting on each other; the lanes are then
//  hashed together in order, then the tail a byte at a time. Continuing
//  from a previous result as the seed chains runs together.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t StateHash::Hash (
    const Byte  * data,
    size_t        size,
    uint64_t      seed)
{
    constexpr uint64_t  kPrime  = 0x00000100000001B3ULL;
    constexpr int       kFold   = 32;
    constexpr size_t    kLanes  = 4;
    constexpr size_t    kStride = kLanes * sizeof (uint64_t);
    uint64_t            lanes[kLanes];
    uint64_t            hash    = seed;
    uint64_t            word    = 0;
    size_t              i       = 0;
    size_t              lane    = 0;



    for (lane = 0; lane < kLanes; lane++)
    {
        lanes[lane] = seed + lane;
    }

    for (i = 0; i + kStride <= size; i += kStride)
    {
        for (lane = 0; lane < kLanes; lane++)
        {
            memcpy (&word, data + i + lane * sizeof (word), sizeof (word));
            lanes[lane]  = (lanes[lane] ^ word) * kPrime;
            lanes[lane] ^= lanes[lane] >> kFold;
        }
    }

    for (lane = 0; lane < kLanes; lane++)
    {
        hash  = (hash ^ lanes[lane]) * kPrime;
        hash ^= hash >> kFold;
    }

    for (; i < size; i++)
    {
        hash = (hash ^ data[i]) * kPrime;
    }

    return hash;
}




