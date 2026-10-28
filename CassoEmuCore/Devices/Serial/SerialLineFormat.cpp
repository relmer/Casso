#include "Pch.h"

#include "Devices/Serial/SerialLineFormat.h"
#include "Devices/Acia6551.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kBaudRates
//
//  Bits per second for control-register baud codes 1-15, from the 6551
//  datasheet's baud-rate generator table. Code 0 is not in the table: it
//  selects the crystal divided by 16 (GetBaudRate).
//
////////////////////////////////////////////////////////////////////////////////

static constexpr double  s_kBaudRates[] =
{
    0.0,     50.0,    75.0,    109.92,  134.58,  150.0,   300.0,   600.0,
    1200.0,  1800.0,  2400.0,  3600.0,  4800.0,  7200.0,  9600.0,  19200.0,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLineFormat::FromRegisters
//
//  Command bit 5 enables parity and bits 7-6 select odd, even, mark or space.
//  Control bit 7 selects two stop bits, which are one and a half with a
//  5-bit word and no parity, and one with an 8-bit word and parity.
//
////////////////////////////////////////////////////////////////////////////////

SerialLineFormat SerialLineFormat::FromRegisters (Byte control, Byte command)
{
    constexpr SerialParity  kParities[]  = { SerialParity::Odd, SerialParity::Even, SerialParity::Mark, SerialParity::Space };
    constexpr int           kParityShift = 6;
    constexpr int           kParityMask  = 0x03;
    constexpr int           kMinWordBits = 5;
    constexpr int           kMaxWordBits = 8;
    SerialLineFormat        format;
    int                     selector     = (control & Acia6551::kControlWordLenMask) >> Acia6551::kControlWordLenShift;
    bool                    hasParity    = (command & Acia6551::kCommandParityEnable) != 0;



    format.baud     = GetBaudRate (control & Acia6551::kControlBaudMask);
    format.wordBits = kMaxWordBits - selector;

    if (hasParity)
    {
        format.parity = kParities[(command >> kParityShift) & kParityMask];
    }

    if ((control & Acia6551::kControlStopBits) == 0)
    {
        format.stopBits = SerialStopBits::One;
    }
    else if (format.wordBits == kMinWordBits && !hasParity)
    {
        format.stopBits = SerialStopBits::OneAndHalf;
    }
    else if (format.wordBits == kMaxWordBits && hasParity)
    {
        format.stopBits = SerialStopBits::One;
    }
    else
    {
        format.stopBits = SerialStopBits::Two;
    }

    return format;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLineFormat::GetBaudRate
//
//  Code 0 runs at the crystal divided by 16: 115,200 baud on the 1.8432 MHz
//  crystal the //c and the Super Serial Card use.
//
////////////////////////////////////////////////////////////////////////////////

double SerialLineFormat::GetBaudRate (int code)
{
    constexpr double  kExternalDivisor = 16.0;
    double            rate             = static_cast<double> (Acia6551::kCrystalHz) / kExternalDivisor;



    if (code > 0 && code < static_cast<int> (size (s_kBaudRates)))
    {
        rate = s_kBaudRates[code];
    }

    return rate;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialLineFormat::GetHalfBitsPerCharacter
//
//  A start bit, the word, a parity bit when there is one, and the stop bits,
//  in half bits so the half stop bit is exact.
//
////////////////////////////////////////////////////////////////////////////////

int SerialLineFormat::GetHalfBitsPerCharacter() const
{
    constexpr int  kHalfBitsPerBit = 2;
    constexpr int  kOneStop        = 2;
    constexpr int  kOneAndHalfStop = 3;
    constexpr int  kTwoStop        = 4;
    int            halfBits        = kHalfBitsPerBit * (1 + wordBits + (parity != SerialParity::None ? 1 : 0));



    switch (stopBits)
    {
    case SerialStopBits::One:        halfBits += kOneStop;        break;
    case SerialStopBits::OneAndHalf: halfBits += kOneAndHalfStop; break;
    case SerialStopBits::Two:        halfBits += kTwoStop;        break;
    }

    return halfBits;
}
