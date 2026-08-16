#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  VolumeTypes
//
//  What a caller passes into and gets back from a volume, normalized across
//  DOS 3.3 and ProDOS.
//
//  Fields a filesystem does not record are marked absent rather than left as
//  zero, using the has-flag idiom the command-line options already use: a load
//  address of $0000 and no load address at all are different answers, and a
//  single value cannot tell them apart. A caller that reads `loadAddress`
//  without checking `hasLoadAddress` gets a plausible lie.
//
////////////////////////////////////////////////////////////////////////////////

//
//  How a payload's bytes relate to what the guest will hold. Selects the
//  conversion in both directions; Verbatim is the absence of one.
//
enum class PayloadEncoding
{
    Verbatim,           // Bytes as they are, in and out.
    HostText,           // Host text <-> high ASCII with the target's line endings.
    ApplesoftListing,   // Host text listing <-> tokenized on-disk form.
};



//
//  One catalog record. `sizeUnits` counts whatever the filesystem allocates in
//  -- sectors for DOS 3.3, blocks for ProDOS -- because the two are not
//  interchangeable and pretending otherwise invites an off-by-eight.
//
struct FileEntry
{
    std::string  name;
    Byte         type           = 0;
    bool         isLocked       = false;
    bool         isDirectory    = false;
    uint32_t     sizeUnits      = 0;
    uint32_t     eofBytes       = 0;
    Word         loadAddress    = 0;
    Word         auxType        = 0;
    bool         hasEofBytes    = false;   // ProDOS records an exact length; DOS 3.3 does not
    bool         hasLoadAddress = false;
    bool         hasAuxType     = false;
};



//
//  Everything a listing reports, including what could NOT be read. Damage is
//  part of the result rather than an error, because a partial catalog is still
//  worth having -- the developer this feature serves is recovering old disks.
//
struct VolumeListing
{
    std::string          volumeName;              // ProDOS
    Byte                 volumeNumber    = 0;     // DOS 3.3
    bool                 hasVolumeName   = false;
    bool                 hasVolumeNumber = false;
    uint32_t             totalUnits      = 0;
    uint32_t             freeUnits       = 0;
    vector<FileEntry>    entries;
    vector<std::string>  damage;                  // empty means a clean read
};



//
//  The bytes of one file plus what is needed to place it correctly.
//
//
//  The auxiliary type is kept separate from the load address even though ProDOS
//  stores them in the same field. For a binary they are the same number; for a
//  text file the auxiliary type is a record length and means nothing about
//  where the file loads. Collapsing them would make a caller that asks "where
//  does this load?" get an answer for files that do not load anywhere.
//
struct FilePayload
{
    vector<Byte>     bytes;
    Byte             type           = 0;
    Word             loadAddress    = 0;
    Word             auxType        = 0;
    bool             hasLoadAddress = false;
    bool             hasAuxType     = false;
    PayloadEncoding  encoding       = PayloadEncoding::Verbatim;
};
