#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DialectId
//
//  Which assembler's syntax is in force. A token rather than a string for the
//  same reason Directive is: a switch over it is exhaustiveness-checked, so
//  adding a dialect fails the build at every site that has to handle it instead
//  of silently falling through one of them.
//
//  As65 is first and is the default everywhere, so every caller that predates
//  dialect selection keeps the behavior it had.
//
//  A dialect is SYNTAX ONLY. It never implies a CPU: 65C02 code can be written
//  in any dialect, and the mechanism must not assume a profile has just one
//  instruction set available to it. Where a profile takes its CPU FROM is part
//  of the profile -- as65 has no in-source directive and takes it from the
//  command line, Merlin has one and takes it from there exclusively.
//
////////////////////////////////////////////////////////////////////////////////

enum class DialectId
{
    As65,           // Casso's original dialect; the inferred default
    Merlin,         // The classic Apple II dialect

    Count,          // sentinel: sizes dialect-indexed tables
};
