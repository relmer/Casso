#pragma once

#include "Pch.h"
#include "GlobalAddressingModes.h"



class Group01
{
public:
    enum Opcode
    {
        ORA,    // 000 = OR
        AND,    // 001 = AND
        EOR,    // 010 = XOR
        ADC,    // 011 = ADD with Carry
        STA,    // 100 = Store
        LDA,    // 101 = Load
        CMP,    // 110 = Compare
        SBC     // 111 = Subtract with Carry
    };

    static constexpr const char * instructionName[] =
    {
        "ORA",
        "AND",
        "EOR",
        "ADC",
        "STA",
        "LDA",
        "CMP",
        "SBC"
    };

    enum AddressingMode
    {
        __AM_First           = 0,
        AM_ZeroPageXIndirect = 0b000,   // 000 (Zero Page, X) -> ($00LL + X)
        AM_ZeroPage          = 0b001,   // 001 Zero Page
        AM_Immediate         = 0b010,   // 010 #Immediate
        AM_Absolute          = 0b011,   // 011 Absolute
        AM_ZeroPageIndirectY = 0b100,   // 100 (Zero Page), Y
        AM_ZeroPageX         = 0b101,   // 101 Zero Page, X
        AM_AbsoluteY         = 0b110,   // 110 Absolute, Y
        AM_AbsoluteX         = 0b111,   // 111 Absolute, X
        __AM_Count
    };

    static constexpr uint32_t s_addressingModeMap[] =
    {
        GlobalAddressingMode::ZeroPageXIndirect,  // AM_ZeroPageXIndirect
        GlobalAddressingMode::ZeroPage,           // AM_ZeroPage
        GlobalAddressingMode::Immediate,          // AM_Immediate
        GlobalAddressingMode::Absolute,           // AM_Absolute
        GlobalAddressingMode::ZeroPageIndirectY,  // AM_ZeroPageIndirectY
        GlobalAddressingMode::ZeroPageX,          // AM_ZeroPageX
        GlobalAddressingMode::AbsoluteY,          // AM_AbsoluteY
        GlobalAddressingMode::AbsoluteX,          // AM_AbsoluteX
    };

    enum AddressingModeFlag
    {
        AMF_ZeroPageXIndirect = 0b00000001,
        AMF_ZeroPage          = 0b00000010,
        AMF_Immediate         = 0b00000100,
        AMF_Absolute          = 0b00001000,
        AMF_ZeroPageIndirectY = 0b00010000,
        AMF_ZeroPageX         = 0b00100000,
        AMF_AbsoluteY         = 0b01000000,
        AMF_AbsoluteX         = 0b10000000,
        __AMF_AllModes        = 0b11111111
    };

};
