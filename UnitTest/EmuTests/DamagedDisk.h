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
//  A TRACK IS NOT BYTE-ALIGNED, which is the trap this helper exists to hide.
//  A sector occupies 3,164 bits, so every one shifts the alignment by 4 bits
//  and only the even-numbered address fields ever begin on a byte boundary.
//  Scanning for the D5 AA 96 prologue byte-wise therefore finds 8 of the 16 and
//  silently renumbers them: ask for "sector 5" and you get sector 10. The first
//  version of this helper did exactly that, and its tests passed while damaging
//  a sector other than the one they named. Everything below addresses a sector
//  by the number DECODED from its address field, so the target is verified
//  rather than assumed.
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

    //  Makes sector `sectorNumber` on `track` undecodable, leaving the rest of
    //  the track readable. This is the partial-decode case: the track still
    //  carries address fields, so a decoder that stops at the first bad sector
    //  silently drops the later ones instead of failing.
    static void  BreakSector (DiskImage & inOutImage, int track, int sectorNumber);

    //  Zeros a whole track. NOT damage -- an unformatted track is a legitimate
    //  state, and the distinction is the point: a check that cannot tell blank
    //  from broken either refuses good disks or accepts broken ones.
    static void  WipeTrack (DiskImage & inOutImage, int track);

    //  Rewrites sector `sectorNumber`'s address field to claim `claimSlot`,
    //  which another sector already fills, so the track decodes sixteen sectors
    //  into fifteen distinct slots.
    //
    //  The redirected sector's own slot is left empty, so this raises the
    //  unrecovered count as well as disturbing coverage. It does NOT isolate
    //  coverage from counting: filling all sixteen slots while duplicating one
    //  sector needs a seventeenth address field, which rewriting an existing
    //  header cannot produce.
    static void  RedirectSectorToSlot (DiskImage & inOutImage,
                                       int         track,
                                       int         sectorNumber,
                                       int         claimSlot);

    //  How many address fields the track carries, found bit-wise. 16 on a disk
    //  this tree built. Exposed so a test can assert the helper is reaching the
    //  whole track rather than the 8 a byte scan would find.
    static int   CountAddressFields (const DiskImage & image, int track);

private:
    //  Bit offset of the address field whose decoded sector is `sectorNumber`,
    //  or SIZE_MAX. Scans BIT-wise, because sector fields do not start on byte
    //  boundaries -- see the header comment.
    static size_t  FindAddressFieldBySector (const vector<Byte> & bits,
                                             size_t               bitCount,
                                             int                  sectorNumber);

    //  The nibble beginning at `bitAt`, MSB first.
    static Byte    ReadNibbleAtBit (const vector<Byte> & bits, size_t bitAt);

    //  Writes a nibble beginning at `bitAt`, MSB first.
    static void    WriteNibbleAtBit (vector<Byte> & bits, size_t bitAt, Byte value);

    //  4-and-4: the odd bits ride one nibble and the even bits the next, both
    //  with the gaps forced to 1 so every byte still reads as a valid nibble.
    static Byte    ReadOddEvenAtBit (const vector<Byte> & bits, size_t bitAt);
    static void    WriteOddEvenAtBit (vector<Byte> & bits, size_t bitAt, Byte value);
};
