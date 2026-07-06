#include "Pch.h"

#include "SettingsSheet.h"

#include "../../EmulatorShell.h"
#include "../../Config/GlobalUserPrefs.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiButtonRow.h"
#include "resource.h"

#include <cmath>
#include <cstdio>


static constexpr int    s_kSheetWidthDip     = 724;
static constexpr int    s_kSheetHeightDip    = 760;

// Window opacity while a Display control is being dragged / keyboard-edited, so
// the live emulator behind the sheet shows through (list #7). The CRT edit is
// already applied live by SettingsDisplayCrtBridge; this just reveals it.
static constexpr float  s_kPreviewOpacity    = 0.4f;




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

    // Amber "press OK to reboot" notice that fills the bottom-bar space left of
    // the OK / Cancel buttons whenever committing would power-cycle the machine
    // (FR-131). Hidden until UpdateRestartNotice (each dialog tick) turns it on.
    m_restartNotice = CreateChild<DxuiLabel> ();
    m_restartNotice->SetColor     (0xFFF0A030);   // amber caution
    m_restartNotice->SetTextAlign (DxuiTextHAlign::Left, DxuiTextVAlign::Center);
    m_restartNotice->SetVisible   (false);
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

    // OK stays the standard command-button width (matching Cancel) until a
    // pending reboot relabels it "OK (reboot)"; RefreshOkLabel widens it then
    // and narrows it back on revert (FR-131), so it is never wider than Cancel
    // while it just reads "OK".

    params.title                    = L"Settings";
    params.hInstance                = hInstance;
    params.ownerHwnd                = ownerHwnd;
    params.initialSizeDip           = { s_kSheetWidthDip, s_kSheetHeightDip };
    params.minSizeDip               = { s_kSheetWidthDip, s_kSheetHeightDip };
    params.resizable                = true;
    params.insetContentBelowCaption = true;   // tab strip sits below the caption
    params.captionStyle             = DxuiCaptionStyle::CloseOnly;
    params.composited               = true;   // enables the live-preview fade (#7)

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

    // Live preview (#7): dragging / keyboard-editing a Display control fades
    // the composited sheet so the running emulator shows through and the CRT
    // edit is visible. Mouse gives a clean start/end; keyboard fires start
    // only, so the preview controller's idle timeout (advanced from
    // OnDialogTick) restores opacity a moment after the last keystroke.
    m_displayPage->SetOnPreview ([this] (int controlId, bool start, bool keyboardMode)
    {
        (void) controlId;   // window-level fade is global; only active-state matters
        if (start)
        {
            m_preview.StartPreview (SettingsPreviewController::Focus::BrightnessSlider, keyboardMode);
            SetComposedOpacity (s_kPreviewOpacity);
            m_previewActive = true;
        }
        else
        {
            m_preview.EndPreview();
            SetComposedOpacity (1.0f);
            m_previewActive = false;
        }
    });

    // Per-monitor CRT plumbing for the Display page. Bind funnels the slider /
    // toggle / monitor / restore-defaults edits through the crtByMode block so
    // the shader picks them up next frame; ReseedFromActiveMode (after the
    // Rebuild below) seeds the widgets from the active mode so the sliders
    // show real values instead of sitting zeroed at the left.
    m_crt.Bind (&prefs, &themes, &m_state, m_displayPage, &emuShell);
    m_crt.WireDisplayPageCallbacks();

    // Text color (#8): a mode change (White / Green / Amber / Custom) applies
    // live and stages the pref; committing to Custom opens the HSV picker,
    // hosted as this window's modal overlay. Cancel of the whole sheet still
    // reverts the text-color choice to baseline via the apply controller.
    m_displayPage->SetOnTextColorChange ([this] (int idx)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->colorMonitorTextMode = (ColorMonitorTextMode) idx;
            if (m_emuShell != nullptr)
            {
                m_emuShell->SetColorMonitorTextArgbLive (
                    ColorUtil::ResolveColorMonitorTextArgb (m_prefs->colorMonitorTextMode,
                                                            m_prefs->colorMonitorTextCustomArgb));
            }
        }
    });
    m_displayPage->SetOnTextColorCommit ([this] (int idx)
    {
        if (m_prefs != nullptr && (ColorMonitorTextMode) idx == ColorMonitorTextMode::Custom)
        {
            m_colorPicker.SetHwnd (Hwnd());
            m_colorPicker.Open (m_prefs->colorMonitorTextCustomArgb);
            Invalidate();
        }
    });
    m_colorPicker.SetOnChange ([this] (uint32_t argb)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->colorMonitorTextCustomArgb = argb;
            m_prefs->colorMonitorTextMode       = ColorMonitorTextMode::Custom;
            m_displayPage->SetTextColor (ColorMonitorTextMode::Custom, argb);
            if (m_emuShell != nullptr)
            {
                m_emuShell->SetColorMonitorTextArgbLive (argb);
            }
        }
    });
    m_colorPicker.SetOnClose ([this] (bool /*accepted*/, uint32_t argb)
    {
        if (m_prefs != nullptr)
        {
            m_prefs->colorMonitorTextCustomArgb = argb;
            m_displayPage->SetTextColor (ColorMonitorTextMode::Custom, argb);
            if (m_emuShell != nullptr)
            {
                m_emuShell->SetColorMonitorTextArgbLive (argb);
            }
        }
        Invalidate();
    });

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
    UpdateRestartNotice();

    // Advance the preview state machine so a keyboard-driven preview idles out;
    // restore full opacity when it does (mouse drags restore in OnPreview end).
    m_preview.Tick ((int64_t) GetTickCount64());
    if (m_previewActive && !m_preview.IsActive())
    {
        SetComposedOpacity (1.0f);
        m_previewActive = false;
    }
}


