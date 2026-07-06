#include "Pch.h"

#include "SettingsSheet.h"

#include "../../EmulatorShell.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "resource.h"

#include <cmath>
#include <cstdio>


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
    SetApplyVisible (false);

    // Widen OK up front to fit the longest label ("OK (reboot)"); the row is
    // sized once at layout and does not reflow, so RefreshOkLabel only swaps
    // the text (repaint) between "OK" and "OK (reboot)" (FR-131).
    SetOkWidthDip (132);

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
    m_apply.Bind (&m_state, &ucs, &prefs, &fs, &emuShell,
                  [this] () { SetTheme (&m_emuShell->m_chromeTheme); },
                  &m_catalog);
    m_machinePage->SetState  (&m_state);
    m_hardwarePage->SetState (&m_state);
    m_displayPage->SetState  (&m_state);

    // Machine page play (>) buttons audition the drive sounds live.
    m_machinePage->SetOnTestSound ([this] (int drive, int kind, bool centered)
    {
        AuditionDriveSound (drive, kind, centered);
    });

    // Staged picks: the machine + theme selectors defer their real apply to
    // OK (CommitApply) so Cancel leaves the running machine / chrome as found.
    m_machinePage->SetOnMachineSelected ([this] (const std::string & name)
    {
        if (!name.empty()) { m_apply.StagePendingMachine (name); }
        RefreshOkLabel();
    });
    m_themePage->SetOnThemeSelected ([this] (const std::string & name)
    {
        if (!name.empty()) { m_apply.StagePendingTheme (name); }
    });
    // FR-132: "Apply now" reskins the real chrome immediately (still staged
    // for OK; a later Cancel reverts to the theme active at open).
    m_themePage->SetOnApplyThemeNow ([this] ()
    {
        m_apply.ApplyThemeLive (m_themePage->SelectedThemeId());
    });

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

    // Capture the CRT / color / theme baseline so OnCancel can revert any
    // live-preview edits (mirrors SettingsPanel::Show).
    m_apply.SnapshotBaselines();

    (void) ShowModalDialog (IDOK);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnOk / OnCancel
//
//  Commit hooks from DxuiPropertySheet's button row. OnOk runs the apply
//  controller's full commit pipeline then closes (return S_OK); OnCancel
//  rolls back live-preview CRT edits + an "Apply now" theme, then the base
//  closes with IDCANCEL.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsSheet::OnOk ()
{
    m_apply.CommitApply();
    return S_OK;
}


void SettingsSheet::OnCancel ()
{
    m_apply.Cancel (m_preview);
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnDialogTick / RefreshOkLabel
//
//  The reboot state can change from either the Machine dropdown or a Hardware
//  edit; re-evaluating each dialog tick keeps the OK label current without
//  wiring every hardware control. SetOkText is a cheap no-op repaint when the
//  label is unchanged.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::OnDialogTick ()
{
    RefreshOkLabel();
}


void SettingsSheet::RefreshOkLabel ()
{
    bool          reboot = m_apply.WillMachineChange() || m_apply.IsResetRequired();
    std::wstring  want   = reboot ? L"OK (reboot)" : L"OK";

    if (OkText() != want)   // only repaint on an actual change, not every tick
    {
        SetOkText (std::move (want));
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  AuditionDriveSound
//
//  Machine page play (>) button handler. Pushes the current drive-audio
//  settings to the engine and fires a one-shot test of the given sound.
//  Ported from SettingsPanel; the m_driveAuditionDirty cancel-revert flag
//  moves in with the commit controller in a later slice.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::AuditionDriveSound (int drive, int kind, bool centered)
{
    char   test[16] = {};
    float  pan0     = 0.0f;
    float  pan1     = 0.0f;


    if (m_emuShell == nullptr)
    {
        return;
    }

    const SettingsUiPrefs &  prefs = m_state.Prefs();

    pan0 = prefs.driveOnePan;
    pan1 = prefs.driveTwoPan;
    if (centered)
    {
        if (drive == 0) { pan0 = 0.0f; }
        else            { pan1 = 0.0f; }
    }

    PushDriveAudioToEngine (prefs.driveMotorVolume,
                            prefs.driveHeadVolume,
                            prefs.driveDoorVolume,
                            pan0,
                            pan1,
                            prefs.floppyMechanism);

    sprintf_s (test, "%d,%d", drive, kind);
    m_emuShell->PostCommand (IDM_AUDIO_DRIVE_TEST, test);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PushDriveAudioToEngine
//
//  Posts volumes + pan + mechanism to the engine command queue. The
//  mechanism is reloaded only when it differs from what the engine last
//  loaded, avoiding a redundant WAV reload.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::PushDriveAudioToEngine (
    float                motor,
    float                head,
    float                door,
    float                pan0,
    float                pan1,
    const std::string  & mechanism)
{
    char  vol[32] = {};
    char  pan[32] = {};


    if (m_emuShell == nullptr)
    {
        return;
    }

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
}
