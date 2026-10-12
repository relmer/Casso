#include "Pch.h"

#include "DiskFieldFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  6-and-2 translate table
//
//  Index is a 6-bit value; entry is the nibble written for it. Every entry has
//  its high bit set, at most one pair of adjacent zero bits, and at least two
//  adjacent one bits after the first, which is what the drive can read back.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr std::array<Byte, DiskFieldFormat::kTableSize62> s_kTable62 =
{
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};





////////////////////////////////////////////////////////////////////////////////
//
//  5-and-3 translate table
//
//  Index is a 5-bit value; entry is the nibble written for it. Every entry has
//  its high bit set and no two adjacent zero bits, the rule the 13-sector
//  controller needs. From Apple's 13-sector read/write routines (May 1978,
//  released by the Computer History Museum) and Beneath Apple DOS, chapter 3.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr std::array<Byte, DiskFieldFormat::kTableSize53> s_kTable53 =
{
    0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
    0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
    0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::GetAddressPrologue
//
//  D5 AA 96 for 16-sector address fields, D5 AA B5 for 13-sector ones. The
//  two differ only in the third nibble, which is how a 16-sector drive skips
//  13-sector address fields and the other way round.
//
////////////////////////////////////////////////////////////////////////////////

DiskMarkPattern DiskFieldFormat::GetAddressPrologue (DiskFieldKind kind)
{
    static constexpr Byte  kMark0          = 0xD5;
    static constexpr Byte  kMark1          = 0xAA;
    static constexpr Byte  kSixteenMark2   = 0x96;
    static constexpr Byte  kThirteenMark2  = 0xB5;



    Byte  third = (kind == DiskFieldKind::Sixteen) ? kSixteenMark2 : kThirteenMark2;



    return DiskMarkPattern::MakeExact (kMark0, kMark1, third);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::GetDataPrologue
//
//  D5 AA AD, the same for both formats.
//
////////////////////////////////////////////////////////////////////////////////

DiskMarkPattern DiskFieldFormat::GetDataPrologue()
{
    static constexpr Byte  kMark0 = 0xD5;
    static constexpr Byte  kMark1 = 0xAA;
    static constexpr Byte  kMark2 = 0xAD;



    return DiskMarkPattern::MakeExact (kMark0, kMark1, kMark2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::GetEpilogue
//
//  DE AA. Writers follow it with EB (kEpilogueTail), but no standard reader
//  checks that third nibble, so a field is complete without it.
//
////////////////////////////////////////////////////////////////////////////////

DiskMarkPattern DiskFieldFormat::GetEpilogue()
{
    static constexpr Byte  kMark0 = 0xDE;
    static constexpr Byte  kMark1 = 0xAA;



    return DiskMarkPattern::MakeExact (kMark0, kMark1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::GetBodyLength
//
//  Data nibbles before the checksum nibble: 342 for 6-and-2, 410 for 5-and-3.
//
////////////////////////////////////////////////////////////////////////////////

int DiskFieldFormat::GetBodyLength (DiskFieldKind kind)
{
    return (kind == DiskFieldKind::Sixteen) ? kBodyLength62 : kBodyLength53;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::GetTableSize
//
////////////////////////////////////////////////////////////////////////////////

int DiskFieldFormat::GetTableSize (DiskFieldKind kind)
{
    return (kind == DiskFieldKind::Sixteen) ? kTableSize62 : kTableSize53;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::TranslateValue
//
//  The nibble written for a 6-bit (16-sector) or 5-bit (13-sector) value.
//  Bits above the value's width are ignored.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskFieldFormat::TranslateValue (DiskFieldKind kind, Byte value)
{
    int  index = value % GetTableSize (kind);



    return (kind == DiskFieldKind::Sixteen) ? s_kTable62[index] : s_kTable53[index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::TranslateNibble
//
//  The value a nibble stands for, or kNotInTable when the nibble is not in
//  the format's table.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskFieldFormat::TranslateNibble (DiskFieldKind kind, Byte nibble)
{
    Byte  value = kNotInTable;
    int   size  = GetTableSize (kind);
    int   i     = 0;



    for (i = 0; i < size && value == kNotInTable; i++)
    {
        if (TranslateValue (kind, static_cast<Byte> (i)) == nibble)
        {
            value = static_cast<Byte> (i);
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::Encode44
//
//  4-and-4: the odd bits in the first nibble, the even bits in the second,
//  every other bit of each set so the nibble is always readable.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::Encode44 (Byte value, Byte & outOdd, Byte & outEven)
{
    static constexpr Byte  kClockBits = 0xAA;



    outOdd  = static_cast<Byte> ((value >> 1) | kClockBits);
    outEven = static_cast<Byte> (value | kClockBits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::Decode44
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskFieldFormat::Decode44 (Byte odd, Byte even)
{
    return static_cast<Byte> (((odd << 1) | 1) & even);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::ComputeAddressChecksum
//
//  The address field's checksum is the exclusive OR of volume, track and
//  sector, in both formats.
//
////////////////////////////////////////////////////////////////////////////////

Byte DiskFieldFormat::ComputeAddressChecksum (Byte volume, Byte track, Byte sector)
{
    return static_cast<Byte> (volume ^ track ^ sector);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::EncodeData
//
//  256 bytes to a data field's body nibbles and checksum nibble. The values
//  are laid out in write order, then each nibble is the table entry for the
//  exclusive OR of a value and the one before it; the checksum nibble is the
//  entry for the last value.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::EncodeData (
    DiskFieldKind          kind,
    std::span<const Byte>  bytes,
    std::vector<Byte>    & outBody,
    Byte                 & outChecksumNibble)
{
    std::vector<Byte>  values;
    Byte               prev   = 0;
    size_t             i      = 0;



    if (kind == DiskFieldKind::Sixteen)
    {
        PrenibbleValues62 (bytes, values);
    }
    else
    {
        PrenibbleValues53 (bytes, values);
    }

    outBody.assign (values.size(), 0);

    for (i = 0; i < values.size(); i++)
    {
        outBody[i] = TranslateValue (kind, static_cast<Byte> (values[i] ^ prev));
        prev       = values[i];
    }

    outChecksumNibble = TranslateValue (kind, prev);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::DecodeData
//
//  Undoes the chain. A nibble outside the table is counted and taken as 0, so
//  decoding goes on and the rest of the sector can still be shown; the chain
//  then carries the error forward, which the checksum reports.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::DecodeData (
    DiskFieldKind          kind,
    std::span<const Byte>  body,
    Byte                   checksumNibble,
    DataFieldDecode      & outDecode)
{
    std::vector<Byte>  values (body.size(), 0);
    Byte               value  = 0;
    Byte               prev   = 0;
    size_t             i      = 0;



    outDecode = DataFieldDecode();

    for (i = 0; i < body.size(); i++)
    {
        value = TranslateNibble (kind, body[i]);

        if (value == kNotInTable)
        {
            CountInvalid (body[i], outDecode.invalidNibbles);
            value = 0;
        }

        values[i] = static_cast<Byte> (value ^ prev);
        prev      = values[i];
    }

    outDecode.storedChecksum   = TranslateNibble (kind, checksumNibble);
    outDecode.computedChecksum = prev;
    outDecode.isStoredValid    = outDecode.storedChecksum != kNotInTable;
    outDecode.isChecksumGood   = outDecode.isStoredValid && outDecode.storedChecksum == prev;

    if (kind == DiskFieldKind::Sixteen)
    {
        PostnibbleValues62 (values, outDecode.bytes);
    }
    else
    {
        PostnibbleValues53 (values, outDecode.bytes);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::PrenibbleValues62
//
//  86 values holding the low two bits of three bytes each, then 256 values
//  holding each byte's top six bits. Value i of the first group holds bytes
//  i, i+86 and i+172, each byte's two low bits swapped, in bit pairs 0-1,
//  2-3 and 4-5. This is the order DOS 3.3 writes, matching Casso's
//  nibblization.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::PrenibbleValues62 (std::span<const Byte> bytes, std::vector<Byte> & outValues)
{
    static constexpr int  kAuxCount     = kBodyLength62 - kSectorBytes;
    static constexpr int  kPairsPerAux  = 3;
    static constexpr int  kBitsPerPair  = 2;
    static constexpr int  kHighShift    = 2;



    int   i      = 0;
    int   pair   = 0;
    int   source = 0;
    Byte  low    = 0;
    Byte  aux    = 0;



    outValues.assign (kBodyLength62, 0);

    for (i = 0; i < kAuxCount; i++)
    {
        aux = 0;

        for (pair = 0; pair < kPairsPerAux; pair++)
        {
            source = i + pair * kAuxCount;

            if (source < kSectorBytes)
            {
                low = static_cast<Byte> (((bytes[source] & 1) << 1) | ((bytes[source] >> 1) & 1));
                aux = static_cast<Byte> (aux | (low << (pair * kBitsPerPair)));
            }
        }

        outValues[i] = aux;
    }

    for (i = 0; i < kSectorBytes; i++)
    {
        outValues[kAuxCount + i] = static_cast<Byte> (bytes[i] >> kHighShift);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::PostnibbleValues62
//
//  The inverse of PrenibbleValues62.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::PostnibbleValues62 (const std::vector<Byte> & values, std::array<Byte, kSectorBytes> & outBytes)
{
    static constexpr int  kAuxCount    = kBodyLength62 - kSectorBytes;
    static constexpr int  kBitsPerPair = 2;
    static constexpr int  kHighShift   = 2;



    int   i     = 0;
    int   shift = 0;
    Byte  pair  = 0;



    for (i = 0; i < kSectorBytes; i++)
    {
        shift       = (i / kAuxCount) * kBitsPerPair;
        pair        = static_cast<Byte> (values[i % kAuxCount] >> shift);
        outBytes[i] = static_cast<Byte> ((values[kAuxCount + i] << kHighShift) | ((pair >> 1) & 1) | ((pair & 1) << 1));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::PrenibbleValues53
//
//  The bytes go in 51 groups of five, B0 to B4, plus byte 255. Group i sits
//  at index j = 50 - i of eight areas of 5-bit values. Areas 1 to 5 (the
//  primary buffer, 256 values, the fifth one longer by one) hold each byte's
//  top five bits. Areas 6 to 8 (the secondary buffer, 154 values, the last
//  one longer by one) hold B0, B1 and B2's low three bits in bits 4-2, and
//  bit 2, 1 and 0 respectively of B3 in bit 1 and of B4 in bit 0. Byte 255's
//  top five bits close area 5 and its low three bits close area 8.
//
//  The secondary buffer is written first, from its last value down to its
//  first, then the primary buffer from its first value up. These facts are
//  from Apple's 13-sector read/write routines (May 1978, released by the
//  Computer History Museum); no code was copied.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::PrenibbleValues53 (std::span<const Byte> bytes, std::vector<Byte> & outValues)
{
    static constexpr int   kGroups        = 51;
    static constexpr int   kGroupSize     = 5;
    static constexpr int   kSecondaryLen  = kBodyLength53 - kSectorBytes;
    static constexpr int   kHighShift     = 3;
    static constexpr Byte  kLowMask       = 0x07;
    static constexpr int   kLowShift      = 2;
    static constexpr int   kFourthByte    = 3;
    static constexpr int   kFifthByte     = 4;
    static constexpr int   kAreas         = 3;
    static constexpr int   kLastByte      = kSectorBytes - 1;



    std::array<Byte, kSectorBytes>   primary   = {};
    std::array<Byte, kSecondaryLen>  secondary = {};
    int                              i         = 0;
    int                              j         = 0;
    int                              area      = 0;
    int                              first     = 0;
    int                              bit       = 0;
    Byte                             value     = 0;



    for (i = 0; i < kGroups; i++)
    {
        j     = (kGroups - 1) - i;
        first = i * kGroupSize;

        for (area = 0; area < kGroupSize; area++)
        {
            primary[area * kGroups + j] = static_cast<Byte> (bytes[first + area] >> kHighShift);
        }

        for (area = 0; area < kAreas; area++)
        {
            bit   = (kAreas - 1) - area;
            value = static_cast<Byte> ((bytes[first + area] & kLowMask) << kLowShift);
            value = static_cast<Byte> (value | (((bytes[first + kFourthByte] >> bit) & 1) << 1));
            value = static_cast<Byte> (value | ((bytes[first + kFifthByte] >> bit) & 1));

            secondary[area * kGroups + j] = value;
        }
    }

    primary[kLastByte]           = static_cast<Byte> (bytes[kLastByte] >> kHighShift);
    secondary[kSecondaryLen - 1] = static_cast<Byte> (bytes[kLastByte] & kLowMask);

    outValues.assign (kBodyLength53, 0);

    for (i = 0; i < kSecondaryLen; i++)
    {
        outValues[i] = secondary[(kSecondaryLen - 1) - i];
    }

    for (i = 0; i < kSectorBytes; i++)
    {
        outValues[kSecondaryLen + i] = primary[i];
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::PostnibbleValues53
//
//  The inverse of PrenibbleValues53.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::PostnibbleValues53 (const std::vector<Byte> & values, std::array<Byte, kSectorBytes> & outBytes)
{
    static constexpr int   kGroups        = 51;
    static constexpr int   kGroupSize     = 5;
    static constexpr int   kSecondaryLen  = kBodyLength53 - kSectorBytes;
    static constexpr int   kHighShift     = 3;
    static constexpr int   kLowShift      = 2;
    static constexpr int   kFourthByte    = 3;
    static constexpr int   kFifthByte     = 4;
    static constexpr int   kAreas         = 3;
    static constexpr int   kLastByte      = kSectorBytes - 1;



    std::array<Byte, kSecondaryLen>  secondary = {};
    int                              i         = 0;
    int                              j         = 0;
    int                              area      = 0;
    int                              first     = 0;
    int                              bit       = 0;
    Byte                             aux       = 0;
    Byte                             fourth    = 0;
    Byte                             fifth     = 0;



    for (i = 0; i < kSecondaryLen; i++)
    {
        secondary[(kSecondaryLen - 1) - i] = values[i];
    }

    for (i = 0; i < kGroups; i++)
    {
        j      = (kGroups - 1) - i;
        first  = i * kGroupSize;
        fourth = static_cast<Byte> (values[kSecondaryLen + kFourthByte * kGroups + j] << kHighShift);
        fifth  = static_cast<Byte> (values[kSecondaryLen + kFifthByte * kGroups + j] << kHighShift);

        for (area = 0; area < kAreas; area++)
        {
            bit = (kAreas - 1) - area;
            aux = secondary[area * kGroups + j];

            outBytes[first + area] = static_cast<Byte> ((values[kSecondaryLen + area * kGroups + j] << kHighShift) | (aux >> kLowShift));

            fourth = static_cast<Byte> (fourth | (((aux >> 1) & 1) << bit));
            fifth  = static_cast<Byte> (fifth  | ((aux & 1) << bit));
        }

        outBytes[first + kFourthByte] = fourth;
        outBytes[first + kFifthByte]  = fifth;
    }

    outBytes[kLastByte] = static_cast<Byte> ((values[kSecondaryLen + kLastByte] << kHighShift) | secondary[kSecondaryLen - 1]);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat::CountInvalid
//
//  Adds one to the count for this nibble value, in the order values were
//  first seen.
//
////////////////////////////////////////////////////////////////////////////////

void DiskFieldFormat::CountInvalid (Byte nibble, std::vector<InvalidNibbleCount> & inOutCounts)
{
    auto  it = std::find_if (inOutCounts.begin(), inOutCounts.end(), [nibble] (const InvalidNibbleCount & c) { return c.value == nibble; });



    if (it == inOutCounts.end())
    {
        inOutCounts.push_back ({ nibble, 1 });
    }
    else
    {
        it->count++;
    }
}
