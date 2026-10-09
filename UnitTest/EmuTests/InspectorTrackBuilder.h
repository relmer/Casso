#pragma once

#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Disk/Inspector/TrackCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorTrackBuilder
//
//  Lays down a made-up track cell by cell for the disk inspector's tests:
//  nibbles with any number of extra zero cells after them, sync runs of a
//  chosen width, raw cells, and whole address and data fields in either
//  format, with checksums that can be made to fail. It keeps the nibble
//  values in the order they were written, so a test can compare what the
//  framer read with what was put there.
//
////////////////////////////////////////////////////////////////////////////////

class InspectorTrackBuilder
{
public:
    using SectorFill = std::function<void (int sector, std::array<Byte, DiskFieldFormat::kSectorBytes> & bytes)>;

    static constexpr int  kSyncWidth = 10;

    void  AppendNibble          (Byte value, int extraZeroCells = 0);
    void  AppendSync            (int count, int widthCells = kSyncWidth);
    void  AppendZeros           (int cells);
    void  AppendRawCells        (const vector<Byte> & cells);
    void  AppendAddressField    (DiskFieldKind kind, Byte volume, Byte track, Byte sector, bool isChecksumBad = false);
    void  AppendDataField       (DiskFieldKind kind, std::span<const Byte> bytes, bool isChecksumBad = false);
    void  AppendStandardTrack   (DiskFieldKind kind, Byte volume, Byte track, const SectorFill & fill);
    void  PadToCells            (size_t cells);

    const vector<Byte> &  GetCells   () const { return m_cells; }
    const vector<Byte> &  GetNibbles () const { return m_nibbles; }
    size_t                GetCellCount () const { return m_cells.size(); }

    std::shared_ptr<const TrackCopy>  MakeBitCopy     (int slot = 0) const;
    std::shared_ptr<const TrackCopy>  MakeFluxCopy    (int slot = 0) const;
    void                              PackBits        (vector<Byte> & outPacked) const;

    static void  FillPattern (int sector, std::array<Byte, DiskFieldFormat::kSectorBytes> & bytes);

private:
    vector<Byte>  m_cells;
    vector<Byte>  m_nibbles;
};
