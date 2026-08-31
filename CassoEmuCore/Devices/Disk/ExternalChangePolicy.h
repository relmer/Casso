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
//  it as a distinct value is what keeps "nobody said" from being confused with
//  "somebody said carry on".
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

    //  Put the question to the user, because nobody stated an intent.
    Ask,

    //  The guest has written and the file changed: a two-sided conflict.
    Conflict,

    //  The bytes cannot be used as this disk. Carry on with what is held.
    Unusable,

    //  Wait: something else is holding the file, or the guest is mid-operation.
    Defer,

    //  Keep what the emulator holds and leave the file as it is.
    //
    //  NOT THE SAME AS Ignore, WHICH THE POLICY REACHES WHEN NOTHING HAPPENED.
    //  This is a decision about a change that did: the user was told and chose
    //  the disk in memory. The file is left exactly as the external writer left
    //  it, and a later flush over it stays refused -- so "ignore the changes"
    //  means ignore them, not overwrite them at the next opportunity.
    KeepHeld,

    //  Write what is held to a timestamped copy beside the original.
    //
    //  AN ANSWER RATHER THAN A DECISION. Nothing chooses this on its own; it is
    //  what the user picks when told the file behind a mounted disk is gone,
    //  where what the emulator holds may be the only copy left.
    PreserveCopy,
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
//  THERE IS NO CONFIGURABLE FALLBACK, and its absence is a decision rather than
//  an omission. There was one -- a stored answer saying what to do when nobody
//  stated an intent -- and it earned nothing: a writer that can speak states its
//  intent and never reaches this branch, so the only writers left are text
//  editors, copies and other emulators. Being asked about those is rare enough
//  that a setting to suppress it would sit at its default forever, while a
//  stored answer that disagrees with what the user meant is a real way to lose
//  a disk. Nobody stated an intent, so we ask.
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
