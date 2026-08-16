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
//  THE THREE CONVERSIONS DIFFER IN WHETHER THEY ROUND-TRIP, and a caller
//  reasoning about "extract, edit, put back" needs to know which it is using:
//
//      Verbatim     No character conversion. Length and header semantics still
//                   apply -- those record where a file ENDS and are its
//                   identity, not a transformation. Never yields sector slack.
//
//      HostText     LOSSY, deliberately: decoding is many-to-one so that a real
//                   vendor file carrying mixed high and low bytes can be read at
//                   all. Host -> Apple -> host is the identity; the other
//                   direction normalizes, turning a plain space high and a
//                   CR/LF pair into one terminator.
//
//      Applesoft    Detokenizing and retokenizing is the LEAST likely of the
//                   three to round-trip exactly, because tokenizers normalize
//                   spacing and canonicalize forms. It is also where loss is
//                   most surprising: someone extracting a text file has opted
//                   into a conversion, while someone whose saved program comes
//                   back subtly reformatted has not. This must be settled
//                   deliberately when the tokenizer is built rather than
//                   inherited from whatever it happens to do.
//
//  Note that a DOS 3.3 binary needs no verbatim mode of its own: stripping the
//  four-byte header is REVERSIBLE, because reading reports the load address and
//  writing rebuilds the header from it. The raw form would be less useful, since
//  the caller would then have to parse the header itself.
//
enum class PayloadEncoding
{
    Verbatim,           // Bytes as they are, in and out. Lossless.
    HostText,           // Host text <-> Apple text. Lossy in the Apple direction.
    ApplesoftListing,   // Host text listing <-> tokenized form. Loss to be settled.
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
    std::string               volumeName;   // ProDOS
    Byte                      volumeNumber    = 0;   // DOS 3.3
    bool                      hasVolumeName   = false;
    bool                      hasVolumeNumber = false;
    uint32_t                  totalUnits      = 0;
    uint32_t                  freeUnits       = 0;
    std::vector<FileEntry>    entries;
    std::vector<std::string>  damage;   // empty means a clean read
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
    std::vector<Byte>  bytes;
    Byte               type           = 0;
    Word               loadAddress    = 0;
    Word               auxType        = 0;
    bool               hasLoadAddress = false;
    bool               hasAuxType     = false;
    PayloadEncoding    encoding       = PayloadEncoding::Verbatim;
};
