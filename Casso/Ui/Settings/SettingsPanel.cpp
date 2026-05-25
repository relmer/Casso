#include "Pch.h"

#include "SettingsPanel.h"

#include "../UiShell.h"
#include "../../EmulatorShell.h"
#include "../../AssetBootstrap.h"
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
    constexpr int    s_kPanelHeightDp  = 540;
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
    m_displayPage.SetOnBrightnessChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->crt.brightness   = pct / 50.0f;
            m_prefs->crt.userOverride = true;   // bypass theme defaults during live preview
        }
    });
    m_displayPage.SetOnContrastChange ([this] (float pct)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->crt.contrast     = pct / 50.0f;
            m_prefs->crt.userOverride = true;
        }
    });
    m_displayPage.SetOnMonitorChange ([this] (int idx)
    {
        if (m_emuShell != nullptr)
        {
            m_emuShell->SetColorModeLive (idx);
        }
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

    return S_OK;
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
    HRESULT  hr = S_OK;



    LoadCurrentMachineIntoState();
    PopulateMachineList();
    PopulateThemeList();
    m_pendingMachineSelect.clear();
    m_pendingTheme.clear();

    // Snapshot the baseline CRT params + color mode so Cancel can
    // revert. Brightness / contrast values are mutated LIVE on every
    // slider tick (so the user sees the shader respond while the
    // panel is faded out); Cancel writes the baselines back to
    // GlobalUserPrefs.crt so the shader picks them up next frame.
    if (m_prefs != nullptr)
    {
        m_baselineBrightness   = m_prefs->crt.brightness;
        m_baselineContrast     = m_prefs->crt.contrast;
        m_baselineUserOverride = m_prefs->crt.userOverride;
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
    m_displayPage.SetInitialCrt (m_baselineBrightness, m_baselineContrast);

    m_visible = true;
    RebuildFocusOrder();
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  StartPreview / EndPreview / UpdatePreviewFade
//
//  The live-preview state machine. While a slider is being dragged
//  or a dropdown is open, the panel fades out (alpha -> 0) so the
//  user can see the emulator respond to the change; the focused
//  control stays at ~50% alpha so they can still see what they're
//  manipulating. Keyboard-driven changes auto-end the preview 500ms
//  after the last keystroke.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::StartPreview (int focus, bool keyboardMode)
{
    m_previewFocus      = (PreviewFocus) focus;
    m_previewKeyboard   = keyboardMode;
    m_lastInteractionMs = (int64_t) GetTickCount64();
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

    float    targetPanel    = 1.0f;
    float    targetFocused  = 1.0f;
    int64_t  dtMs           = 0;
    float    maxStep        = 0.0f;



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
    // Exponential lerp (`alpha += (target-alpha) * dtFraction`)
    // asymptotes -- it never actually reaches the target -- so the
    // scrim never fully cleared during preview. A clamped linear
    // step guarantees we land on the target after the configured
    // fade duration.
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
    assetBaseDir = AssetBootstrap::GetAssetBaseDirectory (searchPaths,
                                                          PathResolver::GetExecutableDirectory());

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
//  Centre a fixed-size panel within the viewport, then lay tabs,
//  active page, modal scrim, and the Apply / Cancel button row.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Layout (int viewportWidthPx, int viewportHeightPx, const DpiScaler & scaler)
{
    UINT    dpi          = scaler.Dpi();
    int     marginPx     = scaler.Px (s_kPanelMarginDp);
    int     basePanelW   = scaler.Px (s_kPanelWidthDp);
    int     basePanelH   = scaler.Px (s_kPanelHeightDp);
    int     captionH     = scaler.Px (s_kCaptionHeightDp);
    int     tabHeight    = scaler.Px (s_kTabHeightDp);
    int     bottomBar    = scaler.Px (s_kBottomBarDp);
    int     buttonWidth  = scaler.Px (s_kButtonWidthDp);
    int     buttonHeight = scaler.Px (s_kButtonHeightDp);
    int     buttonGap    = scaler.Px (s_kButtonGapDp);
    int     pad          = scaler.Px (s_kPanelPadDp);
    int     panelWidth   = std::min<int> (basePanelW, std::max<int> (0, viewportWidthPx  - marginPx * 2));
    int     panelHeight  = std::min<int> (basePanelH, std::max<int> (0, viewportHeightPx - marginPx * 2));
    int     left         = (viewportWidthPx  - panelWidth)  / 2;
    int     top          = (viewportHeightPx - panelHeight) / 2;
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
    constexpr uint32_t  s_kBackdropArgb = 0x60000000;

    ChromeTheme  theme     = (m_uiShell != nullptr) ? m_uiShell->Theme() : ChromeTheme::Skeuomorphic();
    float        edgeThick = (m_uiShell != nullptr) ? m_uiShell->Scaler().Pxf (s_kEdgeThickDp)
                                                    : s_kEdgeThickDp;



    if (!m_visible)
    {
        return;
    }

    // Detect monitor-dropdown open/close transitions so the preview
    // state fades in/out as the user opens / dismisses it. Driven by
    // polling here rather than dropdown callbacks because Dropdown
    // doesn't expose OnOpen / OnClose hooks today.
    {
        bool  monitorOpen = m_displayPage.MonitorDropdown().IsOpen();

        if (monitorOpen && m_previewFocus != PreviewFocus::MonitorDropdown)
        {
            StartPreview ((int) PreviewFocus::MonitorDropdown, false);
        }
        else if (!monitorOpen && m_previewFocus == PreviewFocus::MonitorDropdown)
        {
            // Dropdown was open and is now closed. If the user
            // committed an item (mouse click / Enter), the live
            // colour mode already matches the dropdown's selection
            // -- this push is a no-op. If they Escaped or clicked
            // outside, the live colour mode is whatever they last
            // hovered through the highlight channel; we revert it
            // to the committed selection so the emulator goes back
            // to the user's actual pick.
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

    float  panelA   = m_panelAlpha;
    float  focusedA = m_focusedAlpha;
    int    focusedControlId = (m_previewFocus == PreviewFocus::None) ? -1 : (int) m_previewFocus;

    // ----- Dimmed backdrop behind the panel (fades with the panel). -----
    painter.SetGlobalAlpha (panelA);
    text.SetGlobalAlpha    (panelA);
    painter.FillRect ((float) m_viewport.left,
                      (float) m_viewport.top,
                      (float) (m_viewport.right  - m_viewport.left),
                      (float) (m_viewport.bottom - m_viewport.top),
                      s_kBackdropArgb);

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

    // Caption bar.
    {
        HRESULT  hrCaption  = S_OK;
        float    captionFont = (m_uiShell != nullptr) ? m_uiShell->Scaler().Pxf (s_kCaptionFontDp)
                                                      : s_kCaptionFontDp;

        painter.FillRect ((float) m_captionRect.left,
                          (float) m_captionRect.top,
                          (float) (m_captionRect.right  - m_captionRect.left),
                          (float) (m_captionRect.bottom - m_captionRect.top),
                          s_kCaptionBgArgb);
        IGNORE_RETURN_VALUE (hrCaption, text.DrawString (L"Settings",
                                                         (float) m_captionRect.left + 12.0f,
                                                         (float) m_captionRect.top,
                                                         (float) (m_captionRect.right  - m_captionRect.left) - 24.0f,
                                                         (float) (m_captionRect.bottom - m_captionRect.top),
                                                         s_kCaptionTextArgb,
                                                         captionFont,
                                                         L"Segoe UI",
                                                         DwriteTextRenderer::HAlign::Left,
                                                         DwriteTextRenderer::VAlign::Center));
    }

    m_tabs.Paint  (painter, text);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.Paint  (painter, text); break;
        case TabIndex::Hardware: m_hardwarePage.Paint (painter, text); break;
        case TabIndex::Theme:    m_themePage.Paint    (painter, text); break;
        case TabIndex::Display:
            // DisplayPage paints its own controls at per-control alpha
            // (focused vs non-focused). It restores global alpha to 1.0
            // on exit; we re-clamp to panelA below so the buttons keep
            // honouring the panel-fade.
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
        // so only user-changed keys persist to <Machine>_user.json.
        hr = m_ucs->SaveDelta (m_state.MachineName(),
                                currentJson,
                                currentJson,   // default fallback to current when caller lacks it
                                *m_fs);
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    pendingMachine.swap (m_pendingMachineSelect);

    // CRT brightness / contrast were already written live to
    // GlobalUserPrefs.crt by the slider callbacks; CommitApply just
    // flips userOverride (so theme defaults stop applying) and Saves
    // to disk when the values changed from the baseline.
    if (m_prefs != nullptr)
    {
        bool  brightnessChanged = (m_prefs->crt.brightness != m_baselineBrightness);
        bool  contrastChanged   = (m_prefs->crt.contrast   != m_baselineContrast);

        if (brightnessChanged || contrastChanged)
        {
            m_prefs->crt.userOverride = true;
        }

        if (m_emuShell != nullptr && (brightnessChanged || contrastChanged))
        {
            HRESULT  hrSave = m_prefs->Save (m_emuShell->AssetBaseDir(), *m_fs);

            IGNORE_RETURN_VALUE (hrSave, S_OK);
        }

        m_baselineBrightness = m_prefs->crt.brightness;
        m_baselineContrast   = m_prefs->crt.contrast;
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

    // Roll back any live-preview CRT changes so the emulator
    // immediately reverts to its pre-panel state. The shader picks the
    // restored values up on the next frame via the existing per-frame
    // MakeCrtParams path.
    if (m_prefs != nullptr)
    {
        m_prefs->crt.brightness   = m_baselineBrightness;
        m_prefs->crt.contrast     = m_baselineContrast;
        m_prefs->crt.userOverride = m_baselineUserOverride;
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
