#pragma once

#include "Pch.h"
#include "GlobalAddressingModes.h"



class GroupMisc
{
public:
    enum Opcode
    {
        BPL,
        BMI,
        BVC,
        BVS,
        BCC,
        BCS,
        BNE,
        BEQ,

        BRK,
        JSR,
        RTI,
        RTS,

        PHP,
        PLP,
        PHA,
        DEY,
        PLA,
        TAY,
        INY,
        INX,
        	
        CLC,
        SEC,
        CLI,
        SEI,
        TYA,
        CLV,
        CLD,
        SED,   	
        
        TXA,
        TXS,
        TAX,
        TSX,
        DEX,
        NOP  	
    };
        	
        	
     
    struct InstructionMap
    {
        Byte          instruction;
        const char  * name;
    };

    static constexpr InstructionMap instruction[] =
    {
        { 0x10, "BPL" },
        { 0x30, "BMI" },
        { 0x50, "BVC" },
        { 0x70, "BVS" },
        { 0x90, "BCC" },
        { 0xB0, "BCS" },
        { 0xD0, "BNE" },
        { 0xF0, "BEQ" },

        { 0x00, "BRK" },
        { 0x20, "JSR" },
        { 0x40, "RTI" },
        { 0x60, "RTS" },

        { 0x08, "PHP" },
        { 0x28, "PLP" },
        { 0x48, "PHA" },
        { 0x68, "PLA" },
        { 0x88, "DEY" },
        { 0xA8, "TAY" },
        { 0xC8, "INY" },
        { 0xE8, "INX" },

        { 0x18, "CLC" },
        { 0x38, "SEC" },
        { 0x58, "CLI" },
        { 0x78, "SEI" },
        { 0x98, "TYA" },
        { 0xB8, "CLV" },
        { 0xD8, "CLD" },
        { 0xF8, "SED" },

        { 0x8A, "TXA" },
        { 0x9A, "TXS" },
        { 0xAA, "TAX" },
        { 0xBA, "TSX" },
        { 0xCA, "DEX" },
        { 0xEA, "NOP" },
    };
};
