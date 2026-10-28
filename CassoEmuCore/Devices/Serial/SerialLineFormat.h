#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialParity, SerialStopBits
//
////////////////////////////////////////////////////////////////////////////////

enum class SerialParity : uint8_t
{
    None,
    Odd,
    Even,
    Mark,
    Space,
};

enum class SerialStopBits : uint8_t
{
    One,
    OneAndHalf,
    Two,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLineFormat
//
//  The baud rate, word length, parity and stop bits a 6551's control and
//  command registers select. The rate is the transmitter's: a PC port has
//  one rate for both directions.
//
////////////////////////////////////////////////////////////////////////////////

struct SerialLineFormat
{
    double          baud     = 0.0;
    int             wordBits = 8;
    SerialParity    parity   = SerialParity::None;
    SerialStopBits  stopBits = SerialStopBits::One;

    bool operator== (const SerialLineFormat & other) const = default;

    static SerialLineFormat  FromRegisters           (Byte control, Byte command);
    static double            GetBaudRate             (int code);
    int                      GetHalfBitsPerCharacter () const;
};
