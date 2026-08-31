#pragma once

#include "Pch.h"
#include "ExternalChangePolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PromptAnswer
//
//  One thing the user may choose when asked about a change.
//
//  THE LABEL AND THE OUTCOME TRAVEL TOGETHER, so a shell cannot draw a button
//  that means something other than what the core will do when it is pressed.
//
////////////////////////////////////////////////////////////////////////////////

struct PromptAnswer
{
    std::wstring  label;
    ChangeAction  action = ChangeAction::Ignore;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt
//
//  What is said about a changed image, and the answers it accepts.
//
//  COMPOSED IN CORE, DRAWN IN THE SHELL. Which question gets asked, what it
//  says, which answers exist and what each one means are all decisions, and a
//  decision that lives in an executable is a decision no test can reach. The
//  shell receives a title, a message and a list of labeled answers, and its
//  entire job is to put them on the screen and report which was chosen.
//
//  IT IS A SEPARATE JOB FROM DECIDING. ExternalChangePolicy says what should
//  happen; this says what to tell the user about it. Folding the wording into
//  the policy would give the pure decision table a dependency on presentation,
//  and there is more than one thing to say here.
//
//  EVERY MESSAGE NAMES THE IMAGE AND THE DRIVE. A user with two disks mounted
//  cannot act on a message that says neither, and making both parameters of
//  composition is what stops a caller from producing one that omits them.
//
////////////////////////////////////////////////////////////////////////////////

struct ChangePrompt
{
    std::wstring               title;
    std::wstring               message;
    std::vector<PromptAnswer>  answers;

    //  Whether there is anything to show at all. An action that needs no answer
    //  composes an empty prompt rather than a blank dialog.
    bool  IsAsked () const { return !answers.empty(); }



    //  What to say about this action, for this image in this drive.
    //
    //  `drive` IS THE STORE'S ZERO-BASED INDEX and is written out as the
    //  one-based number on the machine, because that is the number printed on
    //  the drive and shown on the widget.
    static ChangePrompt  Compose (const std::string & imagePath, int drive,
                                  ChangeAction action);

    //  The report shown when contents were taken up without asking.
    //
    //  IT OFFERS NO REBOOT. The toolbar already carries one, and a notice with
    //  a duplicate is one more thing to dismiss rather than one more thing to
    //  reach for. The report says what happened and, while the machine is still
    //  running, why a reboot might be needed; the single action it does carry
    //  is its own dismissal, which every standing notice needs.
    static ChangePrompt  ComposePickUpReport (const std::string & imagePath, int drive,
                                              bool machineRestarted);

    //  What is said once a conflict has been resolved.
    //
    //  A REPORT, NOT A QUESTION. Both versions survive whatever happens, so
    //  there is no wrong answer to protect the user from -- only a fact to
    //  tell them, including where the version that did not stay mounted went.
    static ChangePrompt  ComposeConflictReport (const std::string & imagePath, int drive,
                                                const std::string & preservedPath,
                                                bool                keptWhatTheGuestWrote);

    //  What is said when the version that would be displaced could not be
    //  written anywhere.
    //
    //  THE ACTION THAT WOULD HAVE DESTROYED IT DOES NOT PROCEED, so this says
    //  what did not happen as well as what failed. A preserve that silently
    //  did not happen breaks the promise exactly where it matters most.
    static ChangePrompt  ComposePreserveFailure (const std::string & imagePath, int drive);

    //  What is said when the file behind a mounted disk has gone or can no
    //  longer be read, with the offer to save what is still in memory.
    //
    //  THE OFFER DOES NOT DEPEND ON THE GUEST HAVING WRITTEN. With the file
    //  gone, what the emulator holds may be the only copy of that disk either
    //  way.
    static ChangePrompt  ComposeLostFile (const std::string & imagePath, int drive,
                                          ChangeAction action);

    //  Why a running program may not see a swapped disk correctly.
    //
    //  ONE SENTENCE IN ONE PLACE, because it is the same hazard whether the
    //  user was asked or merely told, and two copies would drift.
    //
    //  IT CONTAINS NO LINE BREAK, and callers that want one add it. A banner
    //  measures its own height by dividing the character count by a per-line
    //  estimate and never looks for a newline, so a message carrying one is
    //  drawn taller than the box that was sized for it -- measured, the warning
    //  ran out through the bottom border and off the right edge of the window.
    //  Dialogs lay text out in paragraphs and are free to add breaks.
    static const wchar_t *  StaleDirectoryWarning ();

    //  "Loader.dsk in Drive 1", for the middle of a sentence or a title.
    static std::wstring  NameInDrive (const std::string & imagePath, int drive);

    //  "The disk Loader.dsk in Drive 1", for the START of one.
    //
    //  A SENTENCE MAY NOT OPEN WITH A FILENAME. Capitalizing the name would be
    //  a lie about what the file is called, and leaving it lower-case opens
    //  every notice this feature shows with a small letter. The article carries
    //  the capital so the name stays exactly as it is on disk.
    static std::wstring  SentenceSubject (const std::string & imagePath, int drive);
};
