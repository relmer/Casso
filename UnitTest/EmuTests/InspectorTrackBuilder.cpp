#include "Pch.h"

#include "InspectorTrackBuilder.h"
#include "FluxTestImages.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendNibble
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendNibble (Byte value, int extraZeroCells)
{
    int  i = 0;



    for (i = 7; i >= 0; i--)
    {
        m_cells.push_back (static_cast<Byte> ((value >> i) & 1));
    }

    AppendZeros (extraZeroCells);
    m_nibbles.push_back (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendSync
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendSync (int count, int widthCells)
{
    int  i = 0;



    for (i = 0; i < count; i++)
    {
        AppendNibble (0xFF, widthCells - 8);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendZeros
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendZeros (int cells)
{
    m_cells.insert (m_cells.end(), static_cast<size_t> (std::max (cells, 0)), 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendRawCells
//
//  Cells as they are, with no nibble recorded for them.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendRawCells (const vector<Byte> & cells)
{
    m_cells.insert (m_cells.end(), cells.begin(), cells.end());
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendAddressField
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendAddressField (DiskFieldKind kind, Byte volume, Byte track, Byte sector, bool isChecksumBad)
{
    DiskMarkPattern  prologue = DiskFieldFormat::GetAddressPrologue (kind);
    Byte             checksum = DiskFieldFormat::ComputeAddressChecksum (volume, track, sector);
    Byte             odd      = 0;
    Byte             even     = 0;
    int              i        = 0;



    if (isChecksumBad)
    {
        checksum ^= 0x01;
    }

    for (i = 0; i < prologue.GetLength(); i++)
    {
        AppendNibble (prologue.GetValue (i));
    }

    for (Byte v : { volume, track, sector, checksum })
    {
        DiskFieldFormat::Encode44 (v, odd, even);
        AppendNibble (odd);
        AppendNibble (even);
    }

    AppendNibble (0xDE);
    AppendNibble (0xAA);
    AppendNibble (0xEB);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendDataField
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendDataField (DiskFieldKind kind, std::span<const Byte> bytes, bool isChecksumBad)
{
    DiskMarkPattern  prologue = DiskFieldFormat::GetDataPrologue();
    vector<Byte>     body;
    Byte             checksum = 0;
    int              i        = 0;



    DiskFieldFormat::EncodeData (kind, bytes, body, checksum);

    if (isChecksumBad)
    {
        //  Another nibble from the same table, so the field stays readable.
        checksum = DiskFieldFormat::TranslateValue (kind, static_cast<Byte> (DiskFieldFormat::TranslateNibble (kind, checksum) ^ 1));
    }

    for (i = 0; i < prologue.GetLength(); i++)
    {
        AppendNibble (prologue.GetValue (i));
    }

    for (Byte nibble : body)
    {
        AppendNibble (nibble);
    }

    AppendNibble (checksum);
    AppendNibble (0xDE);
    AppendNibble (0xAA);
    AppendNibble (0xEB);
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::AppendStandardTrack
//
//  A DOS-style track: a long sync run, then each sector in physical order as
//  an address field, a short sync run, a data field and a longer sync run.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::AppendStandardTrack (DiskFieldKind kind, Byte volume, Byte track, const SectorFill & fill)
{
    static constexpr int  kLeadSync  = 48;
    static constexpr int  kGap2Sync  = 6;
    static constexpr int  kGap3Sync  = 20;



    std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes   = {};
    int                                              sectors = (kind == DiskFieldKind::Sixteen) ? 16 : 13;
    int                                              s       = 0;



    AppendSync (kLeadSync);

    for (s = 0; s < sectors; s++)
    {
        fill (s, bytes);

        AppendAddressField (kind, volume, track, static_cast<Byte> (s));
        AppendSync         (kGap2Sync);
        AppendDataField    (kind, bytes);
        AppendSync         (kGap3Sync);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::PadToCells
//
//  Sync nibbles until the track is at least this long.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::PadToCells (size_t cells)
{
    while (m_cells.size() + kSyncWidth <= cells)
    {
        AppendSync (1);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::PackBits
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::PackBits (vector<Byte> & outPacked) const
{
    size_t  i = 0;



    outPacked.assign ((m_cells.size() + 7) / 8, 0);

    for (i = 0; i < m_cells.size(); i++)
    {
        outPacked[i >> 3] = static_cast<Byte> (outPacked[i >> 3] | (m_cells[i] << (7 - (i & 7))));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::MakeBitCopy
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const TrackCopy> InspectorTrackBuilder::MakeBitCopy (int slot) const
{
    auto  copy = std::make_shared<TrackCopy>();



    copy->slot     = slot;
    copy->kind     = TrackKind::Bits;
    copy->bitCount = m_cells.size();
    PackBits (copy->bits);

    return copy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::MakeFluxCopy
//
//  The same cells as a flux track at the drive's nominal cell.
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const TrackCopy> InspectorTrackBuilder::MakeFluxCopy (int slot) const
{
    auto          copy = std::make_shared<TrackCopy>();
    vector<Byte>  packed;



    PackBits (packed);

    copy->slot      = slot;
    copy->kind      = TrackKind::Flux;
    copy->fluxBytes = FluxTestImages::BitsToNominalFlux (packed, m_cells.size());

    return copy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::FillPattern
//
//  Bytes that differ in every sector and every position, so a sector filed
//  under the wrong number or a byte out of place shows.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorTrackBuilder::FillPattern (int sector, std::array<Byte, DiskFieldFormat::kSectorBytes> & bytes)
{
    size_t  i = 0;



    for (i = 0; i < bytes.size(); i++)
    {
        bytes[i] = static_cast<Byte> (sector * 37 + i * 11 + (i >> 4));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder::MakeStandardWoz
//
//  A 35-track 16-sector WOZ in the standard layout (N-0.25, N and N+0.25 on
//  track N's record), with the given volume in every address field. Reversed
//  puts track t in record 34 - t, so a record's number is never its track's.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT InspectorTrackBuilder::MakeStandardWoz (Byte volume, bool isFlux, bool isReversed, vector<Byte> & outBytes)
{
    static constexpr int  kTracks = 35;



    HRESULT                    hr     = S_OK;
    vector<WozSyntheticTrack>  tracks (kTracks);
    int                        t      = 0;
    int                        record = 0;



    for (t = 0; t < kTracks; t++)
    {
        InspectorTrackBuilder  builder;

        record = isReversed ? (kTracks - 1) - t : t;

        builder.AppendStandardTrack (DiskFieldKind::Sixteen, volume, static_cast<Byte> (t), FillPattern);

        if (isFlux)
        {
            tracks[record].data   = builder.MakeFluxCopy()->fluxBytes;
            tracks[record].isFlux = true;
        }
        else
        {
            builder.PackBits (tracks[record].data);
            tracks[record].bitCount = builder.GetCellCount();
        }

        tracks[record].quarterTracks = { 4 * t, 4 * t + 1 };

        if (t > 0)
        {
            tracks[record].quarterTracks.push_back (4 * t - 1);
        }
    }

    hr = WozLoader::BuildSyntheticV21 (tracks, outBytes);
    CHR (hr);

Error:
    return hr;
}