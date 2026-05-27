#include "Pch.h"

#include "SettingsPanel.h"

#include "../UiShell.h"
#include "../../EmulatorShell.h"
#include "../../AssetBootstrap.h"
#include "../../Config/CrtPresets.h"
#include "../../Config/UserConfigStore.h"
#include "../../Config/IFileSystem.h"
#include "../ThemeManager.h"
#include "../Chrome/ChromeMetrics.h"

#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"

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
    constexpr uint32_t s_kPanelBgArgb  = 0xFF1A2230;
    constexpr uint32_t s_kPanelEdgeArgb = 0xFF334050;
    constexpr uint32_t s_kCaptionBgArgb = 0xFF0F1620;
    constexpr uint32_t s_kCaptionTextArgb = 0xFFE8EEF4;
    constexpr float    s_kEdgeThickDp  = 1.0f;
    constexpr float    s_kCaptionFontDp = 14.0f;


    ////////////////////////////////////////////////////////////////////////////
    //
    //  SettingsApplyAdapter
    //
    //  Bridges the pure-logic ISettingsApplySink contract into the
    //  EmulatorShell command queue. Live-effect fields post commands so
    //  the audio mixer / CRT pipeline picks them up on the next CPU
    //  tick; QueueMachineReset is recorded and consumed by the modal
    //  confirm path in SettingsPanel.
    //
    ////////////////////////////////////////////////////////////////////////////

    class SettingsApplyAdapter : public ISettingsApplySink
    {
    public:
        explicit SettingsApplyAdapter (EmulatorShell & shell)
            : m_shell (shell)
        {
        }

        void ApplySpeedMode (SettingsSpeedMode mode) override
        {
            WORD  id = IDM_MACHINE_SPEED_1X;

            switch (mode)
            {
                case SettingsSpeedMode::Authentic: id = IDM_MACHINE_SPEED_1X;  break;
                case SettingsSpeedMode::Double:    id = IDM_MACHINE_SPEED_2X;  break;
                case SettingsSpeedMode::Maximum:   id = IDM_MACHINE_SPEED_MAX; break;
            }
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void ApplyColorMode (SettingsColorMode mode) override
        {
            WORD  id = IDM_VIEW_COLOR;

            switch (mode)
            {
                case SettingsColorMode::Color: id = IDM_VIEW_COLOR; break;
                case SettingsColorMode::Green: id = IDM_VIEW_GREEN; break;
                case SettingsColorMode::Amber: id = IDM_VIEW_AMBER; break;
                case SettingsColorMode::White: id = IDM_VIEW_WHITE; break;
            }
            PostMessageW (m_shell.GetHwnd(), WM_COMMAND, MAKEWPARAM (id, 0), 0);
        }

        void ApplyFloppySound  (bool enabled) override
        {
            m_shell.PostCommand (enabled ? IDM_AUDIO_DRIVE_ENABLE
                                         : IDM_AUDIO_DRIVE_DISABLE);
        }

        void ApplyMechanism    (const std::string & mechanism) override
        {
            m_shell.PostCommand (IDM_AUDIO_DRIVE_MECHANISM, mechanism);
        }

        void ApplyWriteProtect (int drive, bool wp)            override { UNREFERENCED_PARAMETER (drive); UNREFERENCED_PARAMETER (wp); }
        void QueueMachineReset ()                              override { m_resetQueued = true; }

        bool  ResetQueued () const { return m_resetQueued; }

    private:
        EmulatorShell & m_shell;
        bool            m_resetQueued = false;
    };


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
    }


    std::string NarrowMachineName (const std::wstring & wideName)
    {
        std::string  narrowName;



        narrowName.reserve (wideName.size());
        for (wchar_t c : wideName)
        {
            narrowName.push_back ((char) (unsigned char) c);
        }
        return narrowName;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanel::SettingsPanel ()
{
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

    m_machinePage.SetState  (&m_state);
    m_machinePage.SetOnMachineSelected ([this] (const std::string & machineName) { OnMachineSelected (machineName); });
    m_hardwarePage.SetState (&m_state);
    m_displayPage.SetState  (&m_state);

    // Brightness / contrast slide callbacks write LIVE to GlobalUserPrefs.crt
    // so the emulator's per-frame MakeCrtParams picks the new value up on
    // the next CPU frame. Cancel undoes this by restoring the baseline
    // values; OK simply flips userOverride + Saves to disk.
    // CRT slider callbacks write LIVE to the CURRENTLY-ACTIVE monitor's
    // crt block. ActiveModeIdx() reads m_state.Prefs().colorMode so
    // every edit lands on whichever monitor type the user has selected
    // in the dropdown. MakeCrtParams picks up the new values on the
    // next frame; Cancel restores the per-block baselines.
    m_displayPage.SetOnBrightnessChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveCrtToOverride();
            auto & blk = m_prefs->crtByMode[ActiveModeIdx()];
            blk.brightness   = pct / 100.0f;     // slider 0..200% -> shader 0..2.0
        }
    });
    m_displayPage.SetOnContrastChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveCrtToOverride();
            auto & blk = m_prefs->crtByMode[ActiveModeIdx()];
            blk.contrast     = pct / 100.0f;
        }
    });
    m_displayPage.SetOnGammaChange ([this] (float g)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveCrtToOverride();
            auto & blk = m_prefs->crtByMode[ActiveModeIdx()];
            blk.gamma        = g;
        }
    });
    m_displayPage.SetOnPersistenceChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            PromoteActiveCrtToOverride();
            auto & blk = m_prefs->crtByMode[ActiveModeIdx()];
            blk.persistence  = pct / 100.0f;
        }
    });
    m_displayPage.SetOnMonitorChange ([this] (int idx)
    {
        if (m_emuShell != nullptr)
        {
            m_emuShell->SetColorModeLive (idx);
        }
        // After the monitor switch, push the new monitor's CRT block
        // into the slider widgets so the user sees its current values
        // (either their previous user override or, for an untouched
        // monitor, the preset's defaults projected through MakeCrtParams).
        ReseedDisplayCrtFromActiveMode();
    });

    // Per-effect toggles and parameter sliders write LIVE to the active
    // monitor's CRT block.
    m_displayPage.SetOnScanlinesEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].scanlinesEnabled  = on; }
    });
    m_displayPage.SetOnScanlinesIntChange ([this] (float pct)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].scanlinesIntensity = pct / 100.0f; }
    });
    m_displayPage.SetOnBloomEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomEnabled       = on; }
    });
    m_displayPage.SetOnBloomRadiusChange ([this] (float px)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomRadius        = px; }
    });
    m_displayPage.SetOnBloomStrengthChange ([this] (float pct)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].bloomStrength      = pct / 100.0f; }
    });
    m_displayPage.SetOnColorBleedEnChange ([this] (bool on)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].colorBleedEnabled  = on; }
    });
    m_displayPage.SetOnColorBleedWChange ([this] (float px)
    {
        if (m_prefs != nullptr) { PromoteActiveCrtToOverride(); m_prefs->crtByMode[ActiveModeIdx()].colorBleedWidth    = px; }
    });
    m_displayPage.SetOnRestoreDefaults ([this] ()
    {
        // Restore Defaults gives the user the RESOLVED defaults
        // (theme override layered on monitor preset) -- the same
        // values the "(theme default)" / "(monitor default)" badges
        // refer to. Anything else creates the confusing experience
        // where Restore moves a slider AWAY from a position the
        // badge had just marked as "default".
        if (m_prefs == nullptr)
        {
            return;
        }

        auto &                     blk           = m_prefs->crtByMode[ActiveModeIdx()];
        const auto &               preset        = CrtPresets::ForMode ((size_t) ActiveModeIdx());
        const ThemeCrtDefaults *   themeDefaults = nullptr;

        if (m_themes != nullptr)
        {
            const LoadedTheme *  active = m_themes->GetActiveTheme();
            if (active != nullptr)
            {
                themeDefaults = &active->crtDefaults;
            }
        }

        blk = preset;
        if (themeDefaults != nullptr)
        {
            if (themeDefaults->hasBrightness) { blk.brightness = themeDefaults->brightness; }
            if (themeDefaults->hasContrast)   { blk.contrast   = themeDefaults->contrast;   }
            if (themeDefaults->hasScanlines)
            {
                blk.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
                blk.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            }
            if (themeDefaults->hasBloom)
            {
                blk.bloomEnabled = themeDefaults->bloomEnabled;
                blk.bloomRadius  = themeDefaults->bloomRadius;
                blk.bloomStrength = themeDefaults->bloomStrength;
            }
            if (themeDefaults->hasColorBleed)
            {
                blk.colorBleedEnabled = themeDefaults->colorBleedEnabled;
                blk.colorBleedWidth   = themeDefaults->colorBleedWidth;
            }
        }
        blk.userOverride = true;

        ReseedDisplayCrtFromActiveMode();
    });
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



    LoadCurrentMachineIntoState();
    PopulateMachineList();
    PopulateThemeList();
    m_pendingMachineSelect.clear();
    m_pendingTheme.clear();

    // Snapshot ALL 4 monitor CRT blocks so Cancel can revert any edits
    // the user made -- including edits to monitors other than the one
    // active at panel open (they may have switched mid-edit).
    if (m_prefs != nullptr)
    {
        for (size_t i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_baselineCrt[i] = m_prefs->crtByMode[i];
        }
    }
    m_baselineColorMode = (int) m_state.Prefs().colorMode;

    // Reset preview state so a previous session's interaction doesn't
    // leak in (e.g. user closed the panel mid-drag via Esc).
    m_previewFocus      = PreviewFocus::None;
    m_previewKeyboard   = false;
    m_lastInteractionMs = 0;
    m_panelAlpha        = 1.0f;
    m_focusedAlpha      = 1.0f;
    m_lastFrameMs       = 0;

    if (m_uiShell != nullptr)
    {
        Layout (m_uiShell->ViewportWidth(), m_uiShell->ViewportHeight(), m_uiShell->Scaler());
    }

    // Rebuild AFTER Layout so widgets that need to be populated by Layout
    // (e.g. Dropdown items) exist before Rebuild calls SetSelected.
    m_machinePage.Rebuild();
    m_hardwarePage.Rebuild();
    m_displayPage.Rebuild();
    ReseedDisplayCrtFromActiveMode();

    m_visible = true;
    RebuildFocusOrder();

    if (m_emuShell != nullptr)
    {
        hwnd = m_emuShell->GetHwnd();
        hr = m_window.Create (hwnd,
                              this,
                              m_emuShell->m_d3dRenderer.GetDevice(),
                              m_emuShell->m_d3dRenderer.GetContext(),
                              &m_emuShell->m_chromeTheme);
        CHRA (hr);
    }

