#pragma once

#include "Pch.h"

#include "Window/DxuiPropertySheet.h"

#include "SettingsPanelState.h"
#include "SettingsMachineCatalog.h"
#include "SettingsDisplayCrtBridge.h"
#include "SettingsApplyController.h"
#include "SettingsPreviewController.h"
#include "SettingsCompositor.h"
#include "ColorPickerOverlay.h"
#include "HardwarePage.h"
#include "DiskPage.h"
#include "ThemePage.h"
#include "DisplayPage.h"


class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;
class DxuiLabel;




////////////////////////////////////////////////////////////////////////////////
//
//  SettingsSheet
//
//  T162: the DxuiPropertySheet-based Settings dialog that replaced the retired
//  bespoke SettingsPanel + SettingsWindow pair. Hosts the four DxuiPropertyPage
//  setting pages (Machine / Disk / Theme / Display) behind the framework's tab
//  strip + (Apply-hidden) OK / Cancel row, shown modeless from IDM_VIEW_SETTINGS
//  (Ctrl+,) so the emulator keeps running and interactive behind it.
//
//  The window is composited: SettingsCompositor is installed as its compose
//  hook (SetComposeHook) to blur + dim the panel and reveal the running
//  emulator through a see-through region while a Display control is being
//  adjusted (#8); when idle the same hook draws the panel sharp + opaque.
//  The cross-cutting commit / revert lives in OnOk / OnCancel (delegating to
//  SettingsApplyController, whose ordering is load-bearing).
//
////////////////////////////////////////////////////////////////////////////////

class SettingsSheet : public DxuiPropertySheet
{
public:
    SettingsSheet  () = default;
    ~SettingsSheet () override;   // detaches the compose hook before teardown

    //
    //  Wire dependencies, create the window (which builds + populates the
    //  pages), and show it MODELESS (FR-041): the emulator keeps running and
    //  interactive behind the sheet. Returns as soon as the window is shown,
    //  NOT when it closes -- the caller owns the SettingsSheet (heap) and
    //  destroys it from the close callback (SetOnDialogEnd). The host message
    //  loop must call ProcessDialogMessage each iteration to drive it.
    //
    HRESULT OpenModeless (HINSTANCE         hInstance,
                          HWND              ownerHwnd,
                          UserConfigStore & ucs,
                          GlobalUserPrefs & prefs,
                          ThemeManager    & themes,
                          EmulatorShell   & emuShell,
                          IFileSystem     & fs);

protected:
    void     OnBuildPages () override;

    //  Commit / revert hooks. OnOk runs the apply controller's full commit
    //  pipeline (save deltas, persist CRT, activate staged theme, machine
    //  switch) and closes; OnCancel rolls back live-preview edits + an
    //  "Apply now" theme. Ordering is load-bearing -- see SettingsApplyController.
    HRESULT  OnOk     () override;
    void     OnCancel () override;

    //  Re-evaluate the "OK" / "OK (reboot)" label each dialog tick so it
    //  tracks machine-dropdown + hardware edits (FR-131).
    void     OnDialogTick () override;

