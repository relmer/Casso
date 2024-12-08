#pragma once

#include "Pch.h"



class GroupMisc
{
public:
    enum Opcode
    {
        // Branch $relative (two bytes)
        BPL = 0x10,
        BMI = 0x30,
        BVC = 0x50,
        BVS = 0x70,
        BCC = 0x90,
        BCS = 0xB0,
        BNE = 0xD0,
        BEQ = 0xF0,

        BRK = 0x00,
        JSR = 0x20, // Absolute
        RTI = 0x40,
        RTS = 0x60,

        PHP = 0x08,
        PLP = 0x28,
        PHA = 0x48,
        PLA = 0x68,
        DEY = 0x88,
        TAY = 0xA8,
        INY = 0xC8,
        INX = 0xE8,
        	
        CLC = 0x18,
        SEC = 0x38,
        CLI = 0x58,
        SEI = 0x78,
        TYA = 0x98,
        CLV = 0xB8,
        CLD = 0xD8,
        SED = 0xF8,   	
        
        TXA = 0x8A,
        TXS = 0x9A,
        TAX = 0xAA,
        TSX = 0xBA,
        DEX = 0xCA,
        NOP = 0xEA  	
    };
        	
        	
        	
    static constexpr const char * instructionName[] =
    {
        "BPL",
        "BMI",
        "BVC",
        "BVS",
        "BCC",
        "BCS",
        "BNE",
        "BEQ",
        "BRK",
        "JSR",
        "RTI",
        "RTS",
        "PHP",
        "PLP",
        "PHA",
        "PLA",
        "DEY",
        "TAY",
        "INY",
        "INX",
        "CLC",
        "SEC",
        "CLI",
        "SEI",
        "TYA",
        "CLV",
        "CLD",
        "SED",
        "TXA",
        "TXS",
        "TAX",
        "TSX",
        "DEX",
        "NOP"
    };
};
