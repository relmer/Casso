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
//  The words are read through an unaligned pointer, and the lanes are
//  locals, not an array: a debug build compiles a memcpy of a word to a
//  call, once for every eight bytes of each keyframe's state.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t StateHash::Hash (
    const Byte  * data,
    size_t        size,
    uint64_t      seed)
{
    constexpr uint64_t                 kPrime  = 0x00000100000001B3ULL;
    constexpr int                      kFold   = 32;
    constexpr size_t                   kLanes  = 4;
    constexpr size_t                   kStride = kLanes * sizeof (uint64_t);
    const uint64_t UNALIGNED         * words   = reinterpret_cast<const uint64_t UNALIGNED *> (data);
    const uint64_t UNALIGNED * const   end     = words + (size / kStride) * kLanes;
    uint64_t                           lane0   = seed;
    uint64_t                           lane1   = seed + 1;
    uint64_t                           lane2   = seed + 2;
    uint64_t                           lane3   = seed + 3;
    uint64_t                           hash    = seed;
    size_t                             i       = (size / kStride) * kStride;



    for (; words != end; words += kLanes)
    {
        lane0  = (lane0 ^ words[0]) * kPrime;
        lane1  = (lane1 ^ words[1]) * kPrime;
        lane2  = (lane2 ^ words[2]) * kPrime;
        lane3  = (lane3 ^ words[3]) * kPrime;
        lane0 ^= lane0 >> kFold;
        lane1 ^= lane1 >> kFold;
        lane2 ^= lane2 >> kFold;
        lane3 ^= lane3 >> kFold;
    }

    for (uint64_t lane : { lane0, lane1, lane2, lane3 })
    {
        hash  = (hash ^ lane) * kPrime;
        hash ^= hash >> kFold;
    }

    for (; i < size; i++)
    {
        hash = (hash ^ data[i]) * kPrime;
    }

    return hash;
}




