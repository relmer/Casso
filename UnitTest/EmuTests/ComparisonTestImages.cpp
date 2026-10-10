#include "Pch.h"

#include "ComparisonTestImages.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/Inspector/DiskAnalyzer.h"
#include "InspectorTrackBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonTestImages::MakeTrack
//
//  The track's cells laid down, then rotated if asked, and handed over as a
//  bit record or a flux record; a retimed flux record has every transition
//  from the given one on two ticks slower, still within the same cells.
//
////////////////////////////////////////////////////////////////////////////////

WozSyntheticTrack ComparisonTestImages::MakeTrack (int track, const TrackSpec & spec)
{
    InspectorTrackBuilder  builder;
    WozSyntheticTrack      out;
    int                    changed = spec.changedSector;



    builder.AppendSync (spec.extraSync);

    builder.AppendStandardTrack (spec.isThirteen ? DiskFieldKind::Thirteen : DiskFieldKind::Sixteen, spec.volume, static_cast<Byte> (track),
                                 [changed] (int sector, std::array<Byte, DiskFieldFormat::kSectorBytes> & bytes)
                                 {
                                     InspectorTrackBuilder::FillPattern (sector, bytes);
                                     bytes[0x40] ^= (sector == changed) ? 0x01 : 0x00;
                                 });

    if (spec.rotation != 0)
    {
        vector<Byte>  cells = builder.GetCells();

        std::rotate (cells.begin(), cells.begin() + spec.rotation, cells.end());

        InspectorTrackBuilder  rotated;

        rotated.AppendRawCells (cells);
        builder = rotated;
    }

    if (spec.isFlux)
    {
        out.data   = builder.MakeFluxCopy()->fluxBytes;
        out.isFlux = true;

        for (size_t i = (spec.retimeFrom >= 0) ? static_cast<size_t> (spec.retimeFrom) : out.data.size(); i < out.data.size(); i++)
        {
            out.data[i] = static_cast<Byte> (out.data[i] + 2);
        }
    }
    else
    {
        builder.PackBits (out.data);
        out.bitCount = builder.GetCellCount();
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonTestImages::Analyze
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonTestImages::Analyze (const vector<WozSyntheticTrack> & tracks, DiskAnalysis & out)
{
    vector<Byte>  woz;
    DiskImage     image;



    AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz));
    AssertSucceeded (WozLoader::Load (woz, image));
    DiskAnalyzer::Analyze (DiskCopy::MakeFromImage (image, 1, "test.woz", woz.size(), false), DecodeSettings::MakeStandard(), out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonTestImages::AnalyzeDisk
//
//  Each record at its whole track and the quarter tracks either side, as
//  the standard layout maps them.
//
////////////////////////////////////////////////////////////////////////////////

void ComparisonTestImages::AnalyzeDisk (int changedTrack, const TrackSpec & changed, DiskAnalysis & out, bool isFlux)
{
    vector<WozSyntheticTrack>  tracks;
    TrackSpec                  standard;



    standard.isFlux = isFlux;

    for (int t = 0; t < kTracks; t++)
    {
        const TrackSpec &  spec = (t == changedTrack) ? changed : standard;

        if (spec.isPresent)
        {
            tracks.push_back (MakeTrack (t, spec));
            tracks.back().quarterTracks = { 4 * t, 4 * t + 1 };

            if (t > 0)
            {
                tracks.back().quarterTracks.push_back (4 * t - 1);
            }
        }
    }

    Analyze (tracks, out);
}
