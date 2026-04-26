#pragma once

#include "Pch.h"
#include "GlobalAddressingModes.h"





class Group00
{
public:
    enum Opcode
    {
        BIT          = 0b001,   // 001 BIT
        JMP          = 0b010,   // 010 JMP              0100 1100 4C
        JMP_indirect = 0b011,   // 011 JMP (indirect)   0110 1100 6C
        STY          = 0b100,   // 100 STY
        LDY          = 0b101,   // 101 LDY
        CPY          = 0b110,   // 110 CPY
        CPX          = 0b111,   // 111 CPX
    };

    static constexpr const char * instructionName[] =
    {
        "",
        "BIT",
        "JMP",
        "JMP",
        "STY",
        "LDY",
        "CPY",
        "CPX",
    };

    enum AddressingMode
    {
        __AM_First = 0,
        AM_Immediate    = 0b000,    // 010 #Immediate
        AM_ZeroPage     = 0b001,    // 001 Zero Page
        __AM_Unused_010 = 0b010,
        AM_Absolute     = 0b011,    // 011 Absolute        
        __AM_Unused_100 = 0b100,
        AM_ZeroPageX    = 0b101,    // 101 Zero Page, X
        __AM_Unused_110 = 0b110,
        AM_AbsoluteX    = 0b111,    // 111 Absolute, X
        __AM_Count
    };

    static constexpr uint32_t s_addressingModeMap[] =
    {
        GlobalAddressingMode::Immediate,   // AM_Immediate,
        GlobalAddressingMode::ZeroPage,    // AM_ZeroPage,
        0,                                 // 0,
        GlobalAddressingMode::Absolute,    // AM_Absolute,
        0,                                 // 0,
        GlobalAddressingMode::ZeroPageX,   // AM_ZeroPageX,
        0,                                 // 0,
        GlobalAddressingMode::AbsoluteX,   // AM_AbsoluteX
    };

    enum AddressingModeFlag
    {
        AMF_Immediate       = 0b00000001,
        AMF_ZeroPage        = 0b00000010,
        __AMF_Unused_010    = 0b00000100,
        AMF_Absolute        = 0b00001000,
        __AMF_Unused_100    = 0b00010000,
        AMF_ZeroPageX       = 0b00100000,
        __AMF_Unused_110    = 0b01000000,
        AMF_AbsoluteX       = 0b10000000,
        __AMF_AllModes      = 0b11111111 & ~(__AMF_Unused_010 | __AMF_Unused_100 | __AMF_Unused_110)
    };

};
