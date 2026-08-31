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
//    guest has written   -- two sides have work, and only a person may choose
//    a stated intent     -- the writer knew what they changed
//    the fallback        -- nobody stated one, so the user's standing answer
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
    else if (situation.fallback == FallbackAnswer::TakeUpInPlace)
    {
        action = ChangeAction::TakeUpInPlace;
    }
    else if (situation.fallback == FallbackAnswer::Restart)
    {
        action = ChangeAction::Restart;
    }
    else
    {
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
    return action == ChangeAction::Ask
        || action == ChangeAction::Conflict
        || action == ChangeAction::Unusable;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::ParseFallbackAnswer
//
//  What a stored spelling means.
//
//  SPELLED THE WAY audioDownloadConsent IS -- lower-case words, stored as
//  text rather than as a number -- so a preferences file stays readable and a
//  value written by one version is legible to the next.
//
////////////////////////////////////////////////////////////////////////////////

FallbackAnswer ExternalChangePolicy::ParseFallbackAnswer (const std::string & stored)
{
    FallbackAnswer  answer = FallbackAnswer::Ask;



    if (stored == "reload")
    {
        answer = FallbackAnswer::TakeUpInPlace;
    }
    else if (stored == "restart")
    {
        answer = FallbackAnswer::Restart;
    }

    return answer;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::SpellFallbackAnswer
//
//  How an answer is written down. The inverse of the parse above, and the two
//  are kept together so a round trip cannot lose a value.
//
////////////////////////////////////////////////////////////////////////////////

const char * ExternalChangePolicy::SpellFallbackAnswer (FallbackAnswer answer)
{
    const char *  spelling = "ask";



    switch (answer)
    {
    case FallbackAnswer::TakeUpInPlace:  spelling = "reload";   break;
    case FallbackAnswer::Restart:        spelling = "restart";  break;
    default:                             spelling = "ask";      break;
    }

    return spelling;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::IndexOfFallbackAnswer
//
//  Which row of an offered list this answer is.
//
//  ASKING IS ROW ZERO, which is the default and the one that acts on nothing --
//  so a control that fails to find a match lands on the harmless answer rather
//  than on one that swaps a disk under a running program.
//
////////////////////////////////////////////////////////////////////////////////

int ExternalChangePolicy::IndexOfFallbackAnswer (FallbackAnswer answer)
{
    int  index = 0;



    switch (answer)
    {
    case FallbackAnswer::TakeUpInPlace:  index = 1;  break;
    case FallbackAnswer::Restart:        index = 2;  break;
    default:                             index = 0;  break;
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExternalChangePolicy::FallbackAnswerAtIndex
//
//  Which answer a row is. The inverse of the above, kept beside it so the two
//  cannot drift and a reordering has to be one edit rather than two.
//
////////////////////////////////////////////////////////////////////////////////

FallbackAnswer ExternalChangePolicy::FallbackAnswerAtIndex (int index)
{
    FallbackAnswer  answer = FallbackAnswer::Ask;



    switch (index)
    {
    case 1:   answer = FallbackAnswer::TakeUpInPlace;  break;
    case 2:   answer = FallbackAnswer::Restart;        break;
    default:  answer = FallbackAnswer::Ask;            break;
    }

    return answer;
}
