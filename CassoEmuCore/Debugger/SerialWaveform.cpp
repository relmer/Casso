#include "Pch.h"

#include "Debugger/SerialWaveform.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveform::LayOut
//
//  Odd parity makes the count of ones in the word and the parity bit odd,
//  even parity even; mark and space parity send a fixed 1 or 0.
//
////////////////////////////////////////////////////////////////////////////////

void SerialWaveform::LayOut (
    Byte                      value,
    Byte                      control,
    Byte                      command,
    std::vector<SerialBit>  & outBits)
{
    int     wordBits = GetWordBits (control);
    int     ones     = 0;
    int     i        = 0;
    double  at       = 0.0;
    Parity  parity   = (Parity) (command >> kCommandParityShift);
    bool    isMark   = false;



    outBits.clear();
    outBits.push_back ({ SerialBit::Kind::Start, 0, false, at, 1.0 });
    at += 1.0;

    for (i = 0; i < wordBits; i++)
    {
        isMark = ((value >> i) & 1) != 0;
        ones  += isMark ? 1 : 0;

        outBits.push_back ({ SerialBit::Kind::Data, i, isMark, at, 1.0 });
        at += 1.0;
    }

    if (IsParityOn (command))
    {
        switch (parity)
        {
        case Parity::Odd:   isMark = (ones % 2) == 0; break;
        case Parity::Even:  isMark = (ones % 2) != 0; break;
        case Parity::Mark:  isMark = true;            break;
        case Parity::Space: isMark = false;           break;
        }

        outBits.push_back ({ SerialBit::Kind::Parity, 0, isMark, at, 1.0 });
        at += 1.0;
    }

    outBits.push_back ({ SerialBit::Kind::Stop, 0, true, at, GetStopBits (control, command) });
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveform::GetCharacterBits
//
//  One character's length in bit times: the start bit, the word, any parity
//  bit and the stop bits.
//
////////////////////////////////////////////////////////////////////////////////

double SerialWaveform::GetCharacterBits (
    Byte  control,
    Byte  command)
{
    return 1.0 + GetWordBits (control) + (IsParityOn (command) ? 1.0 : 0.0) + GetStopBits (control, command);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveform::GetWordBits
//
//  WL1 and WL0: 00 is eight bits, 01 seven, 10 six and 11 five.
//
////////////////////////////////////////////////////////////////////////////////

int SerialWaveform::GetWordBits (Byte control)
{
    return kMaxWordBits - ((control & kControlWordLenMask) >> kControlWordLenShift);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialWaveform::GetStopBits
//
////////////////////////////////////////////////////////////////////////////////

double SerialWaveform::GetStopBits (
    Byte  control,
    Byte  command)
{
    constexpr int  kMinWordBits = 5;
    int            wordBits     = GetWordBits (control);
    bool           isParityOn   = IsParityOn (command);



    if ((control & kControlStopBits) == 0 || (wordBits == kMaxWordBits && isParityOn))
    {
        return 1.0;
    }

    return (wordBits == kMinWordBits && !isParityOn) ? 1.5 : 2.0;
}
