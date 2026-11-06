#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BayChange
//
//  What happened to the disk in a bay.
//
//  Inserted     a disk went into an empty or freshly mounted bay.
//  Ejected      the bay was emptied, by a user eject or a file that vanished.
//  Swapped      the disk was replaced under the running machine, a pick-up of
//               an external change. It reads as the door opening and closing
//               where a plain insert only closes.
//  OutOfDrive   an eject took the disk out of the drive before its save. The
//               bay has the disk until the save completes, and the drive does
//               not.
//  BackInDrive  the eject did not empty the bay, and the disk is in the drive
//               again.
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
    OutOfDrive,
    BackInDrive,
};
