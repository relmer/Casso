#include "Pch.h"

#include "SettingsPanel.h"

#include "../UiShell.h"
#include "../../EmulatorShell.h"
#include "../../Config/CrtPresets.h"
#include "../../Config/UserConfigStore.h"
#include "../../Config/IFileSystem.h"
#include "../ThemeManager.h"
#include "../Chrome/ChromeMetrics.h"

#include "resource.h"


////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kPanelWidthDp   = 720;
    constexpr int    s_kPanelHeightDp  = 720;
    constexpr int    s_kCaptionHeightDp = 32;
    constexpr int    s_kTabHeightDp    = 32;
    constexpr int    s_kBottomBarDp    = 56;
    constexpr int    s_kButtonWidthDp  = 96;
    constexpr int    s_kButtonHeightDp = 28;
    constexpr int    s_kButtonGapDp    = 8;
    constexpr int    s_kPanelPadDp     = 16;
    constexpr int    s_kPanelMarginDp  = 16;
    constexpr uint32_t s_kCaptionBgArgb = 0xFF0F1620;
    constexpr uint32_t s_kCaptionTextArgb = 0xFFE8EEF4;
    constexpr float    s_kEdgeThickDp  = 1.0f;
    constexpr float    s_kCaptionFontDp = 14.0f;


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    //
    //  Mouse / key event construction helpers. SettingsPanel still
    //  receives raw (x, y) / WPARAM values from UiShell; pages and
    //  widgets implement IDxuiControl::OnMouse / OnKey. These helpers
    //  bridge the legacy signatures into the uniform DxuiMouseEvent /
    //  DxuiKeyEvent values the Dxui dispatch path expects.
    //
    DxuiMouseEvent MakeMouseMove (int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = { x, y };
        return ev;
    }


    DxuiMouseEvent MakeMouseButton (DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = { x, y };
        return ev;
    }


    DxuiKeyEvent MakeKeyDown (WPARAM vk)
    {
        DxuiKeyEvent  ev;

        ev.kind  = DxuiKeyEventKind::Down;
        ev.vk    = vk;
        ev.shift = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
        ev.ctrl  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;
        ev.alt   = (GetKeyState (VK_MENU)    & 0x8000) != 0;
        return ev;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanel::SettingsPanel ()
{
    // Register each owned widget into the panel's child list via Adopt
    // so they participate in the IDxuiControl tree (Bounds, Visible,
    // focus, parent pointers). The widgets remain SettingsPanel-owned
    // members; Adopt is non-owning. UiShell still drives input/paint
    // through the bespoke shims below; collapsing the duality is
    // deferred to a follow-up session that also threads a popup host
    // through to the page-level dropdowns.
    Adopt (m_tabs);
    Adopt (m_machinePage);
    Adopt (m_hardwarePage);
    Adopt (m_themePage);
    Adopt (m_displayPage);
    Adopt (m_scrim);
    Adopt (m_applyButton);
    Adopt (m_cancelButton);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~SettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanel::~SettingsPanel ()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::Initialize (
    UiShell         & uiShell,
    UserConfigStore & ucs,
    GlobalUserPrefs & prefs,
    ThemeManager    & themes,
    EmulatorShell   & emuShell,
    IFileSystem     & fs)
{
    HRESULT    hr        = S_OK;
    HINSTANCE  hInstance = nullptr;



    m_uiShell  = &uiShell;
    m_ucs      = &ucs;
    m_prefs    = &prefs;
    m_themes   = &themes;
    m_emuShell = &emuShell;
    m_fs       = &fs;

    m_crt.Bind (&prefs, &themes, &m_state, &m_displayPage, &emuShell);
    m_catalog.Bind (&emuShell, &ucs, &prefs, &fs, &themes, &m_state,
                    &m_machinePage, &m_themePage);
    m_apply.Bind (&m_state, &ucs, &prefs, &fs, &emuShell, &m_window, &m_catalog);
    m_focusOrder.Bind (&uiShell, &m_tabs, &m_machinePage, &m_hardwarePage,
                       &m_themePage, &m_displayPage, &m_applyButton, &m_cancelButton);

    m_machinePage.SetState  (&m_state);
    m_machinePage.SetOnMachineSelected ([this] (const std::string & machineName) { OnMachineSelected (machineName); });
    m_machinePage.SetOnTestSound ([this] (int drive, int kind, bool centered) { AuditionDriveSound (drive, kind, centered); });
    m_hardwarePage.SetState (&m_state);
    m_displayPage.SetState  (&m_state);

    // Live-edit slider / toggle / monitor + Restore Defaults callbacks
    // funnel through SettingsDisplayCrtBridge so the CRT shader picks
    // edits up on the next frame via the per-monitor crtByMode block.
    m_crt.WireDisplayPageCallbacks();

    m_displayPage.SetOnPreview ([this] (int controlId, bool start, bool keyboardMode)
    {
        if (start)
        {
            StartPreview (controlId, keyboardMode);
        }
        else
        {
            EndPreview();
        }
    });

    m_themePage.SetOnThemeSelected ([this] (const std::string & themeName) { OnThemeSelected (themeName); });

    // Live framebuffer source for the Settings → Theme preview. The
    // ThemePage paints inside chrome composition, AFTER D3DRenderer
    // has uploaded the current frame to the back buffer, so the
    // CPU-side buffer the source returns is always one frame fresh.
    if (m_emuShell != nullptr)
    {
        EmulatorShell  & shellRef = *m_emuShell;

        m_themePage.SetFramebufferSource ([&shellRef] (int & outW, int & outH) -> const uint32_t *
        {
            outW = ChromeMetrics::kFramebufferWidthPx;
            outH = ChromeMetrics::kFramebufferHeightPx;
            return shellRef.UiFramebufferPixels();
        });

        m_themePage.SetMountedPathSource ([&shellRef] (int driveIndex) -> std::wstring
        {
            return shellRef.MountedImagePath (driveIndex);
        });
    }

    m_applyButton.SetLabel  (L"OK");
    m_applyButton.SetClick  ([this] { OnApplyClicked();  });
    m_applyButton.SetColors (0xFF2A6FB7u, 0xFF3380C8u, 0xFF1E548Cu);
    m_applyButton.SetTextColor (0xFFFFFFFFu);

    m_cancelButton.SetLabel    (L"Cancel");
    m_cancelButton.SetClick    ([this] { OnCancelClicked(); });
    m_cancelButton.SetColors   (0xFF3A3F46u, 0xFF4A5058u, 0xFF2A2F36u);
    m_cancelButton.SetTextColor (0xFFE8EEF4u);
    m_cancelButton.SetOutline   (1.0f, 0xFF5A6068u);

    hInstance = (HINSTANCE) GetWindowLongPtrW (m_emuShell->GetHwnd(), GWLP_HINSTANCE);
    hr = m_window.RegisterClass (hInstance);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Pulls the latest machine config + user JSON into the state, lays
//  the panel out against the current viewport, and flips visibility.
//  The emulator is NOT paused (FR-041) -- the panel is non-modal.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::Show ()
{
    HRESULT  hr      = S_OK;
    HWND     hwnd    = nullptr;



    m_catalog.LoadCurrentMachineIntoState();
    m_catalog.PopulateMachineList();
    m_catalog.PopulateThemeList();
    m_apply.ClearPending();

    // Snapshot ALL 4 monitor CRT blocks so Cancel can revert any edits
    // the user made -- including edits to monitors other than the one
    // active at panel open (they may have switched mid-edit).
    m_apply.SnapshotBaselines();

    // Capture the applied drive-audio levels so a play-button audition
    // (which pushes the in-progress edit to the engine) can be reverted
    // if the user Cancels. m_lastAuditionMechanism starts at the applied
    // mechanism so the first preview only reloads WAVs if it changed.
    m_baselineDriveMotorVol = m_state.Prefs().driveMotorVolume;
    m_baselineDriveHeadVol  = m_state.Prefs().driveHeadVolume;
    m_baselineDriveDoorVol  = m_state.Prefs().driveDoorVolume;
    m_baselineDriveOnePan   = m_state.Prefs().driveOnePan;
    m_baselineDriveTwoPan   = m_state.Prefs().driveTwoPan;
    m_baselineMechanism     = m_state.Prefs().floppyMechanism;
    m_lastAuditionMechanism = m_state.Prefs().floppyMechanism;
    m_driveAuditionDirty    = false;

    // Reset preview state so a previous session's interaction doesn't
    // leak in (e.g. user closed the panel mid-drag via Esc).
    m_previewCtrl.Reset();

    if (m_uiShell != nullptr)
    {
        Layout (m_uiShell->ViewportWidth(), m_uiShell->ViewportHeight(), m_uiShell->Scaler());
    }

    // Rebuild AFTER Layout so widgets that need to be populated by Layout
    // (e.g. DxuiDropdown items) exist before Rebuild calls SetSelected.
    m_machinePage.Rebuild();
    m_hardwarePage.Rebuild();
    m_displayPage.Rebuild();
    m_crt.ReseedFromActiveMode();

    m_panelVisible = true;
    m_focusOrder.Rebuild (m_activeTab);

    if (m_emuShell != nullptr)
    {
        hwnd = m_emuShell->GetHwnd();
        hr = m_window.Create (hwnd,
                              this,
                              m_emuShell->m_d3dRenderer.GetDevice(),
                              m_emuShell->m_d3dRenderer.GetContext(),
                              &m_emuShell->m_chromeTheme);
        CHRA (hr);

        // Now that the settings popup HWND (and its adopted
        // DxuiHostWindow) exist, thread the host into every page's
        // owned dropdowns so their menus render through the popup-
        // host pool. This escapes the SettingsWindow client clip so
        // a dropdown near the bottom of the panel can flip its menu
        // upward (or extend past the lower edge) without being cut
        // off (FR-054 / FR-061; SC-008).
        DxuiHostWindow *  host = m_window.Host();

        m_machinePage.SetPopupHost (host);
        m_themePage.SetPopupHost   (host);
        m_displayPage.SetPopupHost (host);
    }

Error:
    if (FAILED (hr))
    {
        m_panelVisible = false;
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Hide ()
{
    m_panelVisible = false;
    m_scrim.Hide();
    DetachPopupHosts();
    m_window.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DetachPopupHosts
//
//  Close any open dropdown menu (which releases its pooled
//  DxuiPopupHost back to the host) and clear each page dropdown's
//  popup-host pointer. Must run before the owning SettingsWindow
//  tears its DxuiHostWindow down so we don't leave the dropdowns
//  with a dangling host.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::DetachPopupHosts ()
{
    m_machinePage.SetPopupHost (nullptr);
    m_themePage.SetPopupHost   (nullptr);
    m_displayPage.SetPopupHost (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Accept()
{
    OnApplyClicked();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Cancel()
{
    OnCancelClicked();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderPopup
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanel::RenderPopup()
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (!m_panelVisible, S_OK);

    hr = m_window.Render();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePreviewOverlap
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::UpdatePreviewOverlap (const RECT & emulatorContentScreenRect)
{
    HRESULT  hr          = S_OK;
    POINT    origin      = {};
    RECT     windowRect  = {};
    RECT     intersect   = {};
    BOOL     ok          = FALSE;
    BOOL     overlaps    = FALSE;



    m_previewOverlapsEmulatorOutput = false;
    m_emulatorOverlapClientRect     = {};
    BAIL_OUT_IF (!m_panelVisible || !m_window.IsOpen() || IsRectEmpty (&emulatorContentScreenRect), S_OK);

    ok = GetWindowRect (m_window.Hwnd(), &windowRect);
    CWRA (ok);

    overlaps = IntersectRect (&intersect, &windowRect, &emulatorContentScreenRect);
    m_previewOverlapsEmulatorOutput = (overlaps != FALSE);

    if (overlaps)
    {
        // Translate the screen-space intersection into client-space
        // for use during Paint. The renderer paints in client coords
        // (its viewport is the window client area).
        origin.x = 0;
        origin.y = 0;
        ok = ClientToScreen (m_window.Hwnd(), &origin);
        CWRA (ok);
        m_emulatorOverlapClientRect = { intersect.left   - origin.x,
                                         intersect.top    - origin.y,
                                         intersect.right  - origin.x,
                                         intersect.bottom - origin.y };
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPreviewTransparencyActive
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanel::IsPreviewTransparencyActive() const
{
    // Engage blur+dim+compose any time the user is actively
    // interacting with a Display-page control, regardless of whether
    // the popup overlaps the emulator. When there's no overlap the
    // emu-clip rect is empty so the compose shader's per-pixel
    // transparent zone is empty too -- the blur+dim still happens to
    // focus attention on the control being adjusted.
    return m_panelVisible &&
           m_previewCtrl.IsActive() &&
           ((TabIndex) m_activeTab == TabIndex::Display);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetFocusedControlClientRect
//
////////////////////////////////////////////////////////////////////////////////

RECT SettingsPanel::GetFocusedControlClientRect() const
{
    RECT  rect = {};



    if (m_previewCtrl.IsActive() && ((TabIndex) m_activeTab == TabIndex::Display))
    {
        rect = m_displayPage.FocusedControlRect (m_previewCtrl.FocusedId());
    }

    return rect;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::SetTheme (const ChromeTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredClientSize
//
////////////////////////////////////////////////////////////////////////////////

SIZE SettingsPanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};



    size.cx = MulDiv (s_kPanelWidthDp,  (int) dpi, (int) DxuiDpiScaler::kBaseDpi);
    size.cy = MulDiv (s_kPanelHeightDp, (int) dpi, (int) DxuiDpiScaler::kBaseDpi);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartPreview / EndPreview / UpdatePreviewFade
//
//  The live-preview state machine. While a slider is being dragged
//  or a dropdown is open, the renderer can reveal the emulator under
//  the settings window. Keyboard-driven changes auto-end the preview
//  500ms after the last keystroke.
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  StartPreview / EndPreview
//
//  Thin shims over SettingsPreviewController so DisplayPage's preview
//  callback (which deals in `int` control IDs) doesn't need to know
//  about the controller's strongly-typed enum.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::StartPreview (int focus, bool keyboardMode)
{
    m_previewCtrl.StartPreview ((SettingsPreviewController::Focus) focus, keyboardMode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndPreview
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::EndPreview ()
{
    m_previewCtrl.EndPreview();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreparePreviewFrame
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PreparePreviewFrame()
{
    if (m_panelVisible && (TabIndex) m_activeTab == TabIndex::Display)
    {
        bool  monitorOpen = m_displayPage.MonitorDropdown().IsOpen();



        if (monitorOpen && ! m_monitorWasOpen)
        {
            // Capture the active monitor at open time so the close-
            // without-commit path can revert to it even when the
            // close was triggered by clicking a different control
            // (which moves m_previewFocus away from MonitorDropdown
            // in the same frame).
            m_monitorOpenedAt = m_displayPage.MonitorDropdown().SelectedIndex();
            if (m_previewCtrl.FocusedControl() != SettingsPreviewController::Focus::MonitorDropdown)
            {
                StartPreview ((int) SettingsPreviewController::Focus::MonitorDropdown, false);
            }
        }
        else if (! monitorOpen && m_monitorWasOpen)
        {
            // DxuiDropdown just closed. SelectedIndex() reflects whatever
            // was committed by a click (or stays at the dropdown-open
            // value if the user cancelled by clicking outside). Apply
            // unconditionally to revert any hover-preview that ran
            // while the dropdown was open: palette, active CRT mode
            // index, and slider widgets all snap to the committed
            // monitor's full state.
            int  committed = m_displayPage.MonitorDropdown().SelectedIndex();

            if (committed < 0)
            {
                committed = m_monitorOpenedAt;
            }
            if (committed >= 0)
            {
                m_state.SetColorMode ((SettingsColorMode) committed);
                if (m_emuShell != nullptr)
                {
                    m_emuShell->SetColorModeLive (committed);
                }
                m_crt.ReseedFromActiveMode();
            }
            m_monitorOpenedAt = -1;
            if (m_previewCtrl.FocusedControl() == SettingsPreviewController::Focus::MonitorDropdown)
            {
                EndPreview();
            }
        }

        m_monitorWasOpen = monitorOpen;
    }

    m_previewCtrl.Tick ((int64_t) GetTickCount64());
    m_window.GetRenderer().SetTransparencyState (IsPreviewTransparencyActive(),
                                                 m_emulatorOverlapClientRect,
                                                 GetFocusedControlClientRect());
}












////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineSelected
//
//  Stage the user's pick. The actual SwitchMachine (including the
//  possibly-modal ROM bootstrap) is deferred until OK is hit, so
//  the user can still Cancel out without disturbing the running
//  machine. CommitApply calls m_catalog.DoMachineSelect when this
//  differs from the currently-loaded machine.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnMachineSelected (const std::string & machineName)
{
    if (machineName.empty())
    {
        return;
    }
    m_apply.StagePendingMachine (machineName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnThemeSelected
//
//  Stage the user's theme pick. Like the machine selector, the actual
//  Activate + persist is deferred until OK so Cancel leaves the chrome
//  exactly as the user found it. m_apply.CommitApply consumes the
//  staged pick.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnThemeSelected (const std::string & themeName)
{
    if (themeName.empty())
    {
        return;
    }
    m_apply.StagePendingTheme (themeName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Fill the popup client area, then lay tabs, active page, modal scrim,
//  and the Apply / Cancel button row.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Layout (int viewportWidthPx, int viewportHeightPx, const DxuiDpiScaler & scaler, int topInsetPx)
{
    UINT    dpi          = scaler.Dpi();
    int     captionH     = 0;
    int     contentTop   = std::max<int> (0, topInsetPx);
    int     tabHeight    = scaler.Px (s_kTabHeightDp);
    int     bottomBar    = scaler.Px (s_kBottomBarDp);
    int     buttonWidth  = scaler.Px (s_kButtonWidthDp);
    int     buttonHeight = scaler.Px (s_kButtonHeightDp);
    int     buttonGap    = scaler.Px (s_kButtonGapDp);
    int     pad          = scaler.Px (s_kPanelPadDp);
    int     panelWidth   = std::max<int> (0, viewportWidthPx);
    int     panelHeight  = std::max<int> (0, viewportHeightPx - contentTop);
    int     left         = 0;
    int     top          = contentTop;
    int     tabsTop      = top + captionH;
    int     tabWidth     = std::max<int> (40, panelWidth / 4);
    RECT    pageRect     = {};
    RECT    bottomRow    = {};
    int     applyX       = 0;
    int     cancelX      = 0;
    int     buttonY      = 0;
    std::vector<DxuiTabStrip::Tab>  tabs;



    m_viewport   = { 0, 0, viewportWidthPx, viewportHeightPx };
    m_panelRect  = MakeRect (left, top, panelWidth, panelHeight);
    m_captionRect = MakeRect (left, top, panelWidth, captionH);

    tabs.push_back ({ MakeRect (left,                  tabsTop, tabWidth, tabHeight), L"Machine"  });
    tabs.push_back ({ MakeRect (left + tabWidth,       tabsTop, tabWidth, tabHeight), L"Hardware" });
    tabs.push_back ({ MakeRect (left + tabWidth * 2,   tabsTop, tabWidth, tabHeight), L"Theme"    });
    tabs.push_back ({ MakeRect (left + tabWidth * 3,   tabsTop, tabWidth, tabHeight), L"Display"  });
    m_tabs.SetTabs (std::move (tabs));
    m_tabs.SetSelected (m_activeTab);
    m_tabs.SetOnChange ([this] (int idx) { m_activeTab = idx; m_focusOrder.Rebuild (m_activeTab); });
    m_tabs.SetDpi (dpi);

    pageRect.left   = m_panelRect.left   + pad;
    pageRect.top    = tabsTop            + tabHeight + pad;
    pageRect.right  = m_panelRect.right  - pad;
    pageRect.bottom = m_panelRect.bottom - bottomBar;

    m_machinePage.Layout   (pageRect, scaler);
    m_hardwarePage.SetRect (pageRect, scaler);
    m_themePage.Layout     (pageRect, scaler);
    m_displayPage.Layout   (pageRect, scaler);

    bottomRow.left   = m_panelRect.left;
    bottomRow.top    = m_panelRect.bottom - bottomBar;
    bottomRow.right  = m_panelRect.right;
    bottomRow.bottom = m_panelRect.bottom;

    applyX  = m_panelRect.right - pad - buttonWidth;
    cancelX = applyX            - buttonGap - buttonWidth;
    buttonY = bottomRow.top     + (bottomBar - buttonHeight) / 2;

    m_applyButton.Layout  (MakeRect (applyX,  buttonY, buttonWidth, buttonHeight));
    m_cancelButton.Layout (MakeRect (cancelX, buttonY, buttonWidth, buttonHeight));
    m_applyButton.SetDpi  (dpi);
    m_cancelButton.SetDpi (dpi);

    m_scrim.SetViewportRect (m_viewport);

    // Mirror the panel footprint into the IDxuiControl tree so future
    // centralized walks see SettingsPanel as a panel covering its
    // popup-client rect. Adopted children already have their bounds
    // written via the per-widget Layout / SetRect calls above.
    DxuiPanel::SetBounds (m_panelRect);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Paint (DxuiPainter & painter, DxuiTextRenderer & text)
{
    ChromeTheme  theme            = (m_uiShell != nullptr) ? m_uiShell->Theme() : ChromeTheme::Skeuomorphic();
    float        edgeThick        = (m_uiShell != nullptr) ? m_uiShell->Scaler().Pxf (s_kEdgeThickDp)
                                                           : s_kEdgeThickDp;
    float        panelA           = 1.0f;
    float        focusedA         = 1.0f;
    int          focusedControlId = m_previewCtrl.IsActive() ? m_previewCtrl.FocusedId() : -1;



    if (!m_panelVisible)
    {
        return;
    }

    painter.SetGlobalAlpha (panelA);
    text.SetGlobalAlpha    (panelA);

    painter.FillRect    ((float) m_panelRect.left,
                          (float) m_panelRect.top,
                          (float) (m_panelRect.right  - m_panelRect.left),
                          (float) (m_panelRect.bottom - m_panelRect.top),
                          theme.panelBgArgb);
    painter.OutlineRect ((float) m_panelRect.left,
                          (float) m_panelRect.top,
                          (float) (m_panelRect.right  - m_panelRect.left),
                          (float) (m_panelRect.bottom - m_panelRect.top),
                          edgeThick, theme.panelEdgeArgb);

    m_tabs.Paint  (painter, text);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.Paint  (painter, text, theme); break;
        case TabIndex::Hardware: m_hardwarePage.Paint (painter, text, theme); break;
        case TabIndex::Theme:    m_themePage.Paint    (painter, text, theme); break;
        case TabIndex::Display:
            // DisplayPage paints its own controls at per-control alpha.
            // It restores global alpha to 1.0 on exit; re-clamp so the
            // buttons inherit the panel alpha consistently.
            m_displayPage.SetFadeState (focusedControlId, focusedA, panelA);
            m_displayPage.Paint  (painter, text, theme);
            painter.SetGlobalAlpha (panelA);
            text.SetGlobalAlpha    (panelA);
            break;
    }

    m_applyButton.Paint  (painter, text, theme);
    m_cancelButton.Paint (painter, text, theme);

    // Modal scrim (reset-required confirmation) always paints fully
    // opaque; it stops the panel from being interactable beneath it.
    painter.SetGlobalAlpha (1.0f);
    text.SetGlobalAlpha    (1.0f);
    m_scrim.Paint (painter);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnMouseMove (int x, int y)
{
    if (!m_panelVisible)
    {
        return;
    }

    m_tabs.SetMouseHover (x, y);
    m_applyButton.SetMouse  (x, y, false);
    m_cancelButton.SetMouse (x, y, false);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  (void) m_machinePage.OnMouse (MakeMouseMove (x, y)); break;
        case TabIndex::Hardware: (void) m_hardwarePage.OnMouse (MakeMouseMove (x, y)); break;
        case TabIndex::Theme:    (void) m_themePage.OnMouse (MakeMouseMove (x, y)); break;
        case TabIndex::Display:
            // IDxuiControl::OnMouse with kind=Move handles both
            // hover updates and active slider drag tracking — the
            // slider only follows the cursor while its m_dragging
            // latch is set, so siblings just refresh their hover.
            (void) m_displayPage.OnMouse (MakeMouseMove (x, y));
            break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnLButtonDown (int x, int y)
{
    if (!m_panelVisible)
    {
        return;
    }

    if (m_scrim.IsVisible())
    {
        return;
    }

    if (m_tabs.OnLButtonDown (x, y))
    {
        return;
    }

    m_applyButton.SetMouse  (x, y, true);
    m_cancelButton.SetMouse (x, y, true);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  (void) m_machinePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Down, x, y)); break;
        case TabIndex::Hardware: (void) m_hardwarePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Down, x, y)); break;
        case TabIndex::Theme:    (void) m_themePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Down, x, y)); break;
        case TabIndex::Display:  (void) m_displayPage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Down, x, y)); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnLButtonUp (int x, int y)
{
    if (!m_panelVisible)
    {
        return;
    }

    if (m_scrim.IsVisible())
    {
        return;
    }

    (void) m_tabs.OnLButtonUp (x, y);

    if (m_applyButton.HitTest (x, y))
    {
        m_applyButton.Click();
    }
    else if (m_cancelButton.HitTest (x, y))
    {
        m_cancelButton.Click();
    }
    else
    {
        switch ((TabIndex) m_activeTab)
        {
            case TabIndex::Machine:  (void) m_machinePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Up, x, y)); break;
            case TabIndex::Hardware: (void) m_hardwarePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Up, x, y)); break;
            case TabIndex::Theme:    (void) m_themePage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Up, x, y)); break;
            case TabIndex::Display:  (void) m_displayPage.OnMouse (MakeMouseButton (DxuiMouseEventKind::Up, x, y)); break;
        }
    }

    m_applyButton.SetMouse  (x, y, false);
    m_cancelButton.SetMouse (x, y, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanel::OnKey (WPARAM vk)
{
    bool  shiftHeld = (GetKeyState (VK_SHIFT)   & 0x8000) != 0;
    bool  ctrlHeld  = (GetKeyState (VK_CONTROL) & 0x8000) != 0;


    if (!m_panelVisible)
    {
        return false;
    }

    if (m_scrim.IsVisible())
    {
        return m_scrim.OnKey (vk);
    }

    // Open dropdowns take priority — Up/Down/Enter/Esc steer the menu
    // rather than the panel-level focus list.
    if (m_focusOrder.AnyDropdownOpenOnActivePage (m_activeTab))
    {
        switch ((TabIndex) m_activeTab)
        {
            case TabIndex::Machine:  if (m_machinePage.OnKey (MakeKeyDown (vk))) { return true; } break;
            default: break;
        }
    }

    // Ctrl+Tab / Ctrl+Shift+Tab cycle through the tab pages regardless
    // of which widget currently owns focus inside the page. Matches the
    // Windows convention used by browser tabs, VS document tabs, etc.
    if (vk == VK_TAB && ctrlHeld)
    {
        constexpr int  s_kTabCount = 4;
        int            next       = m_activeTab + (shiftHeld ? -1 : 1);

        if (next < 0)
        {
            next = s_kTabCount - 1;
        }
        else if (next >= s_kTabCount)
        {
            next = 0;
        }

        m_activeTab = next;
        m_tabs.SetSelected (m_activeTab);
        m_focusOrder.Rebuild (m_activeTab);
        return true;
    }

    if (vk == VK_TAB && m_uiShell != nullptr)
    {
        FocusKey  fk = shiftHeld ? FocusKey::ShiftTab : FocusKey::Tab;

        if (m_uiShell->Focus().HandleKey (fk))
        {
            m_focusOrder.SyncToWidgets();
        }
        return true;
    }

    if (vk == VK_ESCAPE)
    {
        OnCancelClicked();
        return true;
    }

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  if (m_machinePage.OnKey  (MakeKeyDown (vk))) { return true; } break;
        case TabIndex::Hardware: if (m_hardwarePage.OnKey (MakeKeyDown (vk))) { return true; } break;
        case TabIndex::Theme:    if (m_themePage.OnKey    (MakeKeyDown (vk))) { return true; } break;
        case TabIndex::Display:  if (m_displayPage.OnKey  (MakeKeyDown (vk))) { return true; } break;
    }

    if (m_applyButton.OnKey  (vk)) { return true; }
    if (m_cancelButton.OnKey (vk)) { return true; }

    return m_tabs.OnKey (vk);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnApplyClicked
//
//  Hardware enable/disable changes require a confirm + reset; live
//  fields commit unconditionally. The modal-scrim consent step is
//  the user-visible piece of FR-010.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnApplyClicked ()
{
    if (m_apply.IsResetRequired())
    {
        m_scrim.Show ([this] { m_apply.CommitApply(); m_panelVisible = false; },
                      [this] { /* cancel — keep panel open, no commit */ });
        return;
    }

    m_apply.CommitApply();
    m_panelVisible = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCancelClicked
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnCancelClicked ()
{
    m_apply.Cancel (m_previewCtrl);

    // Undo any play-button audition that pushed dialed drive-audio
    // values to the engine for preview.
    if (m_driveAuditionDirty)
    {
        PushDriveAudioToEngine (m_baselineDriveMotorVol,
                                m_baselineDriveHeadVol,
                                m_baselineDriveDoorVol,
                                m_baselineDriveOnePan,
                                m_baselineDriveTwoPan,
                                m_baselineMechanism);
        m_driveAuditionDirty = false;
    }
    m_panelVisible = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AuditionDriveSound
//
//  Previews a single drive sound at the in-progress dialed settings.
//  Volume previews (centered) pan the test drive to centre so the gain
//  is judged without bias; pan previews keep both drives at their dialed
//  positions. Volumes/pan/mechanism are pushed first so the one-shot
//  reflects the current edits, not the last-applied state.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::AuditionDriveSound (int drive, int kind, bool centered)
{
    HRESULT                  hr       = S_OK;
    const SettingsUiPrefs *  prefs    = nullptr;
    char                     test[16] = {};
    float                    pan0     = 0.0f;
    float                    pan1     = 0.0f;



    BAIL_OUT_IF (m_emuShell == nullptr, S_OK);

    prefs = &m_state.Prefs();

    pan0 = prefs->driveOnePan;
    pan1 = prefs->driveTwoPan;
    if (centered)
    {
        if (drive == 0) { pan0 = 0.0f; }
        else            { pan1 = 0.0f; }
    }

    PushDriveAudioToEngine (prefs->driveMotorVolume,
                            prefs->driveHeadVolume,
                            prefs->driveDoorVolume,
                            pan0,
                            pan1,
                            prefs->floppyMechanism);

    sprintf_s (test, "%d,%d", drive, kind);
    m_emuShell->PostCommand (IDM_AUDIO_DRIVE_TEST, test);

    m_driveAuditionDirty = true;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushDriveAudioToEngine
//
//  Posts volumes + pan + mechanism to the engine command queue. The
//  mechanism is reloaded only when it differs from what the engine last
//  loaded (m_lastAuditionMechanism), avoiding a redundant WAV reload.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PushDriveAudioToEngine (
    float                motor,
    float                head,
    float                door,
    float                pan0,
    float                pan1,
    const std::string  & mechanism)
{
    HRESULT  hr      = S_OK;
    char     vol[32] = {};
    char     pan[32] = {};



    BAIL_OUT_IF (m_emuShell == nullptr, S_OK);

    sprintf_s (vol, "%d,%d,%d",
               (int) std::lround (motor * 100.0f),
               (int) std::lround (head  * 100.0f),
               (int) std::lround (door  * 100.0f));
    m_emuShell->PostCommand (IDM_AUDIO_DRIVE_VOLUMES, vol);

    if (mechanism != m_lastAuditionMechanism)
    {
        m_emuShell->PostCommand (IDM_AUDIO_DRIVE_MECHANISM, mechanism);
        m_lastAuditionMechanism = mechanism;
    }

    sprintf_s (pan, "%d,%d",
               (int) std::lround (pan0 * 100.0f),
               (int) std::lround (pan1 * 100.0f));
    m_emuShell->PostCommand (IDM_AUDIO_DRIVE_PAN, pan);

Error:
    return;
}









////////////////////////////////////////////////////////////////////////////////
//
//  SpeedRadioValue
//
////////////////////////////////////////////////////////////////////////////////

const char * SettingsPanel::SpeedRadioValue (SettingsSpeedMode m)
{
    switch (m)
    {
    case SettingsSpeedMode::Authentic: return "authentic";
    case SettingsSpeedMode::Double:    return "2x";
    case SettingsSpeedMode::Maximum:   return "max";
    }

    return "authentic";
}





////////////////////////////////////////////////////////////////////////////////
//
//  ColorRadioValue
//
////////////////////////////////////////////////////////////////////////////////

const char * SettingsPanel::ColorRadioValue (SettingsColorMode m)
{
    switch (m)
    {
    case SettingsColorMode::Color: return "color";
    case SettingsColorMode::Green: return "green";
    case SettingsColorMode::Amber: return "amber";
    case SettingsColorMode::White: return "white";
    }

    return "color";
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Layout(RECT, scaler) onto the
//  bespoke 4-arg viewport variant. The bespoke entry point is what
//  UiShell calls today; the override exists so IDxuiControl-tree
//  walks can reach SettingsPanel without an explicit downcast. The
//  RECT origin is treated as a top-inset to preserve the existing
//  behavior of `Layout (viewportW, viewportH, scaler, topInset)`.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    Layout (boundsDip.right  - boundsDip.left,
            boundsDip.bottom - boundsDip.top,
            scaler,
            boundsDip.top);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Paint(IDxuiPainter, ...) onto
//  the bespoke 2-arg variant. UiShell still drives painting through
//  the bespoke entry point; the override is in place so an
//  IDxuiControl-tree walk targeting SettingsPanel routes back to the
//  bespoke pipeline (which dispatches to the active page, scrim,
//  and buttons in the order the bespoke code requires) rather than
//  through DxuiPanel's blind front-to-back fan-out.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (theme);

    Paint (static_cast<DxuiPainter &> (painter),
           static_cast<DxuiTextRenderer &> (text));
}
