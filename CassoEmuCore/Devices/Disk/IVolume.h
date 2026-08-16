#pragma once

#include "Pch.h"

#include "FilePath.h"
#include "VolumeTypes.h"
#include "VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IVolume
//
//  One filesystem on a flat sector buffer. Two implementations, deliberately
//  narrow: DOS 3.3 and ProDOS share almost no structure below this surface, so
//  a wider interface would only be one filesystem's shape imposed on the other.
//
//  NOTHING MUTATES IN PLACE. The volume holds its buffer as an immutable input
//  and every mutating call produces a COMPLETE new buffer, or fails and
//  produces nothing. That is what makes the all-or-nothing guarantee structural
//  rather than a discipline each operation has to remember -- there is no
//  half-written state to leave behind, because the only thing that can be
//  committed is a finished buffer.
//
//  It is also what makes replacement safe. Replace is delete plus write, and
//  performed in place a failure between the two frees the old file without
//  landing the new one -- losing it outright, which is worse than refusing.
//  Computed whole, that outcome cannot occur.
//
////////////////////////////////////////////////////////////////////////////////

class IVolume
{
public:
    virtual ~IVolume () = default;

    //  Every entry the catalog yields, plus free space and a description of
    //  whatever could not be read. Damage is reported, not thrown: a partial
    //  catalog is still worth having.
    virtual HRESULT  Enumerate (VolumeListing & outListing) const = 0;

    //  One file's contents, addressed by path.
    virtual HRESULT  Read (const FilePath & path, FilePayload & outPayload) const = 0;

    //  Adds or replaces. Produces the complete post-write buffer.
    virtual HRESULT  Write (const FilePath     & path,
                            const FilePayload  & payload,
                            std::vector<Byte>       & outBuffer) const = 0;

    //  Removes a file, returning only space it uniquely owns and reporting the
    //  rest as leaked. Remains available for a file whose chain is damaged, so
    //  a bad entry cannot strand the volume.
    virtual HRESULT  Delete (const FilePath & path, std::vector<Byte> & outBuffer) const = 0;

    //  What the catalog actually references. Consumed by delete, by listing,
    //  by allocation, and by the pre-commit check on every write.
    virtual HRESULT  BuildIntegrityReport (VolumeIntegrityReport & outReport) const = 0;

    //  Sets which program the volume runs after its operating system loads.
    //  The two filesystems do this by entirely different means and are
    //  deliberately NOT unified behind a shared helper.
    virtual HRESULT  SetStartupProgram (const FilePath & path, std::vector<Byte> & outBuffer) const = 0;
};
