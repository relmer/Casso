#pragma once




////////////////////////////////////////////////////////////////////////////////
//
//  IRomBankSwitch
//
//  Hook the //e soft-switch bank calls when $C028 is accessed, so the switch
//  bank can drive the Apple //c's two-bank firmware ROM without knowing any
//  //c-specific details. Null on the //e and earlier (no ROM banking), so
//  those machines see no behavior change.
//
//  $C028 is a toggle flip-flop: any access (read or write) flips the visible
//  16K firmware bank across $C100-$FFFF. Reset restores bank 0 (the main
//  monitor/Applesoft bank) so /RESET always lands in known firmware.
//
////////////////////////////////////////////////////////////////////////////////

class IRomBankSwitch
{
public:
    virtual ~IRomBankSwitch() = default;

    virtual void ToggleRomBank() = 0;
    virtual void ResetRomBank  () = 0;
};
