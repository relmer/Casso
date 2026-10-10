#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterKind / MapRole
//
//  What the platter colors a stretch by in Structure mode (FR-023), and the
//  role the map gives a sector of the volume (FR-086).
//
////////////////////////////////////////////////////////////////////////////////

enum class PlatterKind
{
    Sync,
    AddressMark,
    AddressField,
    DataMark,
    DataField,
    FailedChecksum,
    Other,
    Noise,
    RandomBits,
    NothingRecorded,
    Count,
};


enum class MapRole
{
    Free,
    Boot,
    VolumeHeader,
    Directory,
    UnusedCatalog,
    Bitmap,
    Subdirectory,
    Index,
    FileData,
    BadBlocks,
    AllocatedUnowned,
    OwnedFree,
    CrossLinked,
    Count,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorPalette
//
//  The inspector's colors resolved for one theme: the theme's own where it
//  sets them, the fallback for its surface where it does not. States that
//  must not differ by color alone also take a symbol (FR-079).
//
////////////////////////////////////////////////////////////////////////////////

class DiskInspectorPalette
{
public:
    static constexpr size_t  kKindCount     = static_cast<size_t> (PlatterKind::Count);
    static constexpr size_t  kRoleCount     = static_cast<size_t> (MapRole::Count);
    static constexpr size_t  kSectorStates  = 5;

    std::array<uint32_t, kKindCount>     kinds         = {};
    std::array<uint32_t, kRoleCount>     roles         = {};
    std::array<uint32_t, kSectorStates>  sectorStates  = {};
    DiskInspectorColors                  colors;
    uint32_t                             background    = 0;
    uint32_t                             text          = 0;
    bool                                 isDarkSurface = true;

    static DiskInspectorPalette  MakeFallback   (bool isDarkSurface);
    static DiskInspectorPalette  Resolve        (const DxuiTheme & theme);
    static bool                  IsDarkSurface  (uint32_t background);
    static uint32_t              GetTextColorOn (uint32_t background);
    static LPCWSTR               GetStateSymbol (SectorState state);
    static LPCWSTR               GetMarkSymbol  (bool isAddressMark);

    uint32_t  GetKindColor   (PlatterKind kind) const { return kinds[static_cast<size_t> (kind)]; }
    uint32_t  GetRoleColor   (MapRole role)     const { return roles[static_cast<size_t> (role)]; }
    uint32_t  GetStateColor  (SectorState state) const { return sectorStates[static_cast<size_t> (state)]; }

    //  Nominal at zero, blending to fast or slow at the full range.
    uint32_t  GetTimingColor (double deviation, double range) const;

    //  A nibble as the strip and the zoomed platter show it: its kind, or a
    //  failed checksum, or in Timing mode on a flux track its cells' timing.
    uint32_t  GetNibbleColor (const TrackAnalysis & track, int nibble, bool isTimingMode, double range) const;

private:
    static void  Fill (DiskInspectorPalette & inOut);
};
