#pragma once

#include "Pch.h"

#include "Debugger/Reverse/SnapshotCompressor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PackedKeyframe
//
//  One keyframe as KeyframeStore holds it, copied out so it can be unpacked
//  on another thread: its group's whole snapshot packed, unless the unpacker
//  it is meant for already holds that snapshot unpacked, and for a
//  difference, the difference packed.
//
////////////////////////////////////////////////////////////////////////////////

struct PackedKeyframe
{
    std::vector<Byte>  whole;                  // empty when the unpacker holds the group's whole snapshot
    std::vector<Byte>  difference;             // empty when the keyframe is itself whole
    uint64_t           wholePosition = 0;      // the group's whole snapshot, by position and checksum
    uint64_t           wholeChecksum = 0;
    size_t             stateBytes    = 0;
    bool               isWhole       = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeUnpacker
//
//  Unpacks keyframes copied out of a KeyframeStore, on whatever thread owns
//  it. It keeps the last group's whole snapshot unpacked, so keyframes of
//  the same group, such as neighboring points of a history strip, unpack
//  only their own difference; the store leaves that snapshot out of the copy
//  when this unpacker holds it. A whole snapshot is known by its position
//  and its checksum together, so one recorded again at the same position
//  after the history past it was dropped is never mistaken for the old one.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeUnpacker
{
public:
               KeyframeUnpacker  () = default;

               KeyframeUnpacker  (const KeyframeUnpacker &) = delete;
    KeyframeUnpacker & operator= (const KeyframeUnpacker &) = delete;

    bool       HoldsWhole        (uint64_t position, uint64_t checksum) const;
    HRESULT    Unpack            (const PackedKeyframe & packed, std::vector<Byte> & outState);
    void       Forget            ();

private:
    SnapshotCompressor  m_compressor;
    std::vector<Byte>   m_whole;
    uint64_t            m_wholePosition = 0;
    uint64_t            m_wholeChecksum = 0;
    bool                m_hasWhole      = false;
};
