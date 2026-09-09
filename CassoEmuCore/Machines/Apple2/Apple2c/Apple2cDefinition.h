#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cDefinition
//
//  The Apple //c of 1984: an Enhanced //e with everything soldered
//  in. It runs the //e family devices, which is why their names
//  say //e and not //c -- the //e is where they debuted, not the
//  only machine that has them. What the //c does NOT have is slots;
//  its expansion is a row of back-panel ports, and those live in
//  its JSON definition because what is plugged into them is the
//  owner's choice.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cDefinition
{
public:
    static const MachineDefinition &  Get();
};
