#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/NibblizationLayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayout
//
//  A real disk's track as measured on the Apple: the address fields'
//  volume, the nibbles and bits in one turn, and the bit sector 0's address
//  field begins at. A turn's bits follow from the drive's speed: a bit is
//  4 cycles of the Apple's clock. A layout with no nibbles has the volume
//  alone.
//
////////////////////////////////////////////////////////////////////////////////

struct TrackLayout
{
    Byte       volume  = NibblizationLayer::kDefaultVolume;
    int        nibbles = 0;
    int        bits    = 0;
    long long  sector0 = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  TrackLayoutWriter
//
//  A DOS 3.3 track built from 16 sectors to a TrackLayout, and a WOZ of 35
//  such tracks. Each sector is its address field, a gap of kDataGapSyncs
//  10-bit sync nibbles and its data field; the gap before each address field
//  takes the nibbles and bits the layout has beyond those, as 8-bit FF
//  nibbles and then 10-bit sync nibbles, with at least kMinGapSyncs syncs.
//  A title that measures the time from sector 0 on one track to sector 0
//  on the next, as Wizardry I does, measures the real disk's time on such
//  a WOZ. Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class TrackLayoutWriter
{
public:
    //  The track's bits, one per element; ERROR_INVALID_DATA for a layout
    //  whose nibbles and bits are too few for 16 sectors and their gaps.
    static HRESULT  BuildTrack (const std::vector<Byte> & sectors, int track, const TrackLayout & layout, std::vector<uint8_t> & outBits);

    //  A WOZ from a 140K image in DOS logical order and a layout for each
    //  track. A track whose layout has no nibbles gets kDefaultNibbles and
    //  kDefaultBits, with its sector 0 at the start of the turn.
    static HRESULT  BuildWoz   (const std::vector<Byte> & sectors, const std::vector<TrackLayout> & layouts, std::vector<Byte> & outWoz);

    static constexpr int  kFieldNibbles   = 369;      // a sector's fields and its data gap
    static constexpr int  kFieldBits      = kFieldNibbles * 8 + 12;
    static constexpr int  kDataGapSyncs   = 6;
    static constexpr int  kMinGapSyncs    = 5;
    static constexpr int  kDefaultNibbles = 6300;
    static constexpr int  kDefaultBits    = 51024;    // a turn at 300 rpm

private:
    static void  PutNibble (std::vector<uint8_t> & ioBits, Byte nibble, int zeros);
    static void  PutSector (std::vector<uint8_t> & ioBits, const std::vector<Byte> & sectors, int track, int physical, Byte volume);
};
