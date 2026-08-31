#include "Pch.h"

#include "ChangePrompt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::NameInDrive
//
//  The image as the user will recognize it, and where it is.
//
//  THE LEAF NAME, NOT THE WHOLE PATH. A notice is read in a hurry and the
//  directory is the part already known; a full path pushes the one word that
//  identifies the disk off the end of the line.
//
//  THE DRIVE NUMBER IS ONE-BASED HERE AND ZERO-BASED EVERYWHERE ELSE. The
//  conversion happens once, at the edge, because the number on the machine is
//  the number the user is looking at.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ChangePrompt::NameInDrive (const std::string & imagePath, int drive)
{
    std::wstring  name = fs::path (imagePath).filename().wstring();



    if (name.empty())
    {
        name = L"The disk";
    }

    return name + L" in Drive " + std::to_wstring (drive + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::StaleDirectoryWarning
//
//  Why a reboot might be needed after a disk is swapped underneath a running
//  program.
//
//  IT SAYS WHY RATHER THAN JUST WHAT. "Reboot if it misbehaves" alone reads as
//  superstition; the reason -- that the guest holds the previous disk's
//  directory in its own RAM, where nothing at the disk layer can see or correct
//  it -- is what makes the advice actionable.
//
//  NO BUTTON GOES WITH IT. The toolbar already carries a reboot.
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * ChangePrompt::StaleDirectoryWarning()
{
    return L"The Apple keeps the disk's directory in its own memory, so a running "
           L"program may not see the new disk correctly. Reboot if it misbehaves.";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::Compose
//
//  What to say about this action.
//
//  THE ONLY QUESTION WITH TWO REAL ANSWERS IS THE PLAIN ONE. A change nobody
//  stated an intent for is the one case where the user genuinely has a choice
//  and no way to have expressed it in advance.
//
//  AN ACTION THAT IS NOT A QUESTION COMPOSES NOTHING, which is what lets a
//  caller compose unconditionally and draw only what came back with answers.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::Compose (const std::string & imagePath, int drive,
                                    ChangeAction action)
{
    ChangePrompt  prompt;
    std::wstring  what = NameInDrive (imagePath, drive);



    switch (action)
    {
    case ChangeAction::Ask:
        prompt.title   = L"Disk modified externally";
        prompt.message = what + L" was modified externally.\n\n"
                       + StaleDirectoryWarning();

        //  Accept and Ignore rather than a pair naming "the current disk":
        //  which disk is current is exactly what the reader does not know yet,
        //  and both labels here name the changes instead, which is what the
        //  sentence above is about.
        //
        //  IGNORING DOES NOT MEAN OVERWRITING LATER. The file is left as the
        //  external writer left it, and a flush over it stays refused.
        prompt.answers.push_back (PromptAnswer { L"Accept the changes", ChangeAction::TakeUpInPlace });
        prompt.answers.push_back (PromptAnswer { L"Ignore the changes", ChangeAction::KeepHeld });
        break;

    case ChangeAction::Unusable:
    case ChangeAction::Deleted:
        prompt = ComposeLostFile (imagePath, drive, action);
        break;

    default:
        //  Not a question. Deliberately composes nothing rather than a blank
        //  dialog.
        break;
    }

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposePickUpReport
//
//  What is said after contents were taken up without asking.
//
//  IT OFFERS NO REBOOT, AND THAT IS THE POINT OF SAYING WHY. The user did not
//  choose this swap, so the reboot has to be reachable -- but the toolbar
//  already carries one, and a notice with a duplicate button is a thing to
//  dismiss rather than a thing to use. The warning names the hazard and the
//  toolbar answers it.
//
//  THE ONE ACTION IT DOES CARRY IS ITS OWN DISMISSAL. The notice stands until
//  the user closes it -- absorbing further changes rather than stacking -- so
//  something has to close it.
//
//  A MACHINE THAT HAS JUST REBOOTED GETS NO WARNING. Rebooting is what clears
//  the stale directory, so repeating the advice would be telling the user to do
//  what was just done for them.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposePickUpReport (const std::string & imagePath, int drive,
                                                bool machineRestarted)
{
    ChangePrompt  prompt;
    std::wstring  what = NameInDrive (imagePath, drive);



    prompt.title = L"Disk modified externally";

    if (machineRestarted)
    {
        prompt.message = what + L" was modified externally and mounted. "
                                L"The machine was rebooted.";
    }
    else
    {
        //  ONE PARAGRAPH, NO BREAK. This one is drawn in a banner, which
        //  estimates its height from a character count and cannot see a
        //  newline; the ask below is drawn in a dialog, which can.
        prompt.message = what + L" was modified externally and mounted. "
                       + StaleDirectoryWarning();
    }

    prompt.answers.push_back (PromptAnswer { L"Dismiss", ChangeAction::Ignore });

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposeConflictReport
//
//  What is said once both versions have been dealt with.
//
//  IT NAMES WHERE THE OTHER ONE WENT, which is the entire reason this is worth
//  saying at all. "There was a conflict" helps nobody; "your changes are in
//  Loader.20260830-014233.dsk" is a thing the user can act on.
//
//  THE TWO DIRECTIONS READ DIFFERENTLY ON PURPOSE. Displacing the guest's work
//  is the surprising one and leads with what was saved; displacing the file's
//  version happens when the emulator writes out, and leads with what was kept.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeConflictReport (const std::string & imagePath, int drive,
                                                  const std::string & preservedPath,
                                                  bool                keptWhatTheGuestWrote)
{
    ChangePrompt  prompt;
    std::wstring  what      = NameInDrive (imagePath, drive);
    std::wstring  preserved = fs::path (preservedPath).filename().wstring();
    std::wstring  name      = fs::path (imagePath).filename().wstring();



    prompt.title = L"Conflicting changes to " + what;

    if (keptWhatTheGuestWrote)
    {
        prompt.message = name +
            L" was changed externally, and also within Casso. What Casso wrote is "
            L"now in the file, and we've saved the external version to " +
            preserved + L".";
    }
    else
    {
        prompt.message = name +
            L" was changed externally, and also within Casso. The external changes "
            L"are already mounted, and we've saved your changes within Casso to " +
            preserved + L".";
    }

    prompt.answers.push_back (PromptAnswer { L"OK", ChangeAction::Ignore });

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposePreserveFailure
//
//  What is said when the displaced version could not be written anywhere.
//
//  IT SAYS WHAT DID NOT HAPPEN, not only what failed. The user's question at
//  that moment is whether they have lost anything, and the answer is no --
//  nothing was replaced, precisely because the copy could not be made.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposePreserveFailure (const std::string & imagePath, int drive)
{
    ChangePrompt  prompt;
    std::wstring  what = NameInDrive (imagePath, drive);



    prompt.title   = L"Could not save a second copy of " + what;
    prompt.message = what +
        L" was changed externally, and also within Casso. Neither version has "
        L"been touched: Casso could not write the second copy, so it did not "
        L"replace anything.\n\n"
        L"Free some space or check the folder's permissions, and the disk will be "
        L"picked up on the next change.";

    prompt.answers.push_back (PromptAnswer { L"OK", ChangeAction::Ignore });

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposeLostFile
//
//  What is said when the file behind a mounted disk has gone or become
//  unreadable.
//
//  THE TWO TITLES ARE NOT INTERCHANGEABLE. A user who deleted the file needs to
//  be told it is deleted; one whose share dropped needs to be told it cannot be
//  reached. "Something went wrong with your disk" serves neither.
//
//  THE OFFER IS NOT CONDITIONAL ON THE GUEST HAVING WRITTEN. With the file
//  gone, the bytes in memory may be the only copy of that disk that exists.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeLostFile (const std::string & imagePath, int drive,
                                            ChangeAction action)
{
    ChangePrompt  prompt;
    std::wstring  what = NameInDrive (imagePath, drive);



    if (action == ChangeAction::Deleted)
    {
        prompt.title   = what + L" has been deleted";
        prompt.message = L"The file behind this disk no longer exists. Casso is still "
                         L"holding its contents in memory.\n\n"
                         L"Would you like to save the in-memory copy?";
    }
    else
    {
        prompt.title   = what + L" is no longer accessible";
        prompt.message = L"The file behind this disk can no longer be read as this "
                         L"disk. Casso is still holding its contents in memory.\n\n"
                         L"Would you like to save the in-memory copy?";
    }

    //  The drive is emptied either way -- a drive holding a disk whose file is
    //  gone reports something untrue -- so neither answer is "carry on".
    prompt.answers.push_back (PromptAnswer { L"Save a copy...", ChangeAction::PreserveCopy });
    prompt.answers.push_back (PromptAnswer { L"Don't save",     ChangeAction::KeepHeld });

    return prompt;
}
