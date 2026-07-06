#include "Pch.h"

#include "SettingsSheet.h"

#include "../../EmulatorShell.h"
#include "Ui/Chrome/ChromeMetrics.h"


static constexpr int  s_kSheetWidthDip     = 724;
static constexpr int  s_kSheetHeightDip    = 760;




////////////////////////////////////////////////////////////////////////////////
//
//  OnBuildPages
//
//  DxuiPropertySheet hook (fires inside DxuiWindow::Create). Creates the
//  four setting pages as tab pages; the base then builds the tab strip
//  from their titles and the OK / Cancel row.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::OnBuildPages ()
{
    m_machinePage  = CreatePage<MachinePage>  (L"Machine");
    m_hardwarePage = CreatePage<HardwarePage> (L"Hardware");
    m_themePage    = CreatePage<ThemePage>    (L"Theme");
    m_displayPage  = CreatePage<DisplayPage>  (L"Display");
}




////////////////////////////////////////////////////////////////////////////////
//
//  OpenModal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsSheet::OpenModal (
    HINSTANCE         hInstance,
    HWND              ownerHwnd,
    UserConfigStore & ucs,
    GlobalUserPrefs & prefs,
    ThemeManager    & themes,
    EmulatorShell   & emuShell,
    IFileSystem     & fs)
{
    HRESULT                   hr = S_OK;
    DxuiWindow::CreateParams  params;


    m_ucs      = &ucs;
    m_prefs    = &prefs;
    m_themes   = &themes;
    m_emuShell = &emuShell;
    m_fs       = &fs;

    // No Apply button. Set BEFORE Create so OnCreate honors the hidden Apply.
    // (The "OK (reboot)" relabel + its widened width land together in 3b.)
    SetApplyVisible (false);

    params.title                    = L"Settings";
    params.hInstance                = hInstance;
    params.ownerHwnd                = ownerHwnd;
    params.initialSizeDip           = { s_kSheetWidthDip, s_kSheetHeightDip };
    params.minSizeDip               = { s_kSheetWidthDip, s_kSheetHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;   // tab strip sits below the caption
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;

    hr = DxuiWindow::Create (params);   // fires OnBuildPages + base OnCreate
    CHRA (hr);

    SetTheme (&emuShell.m_chromeTheme);

    // Wire the pages against a fresh SettingsPanelState + machine/theme
    // catalog (the same objects the legacy SettingsPanel owns).
    m_catalog.Bind (&emuShell, &ucs, &prefs, &fs, &themes, &m_state,
                    m_machinePage, m_themePage);
    m_machinePage->SetState  (&m_state);
    m_hardwarePage->SetState (&m_state);
    m_displayPage->SetState  (&m_state);

    // Per-monitor CRT plumbing for the Display page. Bind funnels the slider /
    // toggle / monitor / restore-defaults edits through the crtByMode block so
    // the shader picks them up next frame; ReseedFromActiveMode (after the
    // Rebuild below) seeds the widgets from the active mode so the sliders
    // show real values instead of sitting zeroed at the left.
    m_crt.Bind (&prefs, &themes, &m_state, m_displayPage, &emuShell);
    m_crt.WireDisplayPageCallbacks();

    // Live framebuffer + mounted-path sources for the Theme preview. The page
    // paints inside chrome composition after the current frame is uploaded, so
    // the CPU-side buffer is always one frame fresh.
    m_themePage->SetFramebufferSource ([this] (int & outW, int & outH) -> const uint32_t *
    {
        outW = ChromeMetrics::kFramebufferWidthPx;
        outH = ChromeMetrics::kFramebufferHeightPx;
        return m_emuShell->UiFramebufferPixels();
    });
    m_themePage->SetMountedPathSource ([this] (int driveIndex) -> std::wstring
    {
        return m_emuShell->MountedImagePath (driveIndex);
    });

    // Route each page's dropdown menus through the host popup pool so they
    // escape the client clip (FR-054 / FR-061).
    m_machinePage->SetPopupHost (PopupHost());
    m_themePage->SetPopupHost   (PopupHost());
    m_displayPage->SetPopupHost (PopupHost());

    // Pull the running machine + discovered themes into the pages.
    m_catalog.LoadCurrentMachineIntoState();
    m_catalog.PopulateMachineList();
    m_catalog.PopulateThemeList();

    // Show the window BEFORE the final Rebuild: the first valid layout only
    // happens once the window is shown (WM_SIZE against the real client size),
    // and HardwarePage's tree view (plus any content that flows its rows at
    // Rebuild time) needs real bounds to flow into, else it collapses to the
    // top-left. This mirrors the legacy SettingsPanel::Show order (Layout,
    // then Rebuild). ShowModalDialog re-shows harmlessly below.
    Show();
    m_machinePage->Rebuild();
    m_hardwarePage->Rebuild();
    m_displayPage->Rebuild();
    m_crt.ReseedFromActiveMode();   // seed Display sliders from the active mode

    (void) ShowModalDialog (IDOK);

Error:
    return hr;
}