Error:
    if (FAILED (hr))
    {
        m_visible = false;
    }
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFocusOrder
//
//  Re-collect the focus list from the TabStrip, the currently active
//  page, and the Apply / Cancel buttons; reset FocusManager to the
//  first entry so Tab traversal starts at the tab strip.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::RebuildFocusOrder ()
{
    int  nextId = 0;


    m_focusSetters.clear();

    if (m_uiShell == nullptr)
    {
        return;
    }

    m_focusSetters.push_back ([this] (bool f) { m_tabs.SetFocused (f); });

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.CollectFocusables  (m_focusSetters); break;
        case TabIndex::Hardware: m_hardwarePage.CollectFocusables (m_focusSetters); break;
        case TabIndex::Theme:    m_themePage.CollectFocusables    (m_focusSetters); break;
        case TabIndex::Display:  m_displayPage.CollectFocusables  (m_focusSetters); break;
    }

    m_focusSetters.push_back ([this] (bool f) { m_cancelButton.SetFocused (f); });
    m_focusSetters.push_back ([this] (bool f) { m_applyButton.SetFocused  (f); });

    FocusManager & focus = m_uiShell->Focus();

    focus.Clear();
    for (nextId = 0; nextId < (int) m_focusSetters.size(); ++nextId)
    {
        focus.RegisterFocusable (nextId);
    }

    SyncFocusToWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncFocusToWidgets
