#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackMarks
//
//  The address and data prologs one track's fields use, for a decoder that
//  matches only these. A format stamp provides them for a protected disk's
//  tracks.
//
////////////////////////////////////////////////////////////////////////////////

struct TrackMarks
{
    using Prolog = std::array<Byte, 3>;

    vector<Prolog>  address16;
    vector<Prolog>  address13;
    vector<Prolog>  data;
};
