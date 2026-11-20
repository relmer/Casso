#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkClient
//
//  Casso's client as its last Ready packet reported it, and the Status packet
//  that ends each of its operations. A shim with no program loaded reports
//  program version 0.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoLinkClient
{
    Word  programVersion = 0;
    Byte  shimVersion    = 0;
    Byte  driveMap       = 0;

    //  The program's Ready has the ROM's identification bytes: $FBB3,
    //  $FBC0, $FBBF, $FB1E, and 1 for a IIgs. The shim's has none.
    bool                 hasRomId = false;
    std::array<Byte, 5>  romId    = {};

    //  The CRC-16/XMODEM the program computed over itself as loaded, which
    //  the host compares with the image it sent. Earlier programs sent none.
    bool                 hasProgramCrc = false;
    Word                 programCrc    = 0;

    //  The SOS program's Ready, from an Apple ///.
    bool                 isSos = false;

    //  The model the bytes identify, as Apple //c (ROM 4); empty without them.
    std::wstring  GetModelName () const;

    //  True for a //e, //c or IIgs, whose screen has small letters; false
    //  for a ][ or ][+, and without the bytes.
    bool          HasLowercase () const;
};
