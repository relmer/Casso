#pragma once

#include "GlobalAddressingModes.h"





class Microcode;





////////////////////////////////////////////////////////////////////////////////
//
//  CycleReference
//
//  Renders the two instruction tables as a Markdown cycle reference, so the
//  document a reader consults is derived from the very numbers the emulator
//  bills rather than transcribed from somewhere else.
//
//  Both tables are INJECTED for the same reason InstructionSetProvider injects
//  its own: the CMOS table is built in the emulator library, and this one must
//  not reach across that boundary. The caller that has both hands them over.
//
//  What it reports is baseCycles, plus the one per-instruction timing flag the
//  tables carry. The conditional costs -- the page-crossing cycle on an indexed
//  read, the taken-branch cycles, and the 65C02's decimal ADC/SBC cycle -- are
//  added at run time by Cpu::StepOne, CpuOperations::ChargeBranchCycles and the
//  CMOS arithmetic, so they cannot appear as a per-opcode number. The generated
//  preamble states them instead, which is the only place a reader can be told
//  how to compute the real cost.
//
////////////////////////////////////////////////////////////////////////////////

class CycleReference
{
public:
    // An instruction table is indexed by a whole opcode byte, so this is both
    // the table size and the array bound Format expects.
    static constexpr int    kOpcodeCount = 256;

    // The whole document, newline-terminated lines, LF only. The guard test
    // compares this against the checked-in copy with line endings normalized,
    // so the caller writing it to disk may choose either convention.
    static std::string Format (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount]);

private:
    static constexpr int    kGridColumns  = 16;
    static constexpr int    kMnemonicCell = 6;   // "BBR0" plus the hidden-slot marker
    static constexpr int    kModeCell     = 8;   // "(abs,X)" is the widest operand form
    static constexpr int    kNumberCell   = 3;   // a count plus the conditional-crossing marker

    static std::string  FormatPreamble    ();
    static std::string  FormatGrid        (const char * heading, const Microcode table[kOpcodeCount]);
    static std::string  FormatOpcodeTable (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount]);
    static std::string  FormatTimingDiffs (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount]);
    static std::string  FormatCmosOnly    (const Microcode nmos[kOpcodeCount], const Microcode cmos[kOpcodeCount]);
    static std::string  FormatFooter      ();

    static std::string  DescribeMnemonic  (const Microcode & entry);
    static std::string  DescribeMode      (const Microcode & entry);
    static std::string  DescribeLength    (const Microcode & entry);
    static std::string  DescribeCycles    (const Microcode & entry);
    static std::string  PadRight          (const std::string & text, int width);

    static const char * ModeSyntax        (GlobalAddressingMode::AddressingMode mode);
    static bool         IsAssemblable     (const Microcode & entry);
};
