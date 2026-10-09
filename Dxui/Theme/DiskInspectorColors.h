#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectorColors
//
//  The disk inspector's colors, as packed ARGB, held by every DxuiTheme: one
//  per platter kind, sector state, map role, timing extreme, and mark the
//  views draw. A zero color means the inspector uses its fallback for the
//  surface, so a theme that sets none still draws. MakeDark and MakeLight are
//  the two starting sets; every pair of platter kinds, and every pair of map
//  roles, differs by at least Delta E 2000 10 in each.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskInspectorColors
{
    uint32_t  sync             = 0;
    uint32_t  addressMark      = 0;
    uint32_t  addressField     = 0;
    uint32_t  dataMark         = 0;
    uint32_t  dataField        = 0;
    uint32_t  failedChecksum   = 0;
    uint32_t  other            = 0;
    uint32_t  noise            = 0;
    uint32_t  randomBits       = 0;
    uint32_t  nothingRecorded  = 0;

    uint32_t  sectorGood       = 0;
    uint32_t  sectorBad        = 0;
    uint32_t  sectorNotChecked = 0;
    uint32_t  sectorNoData     = 0;

    uint32_t  mapFree          = 0;
    uint32_t  mapBoot          = 0;
    uint32_t  mapVolumeHeader  = 0;
    uint32_t  mapDirectory     = 0;
    uint32_t  mapUnusedCatalog = 0;
    uint32_t  mapBitmap        = 0;
    uint32_t  mapSubdirectory  = 0;
    uint32_t  mapIndex         = 0;
    uint32_t  mapFileData      = 0;
    uint32_t  mapBadBlocks     = 0;
    uint32_t  mapUnowned       = 0;
    uint32_t  mapOwnedFree     = 0;
    uint32_t  mapCrossLinked   = 0;

    uint32_t  timingFast       = 0;
    uint32_t  timingNominal    = 0;
    uint32_t  timingSlow       = 0;

    uint32_t  pendingEdit      = 0;
    uint32_t  difference       = 0;
    uint32_t  damaged          = 0;
    uint32_t  damageHatch      = 0;
    uint32_t  pendingPattern   = 0;
    uint32_t  beyondReach      = 0;
    uint32_t  headIdle         = 0;
    uint32_t  headReading      = 0;
    uint32_t  headWriting      = 0;
    uint32_t  selection        = 0;



    static DiskInspectorColors MakeDark()
    {
        DiskInspectorColors  c;



        c.sync             = 0xFF3A4A5C;
        c.addressMark      = 0xFFE0B040;
        c.addressField     = 0xFFB07CE0;
        c.dataMark         = 0xFF40C0D0;
        c.dataField        = 0xFF4C8CE0;
        c.failedChecksum   = 0xFFE04040;
        c.other            = 0xFF808890;
        c.noise            = 0xFFE070C0;
        c.randomBits       = 0xFF8A6A40;
        c.nothingRecorded  = 0xFF202326;

        c.sectorGood       = 0xFF5FD35F;
        c.sectorBad        = 0xFFFF7070;
        c.sectorNotChecked = 0xFFC8C8C8;
        c.sectorNoData     = 0xFFE0B040;

        c.mapFree          = 0xFF2C3036;
        c.mapBoot          = 0xFF8E6BD8;
        c.mapVolumeHeader  = 0xFFE0A030;
        c.mapDirectory     = 0xFF3FB5C8;
        c.mapUnusedCatalog = 0xFF6E7A70;
        c.mapBitmap        = 0xFFD8D040;
        c.mapSubdirectory  = 0xFF2E8C9E;
        c.mapIndex         = 0xFFE07AB0;
        c.mapFileData      = 0xFF4A8FE0;
        c.mapBadBlocks     = 0xFF8A5A3A;
        c.mapUnowned       = 0xFFA0A0A0;
        c.mapOwnedFree     = 0xFF60C060;
        c.mapCrossLinked   = 0xFFE03C3C;

        c.timingFast       = 0xFF40A0FF;
        c.timingNominal    = 0xFF505860;
        c.timingSlow       = 0xFFFF9030;

        c.pendingEdit      = 0xFFFFD040;
        c.difference       = 0xFFFF60D0;
        c.damaged          = 0xFF702020;
        c.damageHatch      = 0xFFE04040;
        c.pendingPattern   = 0xFF404850;
        c.beyondReach      = 0xA0000000;
        c.headIdle         = 0xFFA0A8B0;
        c.headReading      = 0xFF40E060;
        c.headWriting      = 0xFFFF4040;
        c.selection        = 0xFFFFFFFF;

        return c;
    }



    static DiskInspectorColors MakeLight()
    {
        DiskInspectorColors  c;



        c.sync             = 0xFFB8C4D0;
        c.addressMark      = 0xFFC08A10;
        c.addressField     = 0xFF8A50C8;
        c.dataMark         = 0xFF1898A8;
        c.dataField        = 0xFF2F6FC8;
        c.failedChecksum   = 0xFFD02828;
        c.other            = 0xFF6E767E;
        c.noise            = 0xFFD050A8;
        c.randomBits       = 0xFF9A7040;
        c.nothingRecorded  = 0xFFF0F0F0;

        c.sectorGood       = 0xFF0E5A0E;
        c.sectorBad        = 0xFFC02020;
        c.sectorNotChecked = 0xFF505050;
        c.sectorNoData     = 0xFF805800;

        c.mapFree          = 0xFFEDEFF2;
        c.mapBoot          = 0xFF7A52C8;
        c.mapVolumeHeader  = 0xFFD08A10;
        c.mapDirectory     = 0xFF1FA0B8;
        c.mapUnusedCatalog = 0xFF7A9AA0;
        c.mapBitmap        = 0xFFC8C020;
        c.mapSubdirectory  = 0xFF0F6E80;
        c.mapIndex         = 0xFFD05898;
        c.mapFileData      = 0xFF2F6FC8;
        c.mapBadBlocks     = 0xFF8A5A3A;
        c.mapUnowned       = 0xFF9A9A9A;
        c.mapOwnedFree     = 0xFF3E9E3E;
        c.mapCrossLinked   = 0xFFD02828;

        c.timingFast       = 0xFF1060D0;
        c.timingNominal    = 0xFFD8DCE0;
        c.timingSlow       = 0xFFD06000;

        c.pendingEdit      = 0xFFE0A000;
        c.difference       = 0xFFC030A0;
        c.damaged          = 0xFFF0C0C0;
        c.damageHatch      = 0xFFC02020;
        c.pendingPattern   = 0xFFD0D4D8;
        c.beyondReach      = 0x80FFFFFF;
        c.headIdle         = 0xFF606870;
        c.headReading      = 0xFF108030;
        c.headWriting      = 0xFFC02020;
        c.selection        = 0xFF000000;

        return c;
    }
};
