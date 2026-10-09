#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/ITrackFormatDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StandardTrackDecoder
//
//  16-sector and 13-sector address and data fields, through FieldLocator.
//
////////////////////////////////////////////////////////////////////////////////

class StandardTrackDecoder : public ITrackFormatDecoder
{
public:
    void  Decode (const FramedTrack & track, const FieldMarks & marks, vector<LocatedField> & outFields) const override;
};