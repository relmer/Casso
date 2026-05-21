#include "Pch.h"

#include "LedElement.h"






////////////////////////////////////////////////////////////////////////////////
//
//  SetState
//
//  Removes the two unused state classes and adds the active one. Done in
//  that order so the final visual state never momentarily shows zero
//  state classes between SetClass (false) + SetClass (true) calls.
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
