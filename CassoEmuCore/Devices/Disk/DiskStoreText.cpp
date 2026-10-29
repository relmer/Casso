#include "Pch.h"

#include "DiskStoreText.h"
#include "DiskImageStore.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/NibbleImageCodec.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Core/TextEncoding.h"
#include "ChangePrompt.h"
#include "CommitPlan.h"
#include "Seams/Win32DiskFileIo.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreText::FormatPreserveFailureMessage
//
//  The notice for a guest's version that could not be written beside a file
//  another program changed, with no question possible. At an eject the disk
//  is in the drive, and another eject raises the question again; at a quit
//  the changes are lost.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatPreserveFailureMessage (const string & path,
                                                      const string & attemptedPath,
                                                      HRESULT        reason,
                                                      int            drive,
                                                      FlushMoment    moment)
{
    wstring  widePath = fs::path (path).wstring();
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    message = L"Casso could not save your changes to the disk image:\n\n" + widePath
            + L"\n\nAnother program changed this file after it was inserted, so Casso did not overwrite it. "
              L"Casso tried to save your changes to " + fs::path (attemptedPath).filename().wstring()
            + L" instead, but failed: " + ChangePrompt::DescribeErrorAsSentence (reason);

    if (moment == FlushMoment::ShuttingDown)
    {
        message += L"\n\nCasso is closing, so your changes are lost. The file on disk has the other "
                   L"program's version.";
    }
    else
    {
        message += L"\n\nThe disk is still in drive " + std::to_wstring (drive + 1)
                 + L" with your changes. Eject it again to choose another place to save them. The file "
                   L"on disk has the other program's version.";
    }

    return message;
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
//  After a declined mount over a disk, that disk is in the drive, and the
//  message states it: the image is loaded before the bay is touched, so no
//  failure empties the bay. A mount declined because the disk in the drive
//  has writes in no file gets FormatUnsavedWritesMessage's text.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatMountFailureMessage (const string & path,
                                                   const MountDiagnosis & diagnosis)
{
    wstring  widePath = fs::path (path).wstring();
    wstring  reason   = TextEncoding::NarrowToWide (diagnosis.Describe());



    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    if (diagnosis.failure == MountFailure::UnsavedWrites)
    {
        message = FormatUnsavedWritesMessage (path, diagnosis);
    }
    else
    {
        message = L"Casso could not open this file as a disk image:\n\n" + widePath +
                  L"\n\nThis file " + reason + L".";

        if (!diagnosis.keptPath.empty())
        {
            message += L"\n\nThe disk already in the drive was not changed.";
        }
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatUnsavedWritesMessage
//
//  The text for a mount declined because the disk in the drive has writes in
//  no file. For another file, the insert did not happen; for the file the disk
//  was read from, the save failed and the file was not read again. With no save
//  attempted, there is no Error paragraph.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatUnsavedWritesMessage (const string & path, const MountDiagnosis & diagnosis)
{
    wstring  widePath  = fs::path (path).wstring();
    wstring  wideKept  = fs::path (diagnosis.keptPath).wstring();
    wstring  errorPara;
    wstring  message;



    if (FAILED (diagnosis.saveError))
    {
        errorPara = L"\n\nError: " + ChangePrompt::DescribeError (diagnosis.saveError);
    }

    if (diagnosis.isSameFile)
    {
        message = L"Casso could not save changes to the disk image:\n\n" + wideKept + errorPara
                + L"\n\nThe file on disk is unchanged, and the disk was not read from it again. "
                  L"The drive still has your changes.";
    }
    else
    {
        message = L"Casso did not insert this disk image:\n\n" + widePath
                + L"\n\nThe disk already in drive " + std::to_wstring (diagnosis.occupiedDrive + 1)
                + (FAILED (diagnosis.saveError) ? L" has changes that could not be saved to its file:\n\n"
                                                : L" has changes that have not been saved to its file yet:\n\n")
                + wideKept + errorPara
                + L"\n\nThat disk is still in the drive with its changes. Eject it to save them to another file.";
    }

    return message;
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

wstring DiskStoreText::FormatDamagedImageMessage (const string & path, bool hasDamagedTracks)
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

wstring DiskStoreText::FormatFlushLossMessage (const string & path,
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
//  FormatFlushLossMessage, from what the save produced
//
//  A disk whose format cannot store its writes, and whose recovery copy could
//  not be written either, gets a separate tail. The copy's error is in place
//  of the head's Error line, which would otherwise contain the serializer's.
//
//  THE TAIL FOR A DISK THAT STAYS IN THE DRIVE IS FALSE AT AN EJECT OR A QUIT.
//  With no copy, a declined eject states that the disk was not ejected, and a
//  quit states that the changes are gone.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatFlushLossMessage (const string & path, const FlushOutcome & outcome, FlushMoment moment)
{
    bool     isCopyFailed = outcome.isUnserializable && FAILED (outcome.recoveryError);
    bool     hasCopy      = SUCCEEDED (outcome.recoveryError) && !outcome.recoveryPath.empty();
    wstring  widePath     = fs::path (path).wstring();
    wstring  tail;
    wstring  message;



    if (widePath.empty())
    {
        widePath = L"(unknown path)";
    }

    if (moment == FlushMoment::Ejecting)
    {
        tail = L"The disk was not ejected, and the drive still has your changes.";
    }
    else if (moment == FlushMoment::ShuttingDown)
    {
        tail = L"Casso is closing, so these changes cannot be kept.";
    }

    if (isCopyFailed)
    {
        message = L"Casso could not save changes to the disk image:\n\n" + widePath
                + L"\n\nThe file on disk is unchanged. Its changes cannot be stored as a "
                + fs::path (path).extension().wstring() + L" image. Casso tried to save them to "
                + fs::path (outcome.recoveryPath).filename().wstring() + L" instead, but failed: "
                + ChangePrompt::DescribeErrorAsSentence (outcome.recoveryError);

        if (!tail.empty())
        {
            message += L" " + tail;
        }
    }
    else if (hasCopy || tail.empty())
    {
        message = FormatFlushLossMessage (path, outcome.saveError, hasCopy ? outcome.recoveryPath : string());
    }
    else
    {
        message = L"Casso could not save changes to the disk image:\n\n" + widePath
                + L"\n\nError: " + ChangePrompt::DescribeError (outcome.saveError)
                + L"\n\nThe file on disk is unchanged. " + tail;
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatSalvageFailedMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatSalvageFailedMessage (const string & path)
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
//  FormatBehindLiveEjectMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatBehindLiveEjectMessage (int drive)
{
    return L"Casso did not eject the disk in drive " + std::to_wstring (drive + 1)
         + L", because the machine was behind live in its history when the eject ran. Go live and try again.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatBehindLiveWriteProtectMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatBehindLiveWriteProtectMessage (int drive)
{
    return L"Casso did not change write protection for drive " + std::to_wstring (drive + 1)
         + L", because the machine was behind live in its history when the change ran. Go live and try again.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreText::FormatExternalChangeMessage
//
//  Why a commit was refused because the file moved under it.
//
//  IT SAYS THE WRITES ARE STILL HELD, which is the difference between this and
//  every other flush failure and the only thing the user actually wants to
//  know. The image keeps its dirty bit, so the writes are in memory and a later
//  flush still carries them.
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatExternalChangeMessage (const string & path)
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





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStoreText::FormatCopySavedMessage
//
////////////////////////////////////////////////////////////////////////////////

wstring DiskStoreText::FormatCopySavedMessage (int drive, const string & copyPath)
{
    return ChangePrompt::DriveLabel (drive) + L"'s disk was also saved to "
         + ChangePrompt::GetFileName (copyPath) + L" in the same folder.";
}
