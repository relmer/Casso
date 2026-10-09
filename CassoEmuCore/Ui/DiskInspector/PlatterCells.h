#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/DiskInspectorPalette.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterCells
//
//  What the platter samples for one ring: a byte per cell holding its kind,
//  and coarser levels for when a pixel covers many cells. Each byte holds the
//  kind's priority above the kind, so the larger of two bytes is the one to
//  draw: a failed checksum over noise, noise over random bits, and those over
//  every other kind (FR-023). Level k holds the largest byte of each run of
//  2^k cells, which keeps a feature narrower than a pixel one pixel wide.
//
////////////////////////////////////////////////////////////////////////////////

class PlatterCells
{
public:
    static constexpr int  kMaxLevels   = 17;
    static constexpr int  kKindBits    = 4;
    static constexpr Byte kKindMask    = 0x0F;

    static Byte         Encode      (PlatterKind kind);
    static PlatterKind  Decode      (Byte value);
    static void         BuildCells  (const TrackAnalysis & analysis, vector<Byte> & outCells);
    static void         BuildLevels (const vector<Byte> & cells, vector<vector<Byte>> & outLevels);
    static PlatterKind  GetKindOf   (NibbleKind kind);
};
