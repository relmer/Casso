#include "Pch.h"

#include "ChangePrompt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::FileName
//
//  The file a message is about, without its folder.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ChangePrompt::FileName (const std::string & imagePath)
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
//  changes", which named the external edit twice and said nothing about the
//  disk in the drive; a reader could reasonably take "ignore" for "throw the
//  new file away". Each label now states the action, and the body states its
//  consequence.
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
    std::wstring  file  = FileName (imagePath);
    std::wstring  where = DriveLabel (drive);
    std::wstring  copy  = FileName (copyPath);



    switch (action)
    {
    case ChangeAction::Ask:
        prompt.title   = L"Disk modified outside Casso";

        prompt.message = L"Another program modified " + file + L" while it was in " + where + L".";

        //  A conflict wrote the copy before anything was asked, so the guest's
        //  writes are already safe and the reader is told so up front.
        if (copyAlreadyWritten && !copy.empty())
        {
            prompt.message += L" Your writes to it hadn't been saved yet, so we saved them as "
                            + copy + L".";
        }

        prompt.message += L"\n\nInsert the modified " + file + L" into " + where + L".";

        prompt.message += L"\n\nKeep your current version in " + where + L".";

        if (!copy.empty())
        {
            prompt.message += copyAlreadyWritten
                                  ? (L" It's already saved as " + copy + L".")
                                  : (L" We'll rename it to " + copy
                                     + L" so that it doesn't conflict with the " + file
                                     + L" that the other program modified.");
        }

        prompt.answers.push_back (PromptAnswer { L"Insert the modified " + file,
                                                 ChangeAction::TakeUpInPlace });
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
//  ChangePrompt::ComposePickUpReport
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
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposePickUpReport (const std::string & imagePath, int drive,
                                                bool machineRebooted,
                                                const std::string & machineName,
                                                const std::string & copyPath)
{
    ChangePrompt  prompt;
    std::wstring  file    = FileName (imagePath);
    std::wstring  where   = DriveLabel (drive);
    std::wstring  copy    = FileName (copyPath);
    std::wstring  machine = fs::path (machineName).wstring();



    prompt.title = L"Disk modified outside Casso";

    if (machineRebooted)
    {
        prompt.message = L"CassoCli modified " + file + L", inserted it into " + where;

        prompt.message += machine.empty() ? L", and rebooted the machine."
                                          : (L", and rebooted the " + machine + L".");
    }
    else
    {
        prompt.message = L"CassoCli modified " + file + L" and inserted it into " + where + L".";
    }

    if (!copy.empty())
    {
        prompt.message += L" Your writes to it hadn't been saved yet, so we saved them as "
                        + copy + L".";
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
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeConflictReport (const std::string & imagePath, int drive,
                                                  const std::string & copyPath)
{
    ChangePrompt  prompt;
    std::wstring  file  = FileName (imagePath);
    std::wstring  where = DriveLabel (drive);
    std::wstring  copy  = FileName (copyPath);



    prompt.title   = L"Disk modified outside Casso";

    prompt.message = L"Another program modified " + file + L", so we saved your changes as "
                   + copy + L". " + where + L" now uses that file, and " + file
                   + L" keeps the other program's changes.";

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
    std::wstring  file  = FileName (imagePath);
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
//  THE DRIVE IS EMPTIED EITHER WAY, and the message says so. That is the part
//  no answer undoes, and neither wording mentioned it.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposeLostFile (const std::string & imagePath, int drive,
                                            ChangeAction action)
{
    ChangePrompt  prompt;
    std::wstring  file  = FileName (imagePath);
    std::wstring  where = DriveLabel (drive);



    if (action == ChangeAction::Deleted)
    {
        prompt.title   = L"Disk file is missing";
        prompt.message = file + L" is no longer on disk, but " + where
                       + L" still has its contents in memory.";
    }
    else
    {
        prompt.title   = L"Disk file can't be read";
        prompt.message = file + L" is still there, but Casso can't read it as a disk any more. "
                       + where + L" still has its contents in memory.";
    }

    prompt.message += L" Would you like to save them? " + where
                    + L" will be empty if you don't.";

    prompt.answers.push_back (PromptAnswer { L"Save as...", ChangeAction::PreserveCopy });
    prompt.answers.push_back (PromptAnswer { L"Don't save", ChangeAction::KeepHeld });

    return prompt;
}
