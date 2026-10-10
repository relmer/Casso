#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorOrder / ExportProblem
//
//  The order sectors are written in, and a sector the export wrote as
//  decoded or as zeros, by track and sector, for the list the user sees
//  before anything is written (FR-058).
//
////////////////////////////////////////////////////////////////////////////////

enum class SectorOrder
{
    Physical,
    Dos33,
    ProDos,
};


struct ExportProblem
{
    int           track  = 0;
    int           sector = 0;
    std::wstring  reason;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TrackExport
//
//  The three Export forms of FR-058, built in memory so the caller writes
//  them through DurableCommit. Sectors: 256 decoded bytes each, a bad or
//  unchecked sector as decoded and a missing one as zeros, each listed.
//  Track nibbles: one byte per framed nibble, one turn from the index, with
//  the random-bit regions listed. Track bits: a WOZ 2.1 file holding only
//  that quarter track, its record unchanged. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class TrackExport
{
public:
    static constexpr int  kSectorsPerTrack = 16;

    static void     ExportSector       (const TrackAnalysis & track, int wholeTrack, int sectorIndex, vector<Byte> & inOutBytes, vector<ExportProblem> & inOutProblems);
    static void     ExportTrackSectors (const TrackAnalysis * track, int wholeTrack, SectorOrder order, vector<Byte> & inOutBytes, vector<ExportProblem> & inOutProblems);
    static void     ExportSectors      (const DiskAnalysis & analysis, int firstTrack, int lastTrack, SectorOrder order, vector<Byte> & outBytes,
                                        vector<ExportProblem> & outProblems);
    static void     ExportNibbles      (const TrackAnalysis & track, vector<Byte> & outBytes, vector<std::wstring> & outNotes);
    static HRESULT  ExportTrackBits    (const TrackCopy & copy, int quarterTrack, vector<Byte> & outBytes);

    //  "<image> T17 sectors.bin", "<image> T03-T05 sectors.bin",
    //  "<image> T17.25 nibbles.bin" and "<image> T17.25.woz".
    static std::wstring  GetSectorsName (const std::wstring & image, int firstTrack, int lastTrack);
    static std::wstring  GetNibblesName (const std::wstring & image, int quarterTrack);
    static std::wstring  GetBitsName    (const std::wstring & image, int quarterTrack);

private:
    static int           FindSector     (const TrackAnalysis & track, int wholeTrack, SectorOrder order, int position);
    static std::wstring  FormatTrack    (int wholeTrack);
};
