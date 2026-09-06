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
        // The open and closed Apple keys ($C061 / $C062), a //e addition.
        // The other keys the //e added -- up and down arrows, TAB, DELETE --
        // need no flag, because they all reach their Apple equivalent under
        // the same name and a row saying so would teach nothing.
        bool  hasAppleKeys = false;

        // A game port or //e soft-switch bank to drive, without which the
        // joystick mapping has nothing to move.
        bool  hasGamePort = false;
    };

    static std::vector<DialogTextRun>  BuildBody (const Machine & machine);
};
