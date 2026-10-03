#include "Pch.h"

#include "FluxBitView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxBitView::Build
//
//  Each gap holds round (gap / cell) cells, at least one: the cells before
//  the transition are 0 and the cell it falls in is 1. A long gap therefore
//  decodes as a run of zeros, never as the random bits a drive would read
//  there, so a sector decode gives the same answer every time.
//
////////////////////////////////////////////////////////////////////////////////

void FluxBitView::Build (const FluxTrack & track)
{
    vector<uint64_t>  ticks;
    vector<uint8_t>   bits;
    uint64_t          prev  = 0;
    uint64_t          gap   = 0;
    uint64_t          cells = 0;
    uint64_t          c     = 0;
    size_t            i     = 0;



    m_bits.clear();
    m_bitStartTick.clear();
    m_bitCount = 0;

    track.GetTransitionTicks (ticks);

    for (i = 0; i < ticks.size(); i++)
    {
        gap   = ticks[i] - prev;
        cells = (gap * FluxTrack::kCellDenominator + FluxTrack::kCellNumerator / 2) / FluxTrack::kCellNumerator;

        if (cells == 0)
        {
            cells = 1;
        }

        // Cell c of this gap is centered at prev + c * gap / cells, so it
        // starts half a cell earlier.
        for (c = 1; c <= cells; c++)
        {
            bits.push_back ((c == cells) ? 1 : 0);
            m_bitStartTick.push_back (prev + ((2 * c - 1) * gap) / (2 * cells));
        }

        prev = ticks[i];
    }

    m_bitCount = bits.size();
    m_bits.assign ((m_bitCount + 7) / 8, 0);

    for (i = 0; i < m_bitCount; i++)
    {
        m_bits[i >> 3] = static_cast<Byte> (m_bits[i >> 3] | (bits[i] << (7 - (i & 7))));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxBitView::GetTickForBit
//
//  The bit index wraps, the way a sector decoder's cursor does.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t FluxBitView::GetTickForBit (size_t bitIndex) const
{
    return (m_bitCount > 0) ? m_bitStartTick[bitIndex % m_bitCount] : 0;
}
