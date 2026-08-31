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
//  A question about a changed image, and the answers it accepts.
//
//  COMPOSED IN CORE, DRAWN IN THE SHELL. Which question gets asked, what it
//  says, which answers exist and what each one means are all decisions, and a
//  decision that lives in an executable is a decision no test can reach. The
//  shell receives a title, a message and a list of labeled answers, and its
//  entire job is to put them on the screen and report which was chosen.
//
//  IT IS A SEPARATE JOB FROM DECIDING. ExternalChangePolicy says what should
//  happen; this says what to ask when what should happen is "ask". Folding the
//  wording into the policy would give the pure decision table a dependency on
//  presentation, and there is more than one question here -- the plain ask, the
//  two-sided conflict, and the image that can no longer be used.
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



    //  The question this action calls for, about this image.
    //
    //  THE IMAGE IS NAMED IN EVERY MESSAGE. A user with several disks mounted
    //  cannot act on a question that does not say which one it is about, and
    //  making the path a parameter of composition is what stops a caller from
    //  producing one that omits it.
    static ChangePrompt  Compose (const std::string & imagePath, ChangeAction action);

    //  The report shown when contents were taken up without asking.
    //
    //  IT CARRIES THE RESTART, and that is the point rather than a courtesy: a
    //  swap can turn out to have been the wrong call, and the recovery is
    //  precisely the action the user was not offered. A report that cleared
    //  itself would take that action away with it.
    static ChangePrompt  ComposePickUpReport (const std::string & imagePath,
                                              bool                machineRestarted);
};
