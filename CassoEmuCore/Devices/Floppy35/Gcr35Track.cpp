#include "Pch.h"

#include "Devices/Disk/DiskFieldFormat.h"
#include "Devices/Floppy35/Gcr35Track.h"




//  The field marks.
static constexpr Byte  s_kMark1        = 0xD5;
static constexpr Byte  s_kMark2        = 0xAA;
static constexpr Byte  s_kAddressMark3 = 0x96;
static constexpr Byte  s_kDataMark3    = 0xAD;
static constexpr Byte  s_kSlip1        = 0xDE;
static constexpr Byte  s_kSlip2        = 0xAA;
static constexpr Byte  s_kSixBits      = 0x3F;
static constexpr Byte  s_kSideBit      = 0x20;

//  Revolutions a minute of each zone, outermost first.
static constexpr int   s_kZoneRpm[]    = { 394, 429, 472, 525, 590 };

//  Sync groups before the address field and before the data field.
static constexpr int   s_kSyncBefore   = 6;





////////////////////////////////////////////////////////////////////////////////
//
//  GetSectorsPerTrack
//
////////////////////////////////////////////////////////////////////////////////

int Gcr35Track::GetSectorsPerTrack (int track)
{
    constexpr int  kOuterSectors = 12;



    return kOuterSectors - track / kTracksPerZone;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetTrackBits
//
//  The 2-microsecond cells in one revolution at the track's zone speed.
//
////////////////////////////////////////////////////////////////////////////////

size_t Gcr35Track::GetTrackBits (int track)
{
    constexpr uint64_t  kCellsPerMinute = 30'000'000;



    return (size_t) (kCellsPerMinute / (uint64_t) s_kZoneRpm[track / kTracksPerZone]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFirstBlock
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Gcr35Track::GetFirstBlock (int track, int side, int sides)
{
    uint32_t  block = 0;



    for (int t = 0; t < track; t++)
    {
        block += (uint32_t) (GetSectorsPerTrack (t) * sides);
    }

    return block + (uint32_t) (side * GetSectorsPerTrack (track));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInterleave
//
//  Each sector two places after the last, moving on past places taken.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> Gcr35Track::GetInterleave (int sectors)
{
    std::vector<int>   order (sectors, -1);
    int                place = 0;



    for (int sector = 0; sector < sectors; sector++)
    {
        while (order[place] >= 0)
        {
            place = (place + 1) % sectors;
        }

        order[place] = sector;
        place        = (place + 2) % sectors;
    }

    return order;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EncodeSector
//
//  Each group of three bytes is XORed with three running checksums and
//  written as four nibbles: one of the bytes' top two bits, then one of each
//  byte's low six bits. An overflow of one checksum is added to the next.
//  The last group has two bytes. The three checksums follow in the same
//  form.
//
////////////////////////////////////////////////////////////////////////////////

void Gcr35Track::EncodeSector (std::span<const Byte> bytes, std::vector<Byte> & outNibbles)
{
    uint32_t  c1 = 0;
    uint32_t  c2 = 0;
    uint32_t  c3 = 0;
    size_t    i  = 0;



    outNibbles.clear();

    while (i < kSectorBytes)
    {
        Byte  w1   = 0;
        Byte  w2   = 0;
        Byte  w3   = 0;
        bool  last = false;



        c1 = (c1 & 0xFF) << 1;

        if ((c1 & 0x100) != 0)
        {
            c1++;
        }

        c3 += bytes[i];

        if ((c1 & 0x100) != 0)
        {
            c3++;
            c1 &= 0xFF;
        }

        w1 = (Byte) (bytes[i++] ^ c1);

        c2 += bytes[i];

        if (c3 > 0xFF)
        {
            c2++;
            c3 &= 0xFF;
        }

        w2   = (Byte) (bytes[i++] ^ c3);
        last = i == kSectorBytes;

        if (!last)
        {
            c1 += bytes[i];

            if (c2 > 0xFF)
            {
                c1++;
                c2 &= 0xFF;
            }

            w3 = (Byte) (bytes[i++] ^ c2);
        }

        outNibbles.push_back (GetGcr ((Byte) (((w1 >> 2) & 0x30) | ((w2 >> 4) & 0x0C) | ((w3 >> 6) & 0x03))));
        outNibbles.push_back (GetGcr (w1));
        outNibbles.push_back (GetGcr (w2));

        if (!last)
        {
            outNibbles.push_back (GetGcr (w3));
        }
    }

    outNibbles.push_back (GetGcr ((Byte) (((c3 >> 2) & 0x30) | ((c2 >> 4) & 0x0C) | ((c1 >> 6) & 0x03))));
    outNibbles.push_back (GetGcr ((Byte) c3));
    outNibbles.push_back (GetGcr ((Byte) c2));
    outNibbles.push_back (GetGcr ((Byte) c1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryDecodeSector
//
//  The reverse of EncodeSector, checking the three checksums.
//
////////////////////////////////////////////////////////////////////////////////

bool Gcr35Track::TryDecodeSector (std::span<const Byte> nibbles, std::vector<Byte> & outBytes)
{
    std::vector<Byte>  values;
    uint32_t           c1 = 0;
    uint32_t           c2 = 0;
    uint32_t           c3 = 0;
    size_t             n  = 0;



    if (nibbles.size() < kDataNibbles + kSumNibbles)
    {
        return false;
    }

    for (size_t k = 0; k < kDataNibbles + kSumNibbles; k++)
    {
        Byte  value = DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, nibbles[k]);



        if (value == DiskFieldFormat::kNotInTable)
        {
            return false;
        }

        values.push_back (value);
    }

    outBytes.clear();

    while (outBytes.size() < kSectorBytes)
    {
        Byte  hi   = values[n++];
        Byte  w1   = (Byte) (values[n++] | ((hi << 2) & 0xC0));
        Byte  w2   = (Byte) (values[n++] | ((hi << 4) & 0xC0));
        Byte  v    = 0;
        bool  last = outBytes.size() + 2 == kSectorBytes;



        c1 = (c1 & 0xFF) << 1;

        if ((c1 & 0x100) != 0)
        {
            c1++;
        }

        v   = (Byte) (w1 ^ c1);
        c3 += v;

        if ((c1 & 0x100) != 0)
        {
            c3++;
            c1 &= 0xFF;
        }

        outBytes.push_back (v);

        v   = (Byte) (w2 ^ c3);
        c2 += v;

        if (c3 > 0xFF)
        {
            c2++;
            c3 &= 0xFF;
        }

        outBytes.push_back (v);

        if (!last)
        {
            Byte  w3 = (Byte) (values[n++] | ((hi << 6) & 0xC0));



            v   = (Byte) (w3 ^ c2);
            c1 += v;

            if (c2 > 0xFF)
            {
                c1++;
                c2 &= 0xFF;
            }

            outBytes.push_back (v);
        }
    }

    {
        Byte  hi = values[n++];
        Byte  s3 = (Byte) (values[n++] | ((hi << 2) & 0xC0));
        Byte  s2 = (Byte) (values[n++] | ((hi << 4) & 0xC0));
        Byte  s1 = (Byte) (values[n++] | ((hi << 6) & 0xC0));



        return s1 == (Byte) c1 && s2 == (Byte) c2 && s3 == (Byte) c3;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildTrack
//
//  Each sector in interleave order: sync, the address field, sync, the data
//  field; the rest of the revolution is sync.
//
////////////////////////////////////////////////////////////////////////////////

void Gcr35Track::BuildTrack (int track, int side, Byte format, std::span<const Byte> sectorData, std::vector<Byte> & outBits)
{
    int                sectors  = GetSectorsPerTrack (track);
    size_t             total    = GetTrackBits (track);
    Byte               track6   = (Byte) (track & s_kSixBits);
    Byte               sideByte = (Byte) ((side != 0 ? s_kSideBit : 0) | (track >> 6));
    std::vector<Byte>  sector (kSectorBytes, 0);
    std::vector<Byte>  nibbles;



    outBits.clear();
    outBits.reserve (total);

    for (int sectorNumber : GetInterleave (sectors))
    {
        Byte  checksum = (Byte) ((track6 ^ sectorNumber ^ sideByte ^ format) & s_kSixBits);



        AppendSync (outBits, s_kSyncBefore);

        for (Byte value : { s_kMark1, s_kMark2, s_kAddressMark3 })
        {
            AppendByte (outBits, value);
        }

        for (Byte value : { track6, (Byte) sectorNumber, sideByte, format, checksum })
        {
            AppendByte (outBits, GetGcr (value));
        }

        AppendByte (outBits, s_kSlip1);
        AppendByte (outBits, s_kSlip2);
        AppendSync (outBits, s_kSyncBefore);

        for (Byte value : { s_kMark1, s_kMark2, s_kDataMark3 })
        {
            AppendByte (outBits, value);
        }

        AppendByte (outBits, GetGcr ((Byte) sectorNumber));

        std::fill (sector.begin(), sector.end(), (Byte) 0);
        std::copy_n (sectorData.begin() + (ptrdiff_t) ((size_t) sectorNumber * kDataBytes), kDataBytes, sector.begin() + kTagBytes);

        EncodeSector (sector, nibbles);

        for (Byte nibble : nibbles)
        {
            AppendByte (outBits, nibble);
        }

        AppendByte (outBits, s_kSlip1);
        AppendByte (outBits, s_kSlip2);
    }

    while (outBits.size() + 10 <= total)
    {
        AppendSync (outBits, 1);
    }

    outBits.resize (total, 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DecodeTrack
//
//  The track's nibbles, read twice round so a sector across the index is
//  whole, searched for address fields of this track and side, each followed
//  by its data field.
//
////////////////////////////////////////////////////////////////////////////////

void Gcr35Track::DecodeTrack (int track, int side, const std::vector<Byte> & bits,
                              std::vector<Byte> & outSectorData, std::vector<bool> & outFound)
{
    constexpr size_t   kDataSearch = 64;
    int                sectors     = GetSectorsPerTrack (track);
    Byte               track6      = (Byte) (track & s_kSixBits);
    Byte               sideByte    = (Byte) ((side != 0 ? s_kSideBit : 0) | (track >> 6));
    std::vector<Byte>  nibbles;
    std::vector<Byte>  decoded;
    Byte               shift       = 0;



    outSectorData.assign ((size_t) sectors * kDataBytes, 0);
    outFound.assign ((size_t) sectors, false);

    for (size_t k = 0; k < bits.size() * 2; k++)
    {
        shift = (Byte) ((shift << 1) | bits[k % bits.size()]);

        if ((shift & 0x80) != 0)
        {
            nibbles.push_back (shift);
            shift = 0;
        }
    }

    for (size_t at = 0; at + 10 < nibbles.size(); at++)
    {
        Byte  values[5] = {};
        bool  ok        = nibbles[at] == s_kMark1 && nibbles[at + 1] == s_kMark2 && nibbles[at + 2] == s_kAddressMark3;



        for (size_t v = 0; ok && v < 5; v++)
        {
            values[v] = DiskFieldFormat::TranslateNibble (DiskFieldKind::Sixteen, nibbles[at + 3 + v]);
            ok        = values[v] != DiskFieldFormat::kNotInTable;
        }

        if (!ok || values[0] != track6 || values[2] != sideByte || values[1] >= sectors ||
            values[4] != ((values[0] ^ values[1] ^ values[2] ^ values[3]) & s_kSixBits) || outFound[values[1]])
        {
            continue;
        }

        for (size_t d = at + 8; d + 4 + kDataNibbles + kSumNibbles < nibbles.size() && d < at + 8 + kDataSearch; d++)
        {
            if (nibbles[d] != s_kMark1 || nibbles[d + 1] != s_kMark2 || nibbles[d + 2] != s_kDataMark3)
            {
                continue;
            }

            if (TryDecodeSector (std::span<const Byte> (nibbles).subspan (d + 4, kDataNibbles + kSumNibbles), decoded))
            {
                std::copy_n (decoded.begin() + kTagBytes, kDataBytes, outSectorData.begin() + (ptrdiff_t) ((size_t) values[1] * kDataBytes));
                outFound[values[1]] = true;
            }

            break;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendByte
//
////////////////////////////////////////////////////////////////////////////////

void Gcr35Track::AppendByte (std::vector<Byte> & bits, Byte value)
{
    for (int bit = 7; bit >= 0; bit--)
    {
        bits.push_back ((Byte) ((value >> bit) & 1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AppendSync
//
//  Self-sync groups: $FF and two zero cells, ten cells each.
//
////////////////////////////////////////////////////////////////////////////////

void Gcr35Track::AppendSync (std::vector<Byte> & bits, int count)
{
    constexpr Byte  kSync = 0xFF;



    for (int i = 0; i < count; i++)
    {
        AppendByte (bits, kSync);
        bits.push_back (0);
        bits.push_back (0);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetGcr
//
////////////////////////////////////////////////////////////////////////////////

Byte Gcr35Track::GetGcr (Byte value)
{
    return DiskFieldFormat::TranslateValue (DiskFieldKind::Sixteen, (Byte) (value & s_kSixBits));
}
