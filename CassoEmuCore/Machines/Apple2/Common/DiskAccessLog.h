#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"





//  A stretch of a quarter track's cells the guest read or wrote, and how
//  many times. `end` is less than `start` for a stretch that crosses the
//  index.
struct CellSpan
{
    uint32_t  start = 0;
    uint32_t  end   = 0;
    uint32_t  count = 0;

    bool operator== (const CellSpan &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog
//
//  Where on each quarter track of one drive the guest read a data field and
//  where it wrote, as cell positions (FR-161). A read is a data field the
//  guest took from the latch, from its D5 AA AD prologue through the DE AA
//  of its epilogue. A write is a run of nibbles the guest loaded into the
//  latch while writing, each close after the one before. The same stretch
//  read or written again counts again. Cleared when the drive's disk
//  changes, with the visit counts (FR-068).
//
////////////////////////////////////////////////////////////////////////////////

class DiskAccessLog
{
public:
    using Spans = std::array<vector<CellSpan>, DiskImage::kQuarterTrackCount>;

    //  A data field's nibbles between its prologue and epilogue, with slack
    //  for a padded field.
    static constexpr uint32_t  kMinDataNibbles = 340;
    static constexpr uint32_t  kMaxDataNibbles = 420;

    //  Write nibbles further apart than this start another run.
    static constexpr uint32_t  kWriteGapCells  = 64;

    void  Clear        ();
    void  ObserveRead  (int quarterTrack, uint8_t nibble, uint32_t cell);
    void  ObserveWrite (int quarterTrack, uint32_t cell);
    void  EndWrite     ();

    const Spans &  GetReads      () const { return m_reads; }
    const Spans &  GetWrites     () const { return m_writes; }
    uint64_t       GetGeneration () const { return m_generation; }

    //  Counts one more access of a stretch: on a stretch already listed that
    //  covers at least half of it or of the new one, or as a new stretch.
    static void  Add (vector<CellSpan> & inOut, uint32_t start, uint32_t end);

private:
    static uint32_t  GetOverlap (const CellSpan & a, uint32_t start, uint32_t end);

    Spans     m_reads;
    Spans     m_writes;
    uint64_t  m_generation  = 0;
    int       m_readTrack   = -1;
    uint8_t   m_prev[2]     = {};
    uint32_t  m_prevCell[2] = {};
    bool      m_isInData    = false;
    uint32_t  m_dataStart   = 0;
    uint32_t  m_dataCount   = 0;
    int       m_writeTrack  = -1;
    uint32_t  m_writeStart  = 0;
    uint32_t  m_writeLast   = 0;
};
