#pragma once

#include "Pch.h"

#include "DisplayPage.h"
#include "ColorPickerOverlay.h"
#include "HardwarePage.h"
#include "MachinePage.h"
#include "SettingsApplyController.h"
#include "SettingsDisplayCrtBridge.h"
#include "SettingsMachineCatalog.h"
#include "SettingsPanelState.h"
#include "SettingsPreviewController.h"
#include "ThemePage.h"
#include "SettingsWindow.h"

#include "Core/DxuiPanel.h"
#include "Core/DxuiFocusManager.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiModalScrim.h"
#include "Widgets/DxuiTabStrip.h"

#include "../../Config/GlobalUserPrefs.h"

#include "Core/MachineScanner.h"


class UiShell;
class UserConfigStore;
struct GlobalUserPrefs;
class ThemeManager;
class EmulatorShell;
class IFileSystem;





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
//  Owner of the consolidated settings surface. Composes the four
//  setting pages (Machine / Hardware / Theme stub / Display stub)
//  behind a DxuiTabStrip plus Apply / Cancel buttons and a `DxuiModalScrim`
//  the apply path lights for reset-required confirmation.
//
//  The panel is render- and input-active only while `IsVisible()`
//  returns true. While visible, the host shell routes mouse + key
//  events into `OnMouse*` / `OnKey` instead of the chrome / emulator.
//  The emulator never pauses (FR-041) -- the panel is non-modal with
//  respect to CPU time.
//
//  Tab pages, the modal scrim, and the apply pipeline are wired
//  through dependency injection rather than singletons so unit tests
//  can drive the same state machine without the chrome stack.
//
////////////////////////////////////////////////////////////////////////////////

class SettingsPanel : public DxuiPanel
{
public:
    SettingsPanel  ();
    ~SettingsPanel () override;

    HRESULT Initialize (UiShell         & uiShell,
                        UserConfigStore & ucs,
                        GlobalUserPrefs & prefs,
                        ThemeManager    & themes,
                        EmulatorShell   & emuShell,
                        IFileSystem     & fs);

    HRESULT Show      ();
    void    Hide      ();
    void    Accept();
    void    Cancel();
    HRESULT RenderPopup();
    void    SetTheme    (const CassoTheme * theme);
    void    UpdatePreviewOverlap (const RECT & emulatorContentScreenRect);
    void    PreparePreviewFrame();
    SIZE    PreferredClientSize (UINT dpi) const;
    bool    IsVisible () const { return m_panelVisible; }
    bool    IsPreviewTransparencyActive() const;
    RECT    GetFocusedControlClientRect() const;

    // Closes any open page dropdown and unwires the DxuiHwndSource
    // pointer from every page dropdown. Called before the owning
    // SettingsWindow tears its DxuiHwndSource down so the dropdowns
    // can't hold a dangling host (or popup-host) pointer past the
    // host's lifetime.
    void    DetachPopupHosts();

    // Render + input routing. The owned settings popup composites Paint
    // into its own swap chain; the routing helpers consume popup-local
    // mouse / key events only while the panel is visible.
    //
    //  Bespoke input + paint shims preserved for UiShell coupling.
    //  UiShell still routes WM_* messages through these signatures
    //  rather than dispatching uniform DxuiMouseEvent / DxuiKeyEvent
    //  values through the IDxuiControl base. Once UiShell + the
    //  Settings popup share the unified Dxui dispatch path, these
    //  collapse into the base DxuiPanel auto-fan-out and vanish.
    //
    void    Layout    (int viewportWidthPx, int viewportHeightPx, const DxuiDpiScaler & scaler, int topInsetPx = 0);
    void    Paint     (DxuiPainter & painter, DxuiTextRenderer & text);
    // True while a modal overlay (the color picker) is open. The window
    // renderer queries this to paint the overlay in a separate sharp pass
    // on top of the (blurred) panel via PaintModalOverlay.
    bool    HasModalOverlay   () const { return m_colorPicker.IsOpen(); }
    void    PaintModalOverlay (DxuiPainter & painter, DxuiTextRenderer & text);
    void    OnMouseMove   (int x, int y);
    void    OnLButtonDown (int x, int y);
    void    OnLButtonUp   (int x, int y);
    bool    OnKey         (WPARAM vk);
    bool    OnChar        (wchar_t ch);

