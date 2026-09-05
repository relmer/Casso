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
//  ChangeAuthor
//
//  Who rewrote a mounted file, as far as this emulator can tell.
//
//  ONLY THE INTENT CHANNEL CAN SAY, and only CassoCli sends on it. A directory
//  watch reports that a file changed and cannot report who changed it, so a
//  change that arrived without a stated intent came from a program this store
//  has no way to identify.
//
//  AN ENUM RATHER THAN A BOOL because it sits beside two other flags on the
//  same call. Naming the author at the call site is what stops a reader, and a
//  caller, from taking one of the three for another.
//
////////////////////////////////////////////////////////////////////////////////

enum class ChangeAuthor
{
    //  The write stated what it was for, over the channel nothing else sends
    //  on.
    CassoCli,

    //  Anything else. The watcher saw the file change and that is all it saw.
    AnotherProgram,
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

    //  Whether this one closes itself after a while.
    //
    //  ONLY THE RELOAD NOTICE DOES, because it is the only one confirming
    //  something the user asked for -- with a switch on the write, or by
    //  answering the question -- and making them dismiss it charges them twice
    //  for the same decision. The notices about a copy being written, or
    //  failing to be, report something they did NOT ask for and stand until
    //  read.
    bool                       selfDismisses = false;

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
    //  before asking anything, so the two cases differ by one tense: "it will
    //  be renamed to" against "we've renamed your disk to".
    //
    //  IT IS ALWAYS PUT AS A RENAME, never as a save. The user's disk is a
    //  file to them, and what happened to it is that it has a new name.
    //  Telling them their writes "hadn't been saved yet" describes a write
    //  cache they have no view of and reads as though work had been at risk.
    static ChangePrompt  Compose (const std::string & imagePath, int drive,
                                  ChangeAction action,
                                  const std::string & copyPath   = std::string(),
                                  bool copyAlreadyWritten        = false);

    //  The notice shown once contents were reloaded, whether the write said
    //  what it was for or the user answered the question above.
    //
    //  IT ATTRIBUTES THE WRITE ONLY WHEN `author` SAYS SO. It used to name
    //  CassoCli unconditionally, on the belief that no other route reached it;
    //  answering the question does reach it, so a change by any program at all
    //  was reported as CassoCli's, and the insertion the user had just asked
    //  for was reported as something the writer did.
    //
    //  `machineName` IS THE MACHINE AS THE USER KNOWS IT -- "Apple //e" --
    //  because "the Apple" is not what is in front of them.
    //
    //  `copyAlreadyWritten` CARRIES THE SAME MEANING IT DOES ABOVE, and is here
    //  for the same reason: a path is where a copy would go, never evidence
    //  that one is there. A bay reserves the name when the question is put and
    //  keeps holding it when a write fails, so reading a non-empty `copyPath`
    //  as a rename reported files that were never created.
    static ChangePrompt  ComposeReloadReport (const std::string & imagePath, int drive,
                                              bool machineRebooted,
                                              const std::string & machineName,
                                              ChangeAuthor author,
                                              const std::string & copyPath   = std::string(),
                                              bool copyAlreadyWritten        = false);

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

    //  "work.dsk". The file alone, not the path: a message about a disk in a
    //  drive is about the disk, and a folder in the middle of a sentence buries
    //  the one word the reader is looking for. Two notices print the whole
    //  path deliberately -- the save failure and the lost file -- and both put
    //  it on a line of its own, where it is the thing to act on.
    static std::wstring  GetFileName (const std::string & imagePath);

    //  "Drive 1", from the store's zero-based index.
    static std::wstring  DriveLabel (int drive);

    //  "0x80070070 There is not enough space on the disk." The code always, the
    //  system's own text when there is one, because a bare code sends the
    //  reader to a search engine and text alone cannot be looked up at all.
    static std::wstring  DescribeError (HRESULT reason);
};
