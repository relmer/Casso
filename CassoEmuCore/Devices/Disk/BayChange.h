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
//
//  ITS OWN HEADER, LIKE PickUpIntent, so the shell can name it without pulling
//  in the whole DiskImageStore definition. DiskManager holds the store by
//  reference behind a forward declaration; the bay-change sink it reacts to
//  crosses that boundary, and a nested enum cannot.
//
////////////////////////////////////////////////////////////////////////////////

enum class BayChange
{
    Inserted,
    Ejected,
    Swapped,
};
