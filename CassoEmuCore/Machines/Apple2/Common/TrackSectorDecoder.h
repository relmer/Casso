#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/TrackBitReader.h"





//
//  What became of one sector.
//
//    Verified   both checksums matched. The bytes are the disk's.
//    Recovered  the data field decoded but did not verify -- a failed data
//               checksum or an illegal 6-and-2 nibble. The bytes are usable
//               but may contain errors, and how many is unknowable: the
//               decode is a running XOR chain, so a single bad nibble skews
//               every byte after it by one constant delta. Worth keeping
//               anyway. A sector that fails its checksum makes DOS report an
//               I/O error, which can cost a whole file; the same sector with
//               a recomputed checksum reads, and may even be perfect (if what
//               rotted was the check nibble itself, the 342 data nibbles are
//               untouched).
//    Lost       nothing usable. Either no data field was found, or the
//               ADDRESS field failed its checksum -- and that one cannot be
//               recovered on principle, because an untrustworthy sector
//               number means writing the data anywhere risks overwriting a
//               good sector.
//
enum class SectorOutcome
{
    Verified,
    Recovered,
    Lost
};





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoder
//
//  Finds and decodes the 16-sector address and data fields on one track's
//  bits.
//
////////////////////////////////////////////////////////////////////////////////

class TrackSectorDecoder
{
public:
    //  The next sector from bitPos on: its address field's sector number,
    //  256 bytes into outData, and where its fields start.
    static HRESULT  DecodeOneSector (const TrackBitReader::TrackBits  &  src,
                                     size_t                           &  bitPos,
                                     Byte                             &  outSector,
                                     Byte                             *  outData,
                                     SectorOutcome                    &  outcome,
                                     size_t                           &  outFieldStart,
                                     size_t                           &  outDataFieldStart);

    static bool     HasDataField    (const TrackBitReader::TrackBits & src);
    static Byte     FindSectorBase  (const TrackBitReader::TrackBits & src);
    static int      PopCount        (uint16_t mask);

private:
    static constexpr Byte  kAddrProlog0    = 0xD5;
    static constexpr Byte  kAltAddrProlog0 = 0xD4;   // Ultima I's address fields
    static constexpr Byte  kAddrProlog1    = 0xAA;
    static constexpr Byte  kAddrProlog2    = 0x96;
    static constexpr Byte  kDataProlog2    = 0xAD;
};