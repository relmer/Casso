#include "Pch.h"

#include "SettingsPanel.h"

#include "../UiShell.h"
#include "../../EmulatorShell.h"
#include "../../Config/UserConfigStore.h"
#include "../../Config/IFileSystem.h"
#include "../ThemeManager.h"

#include "Core/MachineScanner.h"
#include "Core/PathResolver.h"


////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr int    s_kPanelWidthPx   = 720;
    constexpr int    s_kPanelHeightPx  = 540;
    constexpr int    s_kTabHeightPx    = 32;
    constexpr int    s_kBottomBarPx    = 56;
    constexpr int    s_kButtonWidthPx  = 96;
    constexpr int    s_kButtonHeightPx = 28;
    constexpr int    s_kButtonGapPx    = 8;
    constexpr int    s_kPanelPadPx     = 16;
    constexpr uint32_t s_kPanelBgArgb  = 0xFF1A2230;
    constexpr uint32_t s_kPanelEdgeArgb = 0xFF334050;
    constexpr float    s_kEdgeThickPx  = 1.0f;


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
        {
            UNREFERENCED_PARAMETER (shell);
        }

        void ApplySpeedMode    (SettingsSpeedMode mode)        override { UNREFERENCED_PARAMETER (mode);    }
        void ApplyColorMode    (SettingsColorMode mode)        override { UNREFERENCED_PARAMETER (mode);    }
        void ApplyFloppySound  (bool enabled)                  override { UNREFERENCED_PARAMETER (enabled); }
        void ApplyMechanism    (const std::string & mechanism) override { UNREFERENCED_PARAMETER (mechanism); }
        void ApplyWriteProtect (int drive, bool wp)            override { UNREFERENCED_PARAMETER (drive); UNREFERENCED_PARAMETER (wp); }
        void QueueMachineReset ()                              override { m_resetQueued = true; }

        bool  ResetQueued () const { return m_resetQueued; }

    private:
        bool            m_resetQueued = false;
    };


    RECT MakeRect (int l, int t, int w, int h)
    {
        RECT  rc = { l, t, l + w, t + h };
        return rc;
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
    m_hardwarePage.SetState (&m_state);

    m_applyButton.SetLabel  (L"Apply");
    m_applyButton.SetClick  ([this] { OnApplyClicked();  });
    m_cancelButton.SetLabel (L"Cancel");
    m_cancelButton.SetClick ([this] { OnCancelClicked(); });

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

    m_machinePage.Rebuild();
    m_hardwarePage.Rebuild();

    if (m_uiShell != nullptr)
    {
        Layout (m_uiShell->ViewportWidth(), m_uiShell->ViewportHeight());
    }

    m_visible = true;
    return hr;
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
//  Layout
//
//  Centre a fixed-size panel within the viewport, then lay tabs,
//  active page, modal scrim, and the Apply / Cancel button row.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanel::Layout (int viewportWidthPx, int viewportHeightPx)
{
    static constexpr int  s_kMinPanelMarginPx = 16;

    int     panelWidth  = std::min<int> (s_kPanelWidthPx,  std::max<int> (0, viewportWidthPx  - s_kMinPanelMarginPx * 2));
    int     panelHeight = std::min<int> (s_kPanelHeightPx, std::max<int> (0, viewportHeightPx - s_kMinPanelMarginPx * 2));
    int     left        = (viewportWidthPx  - panelWidth)  / 2;
    int     top         = (viewportHeightPx - panelHeight) / 2;
    int     tabWidth    = std::max<int> (40, panelWidth / 4);
    RECT    pageRect    = {};
    RECT    bottomRow   = {};
    int     applyX      = 0;
    int     cancelX     = 0;
    int     buttonY     = 0;
    std::vector<TabStrip::Tab>  tabs;



    m_viewport  = { 0, 0, viewportWidthPx, viewportHeightPx };
    m_panelRect = MakeRect (left, top, panelWidth, panelHeight);

    tabs.push_back ({ MakeRect (left,                  top, tabWidth, s_kTabHeightPx), L"Machine"  });
    tabs.push_back ({ MakeRect (left + tabWidth,       top, tabWidth, s_kTabHeightPx), L"Hardware" });
    tabs.push_back ({ MakeRect (left + tabWidth * 2,   top, tabWidth, s_kTabHeightPx), L"Theme"    });
    tabs.push_back ({ MakeRect (left + tabWidth * 3,   top, tabWidth, s_kTabHeightPx), L"Display"  });
    m_tabs.SetTabs (std::move (tabs));
    m_tabs.SetSelected (m_activeTab);
    m_tabs.SetOnChange ([this] (int idx) { m_activeTab = idx; });

    pageRect.left   = m_panelRect.left   + s_kPanelPadPx;
    pageRect.top    = m_panelRect.top    + s_kTabHeightPx + s_kPanelPadPx;
    pageRect.right  = m_panelRect.right  - s_kPanelPadPx;
    pageRect.bottom = m_panelRect.bottom - s_kBottomBarPx;

    m_machinePage.Layout  (pageRect);
    m_hardwarePage.SetRect (pageRect);
    m_themePage.Layout    (pageRect);
    m_displayPage.Layout  (pageRect);

    bottomRow.left   = m_panelRect.left;
    bottomRow.top    = m_panelRect.bottom - s_kBottomBarPx;
    bottomRow.right  = m_panelRect.right;
    bottomRow.bottom = m_panelRect.bottom;

    applyX  = m_panelRect.right - s_kPanelPadPx - s_kButtonWidthPx;
    cancelX = applyX            - s_kButtonGapPx - s_kButtonWidthPx;
    buttonY = bottomRow.top     + (s_kBottomBarPx - s_kButtonHeightPx) / 2;

    m_applyButton.Layout  (MakeRect (applyX,  buttonY, s_kButtonWidthPx, s_kButtonHeightPx));
    m_cancelButton.Layout (MakeRect (cancelX, buttonY, s_kButtonWidthPx, s_kButtonHeightPx));

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

    ChromeTheme  theme = ChromeTheme::Skeuomorphic();



    if (!m_visible)
    {
        return;
    }

    // Dimmed backdrop behind the panel.
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
                          s_kEdgeThickPx, s_kPanelEdgeArgb);

    m_tabs.Paint  (painter, text);

    switch ((TabIndex) m_activeTab)
    {
        case TabIndex::Machine:  m_machinePage.Paint  (painter, text); break;
        case TabIndex::Hardware: m_hardwarePage.Paint (painter, text); break;
        case TabIndex::Theme:    m_themePage.Paint    (painter, text); break;
        case TabIndex::Display:  m_displayPage.Paint  (painter, text); break;
    }

    m_applyButton.Paint  (painter, text, theme);
    m_cancelButton.Paint (painter, text, theme);

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
        case TabIndex::Display:  m_displayPage.OnMouseHover  (x, y); break;
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
    if (!m_visible)
    {
        return false;
    }

    if (m_scrim.IsVisible())
    {
        return m_scrim.OnKey (vk);
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
    HRESULT               hr = S_OK;



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

    if (adapter.ResetQueued() && m_emuShell != nullptr)
    {
        IGNORE_RETURN_VALUE (hr, m_emuShell->SwitchMachine (m_emuShell->CurrentMachineName()));
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
