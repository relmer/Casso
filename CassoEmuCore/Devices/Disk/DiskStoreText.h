#pragma once

#include "Pch.h"

#include "DiskStoreTypes.h"

class MountDiagnosis;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreText
//
//  The disk store's notices and questions, worded for the user. Pure:
//  no filesystem access, so each is testable on its own.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStoreText
{
public:
    //  Why a mount failed, in the user's terms: the file named, then the
    //  diagnosis worded as a sentence about it. Pure -- no filesystem access
    //  -- and public because the wording is the observable half of a failed
    //  mount, and the only half worth testing on its own.
    //
    //  It takes the diagnosis rather than re-deriving one from the path. The
    //  path can only ever answer "is this an extension we read", which left
    //  every other refusal sharing one sentence that named none of them.
    static wstring  FormatMountFailureMessage (const string & path,
                                               const MountDiagnosis & diagnosis);

    //  FormatMountFailureMessage's text for a mount declined because the disk
    //  in the drive has writes in no file, for another file or the same one.
    static wstring  FormatUnsavedWritesMessage (const string & path,
                                                const MountDiagnosis & diagnosis);

    //  The notices for an eject and a write-protect change declined because
    //  the machine is behind live, where no save can be made.
    static wstring  FormatBehindLiveEjectMessage        (int drive);
    static wstring  FormatBehindLiveWriteProtectMessage (int drive);

    //  Why the guest's version could not be written beside a file another
    //  program changed, at an eject or a quit.
    static wstring FormatPreserveFailureMessage (const string & path,
                                                 const string & attemptedPath,
                                                 HRESULT        reason,
                                                 int            drive,
                                                 FlushMoment    moment);

    // Builds the user-facing "could not save" message from the mount path;
    // handed to CHRN/CBRN in FlushEntry on a genuine persist failure. When a
    // recovery image was written, its path is named so the user can retrieve
    // the session rather than only being told what was lost.
    //
    //  `reason` IS PRINTED, code and system text both. The notice used to name
    //  no cause at all and then advise switching to .woz, which is wrong
    //  advice for a folder that refused the write; the system's own words for
    //  the failure are the useful part.
    //
    //  recoveryPath is optional: the write-protect path has no recovery copy
    //  to name, and the message says something useful either way.
    static wstring FormatFlushLossMessage (const string & path,
                                           HRESULT        reason,
                                           const string & recoveryPath = string());

    // The save-failure notice from what the save produced, including the text
    // for a recovery copy that could not be written. At an eject or a quit with
    // no copy, the tail states what happens to the changes.
    static wstring FormatFlushLossMessage (const string & path, const FlushOutcome & outcome, FlushMoment moment);

    // Why a damaged image will not be written to. A checksum mismatch
    // write-protects the image for the session, so the file is never
    // rewritten and the damage it carries stays detectable.
    static wstring FormatDamagedImageMessage (const string & path, bool hasDamagedTracks);

    // Why a salvaged copy could not be written. Names the copy, and says the
    // original is untouched -- which is the thing the user will worry about.
    static wstring FormatSalvageFailedMessage (const string & path);

    //  Why a commit was refused because the file changed underneath it. Names
    //  the image, and says the writes are still held rather than lost, which is
    //  the part the user will worry about.
    static wstring FormatExternalChangeMessage (const string & path);
};