void SettingsSheet::RefreshOkLabel ()
{
    bool          reboot = m_apply.WillMachineChange() || m_apply.IsResetRequired();
    std::wstring  want   = reboot ? L"OK (reboot)" : L"OK";

    if (OkText() != want)   // only reflow on an actual change, not every tick
    {
        SetOkText (std::move (want));
        SetOkWidthDip (reboot ? 132 : 0);   // 0 => standard width, == Cancel
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Layout / modal-overlay overrides (custom text-color picker, list #8)
//
//  The picker is centred in the same sheet bounds the pages use, painted last
//  of all, and -- while open -- grabs every mouse / key / char event so the
//  page beneath stays inert. Each routed event invalidates so the picker's
//  sliders / hex field / copy flash animate.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    DxuiPropertySheet::Layout (boundsPx, scaler);
    m_colorPicker.Layout (boundsPx, scaler);

    // Restart notice fills the bottom bar from the left edge to just short of
    // the OK / Cancel group (reserve the widest OK, "OK (reboot)").
    if (m_restartNotice != nullptr)
    {
        int   rowH    = scaler.Px (DxuiButtonRow::kRowHeightDip);
        int   edge    = scaler.Px (DxuiButtonRow::kEdgePadDip);
        int   reserve = scaler.Px (DxuiButtonRow::kEdgePadDip + DxuiButtonRow::kButtonWidthDip
                                   + DxuiButtonRow::kGapDip + 132);   // cancel + gap + OK(reboot)
        RECT  r;

        r.left   = boundsPx.left   + edge;
        r.top    = boundsPx.bottom - rowH;
        r.right  = boundsPx.right  - reserve;
        r.bottom = boundsPx.bottom;
        if (r.right < r.left) { r.right = r.left; }

        m_restartNotice->SetRect (r);
        m_restartNotice->SetDpi  (scaler.Dpi());
    }
}


////////////////////////////////////////////////////////////////////////////////
//
//  UpdateRestartNotice
//
//  Shows an amber "Press OK to reboot" caption whenever committing would
//  power-cycle the machine -- a staged machine switch names the target, a
//  reset-requiring hardware edit warns generically (FR-131). Hidden otherwise.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::UpdateRestartNotice ()
{
    std::wstring  notice;


    if (m_apply.WillMachineChange())
    {
        std::wstring  name = (m_machinePage != nullptr) ? m_machinePage->SelectedMachineDisplayName()
                                                        : std::wstring();

        notice  = L"Pending. Press OK to boot ";
        notice += name.empty() ? L"the selected machine" : name;
        notice += L".";
    }
    else if (m_apply.IsResetRequired())
    {
        notice = L"Pending. Press OK to reboot the machine.";
    }

    if (m_restartNotice != nullptr)
    {
        m_restartNotice->SetText    (notice);
        m_restartNotice->SetVisible (!notice.empty());
    }
}


bool SettingsSheet::HasModalOverlay () const
{
    return m_colorPicker.IsOpen();
}


void SettingsSheet::PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_colorPicker.IsOpen())
    {
        m_colorPicker.Paint (painter, text, theme);
    }
}


bool SettingsSheet::OnOverlayMouse (const DxuiMouseEvent & ev)
{
    if (!m_colorPicker.IsOpen())
    {
        return false;
    }

    switch (ev.kind)
    {
    case DxuiMouseEventKind::Down:  m_colorPicker.OnLButtonDown (ev.positionDip.x, ev.positionDip.y); break;
    case DxuiMouseEventKind::Up:    m_colorPicker.OnLButtonUp   (ev.positionDip.x, ev.positionDip.y); break;
    case DxuiMouseEventKind::Move:  m_colorPicker.OnMouseHover  (ev.positionDip.x, ev.positionDip.y);
                                    m_colorPicker.OnMouseMove   (ev.positionDip.x, ev.positionDip.y); break;
    default:                        break;
    }

    Invalidate();
    return true;
}


bool SettingsSheet::OnOverlayChar (wchar_t ch)
{
    bool  handled = m_colorPicker.IsOpen() && m_colorPicker.OnChar (ch);

    if (m_colorPicker.IsOpen())
    {
        Invalidate();
    }
    return handled;
}


bool SettingsSheet::OnOverlayKey (WPARAM vk)
{
    bool  handled = m_colorPicker.IsOpen() && m_colorPicker.OnKey (vk);

    if (m_colorPicker.IsOpen())
    {
        Invalidate();
    }
    return handled;
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
