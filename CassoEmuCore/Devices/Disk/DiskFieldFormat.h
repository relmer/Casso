#pragma once

#include "Pch.h"

#include "../CassoCore/DiskFieldKind.h"
#include "../CassoCore/DiskMarkPattern.h"





struct InvalidNibbleCount;
struct DataFieldDecode;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldFormat
//
//  The standard Disk II field formats in one place: the marks, 4-and-4 for
//  address fields, and 6-and-2 (16-sector) and 5-and-3 (13-sector) for data
//  fields, with their translate tables and checksums.
//
//  A data field's body is a chain of values, each written as the translate
//  table's nibble for it exclusive-ORed with the value before it, and the
//  checksum nibble is the table's nibble for the last value. Decoding undoes
//  the chain; the field is good when the last decoded value equals the
//  checksum nibble's value.
//
//  Built once for the disk inspector, the shared sector writer and the
//  debugger's disk breakpoints, so the three never disagree about a field.
//
////////////////////////////////////////////////////////////////////////////////

class DiskFieldFormat
{
public:
    static constexpr int   kSectorBytes      = 256;
    static constexpr int   kBodyLength62     = 342;
    static constexpr int   kBodyLength53     = 410;
    static constexpr int   kTableSize62      = 64;
    static constexpr int   kTableSize53      = 32;
    static constexpr int   kAddressNibbles   = 8;
    static constexpr Byte  kNotInTable       = 0xFF;
    static constexpr Byte  kEpilogueTail     = 0xEB;

    static DiskMarkPattern  GetAddressPrologue     (DiskFieldKind kind);
    static DiskMarkPattern  GetDataPrologue        ();
    static DiskMarkPattern  GetEpilogue            ();
    static int              GetBodyLength          (DiskFieldKind kind);
    static int              GetTableSize           (DiskFieldKind kind);
    static Byte             TranslateValue         (DiskFieldKind kind, Byte value);
    static Byte             TranslateNibble        (DiskFieldKind kind, Byte nibble);

    static void             Encode44               (Byte value, Byte & outOdd, Byte & outEven);
    static Byte             Decode44               (Byte odd, Byte even);
    static Byte             ComputeAddressChecksum (Byte volume, Byte track, Byte sector);

    static void             EncodeData             (DiskFieldKind          kind,
                                                    std::span<const Byte>  bytes,
                                                    std::vector<Byte>    & outBody,
                                                    Byte                 & outChecksumNibble);
    static void             DecodeData             (DiskFieldKind          kind,
                                                    std::span<const Byte>  body,
                                                    Byte                   checksumNibble,
                                                    DataFieldDecode      & outDecode);

private:
    static void  PrenibbleValues62  (std::span<const Byte> bytes, std::vector<Byte> & outValues);
    static void  PrenibbleValues53  (std::span<const Byte> bytes, std::vector<Byte> & outValues);
    static void  PostnibbleValues62 (const std::vector<Byte> & values, std::array<Byte, kSectorBytes> & outBytes);
    static void  PostnibbleValues53 (const std::vector<Byte> & values, std::array<Byte, kSectorBytes> & outBytes);
    static void  CountInvalid       (Byte nibble, std::vector<InvalidNibbleCount> & inOutCounts);
};





////////////////////////////////////////////////////////////////////////////////
//
//  DataFieldDecode
//
//  What decoding one data field's body and checksum nibble gave. The bytes
//  are decoded even when the checksum fails or a nibble is outside the
//  translate table, so a damaged sector can still be shown and repaired.
//
////////////////////////////////////////////////////////////////////////////////

struct InvalidNibbleCount
{
    Byte  value = 0;
    int   count = 0;
};


struct DataFieldDecode
{
    std::array<Byte, DiskFieldFormat::kSectorBytes>  bytes            = {};
    Byte                                             storedChecksum   = 0;
    Byte                                             computedChecksum = 0;
    bool                                             isStoredValid    = false;
    bool                                             isChecksumGood   = false;
    std::vector<InvalidNibbleCount>                  invalidNibbles;
};
