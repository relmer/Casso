#include "Pch.h"

#include "Ui/Settings/SscSettingsModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  s_kSettings
//
//  The switch tables from Apple's SSC Reference Card and Installation and
//  Operating Manual. Positions are listed in the order of `switches`: 1 ON,
//  0 OFF, -1 left as it is. No parity is SW2-4 ON whatever SW2-3 is.
//
////////////////////////////////////////////////////////////////////////////////

using SettingDef = SscSettingsModel::SettingDef;

static constexpr SettingDef  s_kSettings[] =
{
    { SscSetting::Mode, L"Mode",
      2, { { SscBank::Sw1, 5 }, { SscBank::Sw1, 6 } },
      4, { { L"Communications (bidirectional)",                      { 1, 1 } },
           { L"Printer (output only)",                                { 0, 1 } },
           { L"Emulate Apple II Serial Interface Card P8 firmware",  { 1, 0 } },
           { L"Emulate Apple II Serial Interface Card P8A firmware", { 0, 0 } } } },

    { SscSetting::BaudRate, L"Baud rate",
      4, { { SscBank::Sw1, 1 }, { SscBank::Sw1, 2 }, { SscBank::Sw1, 3 }, { SscBank::Sw1, 4 } },
      16, { { L"50",     { 1, 1, 1, 0 } },
            { L"75",     { 1, 1, 0, 1 } },
            { L"110",    { 1, 1, 0, 0 } },
            { L"135",    { 1, 0, 1, 1 } },
            { L"150",    { 1, 0, 1, 0 } },
            { L"300",    { 1, 0, 0, 1 } },
            { L"600",    { 1, 0, 0, 0 } },
            { L"1,200",  { 0, 1, 1, 1 } },
            { L"1,800",  { 0, 1, 1, 0 } },
            { L"2,400",  { 0, 1, 0, 1 } },
            { L"3,600",  { 0, 1, 0, 0 } },
            { L"4,800",  { 0, 0, 1, 1 } },
            { L"7,200",  { 0, 0, 1, 0 } },
            { L"9,600",  { 0, 0, 0, 1 } },
            { L"19,200", { 0, 0, 0, 0 } },
            { L"115,200 (not in Apple's manual)", { 1, 1, 1, 1 } } } },

    { SscSetting::DataFormat, L"Data format",
      2, { { SscBank::Sw2, 1 }, { SscBank::Sw2, 2 } },
      4, { { L"8 data bits, 1 stop bit",  { 1, 1 } },
           { L"7 data bits, 1 stop bit",  { 1, 0 } },
           { L"8 data bits, 2 stop bits", { 0, 1 } },
           { L"7 data bits, 2 stop bits", { 0, 0 } } } },

    { SscSetting::StopBits, L"Stop bits",
      1, { { SscBank::Sw2, 1 } },
      2, { { L"1", { 1 } },
           { L"2", { 0 } } } },

    { SscSetting::Parity, L"Parity",
      2, { { SscBank::Sw2, 3 }, { SscBank::Sw2, 4 } },
      3, { { L"None", { -1, 1 } },
           { L"Odd",  { 1, 0 } },
           { L"Even", { 0, 0 } } } },

    { SscSetting::CrDelay, L"Delay after carriage return",
      1, { { SscBank::Sw2, 2 } },
      2, { { L"None",         { 0 } },
           { L"1/4 second",   { 1 } } } },

    { SscSetting::LineWidth, L"Line width",
      2, { { SscBank::Sw2, 3 }, { SscBank::Sw2, 4 } },
      4, { { L"40 characters, Apple screen on",   { 1, 1 } },
           { L"72 characters, Apple screen off",  { 1, 0 } },
           { L"80 characters, Apple screen off",  { 0, 1 } },
           { L"132 characters, Apple screen off", { 0, 0 } } } },

    { SscSetting::LineFeed, L"Line feed after carriage return",
      1, { { SscBank::Sw2, 5 } },
      2, { { L"No",  { 0 } },
           { L"Yes", { 1 } } } },

    { SscSetting::Interrupts, L"Interrupts",
      1, { { SscBank::Sw2, 6 } },
      2, { { L"Off", { 0 } },
           { L"On",  { 1 } } } },

    { SscSetting::ClearToSend, L"Clear to send",
      2, { { SscBank::Sw1, 7 }, { SscBank::Sw2, 7 } },
      2, { { L"Normal (pin 4 or 20)", { 1, 0 } },
           { L"Secondary (pin 19)",   { 0, 1 } } } },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GetSettings
//
//  Communications mode reads SW2-1 to SW2-4 as data format and parity; the
//  printer modes, and the SIC emulations that share their switch table, read
//  them as stop bits, carriage-return delay and line width.
//
////////////////////////////////////////////////////////////////////////////////

vector<SscSetting> SscSettingsModel::GetSettings (SscMode mode)
{
    vector<SscSetting>  settings;



    if (mode == SscMode::Communications)
    {
        settings = { SscSetting::Mode,       SscSetting::BaudRate, SscSetting::DataFormat,
                     SscSetting::Parity,     SscSetting::LineFeed, SscSetting::Interrupts,
                     SscSetting::ClearToSend };
    }
    else
    {
        settings = { SscSetting::Mode,       SscSetting::BaudRate,  SscSetting::StopBits,
                     SscSetting::CrDelay,    SscSetting::LineWidth, SscSetting::LineFeed,
                     SscSetting::Interrupts, SscSetting::ClearToSend };
    }

    return settings;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLabel
//
////////////////////////////////////////////////////////////////////////////////

wstring SscSettingsModel::GetLabel (SscSetting setting)
{
    const SettingDef *  def = FindSetting (setting);



    return (def != nullptr) ? wstring (def->label) : wstring();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetChoices
//
////////////////////////////////////////////////////////////////////////////////

vector<wstring> SscSettingsModel::GetChoices (SscSetting setting)
{
    const SettingDef *  def = FindSetting (setting);
    vector<wstring>     labels;



    for (int i = 0; def != nullptr && i < def->choiceCount; ++i)
    {
        labels.emplace_back (def->choices[i].label);
    }

    return labels;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetChoiceIndex
//
////////////////////////////////////////////////////////////////////////////////

int SscSettingsModel::GetChoiceIndex (SscSetting setting, const SscSwitches & switches)
{
    const SettingDef *  def   = FindSetting (setting);
    int                 index = kCustom;



    for (int c = 0; def != nullptr && index == kCustom && c < def->choiceCount; ++c)
    {
        bool  matches = true;

        for (int s = 0; matches && s < def->switchCount; ++s)
        {
            signed char  want = def->choices[c].on[s];

            matches = want < 0 || switches.IsOn (def->switches[s].bank, def->switches[s].number) == (want == 1);
        }

        if (matches)
        {
            index = c;
        }
    }

    return index;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyChoice
//
////////////////////////////////////////////////////////////////////////////////

void SscSettingsModel::ApplyChoice (SscSetting setting, int choice, SscSwitches & switches)
{
    const SettingDef *  def   = FindSetting (setting);
    bool                valid = def != nullptr && choice >= 0 && choice < def->choiceCount;



    for (int s = 0; valid && s < def->switchCount; ++s)
    {
        signed char  want = def->choices[choice].on[s];

        if (want >= 0)
        {
            switches.SetOn (def->switches[s].bank, def->switches[s].number, want == 1);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReadout
//
////////////////////////////////////////////////////////////////////////////////

wstring SscSettingsModel::GetReadout (const SscSwitches & switches, SscBank bank)
{
    wstring  text = (bank == SscBank::Sw1) ? L"SW1:" : L"SW2:";



    for (int n = 1; n <= SscSwitches::kSwitchesPerBank; ++n)
    {
        text += switches.IsOn (bank, n) ? L" ON" : L" OFF";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetNote
//
//  From Apple's SSC manuals: what each mode is for.
//
////////////////////////////////////////////////////////////////////////////////

wstring SscSettingsModel::GetNote (SscSetting setting, const SscSwitches & switches)
{
    wstring  note;



    if (setting == SscSetting::Mode)
    {
        switch (switches.GetMode())
        {
        case SscMode::Communications:
            note = L"A two-way line to a modem or terminal. PR#2 sends, IN#2 receives, "
                   L"and Ctrl-A starts a firmware command.";
            break;

        case SscMode::Printer:
            note = L"Output to a serial printer, broken into lines of the width below. "
                   L"Ctrl-I starts a firmware command.";
            break;

        case SscMode::SicP8:
            note = L"For programs written for Apple's earlier Serial Interface Card that PEEK "
                   L"and POKE its memory locations. Apple recommends Printer otherwise.";
            break;

        case SscMode::SicP8A:
            note = L"As P8, with the P8A firmware's ETX/ACK handshaking. Apple recommends "
                   L"Printer otherwise.";
            break;
        }
    }

    return note;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetInfoTip
//
//  From Apple's SSC manuals.
//
////////////////////////////////////////////////////////////////////////////////

wstring SscSettingsModel::GetInfoTip (SscSetting setting)
{
    wstring  tip;



    if (setting == SscSetting::LineFeed)
    {
        tip = L"Yes sends a line feed after each carriage return, in every mode. Use it for a "
              L"printer or terminal that does not move to a new line on a carriage return "
              L"alone. Apple recommends No in communications mode unless the other end is "
              L"such a printer.";
    }
    else if (setting == SscSetting::Interrupts)
    {
        tip = L"On lets the card interrupt the processor when a byte arrives, so a program can "
              L"receive in the background while it does other work. Turn it on only for a "
              L"program written to use interrupts, such as Kermit; other programs can crash "
              L"with it on.";
    }

    return tip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindSetting
//
////////////////////////////////////////////////////////////////////////////////

const SscSettingsModel::SettingDef * SscSettingsModel::FindSetting (SscSetting setting)
{
    const SettingDef *  found = nullptr;



    for (const SettingDef & def : s_kSettings)
    {
        if (found == nullptr && def.setting == setting)
        {
            found = &def;
        }
    }

    return found;
}