    //  Custom text-color picker (list #8). Hosted as the framework's modal
    //  in-content overlay so it floats above the page and grabs all input while
    //  open, preserving the bespoke look (no separate popup HWND). Layout keeps
    //  it centred in the current sheet bounds.
    void     Layout            (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;
    bool     HasModalOverlay   () const override;
    void     PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnOverlayMouse    (const DxuiMouseEvent & ev) override;
    bool     OnOverlayChar     (wchar_t ch) override;
    bool     OnOverlayKey      (WPARAM vk) override;

private:
    //  Set OK to "OK (reboot)" when committing would power-cycle the machine
    //  (staged machine change or a reset-requiring hardware edit), else "OK".
    void  RefreshOkLabel ();

    //  Show/hide + fill the amber restart-notice caption (list #3 / FR-131).
    void  UpdateRestartNotice ();

    // Drive-sound audition for the Machine page's play (>) buttons. Ported
    // verbatim from SettingsPanel: push the current volumes / pan / mechanism
    // to the engine and post the one-shot test command. Self-contained (needs
    // only the emulator shell + staged prefs); the cancel-revert bookkeeping
    // lands with the commit controller in a later slice.
    void  AuditionDriveSound    (int drive, int kind, bool centered);
    void  PushDriveAudioToEngine (float motor, float head, float door,
                                  float pan0, float pan1,
                                  const std::string & mechanism);

    // Snapshot the as-opened drive-audio values (call at Show) and, on Cancel,
    // undo a play-button audition that pushed dialed values live to the engine.
    // A play (>) audition pushes staged volumes / pan / mechanism straight to
    // the engine mixer for preview; without this the mixer would keep those
    // auditioned values after Cancel even though nothing was persisted.
    void  SnapshotDriveAudioBaseline ();
    void  RevertDriveAuditionIfDirty ();

    UserConfigStore * m_ucs      = nullptr;
    GlobalUserPrefs * m_prefs    = nullptr;
    ThemeManager    * m_themes   = nullptr;
    EmulatorShell   * m_emuShell = nullptr;
    IFileSystem     * m_fs       = nullptr;

    SettingsPanelState        m_state;
    SettingsMachineCatalog    m_catalog;
    SettingsDisplayCrtBridge  m_crt;
    SettingsApplyController   m_apply;
    SettingsPreviewController m_preview;
    ColorPickerOverlay        m_colorPicker;

    // Live-preview post-process (#8): installed as the window's compose hook.
    // The window is composited (per-pixel alpha) so the inactive path draws
    // the panel sharp + opaque, and an active Display drag blurs + dims the
    // panel and reveals the emulator through the overlap region. Declared
    // before the DxuiWindow base's swap chain via member order is irrelevant
    // (base tears down last); its D3D resources are released in its dtor while
    // the borrowed device is still alive.
    SettingsCompositor        m_compositor;

    // Drive the compositor's per-frame transparency state (active flag +
    // emulator-overlap rect + focused-control rect) and invalidate while a
    // Display preview is live so each drag frame recomposes.
    void  UpdatePreviewCompose ();

    // Last mechanism pushed to the engine, so PushDriveAudioToEngine skips a
    // redundant WAV reload when it hasn't changed.
    std::string               m_lastAuditionMechanism;

    // Drive-audio engine state as the dialog opened + whether a play-button
    // audition has since pushed dialed values live, so OnCancel can restore the
    // mixer to the baseline (SnapshotDriveAudioBaseline / RevertDriveAuditionIfDirty).
    bool                      m_driveAuditionDirty    = false;
    float                     m_baselineDriveMotorVol = 0.0f;
    float                     m_baselineDriveHeadVol  = 0.0f;
    float                     m_baselineDriveDoorVol  = 0.0f;
    float                     m_baselineDriveOnePan   = 0.0f;
    float                     m_baselineDriveTwoPan   = 0.0f;
    std::string               m_baselineMechanism;

    // True while a Display control is driving the live-preview blur/reveal, so
    // OnDialogTick keeps recomposing (and stops once a keyboard preview idles
    // out). m_previewFocusId is the kControl* id of the control being edited,
    // so UpdatePreviewCompose can keep it sharp (feathered) over the blur.
    bool                      m_previewActive  = false;
    int                       m_previewFocusId = -1;

    // Amber "press OK to reboot" caption in the bottom bar; owned by the child
    // list (CreateChild), raw pointer for layout / text updates. Null pre-Create.
    DxuiLabel               * m_restartNotice = nullptr;

    // Owned by the DxuiPropertySheet child list (CreatePage); raw pointers
    // for wiring only. m_hardwarePage hosts the merged "Machine" tab (machine
    // selector + CPU speed + hardware spec + device tree, GH #84).
    HardwarePage * m_hardwarePage = nullptr;
    DiskPage     * m_diskPage     = nullptr;
    ThemePage    * m_themePage    = nullptr;
    DisplayPage  * m_displayPage  = nullptr;

    // Registration index of the Disk page, so OnDialogTick can show / hide its
    // tab as the staged Disk ][ controller is toggled (#84 Phase B). -1 until
    // the pages are built.
    int            m_diskPageIndex = -1;

    // Whether the Disk tab last relayouted as visible, so we only recompute the
    // dynamic tab when the controller-present state actually flips.
    bool           m_diskTabVisible = true;

    // Refresh the Disk tab's presence from the staged hardware config.
    void  UpdateDiskTabVisibility ();
};
