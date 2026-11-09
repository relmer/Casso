#include "Pch.h"

#include "Ui/DiskInspector/StripGeometry.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::StripGeometry
//
////////////////////////////////////////////////////////////////////////////////

StripGeometry::StripGeometry (double start, double span, float leftPx, float widthPx) :
    m_start (start - std::floor (start)),
    m_span  (std::clamp (span, kMinSpan, 1.0)),
    m_left  (leftPx),
    m_width (std::max (widthPx, 1.0f))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::GetX
//
//  The x of a turn's first copy at or after the view's start.
//
////////////////////////////////////////////////////////////////////////////////

float StripGeometry::GetX (double turn) const
{
    double  offset = turn - m_start;



    offset -= std::floor (offset);

    return m_left + static_cast<float> (offset / m_span) * m_width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::GetTurn
//
////////////////////////////////////////////////////////////////////////////////

double StripGeometry::GetTurn (float xPx) const
{
    double  turn = m_start + (xPx - m_left) / m_width * m_span;



    return turn - std::floor (turn);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::GetSegments
//
//  A stretch shows at its offset from the view's start and again one turn
//  earlier, which is where the part of it before the view's start lands; each
//  copy is clipped to the view, and the ones left empty are dropped.
//
////////////////////////////////////////////////////////////////////////////////

int StripGeometry::GetSegments (double turnStart, double turnLength, std::array<StripSegment, 2> & outSegments) const
{
    double  offset = turnStart - m_start;
    int     count  = 0;



    offset -= std::floor (offset);

    for (double o : { offset, offset - 1.0 })
    {
        double  a = std::max (o, 0.0);
        double  b = std::min (o + turnLength, m_span);

        if (b > a)
        {
            outSegments[count].x0 = m_left + static_cast<float> (a / m_span) * m_width;
            outSegments[count].x1 = m_left + static_cast<float> (b / m_span) * m_width;
            count++;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::GetStartForZoom
//
////////////////////////////////////////////////////////////////////////////////

double StripGeometry::GetStartForZoom (double start, double span, double newSpan, double anchorFraction)
{
    double  next = start + anchorFraction * (span - newSpan);



    return next - std::floor (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripGeometry::BuildNibbleTurns
//
////////////////////////////////////////////////////////////////////////////////

void StripGeometry::BuildNibbleTurns (const TrackAnalysis & track, vector<double> & outTurns)
{
    const FramedTrack &  framed  = track.framed;
    vector<double>       cellTurn;
    double               elapsed = 0;
    double               turn    = 0;
    uint32_t             cell    = 0;



    outTurns.clear();

    if (framed.cellCount > 0)
    {
        if (framed.isFlux && framed.turnTicks > 0 && framed.cellTicks.size() >= framed.cellCount)
        {
            cellTurn.resize (framed.cellCount);

            for (cell = 0; cell < framed.cellCount; cell++)
            {
                cellTurn[cell]  = elapsed / framed.turnTicks;
                elapsed        += framed.cellTicks[cell];
            }
        }

        for (const FramedNibble & n : framed.nibbles)
        {
            turn = cellTurn.empty() ? static_cast<double> (n.startCell % framed.cellCount) / framed.cellCount : cellTurn[n.startCell % framed.cellCount];

            //  The first nibble can start before the index, so a later one
            //  that wraps past it moves on a turn and the list keeps rising.
            while (!outTurns.empty() && turn < outTurns.back())
            {
                turn += 1.0;
            }

            outTurns.push_back (turn);
        }

        if (!outTurns.empty())
        {
            outTurns.push_back (outTurns.front() + 1.0);
        }
    }
}
