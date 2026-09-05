#pragma once

#include "Pch.h"

#include "DialogDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyboardMapText
//
//  Builds the body of the Keyboard map dialog for one machine. Split from the
//  dialog so the decisions are unit-testable, the first of them being that the
//  text must never describe a key the running machine does not have.
//
////////////////////////////////////////////////////////////////////////////////

class KeyboardMapText
{
public:
    // What the running machine can do, as the dialog needs to know it. Taken
    // from the live devices rather than a model name, so a machine gains a
    // row the moment it gains the hardware.
    struct Machine
    {
        // Open and Closed Apple ($C061 / $C062), a //e addition.
        bool  hasAppleKeys = false;

        // The keys the //e added to the keyboard: up and down arrows, TAB
        // and DELETE. All four arrived together, so one flag covers them.
        bool  hasTwoeKeys = false;

        // A game port or //e soft-switch bank to drive, without which the
        // joystick mapping has nothing to move.
        bool  hasGamePort = false;
    };

    static std::vector<DialogTextRun>  BuildBody (const Machine & machine);
};
