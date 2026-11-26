#pragma once

#include "Pch.h"

struct TrackAnalysis;





////////////////////////////////////////////////////////////////////////////////
//
//  OrphanFields
//
//  Data fields with no address field before them: a sector whose address
//  field is missing or unreadable. The strip brackets each one, and its
//  tooltip includes its most likely sector.
//
////////////////////////////////////////////////////////////////////////////////

class OrphanFields
{
public:
    //  The track's data fields that no sector pairs with an address field.
    static vector<int>  Find           (const TrackAnalysis & track);

    //  The data field with no address field that contains a nibble, or -1.
    static int          FindAt         (const TrackAnalysis & track, int nibble);

    //  The most likely sector of a data field with no address field: the
    //  number between those of the sectors on either side of it,
    //  when they are one apart from it. -1 when they aren't.
    static int          GuessSector    (const TrackAnalysis & track, int field);
};
