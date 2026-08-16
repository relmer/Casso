#pragma once

#include "OpcodeTable.h"





class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  InstructionSetProvider
//
//  The instruction tables an assembly may choose between: a base set, and
//  optionally an extended one a dialect's in-source directive can switch to.
//
//  DELIBERATELY IMMUTABLE. Which set is active is per-RUN state and lives on
//  the session, not here -- a provider is built once and shared across every
//  assembly, so holding the active selection would let one run's CPU directive
//  contaminate the next. That is the same reasoning that makes AssemblySession
//  a session rather than methods on Assembler.
//
//  Both tables are INJECTED rather than looked up. The extended table lives in
//  the emulator library, and this one must not reach across that boundary; the
//  caller that has both hands them over.
//
//  When no extended set is supplied, GetExtended returns the base. A dialect
//  that asks to switch on a provider with nothing to switch to gets the set it
//  already had, which is the honest answer -- the alternative is a null table
//  every caller has to test for.
//
////////////////////////////////////////////////////////////////////////////////

class InstructionSetProvider
{
public:
    explicit InstructionSetProvider (const Microcode base[256]);
    InstructionSetProvider          (const Microcode base[256], const Microcode extended[256]);

    const OpcodeTable &  GetBase () const { return m_base; }

    // With no extended set supplied this returns the BASE table rather than
    // null. The benefit is that no caller needs a null check; the risk is that
    // one reads the result as "extended opcodes are available" and encodes
    // something only the wider set defines.
    //
    // That risk is acceptable because the failure is LOUD rather than silent:
    // the base table does not merely lack the encoding, it does not recognize
    // the mnemonic at all, so the line is reported as an unknown instruction
    // instead of assembling to something plausible and wrong. Pinned by
    // InstructionSetProviderTests::ExtendedSet_EncodesInstructionsTheBaseRejects.
    //
    // A caller that must not reach this state should ask HasExtended first --
    // which is what a dialect promising the source that extended opcodes became
    // legal is obliged to do.
    const OpcodeTable &  GetExtended () const { return m_hasExtended ? m_extended : m_base; }

    // Whether an in-source CPU directive has anything to select. A dialect must
    // ask before promising the source that extended opcodes became legal.
    bool                 HasExtended () const { return m_hasExtended; }

private:
    OpcodeTable  m_base;
    OpcodeTable  m_extended;
    bool         m_hasExtended = false;
};
