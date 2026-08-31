#include "Pch.h"

#include "ChangePrompt.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NameOf
//
//  The image as the user will recognize it.
//
//  THE LEAF NAME, NOT THE WHOLE PATH. A question is read in a hurry and the
//  directory is the part that is already known; a full path pushes the one word
//  that identifies the disk off the end of the line.
//
////////////////////////////////////////////////////////////////////////////////

static std::wstring NameOf (const std::string & imagePath)
{
    std::wstring  name = fs::path (imagePath).filename().wstring();



    if (name.empty())
    {
        name = L"(unnamed image)";
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt::Compose
//
//  What to ask about a changed image.
//
//  THREE QUESTIONS AND NOT ONE, because three different things have gone on and
//  the answers differ:
//
//    Ask       -- the file changed, nothing is at stake, how should the guest
//                 carry on
//    Conflict  -- both sides have written, and neither may be discarded without
//                 the user saying so
//    Unusable  -- the bytes can no longer be this disk, so what is held may be
//                 the only copy left
//
//  ANY OTHER ACTION COMPOSES NOTHING. Taking contents up, restarting, deferring
//  and ignoring are not questions, and returning an empty prompt for them is
//  what lets a caller compose unconditionally and draw only what has answers.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::Compose (const std::string & imagePath, ChangeAction action)
{
    ChangePrompt  prompt;
    std::wstring  name = NameOf (imagePath);



    switch (action)
    {
    case ChangeAction::Ask:
        prompt.title   = L"A disk changed outside Casso";
        prompt.message = name +
            L" was changed by something else while it was mounted.\n\n"
            L"Taking it up leaves the machine running, which is what a build "
            L"loop wants. Restarting is the safe answer: the guest keeps its own "
            L"idea of the disk's structure in memory, and swapping a disk under "
            L"it cannot be made safe from here.";

        prompt.answers.push_back (PromptAnswer { L"Take it up",  ChangeAction::TakeUpInPlace });
        prompt.answers.push_back (PromptAnswer { L"Restart",     ChangeAction::Restart });
        prompt.answers.push_back (PromptAnswer { L"Keep what I have", ChangeAction::KeepHeld });
        break;

    case ChangeAction::Conflict:
        prompt.title   = L"Two versions of one disk";
        prompt.message = name +
            L" was changed by something else, and the machine has written to it "
            L"since it was mounted.\n\n"
            L"Both versions exist and only one can stay mounted. Whichever you do "
            L"not keep is saved beside the original with a timestamp in its name, "
            L"so nothing is discarded either way.";

        prompt.answers.push_back (PromptAnswer { L"Keep the file on disk", ChangeAction::TakeUpInPlace });
        prompt.answers.push_back (PromptAnswer { L"Keep what the machine wrote", ChangeAction::KeepHeld });
        break;

    case ChangeAction::Unusable:
        prompt.title   = L"A mounted disk can no longer be read";
        prompt.message = name +
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
        //  dialog, so a caller may compose for any action and draw only what
        //  came back with answers.
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
//  IT IS NOT A QUESTION AND IT STILL CARRIES AN ANSWER. The one action offered
//  is the restart, because the user did not choose the swap and the swap may
//  turn out to have been wrong: the guest's cached idea of the disk's structure
//  is invisible from here and a restart is the only thing that clears it.
//
////////////////////////////////////////////////////////////////////////////////

ChangePrompt ChangePrompt::ComposePickUpReport (const std::string & imagePath,
                                                bool                machineRestarted)
{
    ChangePrompt  prompt;
    std::wstring  name = NameOf (imagePath);



    prompt.title = L"A disk changed outside Casso";

    if (machineRestarted)
    {
        prompt.message = name + L" changed and the machine was restarted.";
    }
    else
    {
        prompt.message = name +
            L" changed and the new contents were taken up. The machine is still "
            L"running. If it misbehaves, restart it: the guest may still be acting "
            L"on the structure of the disk it had before.";

        prompt.answers.push_back (PromptAnswer { L"Restart", ChangeAction::Restart });
    }

    return prompt;
}
