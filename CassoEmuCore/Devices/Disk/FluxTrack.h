#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack
//
//  One revolution of a WOZ 2.1 flux track, held exactly as the file holds it:
//  one byte per flux transition, giving the time since the previous
//  transition in 125 ns ticks. A 255 adds to the byte after it, so a long gap
//  spans several bytes. The bytes are walked with a cursor rather than
//  expanded, because the timing they record is the whole point of the format.
//
//  Time is absolute and unwrapped: transition k of revolution r falls at
//  r * total + (sum of the bytes up to and including k). The sum of every
//  byte is one revolution.
//
////////////////////////////////////////////////////////////////////////////////

class FluxTrack
{
public:
    // 125 ns ticks. A Disk II bit cell is four CPU cycles of the 45/44 MHz
    // clock, which is 1408/45 = 31.29 ticks, not the format's nominal 32.
    static constexpr uint64_t  kCellNumerator          = 1408;
    static constexpr uint64_t  kCellDenominator        = 45;
    static constexpr uint64_t  kNominalRevolutionCells = 51200;
    static constexpr Byte      kRunByte                = 255;

    // The next transition: its absolute tick and the byte where the run for
    // the transition after it begins.
    struct Cursor
    {
        size_t    nextIndex = 0;
        uint64_t  tick      = 0;
    };

    FluxTrack() = default;

    // False when the bytes cannot be a whole track: the last byte is a 255
    // with nothing after it to end the run.
    static bool             IsValidStream           (const vector<Byte> & bytes);
    static uint64_t         GetCellStartTick        (uint64_t cellIndex);

    void                    Assign                  (const vector<Byte> & bytes);
    void                    AssignBits              (const vector<uint8_t> & bits);
    const vector<Byte>   &  GetBytes                () const { return m_bytes; }
    uint64_t                GetTotalTicks           () const { return m_totalTicks; }
    uint64_t                GetRevolutionTicks      () const;
    size_t                  GetTransitionCount      () const { return m_transitionCount; }
    bool                    HasTransitions          () const { return m_transitionCount > 0; }
    Cursor                  FindTransitionAtOrAfter (uint64_t tickInRevolution) const;
    void                    AdvanceCursor           (Cursor & cursor) const;

    // Replaces the stretch starting at startTick with the given cells, one
    // transition per 1 bit, at the controller's cell. The revolution length
    // is unchanged.
    void                    SpliceWrite             (uint64_t startTick, const vector<uint8_t> & bits);

    // The absolute tick of every transition in one revolution, in order.
    void                    GetTransitionTicks      (vector<uint64_t> & outTicks) const;

    // The track rebuilt from absolute transition ticks in (0, revolution],
    // the inverse of GetTransitionTicks.
    void                    AssignTransitionTicks   (const vector<uint64_t> & ticks, uint64_t revolution);

private:
    void                    Recount                 ();
    void                    Encode                  (const vector<uint64_t> & ticks, uint64_t revolution);
    size_t                  DecodeGap               (size_t index, uint64_t & outGap) const;

    vector<Byte>      m_bytes;
    uint64_t          m_totalTicks      = 0;
    size_t            m_transitionCount = 0;
};
