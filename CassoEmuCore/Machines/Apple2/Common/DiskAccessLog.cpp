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
    m_isClosed     = false;
    m_writeTrack   = -1;
    m_generation++;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAccessLog::ObserveRead
//
//  One nibble the guest took from the latch, at the cell it started at, on
//  a quarter track of the given length in cells. A D5 AA AD opens a data
//  field at the D5's cell, and DE AA after enough nibbles closes it. A gap
//  of more than kPauseCells cells before the next nibble, or a move to
//  another quarter track, ends the field: it counts as read when it is
//  closed, or when it is open with its body taken and the gap comes before
//  kMaxOpenNibbles. A next nibble close behind a closed field drops it, as
//  does a field that runs past kMaxDataNibbles.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::ObserveRead (int quarterTrack, uint8_t nibble, uint32_t cell, uint32_t trackCells)
{
    bool      isValid = quarterTrack >= 0 && quarterTrack < DiskImage::kQuarterTrackCount;
    bool      isSame  = quarterTrack == m_readTrack;
    uint32_t  gap     = (isSame && trackCells > 0) ? (cell + trackCells - m_prevCell[1] % trackCells) % trackCells : UINT32_MAX;



    EndWrite();

    if (gap > kPauseCells)
    {
        EndRead();
    }
    else if (m_isClosed)
    {
        m_isInData = false;
        m_isClosed = false;
    }

    if (!isSame)
    {
        m_readTrack = quarterTrack;
        m_prev[0]   = 0;
        m_prev[1]   = 0;
    }

    if (isValid && m_isInData)
    {
        m_dataCount++;

        if (m_prev[1] == s_kEpilogue0 && nibble == s_kEpilogue1 && m_dataCount >= kMinDataNibbles)
        {
            m_isClosed = true;
        }
        else if (m_dataCount > kMaxDataNibbles)
        {
            m_isInData = false;
        }
    }
    else if (isValid && m_prev[0] == s_kPrologue0 && m_prev[1] == s_kPrologue1 && nibble == s_kDataMark)
    {
        m_isInData  = true;
        m_isClosed  = false;
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
//  Ends the data field in progress, counting it as read when it is closed
//  or has its body taken: the guest skipped cells after it or moved to
//  another quarter track, or the motor stopped.
//
////////////////////////////////////////////////////////////////////////////////

void DiskAccessLog::EndRead()
{
    bool  isRead = m_isClosed || (m_dataCount >= kMinDataNibbles && m_dataCount <= kMaxOpenNibbles);



    if (m_isInData && isRead && m_readTrack >= 0 && m_readTrack < DiskImage::kQuarterTrackCount)
    {
        Add (m_reads[static_cast<size_t> (m_readTrack)], m_dataStart, m_prevCell[1]);
        m_generation++;
    }

    m_isInData = false;
    m_isClosed = false;
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
