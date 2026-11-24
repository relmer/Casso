#include "Pch.h"

#include "Machines/Apple2/Common/TrackSectorDecoder.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/NibbleFormat.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoder::PopCount
//
//  Set bits in a 16-sector mask.
//
////////////////////////////////////////////////////////////////////////////////

int TrackSectorDecoder::PopCount (uint16_t mask)
{
    int   count = 0;
    int   bit   = 0;



    for (bit = 0; bit < 16; bit++)
    {
        if ((mask & (1 << bit)) != 0)
        {
            count++;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoder::HasDataField
//
//  Whether one revolution of the track has a data field's prolog, D5 AA AD.
//
////////////////////////////////////////////////////////////////////////////////

bool TrackSectorDecoder::HasDataField (const TrackBitReader::TrackBits & src)
{
    size_t  bitPos = 0;
    Byte    n0     = 0;
    Byte    n1     = 0;
    Byte    n2     = 0;
    bool    found  = false;



    while (!found && src.bitCount != 0 && bitPos < src.bitCount)
    {
        n0    = n1;
        n1    = n2;
        n2    = TrackBitReader::ReadNibbleAt (src, bitPos);
        found = n0 == kAddrProlog0 && n1 == kAddrProlog1 && n2 == kDataProlog2;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoder::DecodeOneSector
//
//  Walks the bit stream until it locates the next address-field prologue,
//  then decodes the V/T/S header followed by the data field. On success
//  fills outSectorData[256] and returns the address-field track/sector via
//  out params.
//
//  Success means VERIFIED, not merely parsed. Four things are checked, three
//  of which the format was already carrying and this code used to discard:
//
//    * the address field's own checksum, which says the sector number can be
//      trusted (it was read into locals and thrown away);
//    * that a data field turns up before the next address field, so a sector
//      whose data is missing cannot silently take its neighbor's;
//    * that every data nibble is a legal 6-and-2 code;
//    * the data field's 343rd checksum nibble, the boot ROM's success gate
//      (it was never read at all).
//
//  Returns E_FAIL if any of those fail, or if no sector can be found before
//  the cursor wraps. The caller distinguishes the two by whether the cursor
//  moved.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TrackSectorDecoder::DecodeOneSector (
    const TrackBitReader::TrackBits  &  src,
    size_t                              &  bitPos,
    Byte                                &  outSector,
    Byte                                *  outData,
    SectorOutcome                       &  outcome,
    size_t                              &  outFieldStart,
    size_t                              &  outDataFieldStart)
{
    HRESULT          hr                                    = S_OK;
    Byte             n0                                    = 0;
    Byte             n1                                    = 0;
    Byte             n2                                    = 0;
    Byte             vOdd                                  = 0;
    Byte             vEven                                 = 0;
    Byte             tOdd                                  = 0;
    Byte             tEven                                 = 0;
    Byte             sOdd                                  = 0;
    Byte             sEven                                 = 0;
    Byte             cOdd                                  = 0;
    Byte             cEven                                 = 0;
    Byte             foundProlog                           = 0;
    Byte             field[NibbleFormat::kDataNibbleCount] = {};
    Byte             addrVolume                            = 0;
    Byte             addrTrack                             = 0;
    Byte             addrChecksum                          = 0;
    DataFieldDecode  decode;
    size_t           posBeforeProlog                       = 0;
    bool             addrChecksumOk                        = false;
    bool             dataFieldMissing                      = false;
    bool             dataTrusted                           = false;
    size_t           i                                     = 0;
    size_t           trackBits                             = src.bitCount;
    size_t           startBitPos                           = bitPos;
    size_t           bitsConsumed                          = 0;
    size_t           addrFieldStart                        = 0;



    CBR (trackBits != 0);

    foundProlog = 0;

    outFieldStart     = SIZE_MAX;
    outDataFieldStart = SIZE_MAX;

    while (foundProlog == 0)
    {
        addrFieldStart = bitPos;
        n0             = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 != kAddrProlog0 && n0 != kAltAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            CBR (bitsConsumed <= trackBits + 8);
            continue;
        }

        n1 = TrackBitReader::ReadNibbleAt (src, bitPos);
        n2 = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n1 == kAddrProlog1 && n2 == kAddrProlog2)
        {
            foundProlog = 1;
        }
    }

    //  WHERE THE HEADER WAS, not where the cursor ended up. The caller bounds
    //  the scan at one revolution and has to test the header's position to do
    //  it: the cursor can still sit inside the gap trailing the last sector, so
    //  bounding on the cursor lets one more attempt wrap onto a header already
    //  read and report it as a duplicate the disk does not have.
    outFieldStart = addrFieldStart;

    vOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    vEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    tOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    tEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    sOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    sEven = TrackBitReader::ReadNibbleAt (src, bitPos);
    cOdd  = TrackBitReader::ReadNibbleAt (src, bitPos);
    cEven = TrackBitReader::ReadNibbleAt (src, bitPos);

    outSector    = DiskFieldFormat::Decode44 (sOdd, sEven);
    addrVolume   = DiskFieldFormat::Decode44 (vOdd, vEven);
    addrTrack    = DiskFieldFormat::Decode44 (tOdd, tEven);
    addrChecksum = DiskFieldFormat::Decode44 (cOdd, cEven);

    // The address field carries its own checksum, and it was being read and
    // thrown away. It is the only thing that says the sector NUMBER can be
    // trusted -- without it a corrupted address field files a good sector's
    // data under whatever number the damage happened to spell.
    //
    // Only the field's internal consistency is checked, not whether its track
    // matches the one being read: half-track and quarter-track images
    // legitimately carry a different number there.
    // Lost until something proves otherwise, so every early exit below
    // classifies correctly without having to remember to say so.
    outcome = SectorOutcome::Lost;

    addrChecksumOk = (addrChecksum == (addrVolume ^ addrTrack ^ outSector));
    BAIL_OUT_IF (!addrChecksumOk, S_OK);

    foundProlog = 0;

    while (foundProlog == 0)
    {
        posBeforeProlog = bitPos;
        n0              = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 != kAddrProlog0 && n0 != kAltAddrProlog0)
        {
            bitsConsumed = (bitPos - startBitPos);
            CBR (bitsConsumed <= trackBits + 8);
            continue;
        }

        n1 = TrackBitReader::ReadNibbleAt (src, bitPos);
        n2 = TrackBitReader::ReadNibbleAt (src, bitPos);

        if (n0 == kAddrProlog0 && n1 == kAddrProlog1 && n2 == kDataProlog2)
        {
            foundProlog = 1;
        }
        else if (n1 == kAddrProlog1 && n2 == kAddrProlog2)
        {
            // The NEXT sector's address field, reached without ever finding
            // this one's data field. Running on to take that sector's data
            // is how one point of damage used to corrupt two sectors: the
            // bytes decode cleanly and pass their own checksum, they are
            // simply filed under the wrong number.
            //
            // Rewind so the caller's next attempt starts on this address
            // field rather than past it, or the good sector after a damaged
            // one is lost too.
            bitPos           = posBeforeProlog;
            dataFieldMissing = true;
            break;
        }
    }

    BAIL_OUT_IF (dataFieldMissing, S_OK);

    // Where this sector's data field begins, which a write that replaces only
    // the data field needs.
    outDataFieldStart = posBeforeProlog;

    for (i = 0; i < NibbleFormat::kDataNibbleCount; i++)
    {
        field[i] = TrackBitReader::ReadNibbleAt (src, bitPos);
    }

    // An illegal nibble makes the sector untrustworthy, but decoding goes on:
    // the bytes before it are the disk's, and a salvage writes them. The
    // 343rd nibble is the data field's checksum, the boot ROM's success gate.
    DiskFieldFormat::DecodeData (DiskFieldKind::Sixteen,
                                 std::span<const Byte> (field, DiskFieldFormat::kBodyLength62),
                                 field[DiskFieldFormat::kBodyLength62],
                                 decode);

    dataTrusted = decode.isChecksumGood && decode.invalidNibbles.empty();
    outcome     = dataTrusted ? SectorOutcome::Verified : SectorOutcome::Recovered;

    memcpy (outData, decode.bytes.data(), NibbleFormat::kSectorByteSize);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackSectorDecoder::FindSectorBase
//
//  The number a track's address fields count their sectors from: 0, or the
//  lowest number when every sector of the track is numbered 16 or more and
//  the track has sixteen of them in a row. Ultima V numbers its sectors
//  $11 to $20 from track 17 on.
//
////////////////////////////////////////////////////////////////////////////////

Byte TrackSectorDecoder::FindSectorBase (const TrackBitReader::TrackBits & src)
{
    size_t         bitPos                                = 0;
    size_t         field                                 = SIZE_MAX;
    size_t         data                                  = SIZE_MAX;
    Byte           sector                                = 0;
    Byte           lowest                                = 0xFF;
    Byte           highest                               = 0;
    Byte           base                                  = 0;
    Byte           buffer[NibbleFormat::kSectorByteSize] = {};
    SectorOutcome  outcome                               = SectorOutcome::Lost;
    bool           low                                   = false;
    bool           seen[256]                             = {};
    int            count                                 = 0;
    bool           more                                  = src.bitCount != 0;



    while (more && bitPos < src.bitCount)
    {
        HRESULT  hr = DecodeOneSector (src, bitPos, sector, buffer, outcome, field, data);

        more = SUCCEEDED (hr) && field < src.bitCount;

        if (more && outcome != SectorOutcome::Lost)
        {
            count        += seen[sector] ? 0 : 1;
            seen[sector]  = true;
            low           = low || sector < NibbleFormat::kSectorsPerTrack;
            lowest        = std::min (lowest, sector);
            highest       = std::max (highest, sector);
        }
    }

    if (!low && count == NibbleFormat::kSectorsPerTrack && highest - lowest == NibbleFormat::kSectorsPerTrack - 1)
    {
        base = lowest;
    }

    return base;
}





