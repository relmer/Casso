#pragma once

#include "Pch.h"





class DiskImage;
class FluxBitView;





////////////////////////////////////////////////////////////////////////////////
//
//  TrackBitReader
//
//  Reads one track's bits and nibbles: a bit track's buffer, or a flux track
//  decoded at the controller's cell.
//
////////////////////////////////////////////////////////////////////////////////

class TrackBitReader
{
public:
    //  The bits a sector reader walks for one track: a bit track's buffer, or
    //  a flux track decoded at the controller's cell. Null when there are none.
    struct TrackBits
    {
        const vector<Byte> *  bits     = nullptr;
        size_t                bitCount = 0;
    };

    //  One nibble read off a track: shift bits until the high bit sets, which
    //  is the sequencer's own rule for a complete nibble. Advances bitPos;
    //  returns 0 when a whole revolution carries none. Shared with the nibble
    //  image codec so both agree where a nibble ends.
    static Byte       ReadNibbleAt (const DiskImage & img, int track, size_t & bitPos);
    static Byte       ReadNibbleAt (const TrackBits & src, size_t & bitPos);

    //  The bits for one track, decoding a flux track into fluxView. The
    //  result points into fluxView, so it lives only as long as the view.
    static TrackBits  GetTrackBits (const DiskImage & img, int track, FluxBitView & fluxView);

    static Byte       GetBit       (const TrackBits & src, size_t bitIndex);

    //  The bits a field encoder produced, one per entry, from bit `from`.
    static void       UnpackBits   (const vector<Byte>  &  packed,
                                    size_t                 from,
                                    size_t                 to,
                                    vector<uint8_t>     &  outBits);
};