    // IDxuiControl pure-virtual overrides supplied by inheriting
    // DxuiPanel. Implemented as adapters that defer to the bespoke
    // entry points above so existing callers keep working.
    void    Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void    Paint  (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;

    // Surface the base overloads so virtual dispatch through
    // IDxuiControl* still resolves correctly and direct callers can
    // reach the base overload without name-hiding ambiguity.
    using DxuiPanel::Layout;
    using DxuiPanel::Paint;
    using DxuiPanel::OnKey;

    const SettingsPanelState & State () const { return m_state; }
    SettingsPanelState       & MutableState () { return m_state; }

    static const char * SpeedRadioValue (SettingsSpeedMode m);
    static const char * ColorRadioValue (SettingsColorMode m);

private:
    enum class TabIndex
    {
        Machine  = 0,
        Hardware = 1,
        Theme    = 2,
        Display  = 3,
    };

    // Canonical tab-page count (TabIndex enumerators). Used by the page
    // dispatch table and the Ctrl+Tab cycle wrap math.
    static constexpr int  s_kTabCount = 4;


    // Active page for polymorphic input / paint dispatch. m_activeTab is
    // always a valid TabIndex (0..s_kTabCount-1), so no bounds guard.
    IDxuiControl * ActivePage() const { return m_pages[m_activeTab]; }


    void  OnMachineSelected           (const std::string & machineName);
    void  OnThemeSelected             (const std::string & themeName);
    void  OnApplyThemeNow             ();
    void  OnApplyClicked              ();
    void  OnCancelClicked             ();
    void  SetActiveTab                (int tab);
    void  UpdatePageVisibility        ();
    void  StartPreview                (int focus, bool keyboardMode);
    void  EndPreview                  ();
    void  AuditionDriveSound          (int drive, int kind, bool centered);
    void  PushDriveAudioToEngine      (float motor, float head, float door,
                                       float pan0,  float pan1,
                                       const std::string & mechanism);


    UiShell         * m_uiShell   = nullptr;
    UserConfigStore * m_ucs       = nullptr;
    GlobalUserPrefs * m_prefs     = nullptr;
    ThemeManager    * m_themes    = nullptr;
    EmulatorShell   * m_emuShell  = nullptr;
    IFileSystem     * m_fs        = nullptr;

    SettingsPanelState  m_state;
    SettingsDisplayCrtBridge  m_crt;
    SettingsMachineCatalog    m_catalog;
    SettingsApplyController   m_apply;
    DxuiFocusManager          m_focusMgr;
    SettingsWindow      m_window;
    // Bespoke visibility flag distinct from IDxuiControl::m_visible
    // (which is inherited from DxuiPanel and stays at its default of
    // true). The bespoke flag tracks whether the popup HWND is active
    // and gates every input/paint shim; the IDxuiControl bit reflects
    // panel-tree visibility and is reserved for the future unified
    // dispatch path.
    bool                m_panelVisible = false;

    // Tracks whether the monitor dropdown was open last frame so the
    // dropdown-closed revert fires even if a click on another control
    // changes the preview focus in the same frame.
    bool                  m_monitorWasOpen    = false;
    // The monitor index that was active when the dropdown was opened.
    // Used to revert the live preview when the dropdown closes without
    // a click selection (since SelectedIndex() stays at the original
    // until a click commits a new one).
    int                   m_monitorOpenedAt   = -1;

    // Live-preview state machine lives in SettingsPreviewController.
    // While a slider is dragged or a dropdown is open, the renderer
    // can reveal the emulator under the settings window. Keyboard-
    // driven changes auto-dismiss the preview 500ms after the last
    // keystroke.
    SettingsPreviewController  m_previewCtrl;
    bool                m_previewOverlapsEmulatorOutput = false;
    // Emulator content rect translated into Settings-window client
    // coords (or {0,0,0,0} when there's no overlap). The renderer
    // uses it to clip the transparency compose pass per pixel.
    RECT                m_emulatorOverlapClientRect = {};

    DxuiTabStrip            m_tabs;
    MachinePage         m_machinePage;
    HardwarePage        m_hardwarePage;
    ThemePage           m_themePage;
    DisplayPage         m_displayPage;
    ColorPickerOverlay  m_colorPicker;
    DxuiModalScrim          m_scrim;
    DxuiButton              m_applyButton;
    DxuiButton              m_cancelButton;

    // Polymorphic page dispatch table in TabIndex order
    // [Machine, Hardware, Theme, Display]. Populated once in the ctor
    // after the page members are constructed; ActivePage() indexes it by
    // m_activeTab to replace the former per-tab switch dispatch.
    IDxuiControl      * m_pages[s_kTabCount] = {};

    // Drive-audio audition state. A play button posts the dialed
    // volumes / pan / mechanism to the engine for preview; if the user
    // Cancels, these baselines (captured at panel open) are restored.
    // m_driveAuditionDirty gates the restore; m_lastAuditionMechanism
    // tracks what the engine last loaded so a redundant WAV reload is
    // skipped.
    float               m_baselineDriveMotorVol = 0.0f;
    float               m_baselineDriveHeadVol  = 0.0f;
    float               m_baselineDriveDoorVol  = 0.0f;
    float               m_baselineDriveOnePan   = 0.0f;
    float               m_baselineDriveTwoPan   = 0.0f;
    std::string         m_baselineMechanism;
    std::string         m_lastAuditionMechanism;
    bool                m_driveAuditionDirty    = false;

    // Applied //e text-color choice (Color monitor) captured at panel
    // open so Cancel can revert any live edit to the text treatment.
    ColorMonitorTextMode  m_baselineColorTextMode       = ColorMonitorTextMode::White;
    uint32_t              m_baselineColorTextCustomArgb = ColorUtil::kWhiteArgb;

    RECT                m_viewport     = {};
    RECT                m_panelRect    = {};
    RECT                m_captionRect  = {};
    int                 m_activeTab    = (int) TabIndex::Machine;
};
