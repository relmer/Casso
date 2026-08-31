#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PreservedCopy
//
//  Where a version of a disk goes when it is not the one that stays mounted.
//
//  ITS OWN FILE RATHER THAN A METHOD ON MountedImageState, which already
//  carries path matching, coalescing, the watch-degrade flag and the pending
//  record. Naming a file and writing it are a second job with their own
//  failure modes, and the state object is not improved by knowing about
//  either.
//
//  THE NAMING IS PURE AND THE TIMESTAMP IS A PARAMETER. Two conflicts inside
//  one second is the case the disambiguation exists for, and a function that
//  read the clock itself could not be asked about it without waiting.
//
////////////////////////////////////////////////////////////////////////////////

class PreservedCopy
{
public:

    //  How many names to try before giving up. Enough to step past a session's
    //  worth of collisions without ever spinning.
    static constexpr int  kMaxAttempts = 99;



    //  Where the Nth copy stamped `stamp` goes, beside `imagePath`.
    //
    //  `Loader.dsk` + `20260830-014233` gives `Loader.20260830-014233-01.dsk`,
    //  then `-02`, then `-03`.
    //
    //  EVERY COPY IS NUMBERED, INCLUDING THE FIRST, and that is not tidiness.
    //  A bare `Loader.20260830-014233.dsk` sorts AFTER its own successors: the
    //  comparison reaches the extension where the numbered names have a
    //  separator, and every separator worth using is below `.`. Skipping the
    //  counter on the first copy would therefore break the one promise the
    //  naming exists to keep.
    //
    //  IT IS ZERO-PADDED for the same reason: unpadded, `-10` sorts before
    //  `-2`.
    //
    //  THE EXTENSION IS KEPT so the copy is still a mountable image: a disk the
    //  user cannot put back in a drive is not a preserved version of anything.
    static std::string  MakePath (const std::string & imagePath,
                                  const std::string & stamp,
                                  int                 attempt);

    //  `20260830-014233` for a point in time, in local time.
    //
    //  LOCAL RATHER THAN UTC. The name exists to be read by the person sitting
    //  in front of the machine, matching what their clock said when the
    //  conflict happened.
    static std::string  MakeStamp (time_t when);
};
