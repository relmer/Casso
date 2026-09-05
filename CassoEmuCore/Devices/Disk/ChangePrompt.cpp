#include "Pch.h"

#include "ChangePrompt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::FileName
//
//  The file a message is about, without its folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ChangePrompt::GetFileName (const std::string & imagePath)
{
    return fs::path (imagePath).filename().wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::DriveLabel
//
//  Which drive, in the numbering printed on the machine.
//
//  THE CONVERSION HAPPENS ONCE, AT THE EDGE, because the number on the drive is
//  the number the user is looking at while they read this.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ChangePrompt::DriveLabel (int drive)
{
    return L"Drive " + std::to_wstring (drive + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::DescribeError
//
//  The code, and the system's own words for it when it has any.
//
//  THE CODE IS ALWAYS PRINTED, even when the text is good. Text alone cannot be
//  searched for reliably -- it is translated -- and a bug report that quotes
//  only "access is denied" has thrown away the one part that identifies which
//  failure it was.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ChangePrompt::DescribeError (HRESULT reason)
{
    wchar_t        code[16] = {};
    wchar_t *      text     = nullptr;
    DWORD          length   = 0;
    std::wstring   described;



    swprintf_s (code, L"0x%08X", (unsigned int) reason);

    described = code;

    length = FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER
                                 | FORMAT_MESSAGE_FROM_SYSTEM
                                 | FORMAT_MESSAGE_IGNORE_INSERTS,
                             nullptr,
                             (DWORD) reason,
                             MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
                             (LPWSTR) &text,
                             0,
                             nullptr);

    if (length > 0 && text != nullptr)
    {
        std::wstring  words (text, length);

        //  FormatMessage ends its sentences with a line break, which would put
        //  the rest of the notice on a line of its own.
        while (!words.empty()
            && (words.back() == L'\r' || words.back() == L'\n' || words.back() == L' '))
        {
            words.pop_back();
        }

        if (!words.empty())
        {
            described += L" ";
            described += words;
        }
    }

    if (text != nullptr)
    {
        LocalFree (text);
    }

    return described;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::Compose
//
//  The question put when something else changed a mounted file.
//
//  THE BUTTONS SAY WHAT THEY DO. They were "Accept the changes" and "Ignore the
//  changes", which described the external edit twice and said nothing about
//  the disk in the drive; a reader could reasonably take "ignore" for "throw
//  the new file away". Each label states the action, and the body states its
//  consequence.
//
//  THE FILE AND THE DRIVE APPEAR ONCE, ON A LINE OF THEIR OWN. Every
//  sentence used to carry both, so a name of any length appeared six times in
//  one dialog and the sentences around it were unreadable. Hoisting them to a
//  line of their own lets the rest say "this disk" and "it", which is how
//  anyone would say it out loud. The drive follows the file in parentheses,
//  because the file is the thing the reader is scanning for. The copy's name
//  is the one other filename here, printed once, where the rename is
//  described.
//
//  KEEPING IS A SAVE-AS, NOT A SWAP. The disk in the drive does not move, so
//  the sentence about it is a rename and not an insertion. That is also what
//  the store does: it writes the copy and points the bay at it.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::Compose (const std::string & imagePath, int drive,
                                    ChangeAction action,
                                    const std::string & copyPath,
                                    bool copyAlreadyWritten)
{
    ChangePrompt  prompt;
    std::wstring  file  = GetFileName (imagePath);
    std::wstring  where = DriveLabel (drive);
    std::wstring  copy  = GetFileName (copyPath);



    switch (action)
    {
    case ChangeAction::Ask:
        prompt.title   = L"Disk modified outside Casso";

        prompt.message = L"Another program modified this disk while it was mounted in Casso:"
                         L"\n\n" + file + L" (" + where + L")";

        //  A conflict renamed the disk before anything was asked, and the
        //  reader is told that and nothing else. It used to say their writes
        //  "hadn't been saved yet", which describes a write cache they cannot
        //  see and reads as though work had been at risk. What they expect is
        //  that their disk is a file and that it is still there under a new
        //  name, which is exactly what happened.
        if (copyAlreadyWritten && !copy.empty())
        {
            prompt.message += L"\n\nYour disk has been renamed to " + copy + L".";
        }

        prompt.message += L"\n\nInsert the modified disk to use the other program's "
                          L"version.";

        prompt.message += L"\n\nKeep your current version to continue using the disk "
                          L"as it is.";

        //  ONLY WHERE THE RENAME IS STILL AHEAD. Once it has happened the
        //  paragraph above has already said so, and repeating the new name
        //  here put it on the screen twice.
        if (!copy.empty() && !copyAlreadyWritten)
        {
            prompt.message += L" It will be renamed to " + copy
                            + L" so that it doesn't conflict with the modified file.";
        }

        prompt.answers.push_back (PromptAnswer { L"Insert the modified disk",
                                                 ChangeAction::ReloadInPlace });
        prompt.answers.push_back (PromptAnswer { L"Keep your current version",
                                                 ChangeAction::KeepHeld });
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
//  ChangePrompt::ComposeReloadReport
//
//  What is shown after contents went in without a question.
//
//  IT CARRIES NO REBOOT BUTTON AND NO WARNING ABOUT ONE. The toolbar already
//  has a reboot, and the audience for this feature knows what a swapped disk
//  does to a running program; a paragraph explaining it read as alarming for
//  something that is working exactly as asked.
//
//  THE ONE ACTION IT DOES CARRY IS ITS OWN DISMISSAL, which every standing
//  notice needs.
//
//  THE RENAME IS REPORTED ONLY WHEN ONE HAPPENED. The path alone used to be the
//  test, and it is not one: a bay reserves the name while the question is on
//  screen and keeps holding it after a write that failed, both of which left
//  this notice telling the user their disk was renamed to a file nothing
//  created.
//
//  THE WRITE IS ATTRIBUTED ONLY WHEN SOMETHING SAID WHO MADE IT. Every sentence
//  here used to open with CassoCli, which is right for a write that stated its
//  intent and wrong for the other route in: the user answering the question
//  gets here too, and there the writer is unknown and the insertion is theirs.
//  Naming the wrong program sends them to look at a build that never ran.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeReloadReport (const std::string & imagePath, int drive,
                                                bool machineRebooted,
                                                const std::string & machineName,
                                                ChangeAuthor author,
                                                const std::string & copyPath,
                                                bool copyAlreadyWritten)
{
    ChangePrompt  prompt;
    std::wstring  file    = GetFileName (imagePath);
    std::wstring  where   = DriveLabel (drive);
    std::wstring  copy    = GetFileName (copyPath);
    std::wstring  machine = fs::path (machineName).wstring();
    std::wstring  reboot  = machine.empty() ? std::wstring (L"the machine")
                                            : (L"the " + machine);



    prompt.title = L"Disk modified outside Casso";

    //  THE TWO READ DIFFERENTLY BECAUSE THEY DESCRIBE DIFFERENT EVENTS. One
    //  program did all of it; in the other the program did the modifying and
    //  the user did the inserting, so claiming the insertion for the writer
    //  would tell them somebody else pressed the button they just pressed.
    if (author == ChangeAuthor::CassoCli)
    {
        if (machineRebooted)
        {
            prompt.message = L"CassoCli modified " + file + L", inserted it into " + where
                           + L", and rebooted " + reboot + L".";
        }
        else
        {
            prompt.message = L"CassoCli modified " + file + L" and inserted it into " + where
                           + L".";
        }
    }
    else if (machineRebooted)
    {
        prompt.message = L"Another program modified " + file + L", " + where
                       + L" now has the modified version, and " + reboot + L" was rebooted.";
    }
    else
    {
        prompt.message = L"Another program modified " + file + L", and " + where
                       + L" now has the modified version.";
    }

    //  THE SAME SENTENCE THE QUESTION USES. What became of the disk that was
    //  in the drive is one fact, and it reads the same way wherever it is
    //  reported: it has a new name, and here it is.
    if (copyAlreadyWritten && !copy.empty())
    {
        prompt.message += L" Your disk has been renamed to " + copy + L".";
    }

    prompt.answers.push_back (PromptAnswer { L"Dismiss", ChangeAction::Ignore });

    //  The write asked for this, so the notice does not need dismissing.
    prompt.selfDismisses = true;

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposeConflictReport
//
//  What is shown when a flush found the file changed and moved out of its way.
//
//  THE FILE STAYED WITH WHOEVER CHANGED IT. That is the rule in both
//  directions, so this says where the guest's version went rather than which
//  version won.
//
//  IT FLOWS, WHERE THE QUESTION BREAKS INTO PARAGRAPHS. This one is not a
//  dialog: the report sink puts it in the message bar across the machine, and
//  a strip that thin reads better as two or three sentences than as a stack
//  of one-line paragraphs. The question can afford the layout because it has
//  a dialog to itself.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeConflictReport (const std::string & imagePath, int drive,
                                                  const std::string & copyPath)
{
    ChangePrompt  prompt;
    std::wstring  file  = GetFileName (imagePath);
    std::wstring  where = DriveLabel (drive);
    std::wstring  copy  = GetFileName (copyPath);



    prompt.title   = L"Disk modified outside Casso";

    prompt.message = L"Another program modified " + file + L" while it was mounted in "
                   + where + L". Your disk has been renamed to " + copy
                   + L" and remounted in " + where
                   + L". No changes were made to the other program's modified version "
                     L"of " + file + L".";

    prompt.answers.push_back (PromptAnswer { L"Dismiss", ChangeAction::Ignore });

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposeSaveFailure
//
//  A copy that could not be written.
//
//  NOTHING PROCEEDED, and the message says so, because the action this was
//  protecting is the one that would have destroyed the version being copied.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeSaveFailure (const std::string & imagePath, int drive,
                                               const std::string & attemptedPath,
                                               HRESULT             reason,
                                               SaveFailureCause    cause)
{
    ChangePrompt  prompt;
    std::wstring  file  = GetFileName (imagePath);
    std::wstring  where = DriveLabel (drive);
    std::wstring  full  = fs::path (attemptedPath).wstring();



    prompt.title = L"Error saving to disk";

    prompt.message = (cause == SaveFailureCause::FileLost)
                         ? (file + L" is gone. We tried to save your changes to")
                         : (L"Another program modified " + file
                            + L". We tried to save your changes to");

    prompt.message += L"\n\n" + full + L"\n\nError: " + DescribeError (reason) + L"\n\n";

    prompt.message += (cause == SaveFailureCause::FileLost)
                          ? (L"Your disk is still in " + where + L".")
                          : (L"Your changes are still in " + where + L", and the modified "
                             + file + L" hasn't been inserted.");

    prompt.answers.push_back (PromptAnswer { L"Save as...", ChangeAction::PreserveCopy });
    prompt.answers.push_back (PromptAnswer { L"Dismiss",    ChangeAction::Ignore });

    return prompt;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::ComposeLostFile
//
//  The file behind a mounted disk has gone, or stopped being readable.
//
//  THE PATH LEADS, ON A LINE OF ITS OWN. The title says what happened and the
//  first line says to which file, in full, because which file is the one thing
//  the reader has to check. It is the one notice besides the save failure that
//  prints a whole path, and it does so on its own line rather than buried in a
//  sentence. What follows is only what the screen will not show for itself:
//  the disk is still in memory, and the two things that can be done with it.
//  "Discard" needs no gloss; people know what it means.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeLostFile (const std::string & imagePath, int drive,
                                            ChangeAction action)
{
    ChangePrompt  prompt;
    std::wstring  full  = fs::path (imagePath).wstring();
    std::wstring  where = DriveLabel (drive);



    prompt.title = (action == ChangeAction::Deleted) ? L"Mounted disk has been removed"
                                                     : L"Mounted disk can't be read";

    prompt.message = full + L"\n\n" + where
                   + L" still has the disk's contents in memory. "
                     L"You can save it to a new file or discard it.";

    prompt.answers.push_back (PromptAnswer { L"Save as...", ChangeAction::PreserveCopy });
    prompt.answers.push_back (PromptAnswer { L"Discard",    ChangeAction::KeepHeld });

    return prompt;
}
