#include "Pch.h"

#include "Machines/Apple2/Common/DiskAccessLog.h"





static constexpr uint8_t  s_kPrologue0 = 0xD5;
static constexpr uint8_t  s_kPrologue1 = 0xAA;
static constexpr uint8_t  s_kDataMark  = 0xAD;
static constexpr uint8_t  s_kEpilogue0 = 0xDE;
static constexpr uint8_t  s_kEpilogue1 = 0xAA;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::Clear
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::Clear()
{
    for (vector<CellSpan> & spans : m_reads)
    {
        spans.clear();
    }

    for (vector<CellSpan> & spans : m_writes)
    {
        spans.clear();
    }

    m_readTrack    = -1;
    m_isInData     = false;
    m_pendingTrack = -1;
    m_writeTrack   = -1;
    m_generation++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::ObserveRead
//
//  One nibble the guest took from the latch, at the cell it started at, on
//  a quarter track of the given length in cells. A D5 AA AD opens a data
//  field at the D5's cell; DE AA after enough nibbles closes it. The closed
//  field counts as read once the guest skips more than kPauseCells cells
//  before its next nibble; a next nibble close behind drops it. Anything else, or a move to another
//  quarter track, drops a field in progress.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::ObserveRead (int quarterTrack, uint8_t nibble, uint32_t cell, uint32_t trackCells)
{
    bool      isValid = quarterTrack >= 0 && quarterTrack < DiskImage::kQuarterTrackCount;
    uint32_t  gap     = 0;



    EndWrite();

    if (m_pendingTrack >= 0)
    {
        gap = (quarterTrack == m_pendingTrack && trackCells > 0) ? (cell + trackCells - m_pendingEnd % trackCells) % trackCells : UINT32_MAX;

        if (gap > kPauseCells)
        {
            EndRead();
        }

        m_pendingTrack = -1;
    }

    if (quarterTrack != m_readTrack)
    {
        m_readTrack = quarterTrack;
        m_isInData  = false;
        m_prev[0]   = 0;
        m_prev[1]   = 0;
    }

    if (isValid && m_isInData)
    {
        m_dataCount++;

        if (m_prev[1] == s_kEpilogue0 && nibble == s_kEpilogue1 && m_dataCount >= kMinDataNibbles)
        {
            m_pendingTrack = quarterTrack;
            m_pendingStart = m_dataStart;
            m_pendingEnd   = cell;
            m_isInData     = false;
        }
        else if (m_dataCount > kMaxDataNibbles)
        {
            m_isInData = false;
        }
    }
    else if (isValid && m_prev[0] == s_kPrologue0 && m_prev[1] == s_kPrologue1 && nibble == s_kDataMark)
    {
        m_isInData  = true;
        m_dataStart = m_prevCell[0];
        m_dataCount = 0;
    }

    m_prev[0]     = m_prev[1];
    m_prevCell[0] = m_prevCell[1];
    m_prev[1]     = nibble;
    m_prevCell[1] = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::EndRead
//
//  The closed data field counts as read: the guest skipped cells after it
//  or moved to another quarter track, or the motor stopped.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::EndRead()
{
    if (m_pendingTrack >= 0 && m_pendingTrack < DiskImage::kQuarterTrackCount)
    {
        Add (m_reads[static_cast<size_t> (m_pendingTrack)], m_pendingStart, m_pendingEnd);
        m_generation++;
    }

    m_pendingTrack = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::ObserveWrite
//
//  One nibble the guest loaded into the latch while writing, at the head's
//  cell. A nibble far from the last one, or on another quarter track, ends
//  the run and starts another.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::ObserveWrite (int quarterTrack, uint32_t cell)
{
    bool  isNear = m_writeTrack == quarterTrack && cell >= m_writeLast && cell - m_writeLast <= kWriteGapCells;



    if (!isNear)
    {
        EndWrite();
        m_writeTrack = quarterTrack;
        m_writeStart = cell;
    }

    m_writeLast = cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::EndWrite
//
//  The run of writes in progress counts as written.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::EndWrite()
{
    if (m_writeTrack >= 0 && m_writeTrack < DiskImage::kQuarterTrackCount)
    {
        Add (m_writes[static_cast<size_t> (m_writeTrack)], m_writeStart, m_writeLast);
        m_generation++;
    }

    m_writeTrack = -1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::Add
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::Add (vector<CellSpan> & inOut, uint32_t start, uint32_t end)
{
    uint32_t  length = (end >= start) ? end - start : end + (UINT32_MAX - start);
    bool      isSame = false;



    for (CellSpan & span : inOut)
    {
        uint32_t  spanLength = (span.end >= span.start) ? span.end - span.start : span.end + (UINT32_MAX - span.start);
        uint32_t  overlap    = GetOverlap (span, start, end);

        if (!isSame && overlap * 2 >= std::min (length, spanLength) && overlap > 0)
        {
            span.count++;
            isSame = true;
        }
    }

    if (!isSame)
    {
        inOut.push_back ({ start, end, 1 });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::GetOverlap
//
//  How many cells two stretches share; a stretch that crosses the index is
//  two, one to the end of the cells and one from the start.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DiskAccessLog::GetOverlap (const CellSpan & a, uint32_t start, uint32_t end)
{
    using Range = std::pair<uint32_t, uint32_t>;



    auto  split = [] (uint32_t s, uint32_t e) { return (e >= s) ? vector<Range> { { s, e } } : vector<Range> { { s, UINT32_MAX }, { 0, e } }; };

    uint32_t  overlap = 0;



    for (const Range & x : split (a.start, a.end))
    {
        for (const Range & y : split (start, end))
        {
            uint32_t  lo = std::max (x.first, y.first);
            uint32_t  hi = std::min (x.second, y.second);

            overlap += (hi > lo) ? hi - lo : 0;
        }
    }

    return overlap;
}
