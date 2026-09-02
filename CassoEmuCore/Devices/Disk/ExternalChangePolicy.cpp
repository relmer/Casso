#include "Pch.h"

#include "ExternalChangePolicy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::Decide
//
//  What to do about a change that has been noticed.
//
//  READ THE ORDER AS THE RULE. Each test below outranks the ones under it, and
//  the ranking is what the feature promises:
//
//    nothing seen        -- there is no question to answer
//    still being written -- acting now would read a half-written disk
//    cannot be used      -- there is nothing to take up
//    guest has written   -- two sides have work, and neither may be discarded
//    a stated intent     -- the writer knew what they changed
//    otherwise           -- nobody said, so ask
//
//  THE CONFLICT TEST SITS ABOVE THE INTENT TEST DELIBERATELY. An intent says
//  how the guest carries on; it never grants permission to discard work. Moving
//  it below would let `--on-change reload` silently throw away a guest's
//  unsaved writes, which is the one outcome this whole feature exists to
//  prevent.
//
////////////////////////////////////////////////////////////////////////////////

ChangeAction ExternalChangePolicy::Decide (const Situation & situation)
{
    ChangeAction  action = ChangeAction::Ignore;



    if (!situation.changeSeen)
    {
        action = ChangeAction::Ignore;
    }
    else if (situation.heldByOther)
    {
        //  Deferred rather than refused: the pick-up simply happens once the
        //  writer lets go. Both writers here commit atomically, but a text
        //  editor or a copy tool need not, and the quiet period alone does not
        //  cover one that takes its time.
        action = ChangeAction::Defer;
    }
    else if (!situation.usable)
    {
        action = ChangeAction::Unusable;
    }
    else if (situation.guestDirty)
    {
        action = ChangeAction::Conflict;
    }
    else if (situation.intent == PickUpIntent::TakeUpInPlace)
    {
        action = ChangeAction::TakeUpInPlace;
    }
    else if (situation.intent == PickUpIntent::Restart)
    {
        action = ChangeAction::Restart;
    }
    else
    {
        //  Nobody stated an intent, so nobody but the user can supply one.
        //  There is deliberately no stored answer to consult here: see the
        //  header for why one was removed rather than kept.
        action = ChangeAction::Ask;
    }

    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::NeedsAnAnswer
//
//  Whether the machine stops and waits for a person.
//
////////////////////////////////////////////////////////////////////////////////

bool ExternalChangePolicy::NeedsAnAnswer (ChangeAction action)
{
    //  A conflict is NOT here any more, and its absence is the decision. It
    //  used to stop the machine to ask which of two versions to keep; both now
    //  survive whatever happens, so there was no wrong answer to protect
    //  against and the question was ceremony.
    return action == ChangeAction::Ask
        || IsFileLost (action);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::IsFileLost
//
//  Whether the file behind the disk can no longer be used, however it got that
//  way. The two cases say different things and then do the same thing.
//
////////////////////////////////////////////////////////////////////////////////

bool ExternalChangePolicy::IsFileLost (ChangeAction action)
{
    return action == ChangeAction::Unusable
        || action == ChangeAction::Deleted;
}
