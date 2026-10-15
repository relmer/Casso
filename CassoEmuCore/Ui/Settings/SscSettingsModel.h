#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/SscSwitches.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SscSetting
//
//  The settings the Super Serial Card's switches select, as the Serial card
//  page in Settings offers them.
//
////////////////////////////////////////////////////////////////////////////////

enum class SscSetting
{
    Mode,
    BaudRate,
    DataFormat,         // communications mode: data bits and stop bits
    StopBits,           // printer modes
    Parity,             // communications mode
    CrDelay,            // printer modes
    LineWidth,          // printer modes: line width and the Apple's screen
    LineFeed,
    Interrupts,
    ClearToSend,
};





////////////////////////////////////////////////////////////////////////////////
//
//  SscSettingsModel
//
//  The Serial card page's model, from the switch tables on Apple's SSC
//  Reference Card and in its Installation and Operating Manual: which
//  settings each mode offers, the choices for each, and the switch positions
//  each choice sets.
//
//  A setting writes only its own switches, so changing one never disturbs
//  another. A combination of switches no choice produces (the undefined
//  all-ON baud setting, for one) has no choice index, and the page lists it
//  as Custom; the readout lists every position.
//
////////////////////////////////////////////////////////////////////////////////

class SscSettingsModel
{
public:
    static constexpr int  kMaxSwitches = 4;
    static constexpr int  kMaxChoices  = 15;
    static constexpr int  kCustom      = -1;

    // A switch a setting writes. In a choice, `on` is 1 for ON, 0 for OFF, and
    // -1 for a switch the choice does not change.
    struct SwitchRef
    {
        SscBank  bank;
        int      number;
    };

    struct ChoiceDef
    {
        const wchar_t *  label;
        signed char      on[kMaxSwitches];
    };

    struct SettingDef
    {
        SscSetting       setting;
        const wchar_t *  label;
        int              switchCount;
        SwitchRef        switches[kMaxSwitches];
        int              choiceCount;
        ChoiceDef        choices[kMaxChoices];
    };

    static vector<SscSetting>  GetSettings    (SscMode mode);
    static wstring             GetLabel       (SscSetting setting);
    static vector<wstring>     GetChoices     (SscSetting setting);

    // The choice the switches select, or kCustom.
    static int                 GetChoiceIndex (SscSetting setting, const SscSwitches & switches);

    // Sets the switches `choice` selects. Out-of-range choices change nothing.
    static void                ApplyChoice    (SscSetting setting, int choice, SscSwitches & switches);

    // "SW1: OFF OFF OFF ON ON ON ON", one line per bank.
    static wstring             GetReadout     (const SscSwitches & switches, SscBank bank);

private:
    static const SettingDef *  FindSetting    (SscSetting setting);
};
