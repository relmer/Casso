#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialBit
//
//  One bit on a serial data line: what it is, its level, and where it sits,
//  in bit times from the start of its byte. Mark is logic 1, the level the
//  line rests at.
//
////////////////////////////////////////////////////////////////////////////////

struct SerialBit
{
    enum class Kind : Byte
    {
        Start,
        Data,
        Parity,
        Stop,
    };

    Kind    kind   = Kind::Start;
    int     index  = 0;         // a data bit's number, 0 for the first sent
    bool    isMark = false;
    double  start  = 0.0;
    double  length = 1.0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveform
//
//  The bits a 6551 sends or receives for one byte, from its control and
//  command registers: a start bit, the word low bit first, a parity bit when
//  parity is on, then the stop bits. With SBN set there are two stop bits,
//  or one and a half with a five-bit word and no parity, as the emulated
//  6551 times a character.
//
////////////////////////////////////////////////////////////////////////////////

class SerialWaveform
{
public:
    static void    LayOut           (Byte value, Byte control, Byte command, std::vector<SerialBit> & outBits);
    static double  GetCharacterBits (Byte control, Byte command);

private:
    static constexpr Byte  kControlWordLenMask  = 0x60;
    static constexpr Byte  kControlWordLenShift = 5;
    static constexpr Byte  kControlStopBits     = 0x80;
    static constexpr Byte  kCommandParityEnable = 0x20;
    static constexpr Byte  kCommandParityShift  = 6;
    static constexpr int   kMaxWordBits         = 8;

    enum class Parity : Byte
    {
        Odd,
        Even,
        Mark,
        Space,
    };

    static int     GetWordBits (Byte control);
    static double  GetStopBits (Byte control, Byte command);
    static bool    IsParityOn  (Byte command) { return (command & kCommandParityEnable) != 0; }
};
