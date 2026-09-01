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
//  SaveFailureCause
//
//  Why a copy was being written when the write failed.
//
//  THE TWO READ NOTHING ALIKE and used to share one message. A file that was
//  deleted was reported with "another program modified it", which is not what
//  happened and sends the reader looking for a program that does not exist.
//
////////////////////////////////////////////////////////////////////////////////

enum class SaveFailureCause
{
    //  Something else rewrote the file while it was in a drive.
    ExternalChange,

    //  The file is gone, or is no longer readable as a disk.
    FileLost,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChangePrompt
//
//  What the user is told about a changed image, and the answers it accepts.
//
//  COMPOSED IN CORE, DRAWN IN THE SHELL. Which question is put, what it says,
//  which answers exist and what each one means are all decisions, and a
//  decision that lives in an executable is a decision no test can reach. The
//  shell receives a title, a message and a list of labeled answers, and its
//  entire job is to put them on the screen and report which was chosen.
//
//  IT IS A SEPARATE JOB FROM DECIDING. ExternalChangePolicy settles what should
//  happen; this settles what to tell the user about it. Folding the wording
//  into the policy would give the pure decision table a dependency on
//  presentation, and there is more than one thing to tell them here.
//
//  EVERY MESSAGE CARRIES THE FILE AND THE DRIVE. A user with two disks mounted
//  cannot act on a message that gives neither, and making both parameters of
//  composition is what stops a caller from producing one that omits them.
//
//  A TITLE IS A CONDITION, NOT AN INSTANCE. "Disk modified outside Casso", not
//  "work.dsk modified outside Casso". The specifics belong in the body, where
//  there is room for them, and a title that repeats the first line of the body
//  word for word is a wasted line.
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



    //  The question put when something else changed a mounted file and nothing
    //  stated what the change was for.
    //
    //  `drive` IS THE STORE'S ZERO-BASED INDEX and is written out as the
    //  one-based number on the machine, because that is the number printed on
    //  the drive and shown on the widget.
    //
    //  `copyPath` IS WHERE THE VERSION IN THE DRIVE GOES IF IT IS KEPT, and
    //  `copyAlreadyWritten` says whether it is there yet. A conflict writes it
    //  before asking anything, so the two cases differ by one tense: "we'll
    //  rename it to" against "it's already saved as".
    static ChangePrompt  Compose (const std::string & imagePath, int drive,
                                  ChangeAction action,
                                  const std::string & copyPath   = std::string(),
                                  bool copyAlreadyWritten        = false);

    //  The notice shown once contents were taken up without a question, which
    //  only happens when the write stated what it was for.
    //
    //  IT ATTRIBUTES THE WRITE TO CassoCli, and can, because this notice is
    //  unreachable any other way: the intent travels over a channel nothing
    //  else sends on. The question above stays general, since any program at
    //  all can raise that one.
    //
    //  `machineName` IS THE MACHINE AS THE USER KNOWS IT -- "Apple //e" --
    //  because "the Apple" is not what is in front of them.
    static ChangePrompt  ComposePickUpReport (const std::string & imagePath, int drive,
                                              bool machineRebooted,
                                              const std::string & machineName,
                                              const std::string & copyPath = std::string());

    //  The notice shown when a flush found the file changed underneath it and
    //  moved the guest's version to a file of its own.
    static ChangePrompt  ComposeConflictReport (const std::string & imagePath, int drive,
                                                const std::string & copyPath);

    //  The failure shown when a copy could not be written.
    //
    //  IT CARRIES THE PATH IT TRIED AND WHAT THE SYSTEM SAID, and offers
    //  somewhere else to put the file. A message that only advises freeing
    //  space leaves the user to find the folder and guess the reason.
    static ChangePrompt  ComposeSaveFailure (const std::string & imagePath, int drive,
                                             const std::string & attemptedPath,
                                             HRESULT             reason,
                                             SaveFailureCause    cause);

    //  What is shown when the file behind a mounted disk has gone or can no
    //  longer be read, with the offer to write out what is still in memory.
    //
    //  THE OFFER DOES NOT DEPEND ON THE GUEST HAVING WRITTEN. With the file
    //  gone, what the emulator holds may be the only copy of that disk either
    //  way.
    static ChangePrompt  ComposeLostFile (const std::string & imagePath, int drive,
                                          ChangeAction action);

    //  "work.dsk". The file alone, never the path: a message about a disk in a
    //  drive is about the disk, and a folder in the middle of a sentence buries
    //  the one word the reader is looking for. The failure above is the
    //  exception, and prints the whole path deliberately.
    static std::wstring  FileName (const std::string & imagePath);

    //  "Drive 1", from the store's zero-based index.
    static std::wstring  DriveLabel (int drive);

    //  "0x80070070 There is not enough space on the disk." The code always, the
    //  system's own text when there is one, because a bare code sends the
    //  reader to a search engine and text alone cannot be looked up at all.
    static std::wstring  DescribeError (HRESULT reason);
};
