#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IigsResources
//
//  A IIgs resource fork, as the Resource Manager chapter of the Apple IIgs
//  Toolbox Reference sets it out: a header with the map's offset, and the
//  map's index of 20-byte entries, each a type, an ID, and the resource's
//  offset and size in the fork, to an entry of type 0.
//
////////////////////////////////////////////////////////////////////////////////

class IigsResources
{
public:
    static constexpr Word  kStyleBlock = 0x8012;

    //  The resource of a type and ID; false when the fork doesn't have one,
    //  or its index or the resource runs past the fork's end.
    static bool  Find (std::span<const Byte> fork, Word type, uint32_t id, std::span<const Byte> & outResource);

private:
    static constexpr size_t  kMapAt        = 4;
    static constexpr size_t  kToIndex      = 14;
    static constexpr size_t  kEntryBytes   = 20;
    static constexpr size_t  kEntryId      = 2;
    static constexpr size_t  kEntryOffset  = 6;
    static constexpr size_t  kEntrySize    = 12;

    static uint32_t  ReadLong (std::span<const Byte> bytes, size_t at);
};
