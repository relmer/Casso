#pragma once

#include "Devices/Disk/DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DamagedDisk
//
//  Builds a DiskImage that is wrong in one NAMED way, so a test can state which
//  damage it is about.
//
//  IN MEMORY, NEVER A CHECKED-IN IMAGE. Every one of these starts from a blank
//  disk this tree builds itself and breaks one thing about it, so the fault is
//  visible in the test rather than in a binary nobody can diff. A corrupt image
//  in the tree would also rot: the builders that produce good disks change, and
//  a fixture recorded against an older one stops being the same evidence.
//
//  WHY THIS EXISTS. Every data-loss defect found in the disk layer so far has
//  been on a degraded path -- an undecodable sector, a track that stops
//  decoding partway, a partial write -- and a round-trip over a healthy image
//  passes whether or not the code handles those. The suite could not reach that
//  class of bug because nothing in it produced a broken disk to hand over.
//
////////////////////////////////////////////////////////////////////////////////

class DamagedDisk
{
public:
    //  A blank, undamaged DOS 3.3 disk, as a DiskImage with real GCR tracks.
    //  The starting point for every case below, and useful on its own as the
    //  control: a test that asserts a refusal should also show the same call
    //  succeeding here, or it is not testing the damage.
    static void  BuildGoodDos33 (DiskImage & outImage);

    //  Makes ONE sector on `track` undecodable, leaving the rest of the track
    //  readable. This is the partial-decode case: the track still carries
    //  address fields, so a decoder that stops at the first bad sector silently
    //  drops the later ones instead of failing.
    static void  BreakOneSector (DiskImage & inOutImage, int track, int whichSector);

    //  Zeros a whole track. NOT damage -- an unformatted track is a legitimate
    //  state, and the distinction is the point: a check that cannot tell blank
    //  from broken either refuses good disks or accepts broken ones.
    static void  WipeTrack (DiskImage & inOutImage, int track);

    //  Rewrites one sector's address field to claim a slot that another sector
    //  on the same track already fills, so the track decodes sixteen sectors
    //  into fifteen distinct slots.
    //
    //  The redirected sector's own slot is left empty, so this raises the
    //  unrecovered count as well as disturbing coverage. It does NOT isolate
    //  coverage from counting: filling all sixteen slots while duplicating one
    //  sector needs a seventeenth address field, which rewriting an existing
    //  header cannot produce.
    static void  DuplicateSectorIntoSlot (DiskImage & inOutImage,
                                          int         track,
                                          int         whichSector,
                                          int         claimSlot);

private:
    //  Byte offset of the Nth address prologue (D5 AA 96) in a packed track.
    static size_t  FindAddressField (const vector<Byte> & bits, int which);

    //  Writes a checksum that cannot be right for this header.
    //
    //  FLIPPING A BIT IN THE ENCODED BYTE WOULD NOT DO: 4-and-4 forces the odd
    //  bits to 1, so clearing one changes the decoded value only when that bit
    //  was already set -- a corruption that silently does nothing for half a
    //  track, and a test that passes for the wrong reason on the other half.
    static void  PatchFieldChecksum (vector<Byte> & bits,
                                     size_t         addrAt,
                                     Byte           volume,
                                     Byte           track);

    //  The 4-and-4 pair encoding one byte, written at `at`.
    static void  WriteOddEven (vector<Byte> & bits, size_t at, Byte value);

    //  The byte a 4-and-4 pair at `at` decodes to.
    static Byte  ReadOddEven (const vector<Byte> & bits, size_t at);
};
