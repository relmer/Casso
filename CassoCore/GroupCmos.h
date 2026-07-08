#pragma once

#include "Pch.h"
#include "GlobalAddressingModes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  GroupCmos
//
//  The 65C02's "leftover" instructions -- the ones WDC dropped into the holes
//  of the NMOS aaabbbcc opcode map, so (unlike Group00/01/10) they follow no
//  single group formula and each opcode byte must be named explicitly. This is
//  the CMOS analogue of GroupMisc: an enum of the additions plus a companion
//  table pairing each with { opcode byte, mnemonic }, so the byte appears once,
//  next to the mnemonic, and the install logic can reference the mnemonic enum
//  rather than bare hex.
//
//  Multi-mode mnemonics carry an addressing-mode suffix (STZ_ZP, STZ_ABS, ...)
//  since one mnemonic maps to several opcodes here.
//
////////////////////////////////////////////////////////////////////////////////

class GroupCmos
{
public:
    enum Opcode
    {
        STZ_ZP,             // STZ    Zero Page
        STZ_ZPX,            // STZ    Zero Page, X
        STZ_ABS,            // STZ    Absolute
        STZ_ABSX,           // STZ    Absolute, X

        TSB_ZP,             // TSB    Zero Page
        TSB_ABS,            // TSB    Absolute
        TRB_ZP,             // TRB    Zero Page
        TRB_ABS,            // TRB    Absolute

        ORA_ZPI,            // ORA    (Zero Page)
        AND_ZPI,            // AND    (Zero Page)
        EOR_ZPI,            // EOR    (Zero Page)
        ADC_ZPI,            // ADC    (Zero Page)
        STA_ZPI,            // STA    (Zero Page)
        LDA_ZPI,            // LDA    (Zero Page)
        CMP_ZPI,            // CMP    (Zero Page)
        SBC_ZPI,            // SBC    (Zero Page)

        INC_A,              // INC    A (Accumulator)
        DEC_A,              // DEC    A (Accumulator)

        PHX,                // PHX
        PLX,                // PLX
        PHY,                // PHY
        PLY,                // PLY

        BRA,                // BRA    Relative

        BIT_ZPX,            // BIT    Zero Page, X
        BIT_ABSX,           // BIT    Absolute, X
        BIT_IMM,            // BIT    #Immediate

        JMP_IND,            // JMP    (Indirect)      -- page-boundary corrected
        JMP_ABSXIND,        // JMP    (Absolute, X)
    };


    struct InstructionMap
    {
        Byte          instruction;
        const char  * name;
    };

    static constexpr InstructionMap instruction[] =
    {
        { 0x64, "STZ" },
        { 0x74, "STZ" },
        { 0x9C, "STZ" },
        { 0x9E, "STZ" },

        { 0x04, "TSB" },
        { 0x0C, "TSB" },
        { 0x14, "TRB" },
        { 0x1C, "TRB" },

        { 0x12, "ORA" },
        { 0x32, "AND" },
        { 0x52, "EOR" },
        { 0x72, "ADC" },
        { 0x92, "STA" },
        { 0xB2, "LDA" },
        { 0xD2, "CMP" },
        { 0xF2, "SBC" },

        { 0x1A, "INC" },
        { 0x3A, "DEC" },

        { 0xDA, "PHX" },
        { 0xFA, "PLX" },
        { 0x5A, "PHY" },
        { 0x7A, "PLY" },

        { 0x80, "BRA" },

        { 0x34, "BIT" },
        { 0x3C, "BIT" },
        { 0x89, "BIT" },

        { 0x6C, "JMP" },
        { 0x7C, "JMP" },
    };
};
