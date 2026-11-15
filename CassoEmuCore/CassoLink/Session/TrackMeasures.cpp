#include "Pch.h"

#include "CassoLink/Session/TrackMeasures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures::ToLayouts
//
//  A track with no measure, or one whose layout can't fit 16 sectors, as
//  after a misread, has no layout, and the volume of the track before.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<TrackLayout> TrackMeasures::ToLayouts (const std::vector<CassoLinkHost::TrackMeasure> & measures, int bitsPerTurn)
{
    std::vector<TrackLayout>  layouts;
    long long                 sector0 = 0;
    Byte                      volume  = NibblizationLayer::kDefaultVolume;



    for (size_t track = 0; track < measures.size(); track++)
    {
        const CassoLinkHost::TrackMeasure  & measure = measures[track];
        TrackLayout                          layout;
        long long                            bits    = 0;

        if (measure.turn != 0 && bitsPerTurn > 0)
        {
            bits    = track == 0 ? 0 : kStepBits + (long long) llround ((double) measure.fromStep * bitsPerTurn / measure.turn);
            sector0 = (sector0 + bits) % bitsPerTurn;
            volume  = measure.volume;

            layout.nibbles = measure.turn + kTurnNibbles;
            layout.bits    = bitsPerTurn;
            layout.sector0 = sector0;
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
