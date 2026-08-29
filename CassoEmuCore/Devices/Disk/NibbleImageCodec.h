#pragma once

#include "Pch.h"

#include "DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleImageCodec
//
//  Nibble bytes to and from packed bit streams, for headerless nibble disk
//  images (.nib / .nb2). A peer of WozLoader, not a part of
//  NibblizationLayer: that layer converts SECTORS, and this converts the
//  raw byte stream a drive would read.
//
////////////////////////////////////////////////////////////////////////////////

class NibbleImageCodec
{
public:
    static constexpr int     kTrackCount    = 35;
    static constexpr size_t  kNibTrackSize  = 6656;
    static constexpr size_t  kNb2TrackSize  = 6384;
    static constexpr size_t  kNibImageSize  = kNibTrackSize * kTrackCount;
    static constexpr size_t  kNb2ImageSize  = kNb2TrackSize * kTrackCount;
    static constexpr Byte    kSyncNibble    = 0xFF;

    //  Track size a file's total length implies; ERROR_BAD_LENGTH for any other.
    static HRESULT  ResolveGeometry (size_t imageByteSize, size_t & outTrackSize);

    //  Whether the bytes yield a nibble anywhere. Deliberately weak: the
    //  format carries nothing else to check against.
    static bool     HasAnyNibble    (const vector<Byte> & raw);

    static HRESULT  Load            (const vector<Byte> & raw, DiskImage & out);

    //  Clean tracks are copied from sourceBytes; dirty tracks are re-derived.
    static HRESULT  Serialize       (const DiskImage     &  img,
                                     const vector<Byte>  &  sourceBytes,
                                     vector<Byte>        &  out);

private:
    static HRESULT  DeriveTrack     (const DiskImage & img, int track, vector<Byte> & outNibbles);
    static void     RotateGapToEnd  (vector<Byte> & nibbles);
};
