#include "Pch.h"

#include "LedElement.h"






////////////////////////////////////////////////////////////////////////////////
//
//  SetState
//
//  Set the state on an `Rml::Element` already tagged with class "led".
//  No-op when `pLed` is null.
//
////////////////////////////////////////////////////////////////////////////////

void LedElement::SetState (Rml::Element * pLed, State state)
{
    if (pLed == nullptr)
    {
        return;
    }

    const bool  fIdle    = (state == State::Idle);
    const bool  fPresent = (state == State::Present);
    const bool  fActive  = (state == State::Active);

    pLed->SetClass ("led--idle",    fIdle);
    pLed->SetClass ("led--present", fPresent);
    pLed->SetClass ("led--active",  fActive);
}
