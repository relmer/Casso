#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFieldWriter.h"
#include "Devices/Disk/DiskFieldFormat.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"





static constexpr Byte  kAddrProlog0 = NibblizationLayer::kProlog0;
static constexpr Byte  kAddrProlog1 = NibblizationLayer::kProlog1;
static constexpr Byte  kAddrProlog2 = NibblizationLayer::kAddressProlog2;
static constexpr Byte  kDataProlog2 = NibblizationLayer::kDataProlog2;
static constexpr Byte  kEpilog0     = 0xDE;
static constexpr Byte  kEpilog1     = 0xAA;
static constexpr Byte  kEpilog2     = 0xEB;
static constexpr Byte  kSyncNibble  = 0xFF;





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter::Encode44Odd / Encode44Even
//
////////////////////////////////////////////////////////////////////////////////

Byte NibbleFieldWriter::Encode44Odd (Byte val)
{
    return static_cast<Byte> ((val >> 1) | 0xAA);
}


Byte NibbleFieldWriter::Encode44Even (Byte val)
{
    return static_cast<Byte> (val | 0xAA);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter::PackNibbleBits
//
//  Append 8 bits (MSB-first) of `nibble` into a packed bit-stream byte
//  vector at the given bit offset. Caller pre-sized the destination.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleFieldWriter::PackNibbleBits (vector<Byte> & dst, size_t & bitOffset, Byte nibble)
{
    int    bit  = 0;
    Byte   b    = 0;
    Byte   mask = 0;



    for (bit = 7; bit >= 0; bit--)
    {
        b    = static_cast<Byte> ((nibble >> bit) & 1);
        mask = static_cast<Byte> (b << (7 - (bitOffset & 7)));
        dst[bitOffset >> 3] = static_cast<Byte> (dst[bitOffset >> 3] | mask);
        bitOffset++;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter::PackSyncNibbleBits
//
//  Real Disk II hardware writes a sync nibble (0xFF) as a 10-bit pattern
//  on the disk: the 8 nibble bits followed by 2 zero "sync-gap" bits. The
//  zero gap tail makes the next nibble's MSB-set bit arrive ~2 bit-times
//  later than a byte boundary would, which intentionally shifts the
//  Disk II read latch's bit alignment. After enough sync nibbles in a
//  row the latch is guaranteed to be aligned on real nibble MSBs, which
//  is how the boot ROM's $C65E loop synchronizes on the D5 prologue.
//
//  Without these gap bits, an 8-bit-only sync run is byte-aligned with
//  the rest of the bit stream so the latch can lock onto a rotation that
//  *never* contains a 0xD5 nibble -- the boot ROM spins forever.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleFieldWriter::PackSyncNibbleBits (vector<Byte> & dst, size_t & bitOffset)
{
    PackNibbleBits (dst, bitOffset, kSyncNibble);
    bitOffset += kSyncTailZeros;          // bits are zero-initialized
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter::AppendAddressField
//
//  Emits sync gap + address prologue + 4-and-4 V/T/S/checksum + epilogue
//  for one sector.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleFieldWriter::AppendAddressField (
    vector<Byte>   &  dst,
    size_t         &  bitOffset,
    Byte              volume,
    Byte              track,
    Byte              sector)
{
    Byte    checksum = static_cast<Byte> (volume ^ track ^ sector);
    int     i        = 0;



    for (i = 0; i < kAddrPrologueGap; i++)
    {
        PackSyncNibbleBits (dst, bitOffset);
    }

    PackNibbleBits (dst, bitOffset, kAddrProlog0);
    PackNibbleBits (dst, bitOffset, kAddrProlog1);
    PackNibbleBits (dst, bitOffset, kAddrProlog2);
    PackNibbleBits (dst, bitOffset, Encode44Odd  (volume));
    PackNibbleBits (dst, bitOffset, Encode44Even (volume));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (track));
    PackNibbleBits (dst, bitOffset, Encode44Even (track));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (sector));
    PackNibbleBits (dst, bitOffset, Encode44Even (sector));
    PackNibbleBits (dst, bitOffset, Encode44Odd  (checksum));
    PackNibbleBits (dst, bitOffset, Encode44Even (checksum));
    PackNibbleBits (dst, bitOffset, kEpilog0);
    PackNibbleBits (dst, bitOffset, kEpilog1);
    PackNibbleBits (dst, bitOffset, kEpilog2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleFieldWriter::AppendDataField
//
//  Emits sync gap + data prologue + 6-and-2 encoded payload + checksum +
//  epilogue.
//
////////////////////////////////////////////////////////////////////////////////

void NibbleFieldWriter::AppendDataField (
    vector<Byte>   &  dst,
    size_t         &  bitOffset,
    const Byte     *  sectorData)
{
    vector<Byte>  nibbles;
    Byte          checksum = 0;
    int           i        = 0;



    for (i = 0; i < kDataPrologueGap; i++)
    {
        PackSyncNibbleBits (dst, bitOffset);
    }

    PackNibbleBits (dst, bitOffset, kAddrProlog0);
    PackNibbleBits (dst, bitOffset, kAddrProlog1);
    PackNibbleBits (dst, bitOffset, kDataProlog2);

    DiskFieldFormat::EncodeData (DiskFieldKind::Sixteen, std::span<const Byte> (sectorData, NibblizationLayer::kSectorByteSize), nibbles, checksum);
    nibbles.push_back (checksum);

    for (Byte nibble : nibbles)
    {
        PackNibbleBits (dst, bitOffset, nibble);
    }

    PackNibbleBits (dst, bitOffset, kEpilog0);
    PackNibbleBits (dst, bitOffset, kEpilog1);
    PackNibbleBits (dst, bitOffset, kEpilog2);
}
