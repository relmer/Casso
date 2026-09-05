#pragma once

#include "Pch.h"

#include "IDiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MountFailure
//
//  Why a file did not become a mounted disk.
//
//  EVERY ENUMERATOR IS SOMETHING THE LOAD PATH CAN ACTUALLY TELL APART, which
//  is why the list is this short. The loaders answer a handful of separate
//  questions -- does any loader claim this name, can the bytes be read at all,
//  are there any, is the count the one the container demands, does a .woz begin
//  with a WOZ header, do the chunks behind that header hold together -- and
//  each of those is one enumerator. There is no enumerator for anything the
//  code cannot distinguish; guessing a reason is worse than the generic
//  sentence it would replace.
//
//  A WOZ WHOSE CHECKSUM FAILS IS NOT HERE, deliberately. That image loads: a
//  damaged preservation dump is exactly the file a user needs to open, so the
//  mismatch write-protects the image for the session and is reported with an
//  offer of salvage. It is not a mount failure and must not be listed as one.
//
//      None                the mount succeeded, or nothing has been diagnosed
//      UnknownExtension    no loader claims this file name
//      FileUnreadable      the bytes never arrived -- missing, locked, denied
//      EmptyFile           the file was read and holds nothing
//      WrongSizeForFormat  a sector image that is not exactly 143,360 bytes
//      NotAWozFile         named .woz, but the WOZ header is not there
//      MalformedWoz        a real WOZ header over chunks that do not hold up
//      Unrecognized        the loader refused for a reason nothing above names
//      WrongSizeForNibble  a nibble image that is neither 232,960 nor 223,440
//                          bytes. Separate from WrongSizeForFormat because a
//                          nibble image has TWO valid lengths and different
//                          arithmetic behind them; one clause cannot name both
//                          sets of numbers without going vague about each.
//      AlreadyMounted      the file is in another drive already. Refused
//                          before anything is read: the bytes are not the
//                          problem, and a second bay holding them would be a
//                          second independent copy of the disk
//      NotANibbleStream    a nibble image of an accepted length in which no
//                          nibble assembles anywhere. This is the only content
//                          check the format permits: it carries no signature,
//                          header or checksum, so a file that is the right size
//                          and holds high bits is indistinguishable from a real
//                          one until something tries to boot it.
//
////////////////////////////////////////////////////////////////////////////////

enum class MountFailure
{
    None,
    UnknownExtension,
    FileUnreadable,
    EmptyFile,
    WrongSizeForFormat,
    NotAWozFile,
    MalformedWoz,
    Unrecognized,
    WrongSizeForNibble,
    NotANibbleStream,
    AlreadyMounted,
};





////////////////////////////////////////////////////////////////////////////////
//
//  MountDiagnosis
//
//  The reason a mount failed, plus the two facts a message needs to be
//  specific about it: how big the file actually was, and which container its
//  name promised. Carried alongside the HRESULT rather than encoded into it,
//  because an HRESULT is meant to say only whether something worked -- the
//  same reasoning that keeps S_FALSE out of this tree. It is plain data and
//  trivially copyable, so it survives the CPU-thread-to-UI-thread hop the
//  mount report already makes.
//
//  Describe() produces a PREDICATE CLAUSE -- "is 4,096 bytes, but ..." -- with
//  no leading subject and no trailing period. That is what lets one wording
//  serve both edges: the console writes "<image> <clause>" and the GUI writes
//  "This file <clause>." Two wordings for the same refusal is how they come to
//  disagree.
//
////////////////////////////////////////////////////////////////////////////////

class MountDiagnosis
{
public:
    MountFailure  failure      = MountFailure::None;
    DiskFormat    format       = DiskFormat::Dsk;
    size_t        fileByteSize = 0;

    //  Which drive already has the file, zero-based, for AlreadyMounted alone.
    //  Saying which one is the whole use of the message: "already mounted" on
    //  its own sends the reader to look in both drives.
    int           occupiedDrive = -1;

    bool    HasFailure () const { return failure != MountFailure::None; }

    //  The reason as a clause about the file, ready to follow its name.
    string  Describe   () const;

    //  "143,360 bytes". Grouped by hand rather than through a locale, because
    //  a diagnostic that reads differently on a machine set to another region
    //  is a diagnostic two users cannot compare.
    static string        FormatByteCount (size_t byteCount);

    //  ".dsk" and friends, for naming the container in a sentence. A format
    //  outside the known set answers "disk", so the sentence still reads.
    //  PRIMARY because a format may answer to more than one extension --
    //  nibble images are both .nib and .nb2 -- and this returns the
    //  representative name rather than the file's own.
    static const char *  GetPrimaryExtension (DiskFormat fmt);

    //  The same answer for an interface: the extension as wide text, and the
    //  container's name for a chooser -- ".dsk" and "DSK". Both derive from
    //  the one list above, so a surface cannot name a container something the
    //  rest of Casso does not call it.
    static std::wstring  GetPrimaryExtensionText (DiskFormat fmt);
    static std::wstring  GetContainerCaption     (DiskFormat fmt);
};
