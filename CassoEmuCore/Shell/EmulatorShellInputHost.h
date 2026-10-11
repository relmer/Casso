#pragma once

#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/IInputHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InputHost
//
//  The shell's side of IInputHost: each call is one thing the shell's window,
//  chrome, scene, renderer or settings does for the input component. Only
//  EmulatorShell.cpp creates one.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorShell::InputHost : public IInputHost
{
public:
    explicit InputHost (EmulatorShell & shell) : m_shell (shell) {}

    HWND                     GetMainWindow             () const override;
    std::optional<uint32_t>  MapPointerToPicture       (int xPx, int yPx) override;
    void                     HideTooltips              () override;
    void                     SetPlayerPicker           (InputModeRules::Picker picker) override;
    void                     ClaimCaptureBand          () override;
    void                     ReleaseCaptureBand        () override;
    bool                     IsGuestMouseSuppressed    () const override;
    void                     ShowNotice                (const std::wstring & text) override;
    void                     PostNotice                (const std::wstring & text) override;
    void                     SwallowNextChar           () override;
    uint32_t                 TakeKeyRepeatElapsedUs    (uint32_t capUs) override;
    ClipboardManager       * GetClipboard              () override;
    void                     StartNewControllerProfile (ControllerUnitKey unit) override;
    GlobalUserPrefs        & GetPrefs                  () override;
    void                     SavePrefs                 () override;
    UserConfigStore        * GetConfigStore            () override;
    IFileSystem            & GetFileSystem             () override;
    void                     LoadMachineUiPrefs        (JsonValue & outDoc, const JsonValue * & outUiPrefs) override;

private:
    EmulatorShell  & m_shell;
};
