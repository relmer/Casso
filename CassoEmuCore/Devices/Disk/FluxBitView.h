#pragma once

#include "Pch.h"

class FluxTrack;





////////////////////////////////////////////////////////////////////////////////
//
//  FluxBitView
//
//  A flux track decoded into bits at the controller's cell, for code that
//  works in sectors rather than playing the drive: the sector decoder, the
//  disk command, salvage. Each gap between transitions becomes as many cells
//  as fit in it, measured from the transition before, so the cell grid follows
//  the track's own speed the way a data separator does. The tick each bit
//  starts at is kept alongside, which is what lets a sector write land back in
//  the flux at the right place.
//
//  Playback never uses this. The drive plays the flux by time.
//
////////////////////////////////////////////////////////////////////////////////

class FluxBitView
{
public:
    void                    Build           (const FluxTrack & track);

    const vector<Byte>   &  GetBits         () const { return m_bits; }
    size_t                  GetBitCount     () const { return m_bitCount; }
    uint64_t                GetTickForBit   (size_t bitIndex) const;

private:
    vector<Byte>            m_bits;
    size_t                  m_bitCount = 0;
    vector<uint64_t>        m_bitStartTick;
};
