#include "Pch.h"

#include "CassoLink/Session/TrackMeasures.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures::ToLayouts
//
////////////////////////////////////////////////////////////////////////////////

std::vector<TrackLayout> TrackMeasures::ToLayouts (const std::vector<CassoLinkHost::TrackMeasure> & measures, int bitsPerTurn)
{
    std::vector<TrackLayout>  layouts;
    long long                 sector0 = 0;



    for (size_t track = 0; track < measures.size(); track++)
    {
        const CassoLinkHost::TrackMeasure  & measure = measures[track];
        TrackLayout                          layout;
        long long                            bits    = 0;

        if (measure.turn != 0 && bitsPerTurn > 0)
        {
            bits    = track == 0 ? 0 : kStepBits + (long long) llround ((double) measure.fromStep * bitsPerTurn / measure.turn);
            sector0 = (sector0 + bits) % bitsPerTurn;

            layout.nibbles = measure.turn + kTurnNibbles;
            layout.bits    = bitsPerTurn;
            layout.sector0 = sector0;
        }

        layout.volume = measure.volume;

        layouts.push_back (layout);
    }

    return layouts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMeasures::GetBitsPerTurn
//
////////////////////////////////////////////////////////////////////////////////

int TrackMeasures::GetBitsPerTurn (double revolutionMs)
{
    return revolutionMs > 0.0 ? (int) llround (revolutionMs * kCyclesPerMs / kCyclesPerBit) : TrackLayoutWriter::kDefaultBits;
}
