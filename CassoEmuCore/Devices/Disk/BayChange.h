#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BayChange
//
//  What happened to the disk in a bay.
//
//  Inserted  a disk went into an empty or freshly mounted bay.
//  Ejected   a bay's disk left, whether the user asked or the file vanished.
//  Swapped   the disk was replaced under the running machine, a pick-up of an
//            external change. It reads as the door opening and closing where a
//            plain insert only closes.
//  Restored  a machine state was restored, 035's history rewind or a saved
//            state loaded, and the disk's tracks may differ with no guest
//            write. The disk stays in the drive, so there is no door or sound;
//            the drive's counts start over and viewers analyze it again.
//
//  ITS OWN HEADER, LIKE ExternalChangeIntent, so the shell can refer to it
//  without pulling in the whole DiskImageStore definition. DiskManager holds
//  the store by reference behind a forward declaration; the bay-change sink
//  it reacts to crosses that boundary, and a nested enum cannot.
//
////////////////////////////////////////////////////////////////////////////////

enum class BayChange
{
    Inserted,
    Ejected,
    Swapped,
    Restored,
};
