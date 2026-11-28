#include "Pch.h"

#include "CassoLink/Session/RawWriteArcs.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::Find
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::Find (std::span<const Source> sources, std::vector<RawWriteArcGroup> & outGroups)
{
    std::vector<Turn>   turns;
    std::vector<Match>  matches;
    int                 groups = 0;



    outGroups.clear();

    ListTurns  (sources, turns);
    MatchArcs  (turns, matches);
    GroupTurns (turns, groups);

    for (int group = 0; group < groups; group++)
    {
        outGroups.emplace_back();
        BuildGroup (turns, matches, group, outGroups.back());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::Split
//
//  A blank stretch begins after a byte followed by kBlankZeros zero bits or
//  more, and runs through every byte like it, or with no top bit, that is
//  within kJoinBytes of the one before: a flux image has noise there, and a
//  capture one long run of zero bits. The stream is a turn, so a stretch can
//  run past its end to its start.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::Split (const RawWriteStream & stream, std::vector<RawWriteArc> & outArcs, size_t & outTurn)
{
    using Run = std::pair<size_t, size_t>;

    size_t               count = stream.values.size();
    size_t               blank = 0;
    std::vector<Run>     runs;
    std::vector<Run>     gaps;
    std::vector<size_t>  cellAt (count + 1, 0);



    outArcs.clear();
    outTurn = stream.GetCellCount();

    for (size_t at = 0; at < count; at++)
    {
        cellAt[at + 1] = cellAt[at] + RawWriteStream::kBitsPerByte + stream.zeros[at];

        if (!IsBlank (stream, at))
        {
            continue;
        }

        if (!runs.empty() && at - runs.back().second <= kJoinBytes)
        {
            runs.back().second = at;
        }
        else
        {
            runs.push_back ({ at, at });
        }
    }

    if (runs.empty())
    {
        return;
    }

    if (runs.front().first + count - runs.back().second <= kJoinBytes)
    {
        if (runs.size() == 1)
        {
            return;
        }

        runs.back().second = runs.front().second + count;
        runs.erase (runs.begin());
    }

    //  A run's first byte, when it has its top bit, is the arc's last.
    for (const Run & run : runs)
    {
        size_t  cells = cellAt[run.second % count + 1] + (run.second >= count ? outTurn : 0) - cellAt[run.first];
        size_t  data  = (stream.values[run.first] & kTopBit) != 0 ? RawWriteStream::kBitsPerByte : 0;

        if (cells - data >= kMinGapCells)
        {
            gaps.push_back (run);
            blank += cells - data;
        }
    }

    if (gaps.empty() || blank * kPercent < outTurn * kBlankPercent)
    {
        return;
    }

    for (size_t i = 0; i < gaps.size(); i++)
    {
        size_t       next  = (i + 1) % gaps.size();
        size_t       from  = gaps[i].second + 1;
        size_t       to    = gaps[next].first + (next <= i ? count : 0);
        bool         ends  = (stream.values[gaps[next].first] & kTopBit) != 0;
        RawWriteArc  arc;

        for (size_t at = from; at < to + (ends ? 1 : 0); at++)
        {
            arc.stream.values.push_back (stream.values[at % count]);
            arc.stream.zeros.push_back  (at == to ? (Byte) 0 : stream.zeros[at % count]);
        }

        if (arc.stream.values.empty())
        {
            continue;
        }

        arc.start = cellAt[from % count];
        arc.cells = arc.stream.GetCellCount();
        outArcs.push_back (std::move (arc));
    }

    std::sort (outArcs.begin(), outArcs.end(), [] (const RawWriteArc & first, const RawWriteArc & second) { return first.start < second.start; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::ListTurns
//
//  The image's distinct streams that have arcs, each with its positions, in
//  order of its first position.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::ListTurns (std::span<const Source> sources, std::vector<Turn> & outTurns)
{
    outTurns.clear();

    for (const Source & source : sources)
    {
        auto  known = std::find_if (outTurns.begin(), outTurns.end(), [&source] (const Turn & turn) { return turn.stream == source.stream; });

        if (known != outTurns.end())
        {
            known->positions.push_back (source.position);
            continue;
        }

        outTurns.emplace_back();
        outTurns.back().stream = source.stream;
        outTurns.back().positions.push_back (source.position);

        Split (*source.stream, outTurns.back().arcs, outTurns.back().cells);

        outTurns.back().arcOf.assign   (outTurns.back().arcs.size(), -1);
        outTurns.back().isWhole.assign (outTurns.back().arcs.size(), false);
    }

    std::erase_if (outTurns, [] (const Turn & turn) { return turn.arcs.empty(); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::MatchArcs
//
//  Arcs are taken longest first, so each is compared with the longest arc it
//  can be: all of that arc, a part of it, as a read at the arc's edge has, or
//  another arc. A turn has each arc once.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::MatchArcs (std::vector<Turn> & ioTurns, std::vector<Match> & outMatches)
{
    std::vector<Match>  arcs;



    outMatches.clear();

    for (size_t turn = 0; turn < ioTurns.size(); turn++)
    {
        for (size_t arc = 0; arc < ioTurns[turn].arcs.size(); arc++)
        {
            arcs.push_back ({ turn, arc });
        }
    }

    std::stable_sort (arcs.begin(), arcs.end(), [&ioTurns] (const Match & first, const Match & second)
    {
        return ioTurns[first.turn].arcs[first.arc].stream.values.size() > ioTurns[second.turn].arcs[second.arc].stream.values.size();
    });

    for (const Match & each : arcs)
    {
        Turn                  & turn      = ioTurns[each.turn];
        const RawWriteStream  & stream    = turn.arcs[each.arc].stream;
        double                  bestShare = 0.0;
        size_t                  longest   = 0;
        int                     best      = -1;

        for (size_t match = 0; match < outMatches.size(); match++)
        {
            const RawWriteStream  & other = ioTurns[outMatches[match].turn].arcs[outMatches[match].arc].stream;
            double                  share = 0.0;

            if (std::find (turn.arcOf.begin(), turn.arcOf.end(), (int) match) != turn.arcOf.end())
            {
                continue;
            }

            share = GetSharedPart (stream, other);

            if (share >= kPartShare && share > bestShare)
            {
                bestShare = share;
                best      = (int) match;
            }
        }

        if (best < 0)
        {
            best      = (int) outMatches.size();
            bestShare = 1.0;
            outMatches.push_back (each);
        }

        longest = ioTurns[outMatches[(size_t) best].turn].arcs[outMatches[(size_t) best].arc].stream.values.size();

        turn.arcOf[each.arc]   = best;
        turn.isWhole[each.arc] = bestShare >= kSameShare && (double) stream.values.size() >= kSameLength * (double) longest;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GroupTurns
//
//  Turns that have an arc in common and are less than a full track apart are
//  one group, and so are the turns each of those is grouped with. A turn
//  grouped with no other isn't in a group.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::GroupTurns (std::vector<Turn> & ioTurns, int & outGroups)
{
    std::vector<size_t>  root (ioTurns.size());
    std::vector<int>     groupOf (ioTurns.size(), -1);
    std::vector<size_t>  members (ioTurns.size(), 0);



    outGroups = 0;

    std::iota (root.begin(), root.end(), (size_t) 0);

    auto  find = [&root] (size_t at)
    {
        while (root[at] != at)
        {
            at = root[at];
        }

        return at;
    };

    for (size_t first = 0; first < ioTurns.size(); first++)
    {
        for (size_t second = first + 1; second < ioTurns.size(); second++)
        {
            bool  shared = false;

            for (int arc : ioTurns[first].arcOf)
            {
                shared = shared || std::find (ioTurns[second].arcOf.begin(), ioTurns[second].arcOf.end(), arc) != ioTurns[second].arcOf.end();
            }

            if (shared && AreNear (ioTurns[first], ioTurns[second]))
            {
                root[find (second)] = find (first);
            }
        }
    }

    for (size_t at = 0; at < ioTurns.size(); at++)
    {
        members[find (at)]++;
    }

    for (size_t at = 0; at < ioTurns.size(); at++)
    {
        size_t  top = find (at);

        if (members[top] < 2)
        {
            continue;
        }

        if (groupOf[top] < 0)
        {
            groupOf[top] = outGroups++;
        }

        ioTurns[at].group = groupOf[top];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::BuildGroup
//
//  The group's turn is that of the stream with the most bytes in whole
//  arcs, and each arc's bytes are those of the group's longest read of it.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::BuildGroup (const std::vector<Turn> & turns, const std::vector<Match> & matches, int group, RawWriteArcGroup & outGroup)
{
    std::vector<size_t>    members;
    std::map<int, size_t>  starts;
    std::set<int>          positions;
    size_t                 reference = 0;
    size_t                 most      = 0;



    for (size_t at = 0; at < turns.size(); at++)
    {
        size_t  bytes = 0;

        if (turns[at].group != group)
        {
            continue;
        }

        for (size_t arc = 0; arc < turns[at].arcs.size(); arc++)
        {
            bytes += turns[at].isWhole[arc] ? turns[at].arcs[arc].stream.values.size() : 0;
        }

        if (members.empty() || bytes > most)
        {
            most      = bytes;
            reference = at;
        }

        members.push_back (at);
        positions.insert (turns[at].positions.begin(), turns[at].positions.end());
    }

    PlaceArcs (turns, members, reference, starts);

    outGroup.turn = turns[reference].cells;
    outGroup.positions.assign (positions.begin(), positions.end());

    for (size_t match = 0; match < matches.size(); match++)
    {
        RawWriteArc  arc;

        for (size_t member : members)
        {
            const Turn  & turn  = turns[member];
            auto          has   = std::find (turn.arcOf.begin(), turn.arcOf.end(), (int) match);
            size_t        index = (size_t) (has - turn.arcOf.begin());

            if (has == turn.arcOf.end())
            {
                continue;
            }

            if (!turn.isWhole[index])
            {
                arc.partPositions.insert (arc.partPositions.end(), turn.positions.begin(), turn.positions.end());
                continue;
            }

            arc.positions.insert (arc.positions.end(), turn.positions.begin(), turn.positions.end());
            arc.reads.push_back ({ turn.positions, turn.arcs[index].stream });

            if (turn.arcs[index].stream.values.size() > arc.stream.values.size())
            {
                arc.stream = turn.arcs[index].stream;
                arc.cells  = turn.arcs[index].cells;
                arc.start  = turn.arcs[index].start * outGroup.turn / turn.cells;
            }
        }

        if (arc.reads.empty())
        {
            continue;
        }

        arc.start = starts.contains ((int) match) ? starts[(int) match] : arc.start;

        std::sort (arc.positions.begin(), arc.positions.end());
        std::sort (arc.partPositions.begin(), arc.partPositions.end());
        outGroup.arcs.push_back (std::move (arc));
    }

    std::sort (outGroup.arcs.begin(), outGroup.arcs.end(), [] (const RawWriteArc & first, const RawWriteArc & second) { return first.start < second.start; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::PlaceArcs
//
//  The reference turn's whole arcs start where they do in it. Another arc is
//  placed from a turn that has all of it and all of an arc already placed:
//  as far after that arc as it is in that turn, as a part of the turn.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::PlaceArcs (const std::vector<Turn> & turns, const std::vector<size_t> & members, size_t reference, std::map<int, size_t> & ioStarts)
{
    const Turn  & first  = turns[reference];
    bool          placed = true;



    for (size_t arc = 0; arc < first.arcs.size(); arc++)
    {
        if (first.isWhole[arc])
        {
            ioStarts[first.arcOf[arc]] = first.arcs[arc].start;
        }
    }

    while (placed)
    {
        placed = false;

        for (size_t member : members)
        {
            const Turn  & turn = turns[member];

            for (size_t arc = 0; arc < turn.arcs.size(); arc++)
            {
                for (size_t known = 0; known < turn.arcs.size(); known++)
                {
                    bool    usable = turn.isWhole[arc] && turn.isWhole[known] && !ioStarts.contains (turn.arcOf[arc]) && ioStarts.contains (turn.arcOf[known]);
                    size_t  after  = usable ? (turn.arcs[arc].start + turn.cells - turn.arcs[known].start) % turn.cells : 0;

                    if (usable)
                    {
                        ioStarts[turn.arcOf[arc]] = (ioStarts[turn.arcOf[known]] + after * first.cells / turn.cells) % first.cells;
                        placed                    = true;
                    }
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GetSharedPart
//
//  Chunks of sync bytes or of one byte repeated, as arcs begin with, are on
//  every track, so only a chunk with kChunkValues distinct bytes is looked
//  for. An arc with no such chunk is compared by all of its chunks. A chunk
//  is looked for by its bits: an image can have a few bits more or fewer at
//  one place in an arc, as at its seam, and every byte after that differs.
//
////////////////////////////////////////////////////////////////////////////////

double RawWriteArcs::GetSharedPart (const RawWriteStream & arc, const RawWriteStream & other)
{
    std::string       bits      = GetValueBits (arc);
    std::string       otherBits = GetValueBits (other);
    std::string_view  view      = bits;
    size_t            chunks[2] = {};
    size_t            found[2]  = {};



    for (size_t at = 0; at + kChunkBytes <= arc.values.size(); at += kChunkBytes)
    {
        auto            first  = arc.values.begin() + (ptrdiff_t) at;
        std::set<Byte>  values (first, first + (ptrdiff_t) kChunkBytes);
        bool            hit    = otherBits.find (view.substr (at * RawWriteStream::kBitsPerByte, kChunkBytes * RawWriteStream::kBitsPerByte)) != std::string::npos;
        size_t          kind   = values.size() >= kChunkValues ? 0 : 1;

        chunks[kind]++;
        found[kind] += hit ? 1 : 0;
    }

    if (chunks[0] > 0)
    {
        return (double) found[0] / (double) chunks[0];
    }

    return chunks[1] == 0 ? 0.0 : (double) found[1] / (double) chunks[1];
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GetValueBits
//
////////////////////////////////////////////////////////////////////////////////

std::string RawWriteArcs::GetValueBits (const RawWriteStream & stream)
{
    std::string  bits;



    bits.reserve (stream.values.size() * RawWriteStream::kBitsPerByte);

    for (Byte value : stream.values)
    {
        for (size_t bit = 0; bit < RawWriteStream::kBitsPerByte; bit++)
        {
            bits.push_back (((value >> (RawWriteStream::kBitsPerByte - 1 - bit)) & 1) != 0 ? '1' : '0');
        }
    }

    return bits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GetTrackShare
//
////////////////////////////////////////////////////////////////////////////////

double RawWriteArcs::GetTrackShare (const RawWriteStream & position, const RawWriteStream & track)
{
    std::vector<Byte>  turns  = track.values;
    size_t             chunks = 0;
    size_t             found  = 0;



    turns.insert (turns.end(), track.values.begin(), track.values.end());

    for (size_t at = 0; at + kChunkBytes <= position.values.size(); at += kChunkBytes * kLeanStep)
    {
        auto  first = position.values.begin() + (ptrdiff_t) at;

        chunks++;
        found += std::search (turns.begin(), turns.end(), first, first + (ptrdiff_t) kChunkBytes) != turns.end() ? 1 : 0;
    }

    return chunks == 0 ? 0.0 : (double) found / (double) chunks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::AreNear
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteArcs::AreNear (const Turn & first, const Turn & second)
{
    for (int position : first.positions)
    {
        for (int other : second.positions)
        {
            if (std::abs (position - other) < kFullTrack)
            {
                return true;
            }
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GetLean
//
////////////////////////////////////////////////////////////////////////////////

double RawWriteArcs::GetLean (std::span<const Source> sources)
{
    std::map<int, const RawWriteStream *>  streams;
    std::vector<RawWriteArc>               arcs;
    size_t                                 turn   = 0;
    double                                 total  = 0.0;
    int                                    tracks = 0;



    for (const Source & source : sources)
    {
        streams[source.position] = source.stream;
    }

    for (const auto & [position, stream] : streams)
    {
        double  sum   = 0.0;
        int     count = 0;

        Split (*stream, arcs, turn);

        if (position % kFullTrack != 0 || !arcs.empty() || turn == 0)
        {
            continue;
        }

        for (int other = position - (kFullTrack - 1); other < position + kFullTrack; other++)
        {
            auto  found = streams.find (other);
            bool  same  = found != streams.end() && (found->second == stream || GetTrackShare (*found->second, *stream) >= kLeanShare);

            sum   += same ? other : 0;
            count += same ? 1 : 0;
        }

        total += sum / count - position;
        tracks++;
    }

    return tracks == 0 ? 0.0 : total / tracks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::Plan
//
//  The arc with the lowest middle goes to the half or whole track nearest
//  it, a whole track when the two are about as near. Each other arc goes as
//  many half tracks above that as its middle is above that arc's, so two
//  arcs a quarter track apart or more are written at different positions
//  whichever way the drive that read them leaned. The half and whole tracks
//  from the group's lowest position to its highest are erased first, so the
//  turn between the arcs has no other data.
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteArcs::Plan (const RawWriteArcGroup & group, double lean, RawWriteArcPlan & outPlan)
{
    double               lowest = -1.0;
    int                  anchor = 0;
    int                  whole  = 0;
    int                  half   = 0;
    int                  first  = 0;
    int                  last   = 0;
    std::set<int>        written;
    std::vector<size_t>  found;
    std::vector<bool>    dropped;
    RawWriteArcPlan      rest;



    outPlan           = RawWriteArcPlan();
    outPlan.turn      = group.turn;
    outPlan.lean      = lean;
    outPlan.positions = group.positions;

    for (const RawWriteArc & arc : group.arcs)
    {
        if (IsWritten (arc))
        {
            lowest = lowest < 0.0 ? GetMiddle (arc, lean) : std::min (lowest, GetMiddle (arc, lean));
        }
    }

    if (lowest < 0.0 || group.positions.empty())
    {
        return false;
    }

    //  The half track and the whole track on either side of the lowest
    //  middle. A copy's arc can be trimmed to between the two, and disks are
    //  mastered on whole tracks far more often.
    half   = (int) std::floor (lowest / kHalfTrack) * kHalfTrack;
    whole  = half + kHalfTrack;

    if (half % kFullTrack == 0)
    {
        std::swap (half, whole);
    }

    anchor = std::abs (lowest - whole) <= std::abs (lowest - half) + kWholeBias ? whole : half;

    for (const RawWriteArc & arc : group.arcs)
    {
        RawWriteArcPlan::Arc  planned;
        int                   above   = (int) std::round ((GetMiddle (arc, lean) - lowest) / kHalfTrack) * kHalfTrack;
        int                   nearest = -1;
        size_t                longest = arc.stream.values.size();

        if (!IsWritten (arc))
        {
            continue;
        }

        planned.position = std::clamp (anchor + above, 0, RawWriteStream::kWritablePositions - kHalfTrack);
        planned.start    = arc.start;
        planned.stream   = arc.stream;

        //  The bytes are those read nearest the position they are written
        //  at, of the reads with nearly as many bytes as the longest.
        for (const RawWriteArcRead & read : arc.reads)
        {
            for (int position : read.positions)
            {
                bool  isLong = read.stream.values.size() * kPercent >= longest * kNearlyAll;

                if (isLong && (nearest < 0 || std::abs (position - planned.position) < nearest))
                {
                    nearest        = std::abs (position - planned.position);
                    planned.stream = read.stream;
                }
            }
        }

        planned.cells = planned.stream.GetCellCount();

        found.push_back (arc.positions.size());
        outPlan.arcs.push_back (std::move (planned));
    }

    FindOverwritten (outPlan, found, dropped, outPlan.lost);

    for (size_t at = 0; at < outPlan.arcs.size(); at++)
    {
        if (!dropped[at])
        {
            written.insert (outPlan.arcs[at].position);
            rest.arcs.push_back (std::move (outPlan.arcs[at]));
        }
    }

    outPlan.arcs = std::move (rest.arcs);

    std::stable_sort (outPlan.arcs.begin(), outPlan.arcs.end(), [] (const RawWriteArcPlan::Arc & one, const RawWriteArcPlan::Arc & other) { return one.position < other.position; });

    first = std::min (*written.begin(),  group.positions.front() + group.positions.front() % kHalfTrack);
    last  = std::max (*written.rbegin(), group.positions.back()  - group.positions.back()  % kHalfTrack);

    for (int position = first; position <= last; position += kHalfTrack)
    {
        outPlan.erased.push_back (position);
    }

    return written.size() > 1;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::FindOverwritten
//
//  Of two arcs at one angle and less than a full track apart, the one found
//  at fewer positions, or the shorter, is dropped: a read between two arcs
//  mixes them into bytes that match neither. When the one dropped was found
//  at kRealPositions or more it is data on the disk, and the pair is listed.
//
////////////////////////////////////////////////////////////////////////////////

void RawWriteArcs::FindOverwritten (const RawWriteArcPlan & plan, const std::vector<size_t> & found, std::vector<bool> & outDropped, std::vector<std::pair<int, int>> & outLost)
{
    outDropped.assign (plan.arcs.size(), false);

    for (size_t one = 0; one < plan.arcs.size(); one++)
    {
        for (size_t other = one + 1; other < plan.arcs.size(); other++)
        {
            const RawWriteArcPlan::Arc  & first   = plan.arcs[one];
            const RawWriteArcPlan::Arc  & second  = plan.arcs[other];
            bool                          isFirst = found[one] != found[other] ? found[one] > found[other] : first.cells >= second.cells;
            size_t                        loser   = isFirst ? other : one;

            if (outDropped[one] || outDropped[other] || std::abs (first.position - second.position) >= kFullTrack || !AreAtOneAngle (first, second, plan.turn))
            {
                continue;
            }

            outDropped[loser] = true;

            if (found[loser] >= kRealPositions && first.position != second.position)
            {
                outLost.push_back ({ std::min (first.position, second.position), std::max (first.position, second.position) });
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::GetMiddle
//
////////////////////////////////////////////////////////////////////////////////

double RawWriteArcs::GetMiddle (const RawWriteArc & arc, double lean)
{
    double  total = std::accumulate (arc.positions.begin(), arc.positions.end(), 0.0);



    return arc.positions.empty() ? 0.0 : total / (double) arc.positions.size() - lean;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::IsWritten
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteArcs::IsWritten (const RawWriteArc & arc)
{
    size_t  chunks = 0;
    size_t  worth  = 0;



    for (size_t at = 0; at + kChunkBytes <= arc.stream.values.size(); at += kChunkBytes)
    {
        auto            first  = arc.stream.values.begin() + (ptrdiff_t) at;
        std::set<Byte>  values (first, first + (ptrdiff_t) kChunkBytes);

        chunks++;
        worth += values.size() >= kChunkValues ? 1 : 0;
    }

    return arc.stream.values.size() >= kMinArcBytes && worth * kFillerShare >= chunks;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::AreAtOneAngle
//
////////////////////////////////////////////////////////////////////////////////

bool RawWriteArcs::AreAtOneAngle (const RawWriteArcPlan::Arc & first, const RawWriteArcPlan::Arc & second, size_t turn)
{
    size_t  after  = (second.start + turn - first.start) % turn;
    size_t  before = (first.start + turn - second.start) % turn;



    return after < first.cells + kMarginCells || before < second.cells + kMarginCells;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::PlanImage
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawWriteArcs::PlanImage (std::span<const Byte> fileBytes, std::vector<RawWriteArcPlan> & outPlans, std::vector<RawWriteArcGroup> & outGroups)
{
    HRESULT                        hr   = S_OK;
    DiskImage                      image;
    std::vector<Byte>              raw (fileBytes.begin(), fileBytes.end());
    std::map<int, RawWriteStream>  streams;
    std::vector<Source>            sources;
    std::vector<uint8_t>           bits;
    RawWriteArcPlan                plan;
    double                         lean = 0.0;



    outPlans.clear();
    outGroups.clear();

    hr = WozLoader::Load (raw, image);
    CHR (hr);

    for (int position = 0; position < RawWriteStream::kWritablePositions; position++)
    {
        int  slot = image.ResolveQuarterTrack (position);

        if (slot < 0 || image.GetTrackBitCount (slot) == 0)
        {
            continue;
        }

        if (!streams.contains (slot))
        {
            bits.clear();

            for (size_t at = 0; at < image.GetTrackBitCount (slot); at++)
            {
                bits.push_back (image.ReadBit (slot, at));
            }

            RawWriteStream::FromBits (bits, streams[slot]);
        }

        sources.push_back ({ position, &streams[slot] });
    }

    Find (sources, outGroups);

    //  The lean compares every track with the positions beside it, so it is
    //  measured only for an image with arcs.
    lean = outGroups.empty() ? 0.0 : GetLean (sources);

    for (const RawWriteArcGroup & group : outGroups)
    {
        if (Plan (group, lean, plan))
        {
            outPlans.push_back (plan);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWriteArcs::DescribePlan
//
////////////////////////////////////////////////////////////////////////////////

std::string RawWriteArcs::DescribePlan (const RawWriteArcPlan & plan)
{
    std::string  text;



    if (plan.positions.empty() || plan.turn == 0)
    {
        return text;
    }

    text = std::format ("Tracks {} to {} are written as arcs:", plan.positions.front() / (double) kFullTrack, plan.positions.back() / (double) kFullTrack);

    for (const RawWriteArcPlan::Arc & arc : plan.arcs)
    {
        text += std::format (" track {} at {:.1f}% for {:.1f}%;", arc.position / (double) kFullTrack, (double) (kPercent * arc.start) / (double) plan.turn, (double) (kPercent * arc.cells) / (double) plan.turn);
    }

    text.back() = '.';

    for (const auto & [first, second] : plan.lost)
    {
        text += std::format (" Tracks {} and {} have data at the same angle, and one of them is lost.", first / (double) kFullTrack, second / (double) kFullTrack);
    }

    return text;
}
