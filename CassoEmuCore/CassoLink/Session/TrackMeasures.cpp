#include "Pch.h"

#include "CassoLink/Session/TrackMeasures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures::ToLayouts
//
//  Sector 0's place on each track is a share of a turn: the step
//  covers kStepBits of the drive's bitsPerTurn, and the nibbles counted
//  after it their share of the turn's nibbles. A track's bits are its
//  nibbles at TrackLayoutWriter's default bits for a nibble, so a drive
//  that turns slow or fast changes the step's share, not the track's
//  length. A track with no measure, or one whose layout can't fit 16
//  sectors, as after a misread, has no layout, and the volume of the track
//  before.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<TrackLayout> TrackMeasures::ToLayouts (const std::vector<CassoLinkHost::TrackMeasure> & measures, int bitsPerTurn)
{
    std::vector<TrackLayout>  layouts;
    double                    turn    = 0.0;
    Byte                      volume  = NibblizationLayer::kDefaultVolume;



    for (size_t track = 0; track < measures.size(); track++)
    {
        const CassoLinkHost::TrackMeasure  & measure = measures[track];
        TrackLayout                          layout;
        double                               step    = 0.0;

        if (measure.turn != 0 && bitsPerTurn > 0)
        {
            step    = track == 0 ? 0.0 : (double) kStepBits / bitsPerTurn + (double) measure.fromStep / measure.turn;
            turn    = std::fmod (turn + step, 1.0);
            volume  = measure.volume;

            layout.nibbles = measure.turn + kTurnNibbles;
            layout.bits    = (int) llround ((double) layout.nibbles * TrackLayoutWriter::kDefaultBits / TrackLayoutWriter::kDefaultNibbles);
            layout.sector0 = llround (turn * layout.bits) % layout.bits;
        }

        if (!TrackLayoutWriter::IsFeasible (layout))
        {
            layout = TrackLayout();
        }

        layout.volume = volume;

        layouts.push_back (layout);
    }

    return layouts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures::GetBitsPerTurn
//
//  A revolution more than kMaxDeviation from 200 ms is a timing gone wrong,
//  as a real drive is set within a few percent of 300 rpm.
//
////////////////////////////////////////////////////////////////////////////////

int TrackMeasures::GetBitsPerTurn (double revolutionMs)
{
    bool  plausible = std::abs (revolutionMs - kNominalMs) <= kNominalMs * kMaxDeviation;



    return plausible ? (int) llround (revolutionMs * kCyclesPerMs / kCyclesPerBit) : TrackLayoutWriter::kDefaultBits;
}
