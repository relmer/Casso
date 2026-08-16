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
//  An enumerator is added WITH its profile, never ahead of it. DialectRegistry
//  must answer for every value here, and a placeholder enumerator would resolve
//  to the wrong profile while looking like support that exists. The registry
//  sweep in DialectMechanismTests enforces this, and caught exactly that
//  mistake the first time it ran.
//
//  That rule holds BECAUSE this enum is TOTAL over the profiles: every
//  enumerator must have a counterpart, so an enum sweep is a valid test. Do not
//  copy the sweep to an enum where partial coverage is the design --
//  CommandLineOptions::Subcommand is the counter-example, where None, Help,
//  Version and As65 deliberately have no row in the subcommand table because
//  they are not bare words a user types. An enum sweep there would fail
//  correctly-shaped code. DirectiveTokenTests is the pattern to copy: it sweeps
//  spellings to tokens and tokens back to canonical names, and Directive is
//  total the same way this is.
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
    Merlin,         // Glen Bredon's Merlin, in its absolute (non-relocatable) subset

    Count,          // sentinel: sizes dialect-indexed tables
};
