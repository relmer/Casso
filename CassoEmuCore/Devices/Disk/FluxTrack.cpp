#include "Pch.h"

#include "FluxTrack.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::IsValidStream
//
//  A track that ends inside a run of 255s was cut short: the run says the gap
//  continues, and there is no byte left to end it. An empty track is valid --
//  it holds no transitions, which is what a blank surface is.
//
////////////////////////////////////////////////////////////////////////////////

bool FluxTrack::IsValidStream (const vector<Byte> & bytes)
{
    bool  isEmpty = bytes.empty();



    return isEmpty || bytes.back() != kRunByte;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::GetCellStartTick
//
//  The tick at which bit cell cellIndex starts, counting from cell 0 at tick
//  0. Exact integer arithmetic on the rational cell length, so a long run of
//  cells never drifts.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t FluxTrack::GetCellStartTick (uint64_t cellIndex)
{
    return cellIndex * kCellNumerator / kCellDenominator;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::Assign
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::Assign (const vector<Byte> & bytes)
{
    m_bytes = bytes;
    Recount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::GetRevolutionTicks
//
//  One revolution. A track with no data has no length of its own, so it
//  spins for the nominal unformatted length a bit track uses.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t FluxTrack::GetRevolutionTicks() const
{
    return (m_totalTicks > 0) ? m_totalTicks : GetCellStartTick (kNominalRevolutionCells);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::Recount
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::Recount()
{
    size_t  i = 0;



    m_totalTicks      = 0;
    m_transitionCount = 0;

    for (i = 0; i < m_bytes.size(); i++)
    {
        m_totalTicks += m_bytes[i];

        if (m_bytes[i] != kRunByte)
        {
            m_transitionCount++;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::DecodeGap
//
//  Sums one transition's run starting at index: every 255, then the byte that
//  ends it. Returns the index just past the run.
//
////////////////////////////////////////////////////////////////////////////////

size_t FluxTrack::DecodeGap (size_t index, uint64_t & outGap) const
{
    size_t  at   = index;
    bool    more = true;



    outGap = 0;

    while (more && at < m_bytes.size())
    {
        outGap += m_bytes[at];
        more    = (m_bytes[at] == kRunByte);
        at++;
    }

    return at;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::AdvanceCursor
//
//  Moves to the transition after the cursor's, wrapping into the next
//  revolution after the last one. A track with no transitions parks the
//  cursor at the end of time, so nothing ever falls due.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::AdvanceCursor (Cursor & cursor) const
{
    uint64_t  gap   = 0;
    size_t    index = cursor.nextIndex;



    if (m_transitionCount == 0)
    {
        cursor.tick = UINT64_MAX;
        return;
    }

    if (index >= m_bytes.size())
    {
        index = 0;
    }

    cursor.nextIndex  = DecodeGap (index, gap);
    cursor.tick      += gap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::FindTransitionAtOrAfter
//
//  The first transition at or after a point in the first revolution. Used
//  when the head lands on the track, so a linear walk is fine.
//
////////////////////////////////////////////////////////////////////////////////

FluxTrack::Cursor FluxTrack::FindTransitionAtOrAfter (uint64_t tickInRevolution) const
{
    Cursor    cursor;
    uint64_t  gap   = 0;
    bool      found = false;



    if (m_transitionCount == 0)
    {
        cursor.tick = UINT64_MAX;
        return cursor;
    }

    while (!found && cursor.nextIndex < m_bytes.size())
    {
        cursor.nextIndex  = DecodeGap (cursor.nextIndex, gap);
        cursor.tick      += gap;
        found             = (cursor.tick >= tickInRevolution);
    }

    // The last transition sits at the end of the revolution, so a point past
    // every transition can only be the very end: the first one of the next
    // revolution is the answer.
    if (!found)
    {
        AdvanceCursor (cursor);
    }

    return cursor;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::GetTransitionTicks
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::GetTransitionTicks (vector<uint64_t> & outTicks) const
{
    size_t    index = 0;
    uint64_t  gap   = 0;
    uint64_t  tick  = 0;



    outTicks.clear();
    outTicks.reserve (m_transitionCount);

    while (index < m_bytes.size())
    {
        index  = DecodeGap (index, gap);
        tick  += gap;
        outTicks.push_back (tick);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::Encode
//
//  Rebuilds the bytes from transition ticks in (0, revolution]. The format has
//  no way to record time after the last transition, so when the last one falls
//  short of the end the leftover is added to the first gap instead. That
//  rotates the track by the leftover, keeps the revolution its full length,
//  and only happens when a write covered the end of the revolution.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::Encode (const vector<uint64_t> & ticks, uint64_t revolution)
{
    vector<uint64_t>  gaps;
    uint64_t          prev = 0;
    uint64_t          gap  = 0;
    size_t            i    = 0;



    m_bytes.clear();

    for (i = 0; i < ticks.size(); i++)
    {
        gaps.push_back (ticks[i] - prev);
        prev = ticks[i];
    }

    if (!gaps.empty() && prev < revolution)
    {
        gaps[0] += revolution - prev;
    }

    for (i = 0; i < gaps.size(); i++)
    {
        gap = gaps[i];

        while (gap >= kRunByte)
        {
            m_bytes.push_back (kRunByte);
            gap -= kRunByte;
        }

        m_bytes.push_back (static_cast<Byte> (gap));
    }

    Recount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FluxTrack::SpliceWrite
//
//  Puts a write burst into the flux. Every original transition inside the
//  written stretch goes, and one transition per 1 bit takes its place, in the
//  middle of its cell. Everything outside the stretch keeps its recorded
//  timing, and the revolution keeps its length, so a write never moves the
//  rest of the track.
//
////////////////////////////////////////////////////////////////////////////////

void FluxTrack::SpliceWrite (uint64_t startTick, const vector<uint8_t> & bits)
{
    vector<uint64_t>  original;
    vector<uint64_t>  merged;
    uint64_t          revolution = GetRevolutionTicks();
    uint64_t          start      = startTick % revolution;
    uint64_t          length     = GetCellStartTick (bits.size());
    uint64_t          offset     = 0;
    uint64_t          tick       = 0;
    size_t            i          = 0;



    if (bits.empty())
    {
        return;
    }

    if (length > revolution)
    {
        length = revolution;
    }

    GetTransitionTicks (original);

    for (i = 0; i < original.size(); i++)
    {
        offset = (original[i] % revolution + revolution - start) % revolution;

        if (offset >= length)
        {
            merged.push_back (original[i]);
        }
    }

    for (i = 0; i < bits.size(); i++)
    {
        if (bits[i] == 0)
        {
            continue;
        }

        tick = start + (GetCellStartTick (i) + GetCellStartTick (i + 1)) / 2;
        tick = tick % revolution;

        // Time zero and the end of the revolution are the same point; the
        // format records it as the end.
        merged.push_back ((tick == 0) ? revolution : tick);
    }

    sort (merged.begin(), merged.end());
    merged.erase (unique (merged.begin(), merged.end()), merged.end());

    Encode (merged, revolution);
}
