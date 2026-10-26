#pragma once

#include "Pch.h"

#include "IDiskImage.h"
#include "Machines/Apple2/Common/WozCompatibility.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BayStatus
//
//  A copy of one bay for display: whether a disk is in it, the file, the
//  format, every write-protect cause, the salvage verdict and the machines
//  and memory a WOZ image declares. It has no
//  reference into the store or the image, so a copy remains valid after its
//  disk is ejected.
//
////////////////////////////////////////////////////////////////////////////////

struct BayStatus
{
    bool              isMounted        = false;
    std::string       path;
    DiskFormat        format           = DiskFormat::Dsk;
    WriteProtectInfo  writeProtect;
    bool              isSalvageOffered = false;
    WozRequirements   wozRequirements;

    bool  operator== (const BayStatus &) const = default;
};