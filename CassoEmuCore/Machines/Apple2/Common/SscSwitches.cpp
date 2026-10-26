#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MakeDefault
//
//  Communications mode, 9,600 baud, 8 data bits, 1 stop bit, no parity, no
//  line feed after carriage return, interrupts off, normal clear to send:
//  SW1 = OFF OFF OFF ON ON ON ON, SW2 = ON ON OFF ON OFF OFF OFF.
//
////////////////////////////////////////////////////////////////////////////////

SscSwitches SscSwitches::MakeDefault()
{
    constexpr bool  kSw1[kSwitchesPerBank] = { false, false, false, true, true, true, true };
    constexpr bool  kSw2[kSwitchesPerBank] = { true, true, false, true, false, false, false };
    SscSwitches     result;



    for (int i = 0; i < kSwitchesPerBank; ++i)
    {
        result.m_sw1[i] = kSw1[i];
        result.m_sw2[i] = kSw2[i];
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOn
//
////////////////////////////////////////////////////////////////////////////////

bool SscSwitches::IsOn (SscBank bank, int number) const
{
    bool  on = false;



    if (number >= 1 && number <= kSwitchesPerBank)
    {
        on = GetBank (bank)[number - 1];
    }

    return on;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetOn
//
////////////////////////////////////////////////////////////////////////////////

void SscSwitches::SetOn (SscBank bank, int number, bool on)
{
    if (number >= 1 && number <= kSwitchesPerBank)
    {
        GetBank (bank)[number - 1] = on;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDipSwitch1Byte
//
//  Bit 0 SW1-6, bit 1 SW1-5, bits 4-7 SW1-4 to SW1-1. Bits 2 and 3 are not
//  driven. OFF reads 1.
//
////////////////////////////////////////////////////////////////////////////////

Byte SscSwitches::GetDipSwitch1Byte (Byte floating) const
{
    constexpr int   kBit[kSwitchesPerBank] = { 7, 6, 5, 4, 1, 0, -1 };
    constexpr Byte  kDrivenMask            = 0xF3;
    Byte            result                 = static_cast<Byte> (floating & ~kDrivenMask);



    for (int i = 0; i < kSwitchesPerBank; ++i)
    {
        if (kBit[i] >= 0 && !m_sw1[i])
        {
            result |= static_cast<Byte> (1 << kBit[i]);
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDipSwitch2Byte
//
//  Bit 0 CTS (0 while true), bits 1-3 SW2-5 to SW2-3, bit 5 SW2-2, bit 7
//  SW2-1. Bits 4 and 6 are not driven. OFF reads 1.
//
////////////////////////////////////////////////////////////////////////////////

Byte SscSwitches::GetDipSwitch2Byte (bool ctsTrue, Byte floating) const
{
    constexpr int   kBit[kSwitchesPerBank] = { 7, 5, 3, 2, 1, -1, -1 };
    constexpr Byte  kDrivenMask            = 0xAF;
    constexpr Byte  kCtsFalse              = 0x01;
    Byte            result                 = static_cast<Byte> (floating & ~kDrivenMask);



    for (int i = 0; i < kSwitchesPerBank; ++i)
    {
        if (kBit[i] >= 0 && !m_sw2[i])
        {
            result |= static_cast<Byte> (1 << kBit[i]);
        }
    }

    if (!ctsTrue)
    {
        result |= kCtsFalse;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMode
//
////////////////////////////////////////////////////////////////////////////////

SscMode SscSwitches::GetMode() const
{
    constexpr int  kSw15 = 4;
    constexpr int  kSw16 = 5;
    bool           sw15  = m_sw1[kSw15];
    bool           sw16  = m_sw1[kSw16];
    SscMode        mode  = SscMode::SicP8A;



    if (sw15 && sw16)
    {
        mode = SscMode::Communications;
    }
    else if (!sw15 && sw16)
    {
        mode = SscMode::Printer;
    }
    else if (sw15 && !sw16)
    {
        mode = SscMode::SicP8;
    }

    return mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBaudCode
//
//  SW1-1 to SW1-4 as a 4-bit value, SW1-1 the high bit and OFF reading 1:
//  the 6551's own baud-rate code.
//
////////////////////////////////////////////////////////////////////////////////

int SscSwitches::GetBaudCode() const
{
    constexpr int  kBaudSwitches = 4;
    int            code          = 0;



    for (int i = 0; i < kBaudSwitches; ++i)
    {
        code = (code << 1) | (m_sw1[i] ? 0 : 1);
    }

    return code;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsIrqConnected
//
//  SW2-6 ON connects the 6551's IRQ output to the Apple's IRQ line.
//
////////////////////////////////////////////////////////////////////////////////

bool SscSwitches::IsIrqConnected() const
{
    constexpr int  kSw26 = 5;



    return m_sw2[kSw26];
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBankText
//
////////////////////////////////////////////////////////////////////////////////

string SscSwitches::GetBankText (SscBank bank) const
{
    const bool *  positions = GetBank (bank);
    string        text;



    for (int i = 0; i < kSwitchesPerBank; ++i)
    {
        if (i != 0)
        {
            text += ' ';
        }

        text += positions[i] ? "on" : "off";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryParseBankText
//
//  Seven words, each "on" or "off" in any case, separated by spaces.
//
////////////////////////////////////////////////////////////////////////////////

bool SscSwitches::TryParseBankText (SscBank bank, const string & text)
{
    bool    parsed[kSwitchesPerBank] = {};
    int     count                    = 0;
    size_t  pos                      = 0;
    bool    valid                    = true;
    string  word;



    while (valid && pos < text.size())
    {
        size_t  end = text.find (' ', pos);

        if (end == string::npos)
        {
            end = text.size();
        }

        word = text.substr (pos, end - pos);
        pos  = end + 1;

        if (word.empty())
        {
            continue;
        }

        transform (word.begin(), word.end(), word.begin(), [] (char c) { return static_cast<char> (tolower (c)); });

        valid = (word == "on" || word == "off") && count < kSwitchesPerBank;

        if (valid)
        {
            parsed[count] = (word == "on");
            ++count;
        }
    }

    valid = valid && count == kSwitchesPerBank;

    if (valid)
    {
        copy (begin (parsed), end (parsed), GetBank (bank));
    }

    return valid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBank
//
////////////////////////////////////////////////////////////////////////////////

bool * SscSwitches::GetBank (SscBank bank)
{
    return (bank == SscBank::Sw1) ? m_sw1 : m_sw2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBank (const)
//
////////////////////////////////////////////////////////////////////////////////

const bool * SscSwitches::GetBank (SscBank bank) const
{
    return (bank == SscBank::Sw1) ? m_sw1 : m_sw2;
}
