#include "Pch.h"

#include "Machines/Apple2/Common/TrackBitReader.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/FluxBitView.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Read/Find helpers for Denibblize
//
//  Reading nibbles and locating address / data marks in a track's raw
//  bitstream.
//
//  A nibble is not a byte at a known offset. The Disk II shifts bits in until
//  the MSB sets -- that is the state machine's own nibble-complete rule, and
//  it is what makes self-synchronizing sync bytes work -- so a nibble read
//  starts wherever the last one ended and takes however many bits it takes.
//
//  Every read is bounded by ONE REVOLUTION. A track carrying no sync bytes
//  never sets the MSB, and without the bound the search would spin forever on
//  a blank or corrupt track; 0 is the caller's "no nibble".
//
//  The overrun test deliberately sits AFTER the read and outranks a completed
//  nibble. A value whose MSB set on the very read that crossed the revolution
//  boundary is discarded, because its high bits came from before the wrap and
//  the nibble is a splice of two places on the track.
//
//  The bit position is taken by REFERENCE and advanced, so a caller scanning
//  for a mark walks the track continuously instead of restarting each time.
//
//  IT IS PUBLIC BECAUSE THE NIBBLE-IMAGE CODEC NEEDS THE SAME RULE. That codec
//  derives whole bytes from a track to write a .nib back, which is this walk
//  without the sector structure around it. A second implementation of "where
//  does a nibble end" is a second answer waiting to disagree with this one.
//
////////////////////////////////////////////////////////////////////////////////

Byte TrackBitReader::ReadNibbleAt (const DiskImage & img, int track, size_t & bitPos)
{
    FluxBitView  fluxView;
    TrackBits    src = GetTrackBits (img, track, fluxView);



    return ReadNibbleAt (src, bitPos);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReader::GetTrackBits
//
//  The bits a sector reader walks for one slot: a bit track's own buffer, or
//  a flux track decoded into the caller's view at the controller's cell. The
//  view is the caller's so a whole track decode can reuse it.
//
////////////////////////////////////////////////////////////////////////////////

TrackBitReader::TrackBits TrackBitReader::GetTrackBits (
    const DiskImage  &  img,
    int                 track,
    FluxBitView      &  fluxView)
{
    TrackBits  src;



    if (img.GetTrackKind (track) == TrackKind::Flux)
    {
        fluxView.Build (img.GetFluxTrack (track));
        src.bits     = &fluxView.GetBits();
        src.bitCount = fluxView.GetBitCount();
    }
    else if (track >= 0 && track < img.GetTrackCount())
    {
        src.bits     = &img.GetTrackBits (track);
        src.bitCount = img.GetTrackBitCount (track);
    }

    return src;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReader::ReadNibbleAt
//
////////////////////////////////////////////////////////////////////////////////

Byte TrackBitReader::ReadNibbleAt (const TrackBits & src, size_t & bitPos)
{
    static const vector<Byte>  kNoBits;



    const vector<Byte>  &  bits         = (src.bits != nullptr) ? *src.bits : kNoBits;
    size_t                 trackBits    = (src.bits != nullptr) ? src.bitCount : 0;
    size_t                 capacityBits = bits.size() * 8;
    size_t                 start        = bitPos;
    size_t                 pos          = 0;
    Byte                   value        = 0;
    Byte                   bit          = 0;
    bool                   overran      = false;
    bool                   addressable  = trackBits != 0 && trackBits <= capacityBits;



    // Shift in bits until the MSB sets (the LSS's own nibble-complete rule).
    // A full revolution with no MSB means the track carries no sync bytes, so
    // give up rather than spin forever -- 0 is the caller's "no nibble".
    //
    // The overrun test stays AFTER the read and outranks a completed nibble:
    // a value whose MSB set on the very read that crossed the revolution
    // boundary is discarded, because its high bits came from before the wrap.
    //
    // THE WRAP IS A COMPARE, NOT A MODULO, and that is most of what this
    // function costs. Every caller walks the track in order, so the position
    // only ever needs to fall back to zero at the end -- but the general bit
    // accessor cannot know that and divides by the track length on every read,
    // twice over once the caller has taken its own remainder. A division is
    // twenty-odd cycles against the two or three the rest of the loop spends,
    // so a whole disk's decode was paying for arithmetic it had already done.
    if (addressable)
    {
        pos = bitPos % trackBits;

        while ((value & 0x80) == 0 && !overran)
        {
            bit = static_cast<Byte> ((bits[pos >> 3] >> (7 - (pos & 7))) & 1);

            bitPos++;
            pos++;

            if (pos == trackBits)
            {
                pos = 0;
            }

            value   = static_cast<Byte> ((value << 1) | bit);
            overran = (bitPos - start > trackBits);
        }

        if (overran)
        {
            value = 0;
        }
    }
    else if (trackBits != 0)
    {
        // A bit count larger than the buffer behind it should not happen --
        // ResizeTrack sizes the two together -- but SetTrackBitCount can set
        // one without the other, and reading past the end to save a division
        // is not a trade worth making. The general accessor range-checks.
        while ((value & 0x80) == 0 && !overran)
        {
            pos = bitPos % trackBits;
            bit = (pos >> 3 < bits.size()) ? static_cast<Byte> ((bits[pos >> 3] >> (7 - (pos & 7))) & 1) : 0;

            bitPos++;
            value   = static_cast<Byte> ((value << 1) | (bit & 1));
            overran = (bitPos - start > trackBits);
        }

        if (overran)
        {
            value = 0;
        }
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReader::GetBit
//
////////////////////////////////////////////////////////////////////////////////

Byte TrackBitReader::GetBit (const TrackBits & src, size_t bitIndex)
{
    size_t  at = (src.bitCount > 0) ? bitIndex % src.bitCount : 0;



    return (src.bitCount > 0) ? static_cast<Byte> (((*src.bits)[at >> 3] >> (7 - (at & 7))) & 1) : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReader::UnpackBits
//
////////////////////////////////////////////////////////////////////////////////

void TrackBitReader::UnpackBits (
    const vector<Byte>  &  packed,
    size_t                 from,
    size_t                 to,
    vector<uint8_t>     &  outBits)
{
    size_t  i = 0;



    outBits.clear();

    for (i = from; i < to; i++)
    {
        outBits.push_back (static_cast<uint8_t> ((packed[i >> 3] >> (7 - (i & 7))) & 1));
    }
}





