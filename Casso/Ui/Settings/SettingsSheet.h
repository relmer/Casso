#pragma once

#include "Pch.h"

#include "Window/DxuiPropertySheet.h"

#include "SettingsPanelState.h"
#include "SettingsMachineCatalog.h"
#include "SettingsDisplayCrtBridge.h"
#include "SettingsApplyController.h"
#include "SettingsPreviewController.h"
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
//  T162 slice 3: the DxuiPropertySheet-based replacement for the bespoke
//  SettingsPanel + SettingsWindow pair. Hosts the four DxuiPropertyPage
//  setting pages behind the framework's tab strip + (Apply-hidden)
//  OK / Cancel row.
//
//  Built in parallel with the legacy path: slice 3a stands up the pages
//  and shows the sheet modally from a temporary dev trigger to prove the
//  structure. The cross-cutting commit (OnOk / OnCancel), the modeless
//  presentation, and the blur / live-preview compositor land in later
//  slices; the swap onto IDM_VIEW_SETTINGS and deletion of the legacy
//  types happens in 3d.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsSheet : public DxuiPropertySheet
{
public:
    SettingsSheet  () = default;
    ~SettingsSheet () override = default;

    //
    //  Wire dependencies, create the window (which builds + populates the
    //  pages), and show it modally. Returns when the dialog closes.
    //  (Slice 3a is modal for verification; 3d makes it modeless per
    //  FR-041 so the emulator keeps running.)
    //
    HRESULT OpenModal (HINSTANCE         hInstance,
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

    // Last mechanism pushed to the engine, so PushDriveAudioToEngine skips a
    // redundant WAV reload when it hasn't changed.
    std::string               m_lastAuditionMechanism;

    // True while a Display control is driving the live-preview fade, so
    // OnDialogTick can restore opacity once a keyboard preview idles out.
    bool                      m_previewActive = false;

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
