#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SectorDecodeReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SectorDecodeCache
//
//  The sectors decoded from a WOZ or NIB image's nibbles, stored by a hash
//  of the file's bytes, so an image read again with the same contents isn't
//  decoded again. Decoding a track with no standard format searches it for
//  every 13-sector variant, which is slow, and the preview, the toolbar,
//  the saved-game reader and the volume facts worker each read the same
//  image. A change to the file changes its hash, so no entry is ever stale.
//
//  The cache has the most recent images, up to kEntries. Safe to call from
//  any thread.
//
////////////////////////////////////////////////////////////////////////////////

class SectorDecodeCache
{
public:
    static bool  Find     (const std::vector<Byte> & fileBytes, HRESULT & outHr, std::vector<Byte> & outSectors, SectorDecodeReport & outReport);
    static bool  Contains (const std::vector<Byte> & fileBytes);
    static void  Store    (const std::vector<Byte> & fileBytes, HRESULT hr, const std::vector<Byte> & sectors, const SectorDecodeReport & report);

    static constexpr size_t  kEntries = 8;

private:
    struct Entry
    {
        uint64_t            hash  = 0;
        size_t              size  = 0;
        HRESULT             hr    = S_OK;
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;
    };

    static uint64_t  Hash (const std::vector<Byte> & bytes);

    static std::mutex          s_lock;
    static std::deque<Entry>   s_entries;   // most recently used first
};
