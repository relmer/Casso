#pragma once

#include "Pch.h"

#include "Devices/Floppy35/Gcr35Track.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Woz35Builder
//
//  WOZ 2 3.5-inch images for tests, built from block images: each track and
//  side is GCR from Gcr35Track, at TMAP entry track * 2 + side, with INFO
//  marking a 3.5-inch disk of one or two sides.
//
////////////////////////////////////////////////////////////////////////////////

class Woz35Builder
{
public:
    //  `blocks` has 800 blocks for one side or 1,600 for two. TMAP entries in
    //  `omit` are left empty. `diskType` defaults to a 3.5-inch disk.
    static std::vector<Byte> Build (const std::vector<Byte>  & blocks,
                                    const std::set<int>      & omit     = {},
                                    Byte                       diskType = WozLoader::kDiskType35)
    {
        constexpr size_t  kHeader      = 12;
        constexpr size_t  kChunkHeader = 8;
        constexpr size_t  kSidesOffset = 37;
        constexpr size_t  kCrcOffset   = 8;
        constexpr size_t  kCrcBytes    = 4;

        int                             sides  = (blocks.size() / Gcr35Track::kDataBytes == Gcr35Track::kBlocksOneSide) ? 1 : 2;
        Byte                            format = (sides == 1) ? Gcr35Track::kFormatOneSide : Gcr35Track::kFormatTwoSides;
        std::vector<WozSyntheticTrack>  tracks;
        std::vector<Byte>               bits;
        std::vector<Byte>               woz;
        uint32_t                        crc    = 0;



        for (int track = 0; track < Gcr35Track::kTracks; track++)
        {
            for (int side = 0; side < sides; side++)
            {
                size_t             first = (size_t) Gcr35Track::GetFirstBlock (track, side, sides) * Gcr35Track::kDataBytes;
                size_t             count = (size_t) Gcr35Track::GetSectorsPerTrack (track) * Gcr35Track::kDataBytes;
                WozSyntheticTrack  entry;



                if (omit.contains (track * 2 + side))
                {
                    continue;
                }

                Gcr35Track::BuildTrack (track, side, format, std::span<const Byte> (blocks).subspan (first, count), bits);

                entry.data          = Pack (bits);
                entry.bitCount      = bits.size();
                entry.quarterTracks = { track * 2 + side };

                tracks.push_back (std::move (entry));
            }
        }

        Microsoft::VisualStudio::CppUnitTestFramework::Assert::AreEqual (S_OK, WozLoader::BuildSyntheticV21 (tracks, woz));

        woz[kHeader + kChunkHeader + WozLoader::kInfoOffsetDiskType] = diskType;
        woz[kHeader + kChunkHeader + kSidesOffset]                   = (Byte) sides;

        crc = Crc32 (std::span<const Byte> (woz).subspan (kHeader));

        for (size_t i = 0; i < kCrcBytes; i++)
        {
            woz[kCrcOffset + i] = (Byte) (crc >> (8 * i));
        }

        return woz;
    }


    //  Bits, one to a byte, packed top bit first.
    static std::vector<Byte> Pack (const std::vector<Byte> & bits)
    {
        std::vector<Byte>  packed ((bits.size() + 7) / 8, 0);



        for (size_t i = 0; i < bits.size(); i++)
        {
            packed[i / 8] = (Byte) (packed[i / 8] | (bits[i] << (7 - (i % 8))));
        }

        return packed;
    }


    static uint32_t Crc32 (std::span<const Byte> bytes)
    {
        constexpr uint32_t  kPolynomial = 0xEDB88320u;

        uint32_t  crc = 0xFFFFFFFFu;



        for (Byte b : bytes)
        {
            crc ^= b;

            for (int bit = 0; bit < 8; bit++)
            {
                crc = (crc >> 1) ^ ((crc & 1) != 0 ? kPolynomial : 0u);
            }
        }

        return ~crc;
    }
};