//
//  Push the FocusManager's current-id state out to all registered
//  widgets via the cached setter callbacks.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::SyncFocusToWidgets ()
{
    int  current = 0;
    int  i       = 0;


    if (m_uiShell == nullptr)
    {
        return;
    }

    current = m_uiShell->Focus().Current();

    for (i = 0; i < (int) m_focusSetters.size(); ++i)
    {
        m_focusSetters[(size_t) i] (i == current);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AnyDropdownOpenOnActivePage
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanel::AnyDropdownOpenOnActivePage () const
{
    if ((TabIndex) m_activeTab == TabIndex::Machine)
    {
        return m_machinePage.AnyDropdownOpen();
    }
    if ((TabIndex) m_activeTab == TabIndex::Theme)
    {
        return m_themePage.AnyDropdownOpen();
    }
    if ((TabIndex) m_activeTab == TabIndex::Display)
    {
        return m_displayPage.AnyDropdownOpen();
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Hide ()
{
    m_visible = false;
    m_scrim.Hide();
    m_window.Destroy();
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



    BAIL_OUT_IF (!m_visible, S_OK);

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
    BAIL_OUT_IF (!m_visible || !m_window.IsOpen() || IsRectEmpty (&emulatorContentScreenRect), S_OK);

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
    return m_visible &&
           (m_previewFocus != PreviewFocus::None) &&
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



    if ((m_previewFocus != PreviewFocus::None) && ((TabIndex) m_activeTab == TabIndex::Display))
    {
        rect = m_displayPage.FocusedControlRect ((int) m_previewFocus);
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



    size.cx = MulDiv (s_kPanelWidthDp,  (int) dpi, (int) DpiScaler::kBaseDpi);
    size.cy = MulDiv (s_kPanelHeightDp, (int) dpi, (int) DpiScaler::kBaseDpi);
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

void SettingsPanel::StartPreview (int focus, bool keyboardMode)
{
    m_previewFocus      = (PreviewFocus) focus;
    m_previewKeyboard   = keyboardMode;
    m_lastInteractionMs = (int64_t) GetTickCount64();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreparePreviewFrame
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PreparePreviewFrame()
{
    if (m_visible && (TabIndex) m_activeTab == TabIndex::Display)
    {
        bool  monitorOpen = m_displayPage.MonitorDropdown().IsOpen();



        if (monitorOpen && m_previewFocus != PreviewFocus::MonitorDropdown)
        {
            StartPreview ((int) PreviewFocus::MonitorDropdown, false);
        }
        else if (!monitorOpen && m_previewFocus == PreviewFocus::MonitorDropdown)
        {
            if (m_emuShell != nullptr)
            {
                int  committed = m_displayPage.MonitorDropdown().SelectedIndex();



                if (committed >= 0)
                {
                    m_emuShell->SetColorModeLive (committed);
                }
            }
            EndPreview();
        }
    }

    UpdatePreviewFade ((int64_t) GetTickCount64());
    m_window.GetRenderer().SetTransparencyState (IsPreviewTransparencyActive(),
                                                 m_emulatorOverlapClientRect,
                                                 GetFocusedControlClientRect());
}




////////////////////////////////////////////////////////////////////////////////
//
//  ActiveModeIdx
//
//  Returns the currently-selected monitor type as an index into
//  GlobalUserPrefs::crtByMode. Reads SettingsPanelState because the
//  monitor dropdown writes there as the source of truth; the live
//  shell state can lag by a frame.
//
////////////////////////////////////////////////////////////////////////////////

int SettingsPanel::ActiveModeIdx () const
{
    int  idx = (int) m_state.Prefs().colorMode;

    if (idx < 0 || idx >= (int) GlobalUserPrefs::kCrtModeCount)
    {
        return 0;
    }
    return idx;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReseedDisplayCrtFromActiveMode
//
//  Pushes the currently-active monitor's CRT block into the DisplayPage
//  sliders. Called at panel Show, after a monitor change, and after
//  "Restore defaults" so the slider widgets reflect whatever
//  MakeCrtParams will produce on the next frame.
//
//  When the active block has userOverride=false we read the resolved
//  preset values (CrtPresets::ForMode) so the user sees "what the
//  defaults are" rather than the still-zero in-struct defaults.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::ReseedDisplayCrtFromActiveMode ()
{
    GlobalUserPrefsCrtSnapshot  snap;
    int                         idx = ActiveModeIdx();

    if (m_prefs == nullptr)
    {
        m_displayPage.SetInitialCrt (snap);
        return;
    }

    const auto &              blk           = m_prefs->crtByMode[idx];
    const auto &              preset        = CrtPresets::ForMode ((size_t) idx);
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    if (blk.userOverride)
    {
        snap.brightness         = blk.brightness;
        snap.contrast           = blk.contrast;
        snap.gamma              = blk.gamma;
        snap.persistence        = blk.persistence;
        snap.scanlinesEnabled   = blk.scanlinesEnabled;
        snap.scanlinesIntensity = blk.scanlinesIntensity;
        snap.bloomEnabled       = blk.bloomEnabled;
        snap.bloomRadius        = blk.bloomRadius;
        snap.bloomStrength      = blk.bloomStrength;
        snap.colorBleedEnabled  = blk.colorBleedEnabled;
        snap.colorBleedWidth    = blk.colorBleedWidth;
    }
    else
    {
        // No user override: mirror MakeCrtParams's resolution chain
        // (preset, with theme overrides on top). Otherwise the sliders
        // would show preset values while the renderer was actually
        // applying theme-overridden values, and the visual would
        // appear to jump the moment the user touched any slider.
        snap.brightness         = preset.brightness;
        snap.contrast           = preset.contrast;
        snap.gamma              = preset.gamma;
        snap.persistence        = preset.persistence;
        snap.scanlinesEnabled   = preset.scanlinesEnabled;
        snap.scanlinesIntensity = preset.scanlinesIntensity;
        snap.bloomEnabled       = preset.bloomEnabled;
        snap.bloomRadius        = preset.bloomRadius;
        snap.bloomStrength      = preset.bloomStrength;
        snap.colorBleedEnabled  = preset.colorBleedEnabled;
        snap.colorBleedWidth    = preset.colorBleedWidth;
        if (themeDefaults != nullptr)
        {
            if (themeDefaults->hasBrightness) { snap.brightness = themeDefaults->brightness; }
            if (themeDefaults->hasContrast)   { snap.contrast   = themeDefaults->contrast;   }
            if (themeDefaults->hasScanlines)
            {
                snap.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
                snap.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            }
            if (themeDefaults->hasBloom)
            {
                snap.bloomEnabled  = themeDefaults->bloomEnabled;
                snap.bloomRadius   = themeDefaults->bloomRadius;
                snap.bloomStrength = themeDefaults->bloomStrength;
            }
            if (themeDefaults->hasColorBleed)
            {
                snap.colorBleedEnabled = themeDefaults->colorBleedEnabled;
                snap.colorBleedWidth   = themeDefaults->colorBleedWidth;
            }
        }
    }
    m_displayPage.SetInitialCrt (snap);

    // Re-publish the per-control defaults hint so DisplayPage knows
    // which value counts as "the default" and can render the
    // (theme default) / (monitor default) badge in each row.
    PublishDisplayDefaultsHint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishDisplayDefaultsHint
//
//  Computes the resolved default value (theme override layered on
//  monitor preset) for each Display-page control and pushes the
//  snapshot to DisplayPage. Theme schema doesn't carry gamma or
//  persistence, so those are always reported as monitor-owned.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PublishDisplayDefaultsHint ()
{
    DisplayDefaultsHint  hint;

    if (m_prefs == nullptr)
    {
        m_displayPage.SetDefaultsHint (hint);
        return;
    }

    int                       idx           = ActiveModeIdx();
    const auto &              preset        = CrtPresets::ForMode ((size_t) idx);
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    // Start from the monitor preset for every field.
    hint.values.brightness         = preset.brightness;
    hint.values.contrast           = preset.contrast;
    hint.values.gamma              = preset.gamma;
    hint.values.persistence        = preset.persistence;
    hint.values.scanlinesEnabled   = preset.scanlinesEnabled;
    hint.values.scanlinesIntensity = preset.scanlinesIntensity;
    hint.values.bloomEnabled       = preset.bloomEnabled;
    hint.values.bloomRadius        = preset.bloomRadius;
    hint.values.bloomStrength      = preset.bloomStrength;
    hint.values.colorBleedEnabled  = preset.colorBleedEnabled;
    hint.values.colorBleedWidth    = preset.colorBleedWidth;

    // Layer theme overrides ONLY for the field-groups the theme
    // actually declares -- otherwise an unset group's struct-default
    // (scanlinesEnabled=false etc.) would silently overwrite the
    // monitor preset's correct value.
    if (themeDefaults != nullptr)
    {
        if (themeDefaults->hasBrightness)
        {
            hint.values.brightness    = themeDefaults->brightness;
            hint.brightnessFromTheme  = true;
        }
        if (themeDefaults->hasContrast)
        {
            hint.values.contrast    = themeDefaults->contrast;
            hint.contrastFromTheme  = true;
        }
        if (themeDefaults->hasScanlines)
        {
            hint.values.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
            hint.values.scanlinesIntensity = themeDefaults->scanlinesIntensity;
            hint.scanlinesFromTheme        = true;
        }
        if (themeDefaults->hasBloom)
        {
            hint.values.bloomEnabled  = themeDefaults->bloomEnabled;
            hint.values.bloomRadius   = themeDefaults->bloomRadius;
            hint.values.bloomStrength = themeDefaults->bloomStrength;
            hint.bloomFromTheme       = true;
        }
        if (themeDefaults->hasColorBleed)
        {
            hint.values.colorBleedEnabled = themeDefaults->colorBleedEnabled;
            hint.values.colorBleedWidth   = themeDefaults->colorBleedWidth;
            hint.colorBleedFromTheme      = true;
        }
    }

    m_displayPage.SetDefaultsHint (hint);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PromoteActiveCrtToOverride
//
//  First time the user touches any CRT control on an untouched monitor,
//  copy the resolved preset values into the active block before flipping
//  userOverride=true. Without this, the slider's lambda writes only the
//  one changed field and leaves every other field at the default-
//  constructed Crt{} zeros (scanlinesEnabled=false, bloomEnabled=false,
//  brightness=1.0, etc.), which silently turns off whatever the preset
//  had enabled the moment userOverride flips on.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PromoteActiveCrtToOverride ()
{
    if (m_prefs == nullptr)
    {
        return;
    }

    auto &  blk = m_prefs->crtByMode[ActiveModeIdx()];

    if (blk.userOverride)
    {
        return;
    }

    // Seed the block with the SAME values MakeCrtParams would produce
    // right now: preset for this monitor, with the active theme's
    // crtDefaults layered on top. Without the theme layer, a clean
    // theme (e.g. contrast=1.0, scanlines off) would silently flip
    // to the raw preset values (contrast=0.9, scanlines on, bloom on)
    // the instant the user nudged any slider, which looks like a bug.
    const auto &              preset        = CrtPresets::ForMode ((size_t) ActiveModeIdx());
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    if (m_themes != nullptr)
    {
        const LoadedTheme *  active = m_themes->GetActiveTheme();
        if (active != nullptr)
        {
            themeDefaults = &active->crtDefaults;
        }
    }

    blk = preset;
    if (themeDefaults != nullptr)
    {
        if (themeDefaults->hasBrightness) { blk.brightness = themeDefaults->brightness; }
        if (themeDefaults->hasContrast)   { blk.contrast   = themeDefaults->contrast;   }
        if (themeDefaults->hasScanlines)
        {
            blk.scanlinesEnabled   = themeDefaults->scanlinesEnabled;
            blk.scanlinesIntensity = themeDefaults->scanlinesIntensity;
        }
        if (themeDefaults->hasBloom)
        {
            blk.bloomEnabled  = themeDefaults->bloomEnabled;
            blk.bloomRadius   = themeDefaults->bloomRadius;
            blk.bloomStrength = themeDefaults->bloomStrength;
        }
        if (themeDefaults->hasColorBleed)
        {
            blk.colorBleedEnabled = themeDefaults->colorBleedEnabled;
            blk.colorBleedWidth   = themeDefaults->colorBleedWidth;
        }
    }
    blk.userOverride = true;
}


void SettingsPanel::EndPreview ()
{
    m_previewFocus      = PreviewFocus::None;
    m_previewKeyboard   = false;
}


void SettingsPanel::UpdatePreviewFade (int64_t nowMs)
{
    constexpr int64_t  s_kKeyboardIdleMs = 500;     // FR-2: 0.5s after last keystroke
    constexpr float    s_kFadeDurationMs = 180.0f;  // panel/scrim fade-in/out time

    float    targetPanel   = 1.0f;
    float    targetFocused = 1.0f;
    int64_t  dtMs          = 0;
    float    maxStep       = 0.0f;



    if (m_lastFrameMs == 0)
    {
        m_lastFrameMs = nowMs;
    }
    dtMs = nowMs - m_lastFrameMs;
    m_lastFrameMs = nowMs;

    // Keyboard idle timeout: auto-end the preview once the user stops
    // tapping arrow keys. Mouse-drag preview ends explicitly on
    // mouse-up via EndPreview so this check is keyboard-only.
    if (m_previewFocus != PreviewFocus::None && m_previewKeyboard &&
        (nowMs - m_lastInteractionMs) >= s_kKeyboardIdleMs)
    {
        EndPreview();
    }

    if (m_previewFocus != PreviewFocus::None)
    {
        targetPanel   = 0.0f;
        targetFocused = 0.9f;
    }

    if (dtMs <= 0 || s_kFadeDurationMs <= 0.0f)
    {
        m_panelAlpha   = targetPanel;
        m_focusedAlpha = targetFocused;
        return;
    }

    // Linear ramp toward target at a rate of 1.0/duration per ms.
    // Lands exactly on the target after the configured fade duration.
    maxStep = (float) dtMs / s_kFadeDurationMs;

    if (targetPanel > m_panelAlpha)
    {
        m_panelAlpha = std::min (targetPanel, m_panelAlpha + maxStep);
    }
    else
    {
        m_panelAlpha = std::max (targetPanel, m_panelAlpha - maxStep);
    }

    if (targetFocused > m_focusedAlpha)
    {
        m_focusedAlpha = std::min (targetFocused, m_focusedAlpha + maxStep);
    }
    else
    {
        m_focusedAlpha = std::max (targetFocused, m_focusedAlpha - maxStep);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadCurrentMachineIntoState
//
//  Re-snapshot the active machine's default JSON + user JSON into
//  `m_state` so the panel always reflects whatever the shell is
//  presently emulating. Failures fall back silently to the prior
//  state (the panel still opens).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::LoadCurrentMachineIntoState ()
{
    std::wstring                   machineNameW;
    std::string                    machineName;
    std::vector<std::filesystem::path>  searchPaths;
    std::filesystem::path           rel;
    std::filesystem::path           configPath;
    std::ifstream                   configFile;
    std::stringstream               ss;
    std::string                     jsonText;
    JsonValue                       defaultJson;
    JsonValue                       mergedJson;
    JsonParseError                  parseErr;
    HRESULT                         hr = S_OK;



    if (m_emuShell == nullptr || m_ucs == nullptr || m_fs == nullptr)
    {
        return;
    }

    machineNameW = m_emuShell->CurrentMachineName();
    machineName.reserve (machineNameW.size());
    for (wchar_t c : machineNameW)
    {
        machineName.push_back ((char) (unsigned char) c);
    }
    if (machineName.empty())
    {
        return;
    }

    searchPaths = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                  PathResolver::GetWorkingDirectory());
    rel         = std::filesystem::path ("Machines") / machineName / (machineName + ".json");
    configPath  = PathResolver::FindFile (searchPaths, rel);
    if (configPath.empty())
    {
        return;
    }

    configFile.open (configPath);
    if (!configFile.good())
    {
        return;
    }
    ss << configFile.rdbuf();
    jsonText = ss.str();

    hr = JsonParser::Parse (jsonText, defaultJson, parseErr);
    if (FAILED (hr))
    {
        return;
    }

    hr = m_ucs->Load (machineName, defaultJson, *m_fs, mergedJson);
    if (FAILED (hr))
    {
        mergedJson = defaultJson;
    }

    hr = m_state.LoadFromMachine (machineName, defaultJson, mergedJson);
    IGNORE_RETURN_VALUE (hr, S_OK);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PopulateMachineList
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PopulateMachineList ()
{
    std::vector<std::filesystem::path>  searchPaths;
    std::vector<MachineInfo>            machinesInfo;
    std::vector<std::string>            machineIds;
    std::vector<std::wstring>           displayNames;
    std::string                         activeMachine;
    int                                 activeIndex = -1;
    int                                 i           = 0;



    if (m_emuShell == nullptr)
    {
        return;
    }

    activeMachine = NarrowMachineName (m_emuShell->CurrentMachineName());
    searchPaths  = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                   PathResolver::GetWorkingDirectory());
    machinesInfo = MachineScanner::Scan (searchPaths,
                                         MachineScanner::ListDirectory,
                                         MachineScanner::ReadFile);

    for (const MachineInfo & info : machinesInfo)
    {
        std::string  machineId   = NarrowMachineName (info.fileName);
        std::wstring displayName = info.displayName.empty() ? std::wstring (info.fileName) : info.displayName;

        if (machineId == activeMachine)
        {
            activeIndex = i;
        }

        machineIds.push_back   (machineId);
        displayNames.push_back (std::move (displayName));
        i++;
    }

    if (machineIds.empty() && !activeMachine.empty())
    {
        machineIds.push_back (activeMachine);
        displayNames.emplace_back (activeMachine.begin(), activeMachine.end());
        activeIndex = 0;
    }

    m_machinePage.SetMachineList (std::move (machineIds), std::move (displayNames), activeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PopulateThemeList
//
//  Walks the ThemeManager's discovered themes, builds parallel id +
//  display-name vectors, and hands them to the theme page so the user
//  can pick from the live catalogue rather than a hardcoded list.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::PopulateThemeList ()
{
    std::vector<std::string>   themeIds;
    std::vector<std::wstring>  displayNames;
    std::string                activeName;
    int                        activeIndex = -1;
    int                        i           = 0;



    if (m_themes == nullptr)
    {
        return;
    }

    activeName = m_themes->GetActiveThemeName();

    for (const LoadedTheme & t : m_themes->GetAvailableThemes())
    {
        if (t.name == activeName)
        {
            activeIndex = i;
        }
        themeIds.push_back (t.name);
        displayNames.emplace_back (t.name.begin(), t.name.end());
        i++;
    }

    if (themeIds.empty() && !activeName.empty())
    {
        themeIds.push_back (activeName);
        displayNames.emplace_back (activeName.begin(), activeName.end());
        activeIndex = 0;
    }

    m_themePage.SetThemes (std::move (themeIds), std::move (displayNames), activeIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnThemeSelected
//
//  Stage the user's theme pick. Like the machine selector, the actual
//  Activate + persist is deferred until OK so Cancel leaves the chrome
//  exactly as the user found it (no resize, no colour change, no drive
//  bar update). CommitApply consumes m_pendingTheme.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnThemeSelected (const std::string & themeName)
{
    if (themeName.empty())
    {
        return;
    }
    m_pendingTheme = themeName;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMachineSelected
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnMachineSelected (const std::string & machineName)
{
    // Stage the user's pick. The actual SwitchMachine (including the
    // possibly-modal ROM bootstrap) is deferred until OK is hit, so
    // the user can still Cancel out without disturbing the running
    // machine. CommitApply calls DoMachineSelect when this differs
    // from the currently-loaded machine.
    if (machineName.empty())
    {
        return;
    }
    m_pendingMachineSelect = machineName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DoMachineSelect
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::DoMachineSelect (const std::string & machineName)
{
    HRESULT           hr          = S_OK;
    std::wstring      wideName (machineName.begin(), machineName.end());
    HINSTANCE         hInstance   = (HINSTANCE) GetModuleHandleW (nullptr);
    HWND              hwndRender  = (m_emuShell != nullptr) ? m_emuShell->m_renderHwnd : nullptr;
    HWND              hwndParent  = (hwndRender != nullptr) ? GetAncestor (hwndRender, GA_ROOT)
                                                            : GetActiveWindow();
    std::vector<fs::path>  searchPaths;
    fs::path          assetBaseDir;
    std::string       bootstrapError;



    if (m_emuShell == nullptr || machineName.empty())
    {
        return;
    }

    // Pre-flight: ensure ROMs for the target machine exist on disk
    // before asking MachineManager::SwitchMachine to load the config.
    // Without this, picking an uninstalled machine throws a "ROM file
    // not found" error dialog. Mirrors the startup flow in Main.cpp.
    searchPaths  = PathResolver::BuildSearchPaths (PathResolver::GetExecutableDirectory(),
                                                   PathResolver::GetWorkingDirectory());
    assetBaseDir = AssetBootstrap::GetAssetBaseDirectory();

    hr = AssetBootstrap::CheckAndFetchRoms (hInstance, wideName, hwndParent,
                                            searchPaths, assetBaseDir, bootstrapError);
    if (hr == S_FALSE)
    {
        // User declined the download; leave the active machine alone.
        return;
    }
    if (FAILED (hr))
    {
        std::wstring  wErr (bootstrapError.begin(), bootstrapError.end());

        MessageBoxW (hwndParent,
                     std::format (L"ROM download failed:\n{}", wErr).c_str(),
                     L"Casso", MB_OK | MB_ICONERROR);
        return;
    }

    // Best-effort: drive audio bootstrap for machines with a Disk II.
    {
        bool     hasDisk     = false;
        std::string  hasDiskErr;
        HRESULT  hrHasDisk   = AssetBootstrap::HasDiskController (hInstance, wideName,
                                                                  hasDisk, hasDiskErr);
        IGNORE_RETURN_VALUE (hrHasDisk, S_OK);

        if (hasDisk)
        {
            fs::path     devicesDir = assetBaseDir / L"Devices" / L"DiskII";
            std::string  diskAudioErr;
            HRESULT      hrAudio    = AssetBootstrap::CheckAndFetchDiskAudio (hInstance, wideName, hwndParent,
                                                                              devicesDir, diskAudioErr);
            IGNORE_RETURN_VALUE (hrAudio, S_OK);
        }
    }

    // SwitchMachine mutates CPU/bus/device state and MUST run on the
    // CPU thread (same as the File > Open Machine menu path). Posting
    // IDM_FILE_OPEN routes through CpuManager's command queue so the
    // teardown/recreate happens between CPU frames with no UI/CPU
    // race. Hide the panel before posting -- we don't try to refresh
    // it in place; reopen if the user wants to tweak more settings.
    m_visible = false;
    m_emuShell->PostCommand (IDM_FILE_OPEN, std::string (machineName));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout
//
//  Fill the popup client area, then lay tabs, active page, modal scrim,
//  and the Apply / Cancel button row.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Layout (int viewportWidthPx, int viewportHeightPx, const DpiScaler & scaler, int topInsetPx)
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
    std::vector<TabStrip::Tab>  tabs;



    m_viewport   = { 0, 0, viewportWidthPx, viewportHeightPx };
    m_panelRect  = MakeRect (left, top, panelWidth, panelHeight);
    m_captionRect = MakeRect (left, top, panelWidth, captionH);

    tabs.push_back ({ MakeRect (left,                  tabsTop, tabWidth, tabHeight), L"Machine"  });
    tabs.push_back ({ MakeRect (left + tabWidth,       tabsTop, tabWidth, tabHeight), L"Hardware" });
    tabs.push_back ({ MakeRect (left + tabWidth * 2,   tabsTop, tabWidth, tabHeight), L"Theme"    });
    tabs.push_back ({ MakeRect (left + tabWidth * 3,   tabsTop, tabWidth, tabHeight), L"Display"  });
    m_tabs.SetTabs (std::move (tabs));
    m_tabs.SetSelected (m_activeTab);
    m_tabs.SetOnChange ([this] (int idx) { m_activeTab = idx; RebuildFocusOrder(); });
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Paint (DxUiPainter & painter, DwriteTextRenderer & text)
{
    ChromeTheme  theme            = (m_uiShell != nullptr) ? m_uiShell->Theme() : ChromeTheme::Skeuomorphic();
    float        edgeThick        = (m_uiShell != nullptr) ? m_uiShell->Scaler().Pxf (s_kEdgeThickDp)
                                                           : s_kEdgeThickDp;
    float        panelA           = 1.0f;
    float        focusedA         = 1.0f;
    int          focusedControlId = (m_previewFocus == PreviewFocus::None) ? -1 : (int) m_previewFocus;



    if (!m_visible)
    {
        return;
    }

    painter.SetGlobalAlpha (panelA);
    text.SetGlobalAlpha    (panelA);

    painter.FillRect    ((float) m_panelRect.left,
                          (float) m_panelRect.top,
                          (float) (m_panelRect.right  - m_panelRect.left),
                          (float) (m_panelRect.bottom - m_panelRect.top),
                          s_kPanelBgArgb);
    painter.OutlineRect ((float) m_panelRect.left,
                          (float) m_panelRect.top,
                          (float) (m_panelRect.right  - m_panelRect.left),
                          (float) (m_panelRect.bottom - m_panelRect.top),
                          edgeThick, s_kPanelEdgeArgb);

    m_tabs.Paint  (painter, text);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.Paint  (painter, text); break;
        case TabIndex::Hardware: m_hardwarePage.Paint (painter, text); break;
        case TabIndex::Theme:    m_themePage.Paint    (painter, text); break;
        case TabIndex::Display:
            // DisplayPage paints its own controls at per-control alpha.
            // It restores global alpha to 1.0 on exit; re-clamp so the
            // buttons inherit the panel alpha consistently.
            m_displayPage.Paint  (painter, text, focusedControlId, panelA, focusedA);
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
    if (!m_visible)
    {
        return;
    }

    m_tabs.SetMouseHover (x, y);
    m_applyButton.SetMouse  (x, y, false);
    m_cancelButton.SetMouse (x, y, false);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.OnMouseHover  (x, y); break;
        case TabIndex::Hardware: m_hardwarePage.OnMouseHover (x, y); break;
        case TabIndex::Theme:    m_themePage.OnMouseHover    (x, y); break;
        case TabIndex::Display:
            m_displayPage.OnMouseHover (x, y);
            m_displayPage.OnMouseMove  (x, y);   // slider drag tracking
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
    if (!m_visible)
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
        case TabIndex::Machine:  m_machinePage.OnLButtonDown  (x, y); break;
        case TabIndex::Hardware: m_hardwarePage.OnLButtonDown (x, y); break;
        case TabIndex::Theme:    m_themePage.OnLButtonDown    (x, y); break;
        case TabIndex::Display:  m_displayPage.OnLButtonDown  (x, y); break;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnLButtonUp (int x, int y)
{
    if (!m_visible)
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
            case TabIndex::Machine:  m_machinePage.OnLButtonUp  (x, y); break;
            case TabIndex::Hardware: m_hardwarePage.OnLButtonUp (x, y); break;
            case TabIndex::Theme:    m_themePage.OnLButtonUp    (x, y); break;
            case TabIndex::Display:  m_displayPage.OnLButtonUp  (x, y); break;
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


    if (!m_visible)
    {
        return false;
    }

    if (m_scrim.IsVisible())
    {
        return m_scrim.OnKey (vk);
    }

    // Open dropdowns take priority — Up/Down/Enter/Esc steer the menu
    // rather than the panel-level focus list.
    if (AnyDropdownOpenOnActivePage())
    {
        switch ((TabIndex) m_activeTab)
        {
            case TabIndex::Machine:  if (m_machinePage.OnKey (vk)) { return true; } break;
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
        RebuildFocusOrder();
        return true;
    }

    if (vk == VK_TAB && m_uiShell != nullptr)
    {
        FocusKey  fk = shiftHeld ? FocusKey::ShiftTab : FocusKey::Tab;

        if (m_uiShell->Focus().HandleKey (fk))
        {
            SyncFocusToWidgets();
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
        case TabIndex::Machine:  if (m_machinePage.OnKey  (vk)) { return true; } break;
        case TabIndex::Hardware: if (m_hardwarePage.OnKey (vk)) { return true; } break;
        case TabIndex::Theme:    if (m_themePage.OnKey    (vk)) { return true; } break;
        case TabIndex::Display:  if (m_displayPage.OnKey  (vk)) { return true; } break;
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
    if (m_state.RequiresReset())
    {
        m_scrim.Show ([this] { CommitApply(); },
                      [this] { /* cancel — keep panel open, no commit */ });
        return;
    }

    CommitApply();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommitApply
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::CommitApply ()
{
    SettingsApplyAdapter  adapter (*m_emuShell);
    JsonValue             currentJson;
    HRESULT               hr             = S_OK;
    std::string           pendingMachine;
    std::wstring          currentMachine;
    std::string           currentMachineNarrow;



    hr = m_state.Apply (adapter, currentJson);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (m_ucs != nullptr && m_fs != nullptr && !m_state.MachineName().empty())
    {
        // BuildJson rooted at the merged JSON includes the canonical
        // version stamp; SaveDelta diffs against the embedded default
        // so only user-changed keys persist.
        hr = m_ucs->SaveDelta (m_state.MachineName(),
                                currentJson,
                                m_state.DefaultJson(),
                                *m_fs);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    pendingMachine.swap (m_pendingMachineSelect);

    // CRT sliders were already mutating the active monitor block live;
    // CommitApply diffs ALL monitor blocks (the user may have edited
    // multiple before clicking OK) and saves on any change. Single
    // Save call covers every block since GlobalUserPrefs writes the
    // whole file atomically.
    if (m_prefs != nullptr)
    {
        bool  anyCrtChanged = false;
        size_t  i = 0;

        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            const auto &  cur = m_prefs->crtByMode[i];
            const auto &  base = m_baselineCrt[i];

            if (cur.brightness         != base.brightness         ||
                cur.contrast           != base.contrast           ||
                cur.gamma              != base.gamma              ||
                cur.persistence        != base.persistence        ||
                cur.scanlinesEnabled   != base.scanlinesEnabled   ||
                cur.scanlinesIntensity != base.scanlinesIntensity ||
                cur.bloomEnabled       != base.bloomEnabled       ||
                cur.bloomRadius        != base.bloomRadius        ||
                cur.bloomStrength      != base.bloomStrength      ||
                cur.colorBleedEnabled  != base.colorBleedEnabled  ||
                cur.colorBleedWidth    != base.colorBleedWidth    ||
                cur.userOverride       != base.userOverride)
            {
                anyCrtChanged = true;
            }
        }

        if (m_emuShell != nullptr && anyCrtChanged)
        {
            HRESULT  hrSave = S_OK;

            if (m_ucs != nullptr)
            {
                hrSave = m_ucs->SaveAll (*m_prefs, *m_fs);
            }
            else
            {
                hrSave = m_prefs->Save (m_emuShell->AssetBaseDir(), *m_fs);
            }
            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }

        // Re-snapshot baselines so subsequent Cancel after another
        // round of edits reverts to THIS committed state, not the
        // pre-commit one.
        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_baselineCrt[i] = m_prefs->crtByMode[i];
        }
    }
    m_baselineColorMode = (int) m_state.Prefs().colorMode;

    // Apply the staged theme BEFORE any machine switch so the chrome
    // is already in its final geometry when SwitchMachine triggers a
    // resize / repaint cascade. Theme apply is idempotent when the
    // staged value matches the active theme, so the typical no-change
    // path costs nothing.
    if (!m_pendingTheme.empty() && m_emuShell != nullptr)
    {
        HRESULT  hrTheme = m_emuShell->ApplyAndPersistTheme (m_pendingTheme);

        IGNORE_RETURN_VALUE (hrTheme, S_OK);
        m_window.SetTheme (&m_emuShell->m_chromeTheme);
        m_pendingTheme.clear();
    }

    if (m_emuShell != nullptr)
    {
        currentMachine = m_emuShell->CurrentMachineName();
        currentMachineNarrow.reserve (currentMachine.size());
        for (wchar_t c : currentMachine)
        {
            currentMachineNarrow.push_back ((char) (unsigned char) c);
        }
    }

    // DoMachineSelect handles the ROM bootstrap modal + posts the
    // SwitchMachine command to the CPU thread. Either an explicit
    // machine change OR a hardware-reset-requiring edit drives a
    // full switch; pendingMachine wins because it's the user's
    // explicit choice.
    if (!pendingMachine.empty() && pendingMachine != currentMachineNarrow)
    {
        DoMachineSelect (pendingMachine);
    }
    else if (adapter.ResetQueued() && m_emuShell != nullptr && !currentMachineNarrow.empty())
    {
        DoMachineSelect (currentMachineNarrow);
    }

    m_visible = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnCancelClicked
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::OnCancelClicked ()
{
    m_pendingMachineSelect.clear();
    m_pendingTheme.clear();

    // Roll back live-preview edits across every monitor block. The
    // shader picks the restored values up on the next frame via the
    // per-frame MakeCrtParams path.
    if (m_prefs != nullptr)
    {
        for (size_t i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            m_prefs->crtByMode[i] = m_baselineCrt[i];
        }
    }
    if (m_emuShell != nullptr && m_baselineColorMode >= 0)
    {
        m_emuShell->SetColorModeLive (m_baselineColorMode);
    }

    m_previewFocus      = PreviewFocus::None;
    m_previewKeyboard   = false;
    m_panelAlpha        = 1.0f;
    m_focusedAlpha      = 1.0f;

    m_state.Cancel();
    m_visible = false;
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
