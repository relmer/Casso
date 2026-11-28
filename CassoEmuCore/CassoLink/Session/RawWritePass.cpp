#include "Pch.h"

#include "CassoLink/Session/RawWritePass.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::Build
//
//  Each arc is written after kLeadSyncs sync bytes, and is followed by one
//  more and the stretch's byte, the two self-sync bytes the //c's IWM takes
//  before the drive stops writing. The last wait of the stretch before an
//  arc is as long as puts the arc's first byte at its cell. A stretch's
//  bytes can't cross the end of a page of the stream, so the arc before it
//  gets as many more sync bytes ahead of it as moves them clear.
//
////////////////////////////////////////////////////////////////////////////////

bool RawWritePass::Build (const RawWriteArcPlan & plan, size_t turnCells)
{
    static constexpr size_t  kPage = 256;



    std::vector<size_t>  order;
    std::vector<size_t>  cells;
    std::vector<Byte>    values;
    std::vector<Byte>    zeros;
    std::vector<Part>    parts;
    std::vector<Part>    partsBefore;
    size_t               now         = 0;
    size_t               origin      = 0;
    size_t               tokenStore  = 0;
    size_t               stepCycles  = 0;
    size_t               stepsBefore = 0;
    size_t               waited      = 0;
    Byte                 longRounds  = 0;
    Byte                 shortRounds = 0;



    stream.clear();
    arcs.clear();
    count = 0;

    if (!OrderArcs (plan, turnCells, order, cells))
    {
        return false;
    }

    position = plan.arcs[order[0]].position;

    for (size_t k = 0; k < order.size(); k++)
    {
        const RawWriteArcPlan::Arc  & arc      = plan.arcs[order[k]];
        bool                          isLast   = k + 1 == order.size();
        int                           next     = isLast ? arc.position : plan.arcs[order[k + 1]].position;
        size_t                        lead     = kLeadSyncs;
        size_t                        tokenAt  = 0;
        size_t                        end      = 0;
        size_t                        first    = 0;
        size_t                        fixed    = kEntryCycles + stepsBefore + kWriteCycles;
        std::vector<Byte>             arcValues;
        std::vector<Byte>             arcZeros;

        arc.stream.AppendModuleBytes (arcValues, arcZeros);
        ListStepParts (arc.position / kHalfTrack, (next - arc.position) / kHalfTrack, parts, stepCycles);

        tokenAt = values.size() + (k > 0 ? (partsBefore.size() + 1) * kPartBytes : 0) + lead + arcValues.size() + 1;
        end     = tokenAt + (parts.size() + 1) * kPartBytes;

        while (!isLast && (tokenAt / kPage != end / kPage || end % kPage == kPage - 1))
        {
            lead++;
            tokenAt++;
            end++;
        }

        if (k == 0)
        {
            origin = lead * kSyncCycles;
        }
        else
        {
            first = origin + cells[k] * kCellCycles - lead * kSyncCycles;

            if (first < tokenStore + fixed + kLeastWait)
            {
                return false;
            }

            for (const Part & part : partsBefore)
            {
                GetWait (part.wait, longRounds, shortRounds, waited);
                values.insert (values.end(), { longRounds, shortRounds, part.then });
            }

            GetWait (first - tokenStore - fixed, longRounds, shortRounds, waited);
            values.insert (values.end(), { longRounds, shortRounds, kWrite });
            zeros.resize (values.size(), 0);

            now = tokenStore + fixed + waited;
        }

        for (size_t i = 0; i < lead; i++)
        {
            AppendByte (kSync, RawWriteStream::kSyncZeros, values, zeros, now);
        }

        arcs.push_back ({ order[k], arc.position, (now - origin) / kCellCycles, arc.cells });

        for (size_t i = 0; i < arcValues.size(); i++)
        {
            AppendByte (arcValues[i], arcZeros[i], values, zeros, now);
        }

        if (isLast)
        {
            continue;
        }

        AppendByte (kSync, RawWriteStream::kSyncZeros, values, zeros, now);

        tokenStore  = now;
        partsBefore = parts;
        stepsBefore = stepCycles;

        values.push_back (kSync);
        zeros.push_back (kGapZeros);
    }

    if (values.size() > RawWriteStream::kMaxBytes)
    {
        return false;
    }

    count = values.size();

    values.resize (RawWriteStream::kMaxBytes, 0);
    zeros.resize  (RawWriteStream::kMaxBytes, 0);

    stream = std::move (values);
    stream.insert (stream.end(), zeros.begin(), zeros.end());

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::OrderArcs
//
//  The arcs at the lowest position are written first, in order of angle,
//  then those at the next. The first arc written at a position is the one
//  whose angle comes soonest after the arm has stepped and settled, and the
//  others follow it around the turn.
//
////////////////////////////////////////////////////////////////////////////////

bool RawWritePass::OrderArcs (const RawWriteArcPlan & plan, size_t turnCells, std::vector<size_t> & outOrder, std::vector<size_t> & outCells)
{
    std::map<int, std::vector<size_t>>  byPosition;
    std::vector<size_t>                 angles;
    size_t                              last    = 0;
    size_t                              lastAt  = 0;
    int                                 before  = 0;
    bool                                isFirst = true;



    outOrder.clear();
    outCells.clear();

    if (plan.arcs.empty() || plan.turn == 0 || turnCells == 0)
    {
        return false;
    }

    for (size_t i = 0; i < plan.arcs.size(); i++)
    {
        angles.push_back (plan.arcs[i].start % plan.turn * turnCells / plan.turn);
        byPosition[plan.arcs[i].position].push_back (i);
    }

    for (auto & [position, list] : byPosition)
    {
        size_t  least    = GetLeastStretch (isFirst ? 0 : (position - before) / kHalfTrack) / kCellCycles + 1;
        size_t  earliest = isFirst ? 0 : lastAt + plan.arcs[last].cells + least;
        size_t  best     = 0;
        size_t  bestAt   = 0;

        //  The arc of this position that can be written soonest.
        for (size_t i = 0; i < list.size(); i++)
        {
            size_t  at = isFirst ? angles[list[i]] : lastAt + (angles[list[i]] + turnCells - angles[last]) % turnCells;

            while (at < earliest)
            {
                at += turnCells;
            }

            if (i == 0 || at < bestAt)
            {
                best   = list[i];
                bestAt = at;
            }
        }

        std::sort (list.begin(), list.end(), [&angles, best, turnCells] (size_t one, size_t other)
        {
            return (angles[one] + turnCells - angles[best]) % turnCells < (angles[other] + turnCells - angles[best]) % turnCells;
        });

        for (size_t index : list)
        {
            size_t  at = bestAt + (angles[index] + turnCells - angles[best]) % turnCells;

            if (index != best && at < lastAt + plan.arcs[last].cells + GetLeastStretch (0) / kCellCycles + 1)
            {
                return false;
            }

            outOrder.push_back (index);
            outCells.push_back (at);

            last   = index;
            lastAt = at;
        }

        before  = position;
        isFirst = false;
    }

    //  Cells are counted from the first arc written.
    for (size_t i = outCells.size(); i-- > 0;)
    {
        outCells[i] -= outCells[0];
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::ListStepParts
//
////////////////////////////////////////////////////////////////////////////////

void RawWritePass::ListStepParts (int half, int halfSteps, std::vector<Part> & outParts, size_t & outCycles)
{
    int     direction   = halfSteps < 0 ? -1 : 1;
    size_t  waited      = 0;
    Byte    longRounds  = 0;
    Byte    shortRounds = 0;



    outParts.clear();
    outCycles = 0;

    for (int step = 0; step < std::abs (halfSteps); step++)
    {
        int  next = half + direction;

        outParts.push_back ({ kLeastWait,  (Byte) ((next % kPhases) * kPhaseSwitches + 1) });
        outParts.push_back ({ kStepCycles, (Byte) ((half % kPhases) * kPhaseSwitches) });
        outParts.push_back ({ kLeastWait,  direction > 0 ? kArmUp : kArmDown });

        half = next;
    }

    if (halfSteps != 0)
    {
        outParts.push_back ({ kSettleCycles, (Byte) ((half % kPhases) * kPhaseSwitches) });
    }

    for (const Part & part : outParts)
    {
        GetWait (part.wait, longRounds, shortRounds, waited);

        outCycles += waited + (part.then == kArmUp ? kUpCycles : part.then == kArmDown ? kDownCycles : kPhaseCycles);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::GetLeastStretch
//
//  The stretch's byte and the sync byte before it, the module's code, the
//  arm's steps, the shortest last wait, and the sync bytes ahead of the next
//  arc, with as many more as can be added to move a stretch's bytes clear of
//  the end of a page.
//
////////////////////////////////////////////////////////////////////////////////

size_t RawWritePass::GetLeastStretch (int halfSteps)
{
    std::vector<Part>  parts;
    size_t             stepCycles = 0;



    ListStepParts (0, halfSteps, parts, stepCycles);

    return kSyncCycles + kEntryCycles + stepCycles + kLeastWait + kWriteCycles + (kLeadSyncs + (parts.size() + 1) * kPartBytes + 1) * kSyncCycles;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::GetWait
//
////////////////////////////////////////////////////////////////////////////////

void RawWritePass::GetWait (size_t cycles, Byte & outLong, Byte & outShort, size_t & outCycles)
{
    static constexpr size_t  kMaxLong = 255;



    size_t  total = cycles + kWaitLess;
    size_t  longs = total > kShortCycles ? (total - kShortCycles) / kLongCycles : 0;
    size_t  ones  = 0;



    longs = std::clamp<size_t> (longs, 1, kMaxLong);
    ones  = total > longs * kLongCycles ? (total - longs * kLongCycles + kShortCycles / 2) / kShortCycles : 1;
    ones  = std::clamp<size_t> (ones, 1, kMaxShort);

    outLong   = (Byte) longs;
    outShort  = (Byte) (ones % kMaxShort);
    outCycles = longs * kLongCycles + ones * kShortCycles - kWaitLess;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawWritePass::AppendByte
//
////////////////////////////////////////////////////////////////////////////////

void RawWritePass::AppendByte (Byte value, Byte zeros, std::vector<Byte> & ioValues, std::vector<Byte> & ioZeros, size_t & ioCycles)
{
    ioValues.push_back (value);
    ioZeros.push_back (zeros);

    ioCycles += kByteCycles + kCellCycles * zeros;
}
