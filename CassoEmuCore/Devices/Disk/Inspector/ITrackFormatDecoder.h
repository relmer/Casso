#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DecodeSettings.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ITrackFormatDecoder
//
//  One way of finding fields on a framed track. The standard 16- and 13-sector
//  formats are the first; documented custom formats each get a decoder of
//  their own and join the list beside it.
//
////////////////////////////////////////////////////////////////////////////////

class ITrackFormatDecoder
{
public:
    virtual        ~ITrackFormatDecoder () = default;
    virtual void   Decode               (const FramedTrack & track, const FieldMarks & marks, vector<LocatedField> & outFields) const = 0;
};

