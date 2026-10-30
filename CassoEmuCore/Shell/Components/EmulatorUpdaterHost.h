#pragma once

#include "Pch.h"

#include "Update/IUpdaterHost.h"

class EmulatorShell;





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorUpdaterHost
//
//  ShellUpdater's access to the emulator's window, settings and chrome.
//
////////////////////////////////////////////////////////////////////////////////

class EmulatorUpdaterHost : public IUpdaterHost
{
public:
    explicit EmulatorUpdaterHost (EmulatorShell & shell);

    HWND                       GetResultWindow            () override;
    HWND                       GetOwnerWindow             () override;
    HINSTANCE                  GetInstance                () override;
    const IDxuiTheme         * GetDialogTheme             () override;
    GlobalUserPrefs          & GetPrefs                   () override;
    void                       SavePrefs                  () override;
    HRESULT                    ReadStoredPrefs            (GlobalUserPrefs & outPrefs) override;
    bool                       HasCaption                 () override;
    bool                       TryGetClientWidthPx        (int & outWidthPx) override;
    UINT                       GetDpi                     () override;
    void                       SetCaptionAccessory        (IDxuiControl * accessory) override;
    void                       SetCaptionAccessoryWidth   (int widthDip) override;
    int                        GetCaptionReservedWidthDip () override;
    void                       RequestRepaint             () override;
    void                       ShowIndicatorTip           (const RECT & anchorPx, const std::wstring & text, int64_t nowMs) override;
    void                       HideIndicatorTip           () override;
    void                       ShowNotice                 (const std::wstring & text) override;
    void                       ShowNotification           (const std::wstring & text) override;
    int                        ShowModalDialog            (const DialogDefinition & def) override;
    void                       RefreshUpdateStatus        () override;
    HRESULT                    StartSharedCheckTimer      (UINT periodMs) override;
    void                       StopSharedCheckTimer       () override;
    void                       PrepareForDeploy           () override;
    void                       RequestClose               () override;
    std::vector<std::wstring>  GetRelaunchArguments       () override;

private:
    EmulatorShell  & m_shell;
};
