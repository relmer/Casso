#pragma once

#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/IInputHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost
//
//  The shell's side of IInputHost: each call is one thing the shell's chrome,
//  scene or renderer does for the input component. Only EmulatorShell.cpp
//  creates one.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell::InputHost : public IInputHost
{
public:
    explicit InputHost (EmulatorShell & shell) : m_shell (shell) {}

    void      HideTooltips           () override;
    void      SetPlayerPicker        (InputModeRules::Picker picker) override;
    void      ClaimCaptureBand       () override;
    void      ReleaseCaptureBand     () override;
    bool      IsGuestMouseSuppressed () const override;
    uint32_t  TakeKeyRepeatElapsedUs (uint32_t capUs) override;

private:
    EmulatorShell  & m_shell;
};