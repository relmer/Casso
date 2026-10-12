#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorWrite and its results
//
//  What any caller hands the shared sector writer: a sector's 256 new bytes
//  and where the sector is. The `disk` command and Casso Explorer give a whole
//  track and a sector number; the disk inspector's editor gives the record and
//  the cell where the data field's prologue starts, so it changes the field it
//  showed, not the first one with that number.
//
//  Strict is the `disk` command's and Explorer's policy: only a track whose 16
//  sectors each decode once with good checksums is changed. Editor is the
//  inspector's: a damaged sector can be repaired, so a bad data checksum is
//  allowed, but noise, nibbles outside the table, a damaged record or an
//  address field that failed its checksum still stop the write.
//
////////////////////////////////////////////////////////////////////////////////

enum class SectorWritePolicy
{
    Strict,
    Editor,
};


enum class ChecksumMode
{
    Recompute,
    KeepStored,
};


struct SectorWrite
{
    int                                              track             = -1;
    int                                              slot              = -1;
    int                                              dataFieldCell     = -1;
    Byte                                             sector            = 0;
    std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes             = {};
    ChecksumMode                                     checksumMode      = ChecksumMode::Recompute;
    bool                                             isAddressCheckOff = false;
};


enum class SectorWriteFailure
{
    NoRecord,
    TrackIncomplete,
    SectorNotFound,
    NoDataField,
    AddressChecksumFailed,
    NoiseInDataField,
    NibblesOutsideTable,
    DamagedRecord,
    NibbleCountDiffers,
    VerifyFailed,
};


struct SectorWriteError
{
    int                 track  = -1;
    int                 slot   = -1;
    Byte                sector = 0;
    SectorWriteFailure  reason = SectorWriteFailure::NoRecord;
};
