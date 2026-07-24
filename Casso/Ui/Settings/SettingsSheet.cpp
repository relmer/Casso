#include "Pch.h"

#include "SettingsSheet.h"

#include "../../EmulatorShell.h"
#include "../../Config/GlobalUserPrefs.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiButtonRow.h"
#include "resource.h"





// Width is sized to the Display page, the widest page: its right-hand
// "(monitor default)" annotation column ends ~568 DIP in, so 600 DIP leaves the
// same ~32 DIP margin on the right as on the left. Every other page's controls
// are narrower and fit inside this.
static constexpr int    s_kSheetWidthDip     = 600;
static constexpr int    s_kSheetHeightDip    = 760;



////////////////////////////////////////////////////////////////////////////////
//
//  ~SettingsSheet
//
//  Detach the compose hook (it captures `this`) and release the compositor's
//  D3D resources while the borrowed device is still alive -- the DxuiWindow
//  base (which owns the device + swap chain) tears down after this body runs
//  and after the member subobjects, so clearing the hook here guarantees no
//  late RenderFrame calls a half-destructed compositor.
//
////////////////////////////////////////////////////////////////////////////////

SettingsSheet::~SettingsSheet ()
{
    if (PopupHost() != nullptr)
    {
        PopupHost()->SetComposeHook (nullptr);
    }
    m_compositor.Shutdown();
}




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
    m_hardwarePage = CreatePage<HardwarePage> (L"Machine");   // machine + CPU + hardware
    m_diskPage     = CreatePage<DiskPage>     (L"Disk");
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
//  OpenModeless
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsSheet::OpenModeless (
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

    // Casso app icon so the dialog isn't generic in alt-tab / the taskbar.
    // LR_SHARED hands back a process-cached handle -- no DestroyIcon needed.
    params.appIconBig   = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                              IMAGE_ICON, GetSystemMetrics (SM_CXICON),
                                              GetSystemMetrics (SM_CYICON),
                                              LR_DEFAULTCOLOR | LR_SHARED);
    params.appIconSmall = (HICON) LoadImageW (hInstance, MAKEINTRESOURCEW (IDI_CASSO),
                                              IMAGE_ICON, GetSystemMetrics (SM_CXSMICON),
                                              GetSystemMetrics (SM_CYSMICON),
                                              LR_DEFAULTCOLOR | LR_SHARED);

    // Composited (per-pixel alpha) so the live-preview compositor can punch a
    // see-through hole to the running emulator behind the sheet (#8). The
    // compose hook below drives it: inactive -> panel drawn sharp + opaque
    // (indistinguishable from a plain window); an active Display drag -> blur +
    // dim the panel and reveal the emulator through the overlap region.
    params.composited               = true;

    hr = DxuiWindow::Create (params);   // fires OnBuildPages + base OnCreate
    CHRA (hr);

    SetTheme (&emuShell.m_chromeTheme);

    // Stand up the live-preview post-process and install it as the window's
    // compose hook. The base renders the content tree (panel + caption +
    // color-picker overlay) to an offscreen texture and hands it here; the
    // compositor blurs / reveals / composes onto the back buffer. The device is
    // borrowed from the window's DxuiHwndSource (non-owning); m_compositor
    // releases its own D3D resources in the sheet dtor while it is still alive.
    if (PopupHost() != nullptr)
    {
        hr = m_compositor.Initialize (PopupHost()->GetDevice(), PopupHost()->GetContext());
        CHRA (hr);
        PopupHost()->SetComposeHook (
            [this] (ID3D11ShaderResourceView * contentSrv,
                    ID3D11RenderTargetView   * backBufferRtv,
                    int widthPx, int heightPx)
            {
                m_compositor.Compose (contentSrv, backBufferRtv, widthPx, heightPx);
            });
    }

    // The Disk tab is dynamic (#84): it exists only while the staged config has
    // an enabled Disk ][ controller. Remember its page index for the toggle.
    m_diskPageIndex = IndexOfPage (m_diskPage);

    // Wire the pages against a fresh SettingsPanelState + machine/theme
    // catalog (the same objects the legacy SettingsPanel owns).
    m_catalog.Bind (&emuShell, &ucs, &prefs, &fs, &themes, &m_state,
                    m_hardwarePage, m_themePage);
    m_apply.Bind (&m_state, &ucs, &prefs, &fs, &emuShell,
                  [this] () { SetTheme (&m_emuShell->m_chromeTheme); },
                  &m_catalog);
    m_hardwarePage->SetState (&m_state);
    m_diskPage->SetState     (&m_state);
    m_displayPage->SetState  (&m_state);

    // Disk page play (>) buttons audition the drive sounds live.
    m_diskPage->SetOnTestSound ([this] (int drive, int kind, bool centered)
    {
        AuditionDriveSound (drive, kind, centered);
    });

    // Staged picks: the machine + theme selectors defer their real apply to
    // OK (CommitApply) so Cancel leaves the running machine / chrome as found.
    m_hardwarePage->SetOnMachineSelected ([this] (const std::string & name)
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

    // Skeuo desk-scene opt-in: applies + persists immediately (the monitor
    // framing appears/disappears on the live chrome behind the sheet), so
    // there is no staged state to revert on Cancel.
    m_themePage->SetMonitorFrameChecked (prefs.skeuoMonitorFrame);
    m_themePage->SetOnMonitorFrameToggled ([this] (bool enabled)
    {
        m_emuShell->SetSkeuoMonitorFrame (enabled);
    });

    // Live preview (#8): dragging / keyboard-editing a Display control blurs +
    // dims the sheet and reveals the running emulator through the overlap
    // region so the CRT edit is visible. Mouse gives a clean start/end; a
    // keyboard edit fires start only, so the preview controller's idle timeout
    // (advanced from OnDialogTick) ends it a moment after the last keystroke.
    // controlId (a kControl* id) drives the sharp focus region over the blur.
    m_displayPage->SetOnPreview ([this] (int controlId, bool start, bool keyboardMode)
    {
        if (start)
        {
            m_previewFocusId = controlId;
            m_preview.StartPreview (SettingsPreviewController::Focus::BrightnessSlider, keyboardMode);
            m_previewActive  = true;
        }
        else
        {
            m_preview.EndPreview();
            m_previewActive  = false;
            m_previewFocusId = -1;
        }
        UpdatePreviewCompose();   // reflect the new state on the next composed frame
    });

    // Per-monitor CRT plumbing for the Display page. Bind funnels the slider /
    // toggle / monitor / restore-defaults edits through the crtByMode block so
    // the shader picks them up next frame; ReseedFromActiveMode (after the
    // Rebuild below) seeds the widgets from the active mode so the sliders
    // show real values instead of sitting zeroed at the left.
    m_crt.Bind (&prefs, &themes, &m_state, m_displayPage, &emuShell);
    m_crt.WireDisplayPageCallbacks();

    // "Restore defaults" reverts the CRT block AND the Color-monitor text
    // colour; both live in the bridge's own restore handler (installed by
    // WireDisplayPageCallbacks above) so the single handler stays authoritative
    // -- an earlier attempt to re-wire it here was silently superseded.

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
    m_themePage->SetWriteProtectSource ([this] (int driveIndex) -> WriteProtectInfo
    {
        return m_emuShell->DriveWriteProtect (driveIndex);
    });
    // Drive the preview's disk presence off the STAGED config so toggling the
    // Disk ][ controller on the Machine tab updates the preview immediately --
    // dropping the drive widgets + collapsing the drive bar (#84 Phase C/D),
    // matching what OK will realise into the live chrome.
    m_themePage->SetHasDiskSource ([this] () -> bool
    {
        return m_state.HasDiskIIController();
    });

    // Route each page's dropdown menus through the host popup pool so they
    // escape the client clip (FR-054 / FR-061).
    m_hardwarePage->SetPopupHost (PopupHost());
    m_diskPage->SetPopupHost     (PopupHost());
    m_themePage->SetPopupHost    (PopupHost());
    m_displayPage->SetPopupHost  (PopupHost());

    // Pull the running machine + discovered themes into the pages.
    m_catalog.LoadCurrentMachineIntoState();
    m_catalog.PopulateMachineList();
    m_catalog.PopulateThemeList();

    // Seed the Disk tab's presence from the loaded config before the first
    // Layout so the tab strip is correct on the very first paint.
    UpdateDiskTabVisibility();

    // Show the window BEFORE the final Rebuild: the first valid layout only
    // happens once the window is shown (WM_SIZE against the real client size),
    // and HardwarePage's tree view (plus any content that flows its rows at
    // Rebuild time) needs real bounds to flow into, else it collapses to the
    // top-left. ShowModelessDialog re-shows harmlessly below.
    Show();
    m_hardwarePage->Rebuild();
    m_diskPage->Rebuild();
    m_displayPage->Rebuild();
    m_crt.ReseedFromActiveMode();   // seed Display sliders from the active mode

    // Capture the CRT / color / theme baseline so OnCancel can revert any
    // live-preview edits, plus the drive-audio baseline so OnCancel can undo a
    // play-button audition that pushed dialed values live to the engine.
    m_apply.SnapshotBaselines();
    SnapshotDriveAudioBaseline();

    // Modeless: show + return immediately. The emulator keeps running behind
    // the sheet; the host loop pumps ProcessDialogMessage and destroys us via
    // the SetOnDialogEnd callback the caller installed.
    ShowModelessDialog (IDOK);

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
    RevertDriveAuditionIfDirty();
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
    UpdateDiskTabVisibility();

    // Advance the preview state machine so a keyboard-driven preview idles out;
    // a mouse drag ends explicitly in OnPreview. Either way UpdatePreviewCompose
    // re-drives the compositor each tick so a live drag keeps recomposing (the
    // emulator behind the reveal is animating) and the panel snaps back to sharp
    // the moment the preview ends.
    m_preview.Tick ((int64_t) GetTickCount64());
    if (m_previewActive && !m_preview.IsActive())
    {
        m_previewActive  = false;
        m_previewFocusId = -1;
    }
    UpdatePreviewCompose();
}




