#pragma once

#include "Pch.h"
#include "GlobalAddressingModes.h"





class Group10
{
public:
    enum Opcode
    {
        ASL, // 000 = Arithmetic Shift Left
        ROL, // 001 = Rotate Left
        LSR, // 010 = Logical Shift Right
        ROR, // 011 = Rotate Right
        STX, // 100 = Store X
        LDX, // 101 = Load X
        DEC, // 110 = Dec
        INC, // 111 = Inc
    };

    static constexpr const char * instructionName[] =
    {
        "ASL",
        "ROL",
        "LSR",
        "ROR",
        "STX",
        "LDX",
        "DEC",
        "INC",
    };

    enum AddressingMode
    {
        __AM_First      = 0,
        AM_Immediate    = 0b000,   // 000 #Immediate
        AM_ZeroPage     = 0b001,   // 001 Zero Page
        AM_Accumulator  = 0b010,   // 010 Accumulator
        AM_Absolute     = 0b011,   // 011 Absolute
        __AM_Unused_100 = 0b100,
        AM_ZeroPageX    = 0b101,   // 101 Zero Page, X
        __AM_Unused_110 = 0b110,
        AM_AbsoluteX    = 0b111,   // 111 Absolute, X
        __AM_Count
    };

    static constexpr uint32_t s_addressingModeMap[] =
    {
        GlobalAddressingMode::Immediate,          // AM_Immediate
        GlobalAddressingMode::ZeroPage,           // AM_ZeroPage
        GlobalAddressingMode::Accumulator,        // AM_Accumulator
        GlobalAddressingMode::Absolute,           // AM_Absolute
        0,                                        // Unused
        GlobalAddressingMode::ZeroPageX,          // AM_ZeroPageX
        0,                                        // Unused
        GlobalAddressingMode::AbsoluteX,          // AM_AbsoluteX
    };

    enum AddressingModeFlag
    {
        AMF_Immediate       = 0b00000001,
        AMF_ZeroPage        = 0b00000010,
        AMF_Accumulator     = 0b00000100,
        AMF_Absolute        = 0b00001000,
        __AMF_Unused_100    = 0b00010000,
        AMF_ZeroPageX       = 0b00100000,
        __AMF_Unused_110    = 0b01000000,
        AMF_AbsoluteX       = 0b10000000,
        __AMF_AllModes      = 0b11111111 & ~(__AMF_Unused_100 | __AMF_Unused_110)
    };

};
