#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SscMode
//
//  The firmware mode SW1-5 and SW1-6 select.
//
////////////////////////////////////////////////////////////////////////////////

enum class SscMode
{
    Communications,     // SW1-5 ON,  SW1-6 ON
    Printer,            // SW1-5 OFF, SW1-6 ON
    SicP8,              // SW1-5 ON,  SW1-6 OFF
    SicP8A,             // SW1-5 OFF, SW1-6 OFF
};





////////////////////////////////////////////////////////////////////////////////
//
//  SscBank
//
////////////////////////////////////////////////////////////////////////////////

enum class SscBank
{
    Sw1,
    Sw2,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SscSwitches
//
//  The Super Serial Card's two banks of seven DIP switches, and the two
//  register bytes the firmware reads them through.
//
//  A switch is ON when closed. A closed switch pulls its buffer input low,
//  so ON reads 0 and OFF reads 1. Eleven switches are readable; SW1-7,
//  SW2-6 and SW2-7 are wired to the RS-232 and IRQ lines instead and have no
//  register bit. The register layout is Apple's (SSC Installation and
//  Operating Manual, table A-7).
//
//  Switches are numbered 1-7 as on the card.
//
////////////////////////////////////////////////////////////////////////////////

class SscSwitches
{
public:
    static constexpr int  kSwitchesPerBank = 7;

    static SscSwitches  MakeDefault ();

    bool    IsOn  (SscBank bank, int number) const;
    void    SetOn (SscBank bank, int number, bool on);

    // The register bytes at $C081+s0 and $C082+s0. Bits the card's buffers
    // do not drive come from `floating`.
    Byte    GetDipSwitch1Byte (Byte floating) const;
    Byte    GetDipSwitch2Byte (bool ctsTrue, Byte floating) const;

    SscMode GetMode        () const;
    int     GetBaudCode    () const;
    bool    IsIrqConnected () const;

    // "off off off on on on on": SW-1 first. For anything but seven on/off
    // words, TryParseBankText returns false and does not alter the bank.
    string  GetBankText      (SscBank bank) const;
    bool    TryParseBankText (SscBank bank, const string & text);

    bool    operator== (const SscSwitches & other) const = default;

private:
    bool *        GetBank (SscBank bank);
    const bool *  GetBank (SscBank bank) const;

    bool    m_sw1[kSwitchesPerBank] = {};
    bool    m_sw2[kSwitchesPerBank] = {};
};
