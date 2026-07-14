#pragma once

class Microcode;




////////////////////////////////////////////////////////////////////////////////
//
//  GetCpu65C02InstructionSet
//
//  Returns a process-lifetime 256-entry CMOS 65C02 (Rockwell R65C02) instruction
//  table suitable for building an assembler OpcodeTable. This lets a caller (the
//  CLI's `as65 --cpu 65c02` path) target the 65C02 without pulling the emulator's
//  memory-bus machinery into its own translation units: only the mnemonics,
//  addressing modes, and legality are consulted by the assembler; the operation
//  and register pointers the table also carries are never dereferenced there.
//
////////////////////////////////////////////////////////////////////////////////

const Microcode * GetCpu65C02InstructionSet ();
