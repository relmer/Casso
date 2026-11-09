#pragma once

#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Config/UserConfigStore.h"
#include "Update/IUpdaterHost.h"

class CassoExplorerWindow;
class IFileSystem;
class ShellUpdater;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerUpdaterHost
//
//  ShellUpdater's access to Casso Explorer. Windows come and go, so results and
//  timers arrive at a message-only window that lives as long as the app, and
//  the indicator sits in the caption of one open window. The update fields are
//  the only part of Casso's prefs file written here.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerUpdaterHost : public IUpdaterHost
{
public:
    ExplorerUpdaterHost (IFileSystem & fs, const std::wstring & baseDir, const std::wstring & titlePrefix);
    ~ExplorerUpdaterHost() override;

    HRESULT                 Create             (HINSTANCE instance);
    void                    Destroy            ();
    void                    PostAutomaticCheck ();
    ShellUpdater          & GetUpdater         ()       { return *m_updater; }
    CassoExplorerWindow   * GetWindow          () const { return m_window; }
    void                    SetWindow          (CassoExplorerWindow * window);

    HWND                       GetResultWindow            () override { return m_messageWindow; }
    HWND                       GetOwnerWindow             () override;
    HINSTANCE                  GetInstance                () override { return m_instance; }
    const IDxuiTheme         * GetDialogTheme             () override;
    GlobalUserPrefs          & GetPrefs                   () override { return m_prefs; }
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
    void                       RefreshUpdateStatus        () override {}
    HRESULT                    StartSharedCheckTimer      (UINT periodMs) override;
    void                       StopSharedCheckTimer       () override;
    void                       PrepareForDeploy           () override;
    void                       RequestClose               () override;
    std::vector<std::wstring>  GetRelaunchArguments       () override;

private:
    static constexpr const wchar_t *  kpszMessageClass = L"CassoExplorerUpdaterMessages";
    static constexpr UINT             kFrameMs         = 16;     // a sweep's frames
    static constexpr int64_t          kIdleTickMs      = 1000;   // refits the indicator to a resized window

    static LRESULT CALLBACK  WindowProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

    void  HandleIndicatorTick ();
    void  ArmIndicatorTick    (UINT delayMs);

    IFileSystem                    & m_fs;
    UserConfigStore                  m_store;
    GlobalUserPrefs                  m_prefs;
    std::wstring                     m_titlePrefix;
    HINSTANCE                        m_instance       = nullptr;
    HWND                             m_messageWindow  = nullptr;
    CassoExplorerWindow            * m_window         = nullptr;
    bool                             m_isTickArmed    = false;
    std::unique_ptr<ShellUpdater>    m_updater;
};
