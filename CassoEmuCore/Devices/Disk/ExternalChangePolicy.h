#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PickUpIntent
//
//  What a change to a mounted image should do to the machine running it.
//
//  `Unstated` IS A REAL VALUE, not a missing one. A change written by a text
//  editor, a copy, or a second emulator carries no intent, and that is the
//  ordinary case for everything except this project's own command line. Holding
//  it as a distinct value is what keeps the fallback from being a guess about
//  which of the other two somebody meant.
//
////////////////////////////////////////////////////////////////////////////////

enum class PickUpIntent
{
    Unstated,
    TakeUpInPlace,
    Restart,
};





////////////////////////////////////////////////////////////////////////////////
//
//  FallbackAnswer
//
//  What the user declared should happen when nothing states an intent.
//
////////////////////////////////////////////////////////////////////////////////

enum class FallbackAnswer
{
    Ask,
    TakeUpInPlace,
    Restart,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeAction
//
//  What the emulator does about a change it has noticed.
//
////////////////////////////////////////////////////////////////////////////////

enum class ChangeAction
{
    //  Nothing to do: no change, or one this emulator made itself.
    Ignore,

    //  Take the new contents; leave the machine running.
    TakeUpInPlace,

    //  Take the new contents and restart the machine.
    Restart,

    //  Put the question to the user, because nobody has answered it.
    Ask,

    //  The guest has written and the file changed: a two-sided conflict that
    //  no configuration may resolve.
    Conflict,

    //  The bytes cannot be used as this disk. Carry on with what is held.
    Unusable,

    //  Wait: something else is holding the file, or the guest is mid-operation.
    Defer,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy
//
//  Given what is known about a change, what should happen.
//
//  PURE, AND THAT IS THE WHOLE POINT. No clock, no files, no windows, no
//  emulator. Every rule this feature has about what to do lives here as a
//  function of its inputs, so the entire decision surface is a table a test can
//  sweep in both directions.
//
//  WHAT IT DOES NOT DECIDE is equally deliberate. It does not decide WHEN --
//  that is the pending record's quiet period and the machine's idle moment --
//  and it does not decide how anything looks.
//
////////////////////////////////////////////////////////////////////////////////

class ExternalChangePolicy
{
public:

    //  Everything the decision depends on.
    struct Situation
    {
        //  A change was noticed and has settled.
        bool          changeSeen   = false;

        //  The new bytes can be used as this disk.
        bool          usable       = true;

        //  The guest has written and those writes are not on disk yet.
        bool          guestDirty   = false;

        //  Something else holds the file open right now.
        bool          heldByOther  = false;

        //  What the writer said, if anything.
        PickUpIntent  intent       = PickUpIntent::Unstated;

        //  What the user declared for changes that state nothing.
        FallbackAnswer  fallback   = FallbackAnswer::Ask;
    };



    //  What to do about this situation.
    //
    //  THE ORDER OF THE TESTS IS THE DESIGN. A conflict outranks any stated
    //  intent, because an intent says how the guest continues and never whether
    //  work may be discarded. Unusable outranks both, because there is nothing
    //  to take up. And a file somebody else is still writing outranks
    //  everything, because acting on it would read a half-written disk.
    static ChangeAction  Decide (const Situation & situation);

    //  Whether this action needs the user before anything happens.
    static bool          NeedsAnAnswer (ChangeAction action);
};
