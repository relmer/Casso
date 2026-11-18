#pragma once

#include "Pch.h"

#include "Debugger/Reverse/SnapshotCompressor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCodec
//
//  Packs and unpacks the keyframes of a store. A group's first keyframe is
//  packed whole; each one after it is its XOR against that whole snapshot,
//  packed. The newest group's whole snapshot is in memory unpacked, so it
//  isn't unpacked again to pack the next difference or to unpack a
//  keyframe of the newest group. Pack runs on the store's work queue, one job at a time; the rest
//  runs on the caller's thread while no job is in flight.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeCodec
{
public:
    //  A packed snapshot in the store's arena.
    struct Packed
    {
        const Byte  * data        = nullptr;
        size_t        packedBytes = 0;
        size_t        stateBytes  = 0;
        bool          isWhole     = false;
    };

    void     Reserve        (size_t stateRoom);
    void     ClearLatest    () { m_latestWhole.clear(); }
    void     Release        ();
    size_t   GetBufferBytes () const;

    HRESULT  Pack         (std::vector<Byte> & state, bool isWhole, std::vector<Byte> & outPacked, uint64_t & outChecksum);
    HRESULT  Unpack       (const Packed & whole, const Packed & keyframe, bool isNewestGroup, std::vector<Byte> & outState);
    HRESULT  ReloadLatest (const Packed & whole);

    static uint64_t  ComputeChecksum (const Byte * data, size_t size);
    static void      XorBytes        (const Byte * a, const Byte * b, Byte * out, size_t count);

private:
    std::vector<Byte>   m_latestWhole;     // unpacked copy of the newest whole snapshot
    std::vector<Byte>   m_scratch;
    std::vector<Byte>   m_olderWhole;      // an older group's whole snapshot, unpacked by Unpack
    SnapshotCompressor  m_compressor;
};
