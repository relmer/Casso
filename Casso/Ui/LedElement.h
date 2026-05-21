#pragma once

#include "Pch.h"

#include <RmlUi/Core/Element.h>





////////////////////////////////////////////////////////////////////////////////
//
//  LedElement
//
//  Thin C++ helper around the "LED is just a <div class='led led--idle'>"
//  pattern. Per R5 the LED has no behavior of its own; this class only
//  exists to centralize the class-toggle calls so a future renaming
//  doesn't have to hunt through DriveWidgetElement.cpp.
//
//  States:
//      Idle    -> drive empty                  (.led + .led--idle)
//      Present -> disk mounted, motor off      (.led + .led--present)
//      Active  -> motor on (and/or active r/w) (.led + .led--active)
//
//  Glow is RCSS `box-shadow` per theme; see Resources/Themes/<Theme>/
//  drive_widgets.rcss.
//
////////////////////////////////////////////////////////////////////////////////

class LedElement
{
public:
    enum class State
    {
        Idle,
        Present,
        Active,
    };

    // Set the state on an `Rml::Element` already tagged with class
    // "led". No-op when `pLed` is null (callers in the drive widget
    // sometimes haven't created the child yet).
    static void SetState (Rml::Element * pLed, State state);
};
