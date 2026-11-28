#pragma once

#include "Pch.h"

#include "CassoLink/CassoLinkCodec.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CaptureModuleModel
//
//  What the capture module sends for a read, worked out in C++ from the rules
//  in Capture.s, for a test to compare with what the module sends on an
//  emulated 6502. The values follow the encoder of version 11 step for step:
//  findrev's length, a copy checked for before a repeat, a repeat only within a
//  page, and a token never past RUNMAX bytes or CAPEND.
//
////////////////////////////////////////////////////////////////////////////////

class CaptureModuleModel
{
public:
    static constexpr size_t  kReadBytes  = 0x9000 - 0x5900;
    static constexpr size_t  kPage       = 256;
    static constexpr size_t  kFirstPage  = 0x59;
    static constexpr size_t  kEndPage    = 0x90;
    static constexpr size_t  kMinLength  = 4800;
    static constexpr size_t  kMaxLength  = 7000;
    static constexpr size_t  kSample     = 1024;
    static constexpr size_t  kSampleLen  = 32;
    static constexpr size_t  kRunMax     = 129;
    static constexpr Byte    kTopBit     = 0x80;
    static constexpr Byte    kLowBits    = 0x7F;
    static constexpr Byte    kPacketMark = 0xC1;
    static constexpr size_t  kPayload    = 1 + kPage + 2;

    //  findrev: the first length at which the read's first kSampleLen bytes,
    //  and those from kSample, repeat with the marks ignored; 0 for none.
    static size_t  FindLength (const std::vector<Byte> & read)
    {
        for (size_t length = kMinLength; length <= kMaxLength; length++)
        {
            bool  repeats = true;

            for (size_t at = 0; repeats && at < kSampleLen; at++)
            {
                repeats = ((read[at] ^ read[at + length]) & kLowBits) == 0
                       && ((read[kSample + at] ^ read[kSample + at + length]) & kLowBits) == 0;
            }

            if (repeats)
            {
                return length;
            }
        }

        return 0;
    }

    //  setptrs: the most bytes a token at `at` can have.
    static size_t  GetTokenCap (size_t at)
    {
        size_t  page = kFirstPage + at / kPage;
        size_t  left = kPage - at % kPage;



        if (kEndPage - page >= 2 || left == kPage)
        {
            return kRunMax;
        }

        return std::min (left, kRunMax);
    }

    static bool  AreSameValue (Byte a, Byte b)
    {
        return ((a ^ b) & kLowBits) == 0;
    }

    //  The encoding's length and values, as encode makes them before the marks.
    static std::vector<Byte>  EncodeValues (const std::vector<Byte> & read)
    {
        std::vector<Byte>  out;
        size_t             length    = FindLength (read);
        size_t             copyPage  = length != 0 ? length / kPage + kFirstPage + 1 : 0x100;
        bool               canCopy   = false;
        size_t             at        = 0;



        out.push_back ((Byte) length);
        out.push_back ((Byte) (length >> 8));

        while (at < read.size())
        {
            size_t  cap   = GetTokenCap (at);
            size_t  count = 0;
            Byte    value = (Byte) (read[at] | kTopBit);
            size_t  y     = at % kPage;

            if (canCopy && AreSameValue (read[at], read[at - length]))
            {
                while (count < cap && AreSameValue (read[at + count], read[at + count - length]))
                {
                    count++;
                }

                if (count >= 3)
                {
                    out.push_back (0);
                    out.push_back ((Byte) (count - 2));
                    canCopy = Advance (at, count, copyPage) || canCopy;
                    continue;
                }
            }

            if (y + 2 < kPage && (read[at + 1] | kTopBit) == value && (read[at + 2] | kTopBit) == value)
            {
                count = 1;

                while (count < cap && (read[at + count] | kTopBit) == value)
                {
                    count++;
                }

                if (count >= 3)
                {
                    out.push_back (0);
                    out.push_back ((Byte) ((count - 3) | kTopBit));
                    out.push_back (value);
                    canCopy = Advance (at, count, copyPage) || canCopy;
                    continue;
                }
            }

            out.push_back (value);
            canCopy = Advance (at, 1, copyPage) || canCopy;
        }

        return out;
    }

