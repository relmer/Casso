#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WozChunk
//
//  One WOZ chunk Casso does not model, kept exactly as it was read so a
//  rewrite can put it back byte for byte. The id is the raw 4-byte chunk
//  tag rather than a string so an unrecognized tag needs no interpretation
//  to survive a round trip.
//
////////////////////////////////////////////////////////////////////////////////

struct WozChunk
{
    Byte          id[4] = {};
    vector<Byte>  payload;
};





////////////////////////////////////////////////////////////////////////////////
//
//  WozMetadata
//
//  What a WOZ file carries that Casso's track model cannot express, held
//  alongside the tracks so a flush rebuilds the file instead of degrading
//  it. The writer reconstructs INFO, TMAP and TRKS from the live model --
//  that is what makes guest writes survive -- and everything else in the
//  file exists only here.
//
//      infoPayload   the source INFO chunk verbatim. Casso owns four of
//                    its fields (version, write-protect, disk type and
//                    largest track); the other fifty-odd bytes -- creator,
//                    synchronized, cleaned, boot sector format, timing,
//                    compatible hardware, required RAM -- are the source's
//                    and are re-emitted untouched.
//      passThrough   every other chunk, in source order. META is the one
//                    that matters today; a later format revision's chunks
//                    round-trip through the same path without Casso
//                    learning anything about them.
//
//  An empty infoPayload means the image was synthesized rather than read
//  from a file, which is also what distinguishes a disk Casso authored
//  from one it merely edited.
//
////////////////////////////////////////////////////////////////////////////////

struct WozMetadata
{
    vector<Byte>      infoPayload;
    vector<WozChunk>  passThrough;

    bool  IsFromSourceFile () const { return !infoPayload.empty(); }

    void  Clear ()
    {
        infoPayload.clear();
        passThrough.clear();
    }
};
