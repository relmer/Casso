#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandText
//
//  The disk tool's messages for a volume's errors, a damaged file and a
//  BASIC listing that can't be tokenized.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCommandText
{
public:
    //  What is damaged in a file that could not be read, in the file
    //  system's terms: "the track/sector list is damaged" on DOS 3.3 and
    //  "the index block is damaged" on ProDOS, for a chain that leads off the
    //  disk, loops or ends early. Empty for any other failure. A message
    //  has it after the file, as "Unable to read HELLO: ...".
    static std::string  DescribeDamage (HRESULT hr, VolumeKind kind);

    //  Turns a volume layer's refusal into something a user can act on.
    //
    //  THIS IS WHAT KEEPS A PLATFORM CODE OUT OF THE OUTPUT. The volume layer
    //  answers in Win32 codes because they carry the right meanings without
    //  asserting on user input, but a number is not a reason -- the person
    //  reading it wants to know that their file is locked, not that something
    //  returned 0x80070005.
    //
    //  The text is the reason in "Unable to delete HELLO: it is locked", and
    //  a damaged file's is in the terms of kind's file system.
    static std::string  DescribeVolumeRefusal (HRESULT hr, VolumeKind kind);

    //  Turns a tokenizer refusal into something a user can act on: which line,
    //  the line itself, and what is wrong with it. The line's own text is quoted
    //  because a number alone points at nothing in a file a person is reading.
    static std::string  DescribeListingRefusal (const char                   * leadIn,
                                                const ApplesoftListingError  & error);
};
