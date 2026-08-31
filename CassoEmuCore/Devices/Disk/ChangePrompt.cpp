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

    case ChangeAction::Conflict:
        prompt.title   = L"Two versions of one disk";
        prompt.message = what +
            L" was modified externally, and the machine has written to it since "
            L"it was mounted.\n\n"
            L"Both versions exist and only one can stay mounted. Whichever you do "
            L"not keep is saved beside the original with a timestamp in its name, "
            L"so nothing is discarded either way.";

        prompt.answers.push_back (PromptAnswer { L"Keep the file on disk", ChangeAction::TakeUpInPlace });
        prompt.answers.push_back (PromptAnswer { L"Keep what the machine wrote", ChangeAction::KeepHeld });
        break;

    case ChangeAction::Unusable:
        prompt.title   = L"A mounted disk can no longer be read";
        prompt.message = what +
            L" is gone, or has been replaced by something that cannot be used as "
            L"this disk.\n\n"
            L"Casso is still holding the disk and the machine is still running, so "
            L"what it holds may be the only copy left. Saving it writes a "
            L"timestamped image beside where the original was.";

        prompt.answers.push_back (PromptAnswer { L"Save a copy", ChangeAction::PreserveCopy });
        prompt.answers.push_back (PromptAnswer { L"Carry on", ChangeAction::KeepHeld });
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
        prompt.message = what + L" was modified externally and mounted.\n\n"
                       + StaleDirectoryWarning();
    }

    prompt.answers.push_back (PromptAnswer { L"Dismiss", ChangeAction::Ignore });

    return prompt;
}
