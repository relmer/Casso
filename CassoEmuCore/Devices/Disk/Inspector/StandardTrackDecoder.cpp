#include "Pch.h"

#include "Devices/Disk/Inspector/StandardTrackDecoder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StandardTrackDecoder::Decode
//
////////////////////////////////////////////////////////////////////////////////

void StandardTrackDecoder::Decode (const FramedTrack & track, const FieldMarks & marks, vector<LocatedField> & outFields) const
{
    FieldLocator::Locate (track, marks, outFields);
}