#include "Pch.h"

#include "Devices/Disk/Store/DiskStoreMessages.h"
#include "Devices/Disk/ChangePrompt.h"
#include "Devices/Disk/MountDiagnosis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMessages::FormatDiscardedWritesMessage
//
//  What is said when a disk's last chance to be written has gone.
//
//  IT DOES NOT CLAIM THE WRITES ARE SAFE. The message for a running machine
//  says they are still in memory and still flushable, which is true there and
//  is why a later attempt can succeed. Said as the drive empties or the process
//  exits it was simply false: the memory it pointed at is released a few lines
//  later.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatDiscardedWritesMessage (const string & path,
                                                      const string & attemptedPath,
                                                      HRESULT        reason)
{
    wstring  widePath  = fs::path (path).wstring();
    wstring  wideTried = fs::path (attemptedPath).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not save your changes to the disk image:\n\n" + widePath +
           L"\n\nThe file was changed by something else since it was mounted, so your "
           L"version could not be written over it. Casso tried to save it here "
           L"instead:\n\n" + wideTried +
           L"\n\nError: " + ChangePrompt::DescribeError (reason) +
           L"\n\nThe disk is leaving the drive, so these changes cannot be recovered. "
           L"The file on disk keeps the other program's version.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatMountFailureMessage
//
//  User-facing message for a mount that did not happen: the file named, then
//  the diagnosis worded as a sentence about it.
//
//  IT USED TO GUESS. The mount reported every refusal as one generic HRESULT,
//  so this re-derived what it could from the path -- an extension no loader
//  claims, versus everything else -- and told everyone in the second group
//  that their file "could not be read, or its contents are not a disk image
//  this loader accepts", which is four different problems in one sentence and
//  actionable for none of them. The reason now travels here, and the wording
//  is the diagnosis's to give.
//
//  Deliberately says nothing about what happened to the drive. A rejected
//  file leaves the bay empty, while a file that could not be read at all
//  leaves the previous disk in place, and a message that guessed wrong about
//  that is worse than one that stays quiet on it.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatMountFailureMessage (const string & path,
                                                   const MountDiagnosis & diagnosis)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not open this file as a disk image:\n\n" + widePath +
           L"\n\n" + diagnosis.FormatSentence (L"This file") + L".";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatDamagedImageMessage
//
//  User-facing message for a disk this tool will not write back, because
//  rewriting it would give damaged data a freshly computed checksum and
//  leave nothing able to detect the damage again.
//
//  THE BANNER ABOVE THIS USED TO NAME THE FUNCTION BELOW IT, which was the
//  next one down: this function was spliced in after the banner rather than
//  before it, so both carried the same title and neither described what it
//  sat on.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatDamagedImageMessage (const string & path, bool hasDamagedTracks)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"This disk is damaged, so Casso will not write to it:\n\n" + widePath +
           (hasDamagedTracks
               ? L"\n\nRewriting it would replace the tracks that could not be read "
                 L"with blank ones, hiding the damage. "
               : L"\n\nRewriting it would give the file a newly computed checksum, "
                 L"hiding the damage. ") +
           L"The disk stays readable and the emulated machine sees it as "
           L"write-protected. Work on a copy if you need to write to it.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatFlushLossMessage
//
//  User-facing message for a flush that failed to persist a dirty image,
//  built from the mount path (widened) the store already holds. Handed to
//  the CHRN/CBRN notifications in FlushEntry.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatFlushLossMessage (const string & path,
                                                HRESULT        reason,
                                                const string & recoveryPath)
{
    wstring  widePath     = fs::path (path).wstring();
    wstring  wideRecovery = fs::path (recoveryPath).wstring();
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    //  The code and the system's own words for it, the same way every other
    //  disk notice reports a failure. A notice that names no cause sends the
    //  reader guessing, and this one used to guess for them -- wrongly.
    message = L"Casso could not save changes to the disk image:\n\n" + widePath
            + L"\n\nError: " + ChangePrompt::DescribeError (reason)
            + L"\n\nThe file on disk is unchanged.";

    // A refusal that leaves the user with no way back to their work is only
    // half a fix, so say where the work went rather than only what failed.
    if (!wideRecovery.empty())
    {
        message += L" Your session was preserved here instead:\n\n" + wideRecovery +
                   L"\n\nThat copy is complete. It keeps the track that could not "
                   L"be written back. Mount it to pick up where you left off.";
    }
    else
    {
        //  True and actionable, where the old text advised a format change
        //  that cannot help a permission or disk-space failure.
        message += L" Your recent writes have not been saved. The disk in the drive "
                   L"still has them.";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSalvageFailedMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatSalvageFailedMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso could not write the salvaged copy:\n\n" + widePath +
           L"\n\nThe original disk image was not changed.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreMessages::FormatExternalChangeMessage
//
//  Why a commit was refused because the file moved under it.
//
//  IT SAYS THE WRITES ARE STILL HELD, which is the difference between this and
//  every other flush failure and the only thing the user actually wants to
//  know. The image keeps its dirty bit, so the writes are in memory and a later
//  flush still carries them.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreMessages::FormatExternalChangeMessage (const string & path)
{
    wstring  widePath = fs::path (path).wstring();



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    return L"Casso did not save changes to the disk image:\n\n" + widePath +
           L"\n\nThe file was changed by something else since it was mounted, and "
           L"writing now would discard that change. Your writes are still held in "
           L"memory and were not lost.";
}