    //  `at` moved on by `count`; true once `at` is on a page copies may
    //  start from, as newpage sets evalid.
    static bool  Advance (size_t & at, size_t count, size_t copyPage)
    {
        size_t  before = at / kPage;



        at += count;

        return at / kPage != before && kFirstPage + at / kPage >= copyPage;
    }

    //  The marks eight to a byte, the first byte's in the top bit: 1 for a
    //  byte stored with its top bit set.
    static void  AppendMarkBits (const std::vector<Byte> & read, std::vector<Byte> & ioOut)
    {
        for (size_t at = 0; at < read.size(); at += 8)
        {
            Byte  bits = 0;

            for (size_t bit = 0; bit < 8; bit++)
            {
                bits = (Byte) ((bits << 1) | (read[at + bit] >> 7));
            }

            ioOut.push_back (bits);
        }
    }

    //  The bytes on the line for an encoding: a Capture data packet for each
    //  256 bytes, the last padded with zeros.
    static std::vector<Byte>  BuildPackets (std::vector<Byte> encoded)
    {
        std::vector<Byte>  line;
        Byte               check = kPacketMark ^ (Byte) kPayload ^ (Byte) (kPayload >> 8) ^ CassoLinkCodec::kCaptureData;



        encoded.resize ((encoded.size() + kPage - 1) / kPage * kPage, 0);

        for (size_t packet = 0; packet < encoded.size() / kPage; packet++)
        {
            std::span<const Byte>  data (encoded.data() + packet * kPage, kPage);
            Word                   crc  = CassoLinkCodec::ComputeCrc (data);
            Byte                   peor = (Byte) packet;

            line.insert (line.end(), { kPacketMark, (Byte) kPayload, (Byte) (kPayload >> 8), CassoLinkCodec::kCaptureData, check, (Byte) packet });
            line.insert (line.end(), data.begin(), data.end());
            line.push_back ((Byte) crc);
            line.push_back ((Byte) (crc >> 8));

            for (Byte b : data)
            {
                peor ^= b;
            }

            line.push_back ((Byte) (peor ^ (Byte) crc ^ (Byte) (crc >> 8)));
        }

        return line;
    }

    //  A read of a 16-sector track as the read loop stores it: a revolution
    //  of sync bytes, address fields and data fields, over and over from
    //  `first`. Each sync byte is stored marked, and each other byte with
    //  one chance in `strayShare` of a mark, as a read at 8 cycles marks a
    //  byte followed by one zero bit; 0 for none.
    static std::vector<Byte>  MakeRead (size_t first, uint32_t strayShare, uint32_t seed)
    {
        static constexpr int     kSectors   = 16;
        static constexpr size_t  kSyncs     = 12;
        static constexpr size_t  kDataBytes = 343;

        std::vector<Byte>  revolution;
        std::vector<bool>  sync;
        std::vector<Byte>  read;



        auto  next = [&seed] ()
        {
            seed = seed * 1103515245u + 12345u;
            return (uint32_t) (seed >> 16);
        };

        auto  add = [&] (std::initializer_list<Byte> bytes, bool isSync)
        {
            for (Byte b : bytes)
            {
                revolution.push_back (b);
                sync.push_back (isSync);
            }
        };

        for (int number = 0; number < kSectors; number++)
        {
            for (size_t each = 0; each < kSyncs; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0x96, 0xFF, 0xFE, 0xAA, 0xAA, (Byte) (0xAA | (number >> 1)), (Byte) (0xAA | number), 0xFA, 0xFB, 0xDE, 0xAA, 0xEB }, false);

            for (size_t each = 0; each < kSyncs / 2; each++)
            {
                add ({ 0xFF }, true);
            }

            add ({ 0xD5, 0xAA, 0xAD }, false);

            for (size_t each = 0; each < kDataBytes; each++)
            {
                add ({ (Byte) (0x96 + next() % 0x69) }, false);
            }

            add ({ 0xDE, 0xAA, 0xEB }, false);
        }

        for (size_t at = 0; at < kReadBytes; at++)
        {
            size_t  in     = (first + at) % revolution.size();
            bool    marked = sync[in] || (strayShare != 0 && next() % strayShare == 0);

            read.push_back (marked ? (Byte) (revolution[in] & kLowBits) : revolution[in]);
        }

        return read;
    }
};
