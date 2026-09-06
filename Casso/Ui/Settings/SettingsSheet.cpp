#include "Pch.h"

#include "SettingsSheet.h"

#include "../../EmulatorShell.h"
#include "../../Config/GlobalUserPrefs.h"
#include "../../Shell/ScreenshotCapture.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/PrinterPanel.h"
#include "Widgets/DxuiLabel.h"
#include "Window/DxuiButtonRow.h"
#include "resource.h"





// Width was sized to the Display page, the widest page: its right-hand
// "(monitor default)" annotation column ends ~568 DIP in, which 600 DIP cleared
// with the same ~32 DIP margin on the right as on the left.
//
// WIDENED FOR THE SIXTH TAB. Screenshots brought the strip to six labels, and
// its folder row carries a path beside a Browse button -- both want room the
// old width did not have. Every page is left-aligned, so the extra width falls
// on the right margin and no existing page moves.
static constexpr int    s_kSheetWidthDip     = 720;
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

SettingsSheet::~SettingsSheet()
{
    if (GetPopupHost() != nullptr)
    {
        GetPopupHost()->SetComposeHook (nullptr);
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

void SettingsSheet::OnBuildPages()
{
    m_hardwarePage = CreatePage<HardwarePage> (L"Machine");   // machine + CPU + hardware
    m_diskPage     = CreatePage<DiskPage>     (L"Disk");
    m_themePage    = CreatePage<ThemePage>    (L"Theme");
    m_displayPage  = CreatePage<DisplayPage>  (L"Display");
    m_printingPage = CreatePage<PrintingPage> (L"Printing");
    m_shotsPage    = CreatePage<ScreenshotsPage> (L"Screenshots");

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
//  Creates and shows the Settings sheet as a MODELESS window, wiring in every
//  service its pages need.
//
//  Modeless is the whole design. Settings edits apply live -- brightness,
//  scanlines, text color all reflect in the emulator as they change -- and a
//  modal dialog would block the message loop that presents those frames, so
//  the user would be adjusting a picture they could not see.
//
//  Dependencies arrive as REFERENCES stored for the sheet's lifetime rather
//  than being reached through a global, so the pages are testable against
//  substitutes and the sheet cannot outlive what it borrows.
//
//  There is no Apply button, and its visibility is set BEFORE Create so
//  OnCreate lays out without it. Live application makes Apply meaningless:
//  changes are already in effect, and OK versus Cancel is commit versus
//  revert.
//
//  OK keeps the standard command-button width matching Cancel until a pending
//  reboot relabels it, at which point RefreshOkLabel widens it and narrows it
//  back on revert (FR-131) -- so it is never wider than Cancel while it just
//  reads "OK".
//
//  Minimum size equals the initial size: the pages have no smaller valid form.
//
//  The app icon is loaded LR_SHARED, which hands back a process-cached handle
//  needing no DestroyIcon, so the sheet is not generic in alt-tab.
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

    // Present without waiting for vblank: the sheet shares the UI thread with
    // the emulator window's vsynced present, and a live-preview drag repaints
    // the sheet on every mouse move -- with the default interval of 1 each of
    // those repaints stacks a second vblank wait onto the thread and starves
    // the emulator's own present cadence. Windowed flip-model presents are
    // composed by DWM at vsync either way, so 0 does not tear.
    params.presentSyncInterval      = 0;

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

    // Open alongside the emulator window (its right edge, or its left when
    // the right will not fit on that monitor) rather than wherever the OS
    // would drop it -- which for a WS_POPUP window is the top-left corner
    // of the primary monitor. When neither side fits the sheet stays on the
    // emulator's monitor and overlaps it; splitting across two screens or
    // wandering onto another one is the worse outcome.
    params.placeBesideOwner         = true;

    hr = DxuiWindow::Create (params);   // fires OnBuildPages + base OnCreate
    CHRA (hr);

    SetTheme (&emuShell.m_chromeTheme);

    // Stand up the live-preview post-process and install it as the window's
    // compose hook. The base renders the content tree (panel + caption +
    // color-picker overlay) to an offscreen texture and hands it here; the
    // compositor blurs / reveals / composes onto the back buffer. The device is
    // borrowed from the window's DxuiHwndSource (non-owning); m_compositor
    // releases its own D3D resources in the sheet dtor while it is still alive.
    if (GetPopupHost() != nullptr)
    {
        hr = m_compositor.Initialize (GetPopupHost()->GetDevice(), GetPopupHost()->GetContext());
        CHRA (hr);
        GetPopupHost()->SetComposeHook (
            [this] (ID3D11ShaderResourceView * contentSrv,
                    ID3D11RenderTargetView   * backBufferRtv,
                    int widthPx, int heightPx)
            {
                m_compositor.Compose (contentSrv, backBufferRtv, widthPx, heightPx);

                // After Compose, and not from an after-paint hook: a window
                // that composes never runs one. The finished 2D frame is on
                // the back buffer now, which is what the scene draws over.
                RenderThemePreviewScene (backBufferRtv, widthPx, heightPx);
            });
    }

    // The Disk tab is dynamic (#84): it exists only while the staged config has
    // an enabled Disk ][ controller. Remember its page index for the toggle.
    m_diskPageIndex = IndexOfPage (m_diskPage);

    // Wire the pages against a fresh SettingsPanelState + machine/theme
    // catalog (the same objects the legacy SettingsPanel owns).
    m_catalog.Bind (&emuShell, &ucs, &prefs, &fs, &themes, &m_state,
                    m_hardwarePage, m_themePage);
    // The chrome-theme hook fires on Apply now, on OK, and on the Cancel
    // that puts the baseline theme back. The sheet repaints in the new
    // palette, and the Display page adopts the new theme's CRT defaults --
    // a theme's crtDefaults are defaults, so they arrive with the theme
    // rather than waiting for a relaunch.
    m_apply.Bind (&m_state, &ucs, &prefs, &fs, &emuShell,
                  [this] ()
                  {
                      SetTheme (&m_emuShell->m_chromeTheme);
                      m_crt.AdoptThemeDefaults();
                  },
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
        m_apply.ApplyThemeLive (m_themePage->GetSelectedThemeId());
    });

    // The 3D desk-scene opt in/out: applies + persists immediately (the
    // scene appears/disappears on the live chrome behind the sheet), so
    // there is no staged state to revert on Cancel.
    m_themePage->SetCrtMonitorChecked (prefs.crtMonitor);
    m_themePage->SetOnCrtMonitorToggled ([this] (bool enabled)
    {
        m_emuShell->SetCrtMonitorEnabled (enabled);
    });

    // Scene antialiasing rides the same live-and-persist channel: the cost is
    // what the user is judging, so they need to see it change while they drag.
    m_themePage->SetAntiAliasingSamples (prefs.sceneAntiAliasing);
    m_themePage->SetOnAntiAliasingChanged ([this] (int samples)
    {
        m_emuShell->SetSceneAntiAliasing (samples);
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
            RaiseOwnerBehindSheet();   // the reveal must find the emulator, not a stranger
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
    // toggle / monitor / restore-defaults edits into the override map, one
    // field per edit, so the shader picks them up next frame;
    // ReseedFromActiveMode (after the Rebuild below) seeds the widgets from
    // the active mode so the sliders show real values instead of sitting
    // zeroed at the left.
    m_crt.Bind (&prefs, &themes, &m_state, m_displayPage, &emuShell);
    m_crt.WireDisplayPageCallbacks();

    // "Restore defaults" reverts the CRT block AND the Color-monitor text
    // color; both live in the bridge's own restore handler (installed by
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
            m_colorPicker.SetHwnd (GetHwnd());
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
        return m_emuShell->GetUiFramebufferPixels();
    });
    m_themePage->SetMountedPathSource ([this] (int driveIndex) -> std::wstring
    {
        return m_emuShell->GetMountedImagePath (driveIndex);
    });
    m_themePage->SetWriteProtectSource ([this] (int driveIndex) -> WriteProtectInfo
    {
        return m_emuShell->GetDriveWriteProtect (driveIndex);
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
    m_hardwarePage->SetPopupHost (GetPopupHost());
    m_diskPage->SetPopupHost     (GetPopupHost());
    m_themePage->SetPopupHost    (GetPopupHost());
    m_displayPage->SetPopupHost  (GetPopupHost());
    m_printingPage->SetPopupHost (GetPopupHost());
    m_shotsPage->SetPopupHost    (GetPopupHost());

    // Printing page: bind global prefs (resolution + dot style). Edits persist
    // / revert through the apply controller (SnapshotBaselines captures the
    // printing prefs too).
    m_printingPage->SetPrefs (&prefs);
    m_printingPage->SetPrinterInfo (m_emuShell->GetPrinterBannerMessage());

    m_shotsPage->SetDefaultFolder (ScreenshotCapture::DefaultFolder().wstring());
    m_shotsPage->SetPrefs (&prefs);

    //  Both actions open shell UI, which a settings page cannot do for
    //  itself. The page owns the setting; the sheet owns the window that a
    //  modal has to be parented to.
    m_shotsPage->SetOnBrowseFolder ([this, &prefs] ()
    {
        fs::path   picked;

        if (ScreenshotCapture::BrowseForFolder (GetHwnd(), picked))
        {
            prefs.screenshotFolder = picked.string();
            m_shotsPage->Rebuild();
            m_shotsPage->MarkDirty();
        }
    });

    m_shotsPage->SetOnOpenFolder ([&prefs] ()
    {
        ScreenshotCapture::RevealFolder (prefs.screenshotFolder);
    });

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

HRESULT SettingsSheet::OnOk()
{
    m_apply.CommitApply();
    return S_OK;
}


void SettingsSheet::OnCancel()
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

void SettingsSheet::OnDialogTick()
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
//  RenderThemePreviewScene
//
//  Draws the desk scene into the theme preview's mock window, so a skeuo theme
//  previews as what it actually is. The 2D paint deliberately left the screen
//  and the drive row out for exactly this.
//
//  The models load a SECOND time here. The shell's scene belongs to the
//  emulator window's device and this sheet is its own window with its own
//  device, so there is nothing to borrow -- and a few thousand triangles is a
//  cheap price for not advertising a presentation the app retired.
//
//  Clipped, because the theme dropdown is allowed to cover the preview and
//  this pass runs after the whole panel tree. The scissor crops rather than
//  re-projects: shrinking the viewport instead would squash the scene into
//  whatever is left.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::RenderThemePreviewScene (ID3D11RenderTargetView * rtv, int widthPx, int heightPx)
{
    ThemePage::PreviewSceneRequest  request;
    DeskSceneComposition            comp;
    HRESULT                         hr       = S_OK;
    int                             fbW      = 0;
    int                             fbH      = 0;
    const uint32_t                * fbPixels = nullptr;
    CrtUvRect                       uv       = { 0.0f, 0.0f, 1.0f, 1.0f };



    if (m_themePage == nullptr || rtv == nullptr || widthPx <= 0 || heightPx <= 0)
    {
        return;
    }

    request = m_themePage->TakeSceneRequest();

    if (request.mode == ThemePage::PreviewSceneMode::None ||
        request.rectPx.right <= request.rectPx.left ||
        request.clipPx.bottom <= request.clipPx.top)
    {
        return;
    }

    // First frame that wants it: stand the scene up. Tried-once, because a
    // device that cannot carry it will not start working later, and retrying
    // every frame would just burn the failure over and over.
    if (!m_previewSceneReady && !m_previewSceneTried)
    {
        m_previewSceneTried = true;

        if (GetPopupHost() != nullptr)
        {
            hr = m_previewScene.Initialize (GetPopupHost()->GetDevice(), GetPopupHost()->GetContext());

            if (SUCCEEDED (hr))
            {
                HRESULT  hrModels = LoadPreviewSceneModels();

                m_previewSceneReady = SUCCEEDED (hrModels);
            }
        }
    }

    if (!m_previewSceneReady)
    {
        return;
    }

    // The machine can change under a staged pick, and the desk wears what the
    // machine wore.
    if (m_emuShell != nullptr && m_emuShell->IsApple2c() != m_previewSceneIsC)
    {
        hr = LoadPreviewSceneModels();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    // The DPI does not set the scene's size -- SolveComposition contain-fits
    // the scene's corners to the viewport and never consults it -- so this
    // only feeds the layout's own dp-based reservations.
    hr = (request.mode == ThemePage::PreviewSceneMode::Full)
       ? DeskSceneLayout::Compute      (request.rectPx, request.dpi, 2, m_previewScene.Metrics(), comp)
       : DeskSceneLayout::ComputeStrip (request.rectPx, request.dpi, 2, m_previewScene.Metrics(), comp);

    if (FAILED (hr) || hr == S_FALSE)
    {
        return;
    }

    m_previewScene.SetComposition (comp);
    m_previewScene.SetClipRect    (&request.clipPx);

    if (request.mode == ThemePage::PreviewSceneMode::Full)
    {
        // The picture comes from the emulator's framebuffer bytes: this
        // device has no CRT chain of its own, and the preview already has the
        // pixels for the flat blit it used to do.
        fbPixels = m_themePage->FramebufferPixels (fbW, fbH);

        if (fbPixels != nullptr && fbW > 0 && fbH > 0)
        {
            hr = m_previewScene.UploadPicture (fbPixels, fbW, fbH);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }

        hr = m_previewScene.Render (rtv, m_previewScene.PictureSrv(), uv, fbW, fbH);
    }
    else
    {
        hr = m_previewScene.RenderStrip (rtv, comp);
    }

    m_previewScene.SetClipRect (nullptr);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadPreviewSceneModels
//
//  The same pairing rule the shell uses: the //c gets its platinum monitor
//  over matching drives, everything else the beige Monitor II over Disk IIs.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsSheet::LoadPreviewSceneModels()
{
    HRESULT  hr  = S_OK;
    bool     isC = (m_emuShell != nullptr) && m_emuShell->IsApple2c();



    // The shell has already parsed and baked the pair this machine wears, and
    // the result is device-independent, so take it rather than doing all of
    // that again on the way into the Theme tab.
    CBRA (m_emuShell != nullptr);

    {
        bool  shellHasModels = m_emuShell->m_deskScene.HasModels();

        CBRA (shellHasModels);
    }

    hr = m_previewScene.AdoptModelsFrom (m_emuShell->m_deskScene);
    CHRA (hr);

    m_previewScene.SetPowerLampOn (true);
    m_previewSceneIsC = isC;

Error:
    return hr;
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

void SettingsSheet::UpdatePreviewCompose()
{
    RECT  emuOverlapClient = {};
    RECT  focusClient      = {};
    HWND  hwnd             = nullptr;



    if (!m_compositor.IsInitialized())
    {
        return;
    }

    hwnd = GetHwnd();

    if (m_previewActive && hwnd != nullptr)
    {
        // Emulator content (screen) intersect this window (screen), expressed in
        // client pixels. No overlap => empty rect => blur + dim only (still
        // focuses attention on the control), no see-through zone.
        RECT  winRect   = {};
        RECT  emuScreen = (m_emuShell != nullptr) ? m_emuShell->GetEmulatorContentScreenRect() : RECT{};
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
            focusClient = m_displayPage->GetFocusedControlRect (m_previewFocusId);
        }
    }

    m_compositor.SetTransparencyState (m_previewActive, emuOverlapClient, focusClient);
    if (m_previewActive)
    {
        Invalidate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RaiseOwnerBehindSheet
//
//  Pin the owner (the emulator main window) DIRECTLY below this sheet in the
//  z-order when a live preview begins. The see-through reveal is an OS-level
//  transparency hole: it shows whatever window sits next below the sheet, and
//  nothing in the owned-window contract keeps that slot for the owner -- an
//  unrelated window the user activated between edits (a terminal, an editor)
//  can occupy it, and the reveal then dutifully shows THAT window, which reads
//  as an opaque gray block where the emulator should be.
//
//  NOACTIVATE so focus stays on the sheet mid-drag; the owner only changes
//  z-position.
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::RaiseOwnerBehindSheet()
{
    HWND  sheet = GetHwnd();
    HWND  owner = (sheet != nullptr) ? GetWindow (sheet, GW_OWNER) : nullptr;



    if (owner != nullptr)
    {
        SetWindowPos (owner, sheet, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RefreshOkLabel
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::RefreshOkLabel()
{
    bool          reboot = m_apply.WillMachineChange() || m_apply.IsResetRequired();
    std::wstring  want   = reboot ? L"OK (reboot)" : L"OK";



    if (GetOkText() != want)   // only reflow on an actual change, not every tick
    {
        SetOkText (std::move (want));
        SetOkWidthDip (reboot ? 132 : 0);   // 0 => standard width, == Cancel
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout / modal-overlay overrides (custom text-color picker, list #8)
//
//  The picker is centered in the same sheet bounds the pages use, painted last
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
        int   rowH    = scaler.ToPx (DxuiButtonRow::kRowHeightDip);
        int   edge    = scaler.ToPx (DxuiButtonRow::kEdgePadDip);
        RECT  r;
        int   reserve = scaler.ToPx (DxuiButtonRow::kEdgePadDip + DxuiButtonRow::kButtonWidthDip
                                     + DxuiButtonRow::kGapDip + 132);   // cancel + gap + OK(reboot)

        r.left   = boundsPx.left   + edge;
        r.top    = boundsPx.bottom - rowH;
        r.right  = boundsPx.right  - reserve;
        r.bottom = boundsPx.bottom;
        if (r.right < r.left) { r.right = r.left; }

        m_restartNotice->SetRect (r);
        m_restartNotice->SetDpi  (scaler.GetDpi());
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

void SettingsSheet::UpdateRestartNotice()
{
    std::wstring  notice;



    if (m_apply.WillMachineChange())
    {
        std::wstring  name = (m_hardwarePage != nullptr) ? m_hardwarePage->GetSelectedMachineDisplayName()
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

void SettingsSheet::UpdateDiskTabVisibility()
{
    bool  want = m_state.HasDiskIIController();



    if (m_diskPageIndex < 0 || want == m_diskTabVisible)
    {
        return;
    }

    m_diskTabVisible = want;
    SetPageVisible (m_diskPageIndex, want);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasModalOverlay
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsSheet::HasModalOverlay() const
{
    return m_colorPicker.IsOpen();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PaintModalOverlay
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::PaintModalOverlay (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    if (m_colorPicker.IsOpen())
    {
        m_colorPicker.Paint (painter, text, theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnOverlayMouse
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsSheet::OnOverlayMouse (const DxuiMouseEvent & ev)
{
    // An open color picker is modal over the sheet, so it takes EVERY mouse
    // event -- including the kinds it ignores, which must not reach the
    // controls behind it.
    bool  isOpen = m_colorPicker.IsOpen();



    if (isOpen)
    {
        switch (ev.kind)
        {
        case DxuiMouseEventKind::Down:  m_colorPicker.OnLButtonDown (ev.positionDip.x, ev.positionDip.y); break;
        case DxuiMouseEventKind::Up:    m_colorPicker.OnLButtonUp   (ev.positionDip.x, ev.positionDip.y); break;
        case DxuiMouseEventKind::Move:  m_colorPicker.OnMouseHover  (ev.positionDip.x, ev.positionDip.y);
                                        m_colorPicker.OnMouseMove   (ev.positionDip.x, ev.positionDip.y); break;
        default:                        break;
        }

        Invalidate();
    }

    return isOpen;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnOverlayChar
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsSheet::OnOverlayChar (wchar_t ch)
{
    bool  handled = m_colorPicker.IsOpen() && m_colorPicker.OnChar (ch);



    if (m_colorPicker.IsOpen())
    {
        Invalidate();
    }

    return handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnOverlayKey
//
////////////////////////////////////////////////////////////////////////////////

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
    char                     test[16] = {};
    float                    pan0     = 0.0f;
    float                    pan1     = 0.0f;
    const SettingsUiPrefs  & prefs    = m_state.GetPrefs();



    if (m_emuShell == nullptr)
    {
        return;
    }

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
    // dialog is canceled without persisting.
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

void SettingsSheet::SnapshotDriveAudioBaseline()
{
    const SettingsUiPrefs &  prefs = m_state.GetPrefs();



    m_baselineDriveMotorVol = prefs.driveMotorVolume;
    m_baselineDriveHeadVol  = prefs.driveHeadVolume;
    m_baselineDriveDoorVol  = prefs.driveDoorVolume;
    m_baselineDriveOnePan   = prefs.driveOnePan;
    m_baselineDriveTwoPan   = prefs.driveTwoPan;
    m_baselineMechanism     = prefs.floppyMechanism;
    m_driveAuditionDirty    = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RevertDriveAuditionIfDirty
//
////////////////////////////////////////////////////////////////////////////////

void SettingsSheet::RevertDriveAuditionIfDirty()
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