////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePreviewCompose
//
//  Push this frame's transparency state into the compositor: whether a Display
//  preview is live, the emulator-overlap rect (see-through hole), and the
//  focused-control rect (kept sharp over the blur). While active, invalidate so
//  the window recomposes every tick and the revealed, running emulator animates.
//  All rects are in the sheet's client pixels, matching the compose viewport.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::UpdatePreviewCompose ()
{
    if (!m_compositor.IsInitialized())
    {
        return;
    }

    RECT  emuOverlapClient = {};
    RECT  focusClient      = {};
    HWND  hwnd             = Hwnd();

    if (m_previewActive && hwnd != nullptr)
    {
        // Emulator content (screen) intersect this window (screen), expressed in
        // client pixels. No overlap => empty rect => blur + dim only (still
        // focuses attention on the control), no see-through zone.
        RECT  winRect   = {};
        RECT  emuScreen = (m_emuShell != nullptr) ? m_emuShell->EmulatorContentScreenRect() : RECT{};
        RECT  inter     = {};
        if (GetWindowRect (hwnd, &winRect) && IntersectRect (&inter, &winRect, &emuScreen))
        {
            POINT  origin = { 0, 0 };
            ClientToScreen (hwnd, &origin);
            emuOverlapClient = RECT{ inter.left  - origin.x, inter.top    - origin.y,
                                     inter.right - origin.x, inter.bottom - origin.y };
        }

        if (m_displayPage != nullptr)
        {
            focusClient = m_displayPage->FocusedControlRect (m_previewFocusId);
        }
    }

    m_compositor.SetTransparencyState (m_previewActive, emuOverlapClient, focusClient);
    if (m_previewActive)
    {
        Invalidate();
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
        std::wstring  name = (m_hardwarePage != nullptr) ? m_hardwarePage->SelectedMachineDisplayName()
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




////////////////////////////////////////////////////////////////////////////////
//
//  UpdateDiskTabVisibility
//
//  The Disk tab is present only while the staged hardware config has an enabled
//  Disk ][ controller (#84 Phase B). Toggling the slot-6 checkbox in the
//  Machine tab's tree flips m_state.HasDiskIIController(); reflecting it here
//  (each dialog tick, cheap because SetPageVisible no-ops when unchanged) adds
//  or removes the tab live without wiring the tree toggle directly.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::UpdateDiskTabVisibility ()
{
    bool  want = m_state.HasDiskIIController();


    if (m_diskPageIndex < 0 || want == m_diskTabVisible)
    {
        return;
    }

    m_diskTabVisible = want;
    SetPageVisible (m_diskPageIndex, want);
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
//  Disk page play (>) button handler. Pushes the current drive-audio
//  settings to the engine and fires a one-shot test of the given sound.
//  Flags the audition dirty so OnCancel restores the mixer to the
//  dialog-open baseline (RevertDriveAuditionIfDirty).
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

    // The push above changed the live engine mixer; remember to undo it if the
    // dialog is cancelled without persisting.
    m_driveAuditionDirty = true;

    sprintf_s (test, "%d,%d", drive, kind);
    m_emuShell->PostCommand (IDM_AUDIO_DRIVE_TEST, test);
}




////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotDriveAudioBaseline / RevertDriveAuditionIfDirty
//
//  A play (>) audition pushes the staged drive-audio (volumes / pan /
//  mechanism) straight to the engine mixer for preview. Snapshot the as-opened
//  values at Show; on Cancel, re-push them so an audition that was never
//  committed does not leave the mixer on the dialed values. OK persists the
//  staged config through the normal apply path, so no revert is needed there.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::SnapshotDriveAudioBaseline ()
{
    const SettingsUiPrefs &  prefs = m_state.Prefs();

    m_baselineDriveMotorVol = prefs.driveMotorVolume;
    m_baselineDriveHeadVol  = prefs.driveHeadVolume;
    m_baselineDriveDoorVol  = prefs.driveDoorVolume;
    m_baselineDriveOnePan   = prefs.driveOnePan;
    m_baselineDriveTwoPan   = prefs.driveTwoPan;
    m_baselineMechanism     = prefs.floppyMechanism;
    m_driveAuditionDirty    = false;
}


void SettingsSheet::RevertDriveAuditionIfDirty ()
{
    if (!m_driveAuditionDirty)
    {
        return;
    }

    PushDriveAudioToEngine (m_baselineDriveMotorVol,
                            m_baselineDriveHeadVol,
                            m_baselineDriveDoorVol,
                            m_baselineDriveOnePan,
                            m_baselineDriveTwoPan,
                            m_baselineMechanism);
    m_driveAuditionDirty = false;
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
