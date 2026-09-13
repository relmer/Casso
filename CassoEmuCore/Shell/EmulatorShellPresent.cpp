#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
#include "Ui/Chrome/DriveLabelTruncation.h"
#include "Print/PrintJobStore.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "Ui/PrinterPanel.h"
#include "Core/PathResolver.h"
#include "Version.h"
#include "BuildInfo.h"
#include "resource.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleGamePort.h"
#include "Machines/Apple2/Apple2e/Apple2eSoftSwitchBank.h"
#include "Machines/Apple2/Common/AppleSpeaker.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/LanguageCard.h"
#include "Machines/Apple2/Apple2e/Apple2eMmu.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/FramePacing.h"
#include "Shell/Input/AppleKeyMapping.h"
#include "Shell/Layout/DriveRowLayout.h"
#include "Machines/Apple2/Common/AppleMouse.h"
#include "Core/Prng.h"
#include "Config/DiskSettings.h"
#include "Core/UnicodeSymbols.h"
#include "Core/MachineConfig.h"
#include "Core/JsonParser.h"
#include "Machines/Apple2/Common/AppleTextMode.h"
#include "Machines/Apple2/Common/Apple80ColTextMode.h"
#include "Machines/Apple2/Common/AppleLoResMode.h"
#include "Machines/Apple2/Common/AppleHiResMode.h"
#include "Machines/Apple2/Common/AppleDoubleHiResMode.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"
#include "Ui/Chrome/ChromeMetrics.h"
#include "Ui/DriveWidgetController.h"
#include "Shell/DiskMru.h"
#include "Window/DxuiHwndSource.h"
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InitializeRenderer
//
//  Points the framebuffer renderer at the host's D3D resources and hooks it
//  into the host's paint pump.
//
//  The renderer owns NO device, swap chain, or Present call -- the host owns
//  all three. This is what lets Dxui chrome paint on top of the emulator
//  image in one pass: the renderer composites the Apple ][ framebuffer into
//  the host's back buffer from the before-present hook, then the host paints
//  its chrome over it and presents once (DxuiHwndSource::PaintPump).
//
//  The hook reads m_pendingFramebuffer, which RunMessageLoop stages each UI
//  frame. A null value means "no new emulator frame", and the last upload is
//  re-composited -- that is the case that makes the render-skip gate cheap:
//  chrome can repaint over a static emulator image without re-uploading it.
//
//  The composite result is dropped deliberately. A per-frame present hook has
//  no return channel; a transient failure corrects itself on the next frame,
//  and a persistent one (device lost) is both visible on screen and handled by
//  the renderer's own device-reset path, not from inside the hook.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeRenderer()
{
    HRESULT  hr = S_OK;



    // Initialize the Apple ][ framebuffer renderer against the host's
    // D3D11 device + DXGI swap chain (full host ownership). The host
    // owns Present; the renderer composites the framebuffer into the
    // host back buffer from the before-present hook wired below. The
    // initial target rect is the DxuiViewport bounds computed during
    // CreateEmulatorWindow.
    hr = m_d3dRenderer.Initialize (m_host->GetDevice(),
                                   m_host->GetContext(),
                                   m_host->GetSwapChain(),
                                   kFramebufferWidth,
                                   kFramebufferHeight,
                                   m_viewportBoundsPx);
    CHR (hr);

    // Desk scene (spec 018): shares the host device with the framebuffer
    // renderer. Failure (broken embedded asset) asserts in debug and leaves
    // the 2D chrome paths active.
    {
        HRESULT  hrScene = InitializeDeskScene();

        IGNORE_RETURN_VALUE (hrScene, S_OK);
    }

    // Composite the Apple ][ framebuffer before the host paints chrome on
    // top (DxuiHwndSource::PaintPump). m_pendingFramebuffer is staged each
    // UI frame by RunMessageLoop; nullptr means "no new emulator frame"
    // (re-composite last upload). With the desk scene active, the CRT chain
    // renders to the offscreen scene target and the 3D scene samples it on
    // the monitor glass -- the theme backdrop the host cleared stays visible
    // around the devices. Otherwise the classic direct composite runs.
    m_host->SetBeforePresentHook ([this] ()
    {
        HRESULT  hrComposite = S_OK;



        if (CrtMonitorActive())
        {
            // The CRT chain renders the picture into an exact-aspect rect
            // anchored at the texture origin -- sized to the picture's
            // MEASURED on-screen height so the glass samples ~1:1 texels
            // (over-rendering minifies, and the linear filter then averages
            // away the outermost pixel columns where the sag compresses the
            // edges) -- and NOT positioned by the projected bounding box,
            // whose keystone slop would shear the texel alignment.
            RECT  glassPx     = m_d3dRenderer.GetTargetBounds();
            int   measuredH   = (int) lroundf (DeskSceneLayout::MeasurePictureHeightPx (
                                    m_deskScene.Composition(), m_deskScene.MonitorModel().Surface(),
                                    kFramebufferWidth, kFramebufferHeight));
            int   pictureH    = (measuredH > 0) ? measuredH : (int) (glassPx.bottom - glassPx.top);
            int   pictureW    = 0;
            RECT  pictureRect = {};

            pictureH = std::min (pictureH, m_d3dRenderer.GetBackBufferHeight());
            pictureW = MulDiv (pictureH, kFramebufferWidth, kFramebufferHeight);

            if (pictureW > m_d3dRenderer.GetBackBufferWidth())
            {
                pictureW = m_d3dRenderer.GetBackBufferWidth();
                pictureH = MulDiv (pictureW, kFramebufferHeight, kFramebufferWidth);
            }

            // Anchored at the texture origin: the picture's edges coincide
            // with the texture's, so the CRT chain's neighbor-sampling
            // passes clamp onto the picture itself at the borders -- the
            // same behavior as the classic direct path. (The picture mesh's
            // boundary is the band boundary, so nothing ever samples across
            // the picture's texture edge.)
            pictureRect = RECT{ 0, 0, pictureW, pictureH };

            hrComposite = m_d3dRenderer.UploadAndCompositeOffscreen (m_pendingFramebuffer, pictureRect);

            if (SUCCEEDED (hrComposite))
            {
                // The chain aspect-fits within pictureRect; recompute the same
                // fit so the sampled subrect matches it exactly. The texture
                // IS pictureRect now, so this is very nearly the whole of it
                // -- only a rounding row or column of letterbox survives.
                RECT                       fitted     = ComputeAspectFitRectInRect (pictureRect,
                                                            kFramebufferWidth, kFramebufferHeight);
                CrtUvRect                  uv         = ComputeUvRectForFit (fitted,
                                                            pictureW, pictureH);
                ID3D11ShaderResourceView * displaySrv = m_d3dRenderer.GetSceneContentSrv();

                // Calibration mode: swap in the stripe pattern so the glass
                // texel mapping can be verified end to end.
                if (m_deskSceneDebug >= 2)
                {
                    EnsureSceneCalibration (fitted);

                    if (m_sceneCalibSrv != nullptr)
                    {
                        displaySrv = m_sceneCalibSrv.Get();
                    }
                }

                hrComposite = m_deskScene.Render (m_host->GetBackBufferRtv(),
                                                  displaySrv, uv,
                                                  kFramebufferWidth, kFramebufferHeight);

                // Fullscreen drive overlay strip: the slid band composed by
                // TryPresentUiFrame's FSM tick, plus the hidden-state
                // activity glimmer in the corner (FR-015).
                if (m_d3dRenderer.IsFullscreen())
                {
                    int   bbW = m_d3dRenderer.GetBackBufferWidth();
                    int   bbH = m_d3dRenderer.GetBackBufferHeight();

                    if (m_stripRectPx.bottom > m_stripRectPx.top)
                    {
                        HRESULT  hrStrip = m_deskScene.RenderStrip (m_host->GetBackBufferRtv(), m_stripComp);

                        IGNORE_RETURN_VALUE (hrStrip, S_OK);
                    }

                    if (m_stripState.ActivityIndicator())
                    {
                        RECT  glimmer = { bbW - 34, bbH - 14, bbW - 12, bbH - 8 };

                        m_deskScene.DrawDebugRect (glimmer, bbW, bbH, 0xFFB01818);
                    }
                }

                // Layout diagnosis overlay: scene viewport red, projected
                // glass green, drive band yellow, switch band magenta.
                if (m_deskSceneDebug)
                {
                    int   bbW = m_d3dRenderer.GetBackBufferWidth();
                    int   bbH = m_d3dRenderer.GetBackBufferHeight();

                    m_deskScene.DrawDebugRect (m_deskScene.Composition().viewportPx, bbW, bbH, 0xFFFF3030);
                    m_deskScene.DrawDebugRect (m_deskScene.Composition().glassRectPx, bbW, bbH, 0xFF30FF30);

                    // The projected drive bounds ARE the drop-target rects the
                    // hit registry carries, so drawing them shows whether a
                    // refused drag is a bad rect or something upstream.
                    for (int i = 0; i < m_deskScene.Composition().driveCount; i++)
                    {
                        m_deskScene.DrawDebugRect (m_deskScene.Composition().driveRectPx[i], bbW, bbH, 0xFFFFA030);
                    }

                    m_deskScene.DrawDebugRect (m_driveBand.GetBounds(), bbW, bbH, 0xFFFFFF30);
                    m_deskScene.DrawDebugRect (m_switchBand.GetBounds(), bbW, bbH, 0xFFFF30FF);
                    m_deskScene.DrawDebugRect (m_stripRectPx, bbW, bbH, 0xFF30FFFF);
                }
            }
        }
        else
        {
            // Monitor off: the picture composites straight to the back buffer
            // as it always did. The 3D drives still render -- from the
            // after-paint hook below, since this composite writes the whole
            // back buffer and the opaque drive-band surface paints after it.
            hrComposite = m_d3dRenderer.UploadAndComposite (m_host->GetBackBufferRtv(),
                                                            m_pendingFramebuffer);
        }

        // Per-frame present hook with no return channel to propagate to.
        // A transient composite failure self-corrects next frame; a
        // persistent one (device lost) shows on screen and is handled by
        // the renderer's own device-reset path, not from here.
        IGNORE_RETURN_VALUE (hrComposite, S_OK);

        //  A Crt capture belongs HERE, at the end of the composite and
        //  before the chrome walk: the picture is finished and nothing has
        //  painted over it yet. Waiting until after the panel tree would
        //  collect the readouts and any chrome overlapping the viewport.
        ServiceCaptureRequest (CapturePoint::AfterPicture);
    });

    // With the monitor opted out the drives are the only scene objects, and
    // they render HERE -- after the chrome painted -- because the classic
    // composite blacks out the whole back buffer and the drive band's opaque
    // surface would otherwise paint straight over them. The drive row keeps
    // its own depth pass, so it composes onto the finished frame.
    m_host->SetAfterPaintHook ([this] (ID3D11RenderTargetView * rtv, int bbW, int bbH)
    {
        HRESULT  hrDrives = S_OK;


        //  A Scene capture is taken at the TOP of this hook, ahead of the
        //  early returns below -- it has to run whatever the monitor and
        //  theme state, and by here the panel tree has painted and the scene
        //  is whole. The drives rendered further down belong to the
        //  monitor-off path, which is the one case this ordering gets wrong;
        //  see the note in TakeScreenshot.
        ServiceCaptureRequest (CapturePoint::AfterChrome);

        if (CrtMonitorActive() || !DeskSceneActive())
        {
            return;
        }

        if (m_d3dRenderer.IsFullscreen())
        {
            // Fullscreen without the monitor: the picture owns the client and
            // the drives live in the slide-up overlay strip, same as they do
            // with the monitor on.
            if (m_stripRectPx.bottom > m_stripRectPx.top)
            {
                hrDrives = m_deskScene.RenderStrip (rtv, m_stripComp);
            }

            if (m_stripState.ActivityIndicator())
            {
                RECT  glimmer = { bbW - 34, bbH - 14, bbW - 12, bbH - 8 };

                m_deskScene.DrawDebugRect (glimmer, bbW, bbH, 0xFFB01818);
            }
        }
        else
        {
            hrDrives = m_deskScene.RenderStrip (rtv, m_deskScene.Composition());
        }

        IGNORE_RETURN_VALUE (hrDrives, S_OK);
    });

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RefreshCrtOverrideKeys
//
//  Rebuilds the four override keys for the monitor now on the desk.
//
//  Resolving the monitor costs a path lookup, a file read and a JSON parse,
//  so it cannot happen on the render path. Caching all four modes rather
//  than the active one means a color-mode change needs no invalidation at
//  all, and the per-frame lookup does not build a string.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RefreshCrtOverrideKeys()
{
    const MonitorSpec &  monitor = ResolveMonitorForCurrentMachine();
    size_t               mode    = 0;



    for (mode = 0; mode < kCrtModeCount; mode++)
    {
        m_crtOverrideKeys[mode] = CrtResolver::MakeKey (monitor.configName, mode);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ResolveCrtForCurrentMode
//
//  The picture for the monitor and mode showing right now.
//
//  The color mode is loaded ONCE. Reading it twice would let a preemption
//  between the reads pair one mode preset with another mode overrides, which
//  is a race the previous two-load call sites carried.
//
////////////////////////////////////////////////////////////////////////////////

CrtResolved EmulatorShell::ResolveCrtForCurrentMode() const
{
    const ThemeCrtDefaults *  themeDefaults = nullptr;
    size_t                    mode          = (size_t) m_colorMode.load (std::memory_order_acquire);
    CrtOverrides              overrides;
    auto                      found         = m_globalPrefs.crtOverrides.end();



    if (mode >= kCrtModeCount)
    {
        mode = 0;
    }

    found = m_globalPrefs.crtOverrides.find (m_crtOverrideKeys[mode]);
    if (found != m_globalPrefs.crtOverrides.end())
    {
        overrides = found->second;
    }

    // Resolved defaults, never the base theme: the base drops the machine
    // variant overrides, which is what made the picture change brightness
    // depending on which caller set the parameters last.
    if (m_themeManager != nullptr && m_themeManager->GetActiveTheme() != nullptr)
    {
        themeDefaults = &m_themeManager->ActiveCrtDefaults();
    }

    return CrtResolver::Resolve (CrtPresets::GetPreset (mode), themeDefaults, overrides);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ResolveMonitorForCurrentMachine
//
////////////////////////////////////////////////////////////////////////////////

const MonitorSpec & EmulatorShell::ResolveMonitorForCurrentMachine()
{
    JsonValue          doc;
    const JsonValue *  uiPrefs = nullptr;



    // The merged document, not the shipped one: a machine's monitor is
    // configuration like everything else in there, so a user copy that names a
    // different monitor is answered the same way the machine's own does.
    LoadMachineUiPrefs (doc, uiPrefs);

    return MonitorCatalog::ForMachineJson (doc);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SnapshotStripToPanel
//
//  Force-refreshes the panel from the drain worker WITHOUT stopping it: the
//  panel snapshots only its visible viewport span under the worker's raster
//  lock while the same interpreter keeps running. Fully non-destructive --
//  previewing (or refreshing) mid-print can never reset the guest's in-flight
//  state, so it cannot distort the output. (The original path stopped and
//  re-Start()ed the worker, which rebuilt the interpreter and reset its line
//  feed from Print Shop's ESC T back to the default, stretching everything
//  printed after a mid-print preview.)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SnapshotStripToPanel()
{
    int64_t   nowMs      = 0;
    bool      panelIsUp  = m_printerPanel != nullptr && m_printerPanel->IsOpen();
    bool      hasCard    = m_machine.GetRefs().printerCard != nullptr;



    if (panelIsUp && !hasCard)
    {
        PrintRaster   empty;

        m_printerPanel->SetStrip (empty);   // blank sheet
    }
    else if (panelIsUp)
    {
        nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                    std::chrono::steady_clock::now().time_since_epoch()).count();

        // Forced refresh through the panel's viewport: snapshots and renders
        // only the visible ~1-page span (never the whole strip), same as the
        // live path.
        m_printerPanel->RefreshLive (m_printerWorker, nowMs, true /* force */);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetColorModeLive
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetColorModeLive (int settingsColorModeIndex)
{
    ColorMode  mode = ColorMode::Color;



    switch (settingsColorModeIndex)
    {
        case 0:  mode = ColorMode::Color;     break;
        case 1:  mode = ColorMode::GreenMono; break;
        case 2:  mode = ColorMode::AmberMono; break;
        case 3:  mode = ColorMode::WhiteMono; break;
        default: return;
    }

    m_colorMode.store (mode, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetColorMonitorTextArgbLive
//
//  Updates the Color-monitor text color read by RenderFramebuffer on the
//  next frame. Forces opaque alpha so a stray transparent value can't blank
//  the text.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetColorMonitorTextArgbLive (uint32_t argb)
{
    m_colorMonitorTextArgb.store (0xFF000000u | (argb & 0x00FFFFFFu), std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryPresentUiFrame
//
//  One UI render cycle, and the answer to "was anything actually presented?"
//  -- which is what lets the caller park the thread instead of spinning.
//
//  Presenting is NOT unconditional. The 9-pass CRT post-process is expensive
//  enough (~20%% of GPU at a static BASIC prompt) that the frame is skipped
//  when neither the emulator framebuffer nor any CRT parameter changed and
//  the persistence trail has finished decaying. Everything in the middle of
//  this function exists to answer that question honestly: each subsystem that
//  is mid-animation calls MarkRedrawNeeded so its frames are not dropped.
//
//  The ones that must vote:
//
//    drive widgets   a door mid-open / mid-close, plus one frame after the
//                    last drive goes idle so the activity LED actually clears
//    open menus      a paused machine produces no framebuffer changes, so
//                    without this a menu opens in state only and looks dead
//    settings sheet  live Display edits must land on the very next present;
//                    otherwise a brightness drag waits for a cursor blink
//
//  CRT parameters are pushed every frame rather than on change, so a slider
//  edit is visible on the next present with no change-tracking to get wrong.
//
//  This also runs off a WM_TIMER during an OS modal move / size loop, which
//  is why the preview and printer audio keep running while the user holds the
//  title bar -- see OnModalLoopTick.
//
//  Presenting goes through InvalidateRect / UpdateWindow rather than a direct
//  Present: the host owns the paint pump, so the framebuffer is staged and a
//  synchronous WM_PAINT drives clear -> composite -> chrome -> present in the
//  one order that puts the chrome on top.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryPresentUiFrame()
{
    HRESULT  hr                        = S_OK;
    bool     didPresent                = false;
    bool     anyDriveLive              = false;
    bool     framebufferDirtyThisFrame = false;
    bool     wpMoved                   = false;
    uint32_t driveSig                  = 0;
    std::shared_lock<std::shared_mutex>  lifetime (m_machine.GetLifetimeLock(), std::try_to_lock);



    // The frame reads the machine's devices throughout -- the drive
    // controller for the widgets, the keyboard for the //c strip -- and a
    // machine switch on the CPU thread destroys and rebuilds them. The switch
    // holds the lifetime lock exclusively for the rebuild; the frame holds it
    // shared, and a frame that finds it taken simply does not present. Tried
    // rather than waited for, so a frame re-entered from a modal loop's timer
    // while the outer frame already holds it cannot wedge against a waiting
    // switch.
    if (!lifetime.owns_lock())
    {
        return false;
    }




    ExpireChangeBannerIfDue();

    //  The capture band a lost grab left standing, given back -- here, at the
    //  top of the frame, because re-docking repaints and nothing has been
    //  composed yet. See SyncStandInBanner for what sets this.
    if (m_standInBandStale)
    {
        m_standInBandStale = false;
        ReflowChromeForChangeBand();
    }

    // Copy latest framebuffer under lock, then present with vsync
    {
        lock_guard<mutex> lock (m_framebufferMutex);

        if (m_framebufferReady)
        {
            m_framebufferReady                 = false;
            framebufferDirtyThisFrame = true;
        }
    }

    // / FR-038. Push the latest CRT params (brightness slider,
    // scanlines/bloom/color-bleed toggles + magnitudes) to the
    // renderer every UI frame so user edits land on the very next
    // present. The active theme's `crtDefaults` only apply when the
    // user hasn't customized anything yet (see MakeCrtParams), and they
    // come RESOLVED -- reading the base theme here dropped the machine
    // overrides, so the picture changed brightness whenever a resize let
    // the other caller set the parameters instead.
    {
        CrtParams  params = MakeCrtParams (ResolveCrtForCurrentMode(),
                                           (float) m_d3dRenderer.GetBackBufferWidth(),
                                           (float) m_d3dRenderer.GetBackBufferHeight());
        m_d3dRenderer.SetCrtParams (params);
    }

    // Skip the entire upload + 9-pass post-process when neither the
    // emulator framebuffer nor any CRT param changed (and the
    // persistence trail isn't still decaying). Saves ~20%% GPU at a
    // BASIC prompt. PeekMessage above still drains messages; the
    // brief sleep keeps this thread from spinning.
    //
    // FORCE PRESENT when the nav layer has an open menu so menu
    // hover / open / close transitions paint. Without this, a
    // paused machine produces no fb changes -> no Present -> menus
    // open in state-only and never repaint, looking dead.
    // Per-UI-frame chrome upkeep that used to live in the after-blit
    // hook: advance drive-door animations and force a present while a
    // door is mid-transition so the chrome keeps repainting even when
    // the emulator framebuffer is static.
    if (m_diskManager != nullptr)
    {
        m_diskManager->UpdateDriveWidgets();
    }

    // The capture bar and the fullscreen top chrome's reveal, both per-frame
    // because both answer where the pointer is right now.
    //
    // THE TOP CHROME FIRST: in fullscreen the capture bar hangs under the
    // toolbar, and bounds the tick has not written yet put the bar where the
    // strip was last frame -- visibly trailing it through the reveal.
    TickFullscreenTopChrome();
    SyncStandInBanner();
    TraceControllerState();
    SyncNotice();
    SyncFrameRateReadout();
    SyncSceneViewReadout();


    for (const DriveWidgetState & st : m_driveWidgetState)
    {
        bool  doorMoving = (st.doorState == DriveWidgetState::Door::Opening ||
                            st.doorState == DriveWidgetState::Door::Closing);
        bool  motorOn          = st.motorOn.load    (memory_order_relaxed);
        bool  diskActive       = st.diskActive.load (memory_order_relaxed);
        int   headQuarterTrack = st.headQuarterTrack.load  (memory_order_relaxed);

        anyDriveLive = anyDriveLive || motorOn || diskActive;

        // Everything about a drive that is VISIBLE, folded into one word so
        // the present vote below can ask whether it moved rather than whether
        // it is busy.
        //
        // The head position is in here because a 2D theme draws it: a seek
        // with the motor already running changes no flag, so without this the
        // readout would sit still until something else asked for a frame.
        //
        // EIGHT bits, because the value is in quarter-tracks and runs to 139.
        // Six bits was the first cut and it aliased: quarter-track 64 folded
        // onto 0, so a seek across the outer half of the disk moved the
        // signature not at all. The unknown -1 folds to 0xFF, which is past
        // the largest real position. Eleven bits per drive over two drives
        // stays well inside the word.
        driveSig = (driveSig << 11) | (motorOn ? 1u : 0u)
                                    | (diskActive ? 2u : 0u)
                                    | (doorMoving ? 4u : 0u)
                                    | ((uint32_t) (headQuarterTrack & 0xFF) << 3);

        if (doorMoving)
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }

        // A drive that has just gone quiet is FADING, and a fade nobody
        // redraws is a step with a delay in front of it. Nothing else asks
        // for these frames: the activity flags have already settled, the head
        // is not moving, and a static emulator picture skips the present
        // entirely. Ask for them until the fade is over.
        {
            int64_t  sinceMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                   std::chrono::steady_clock::now().time_since_epoch()).count()
                               - st.lastActiveMs;

            if (st.lastActiveMs != 0 && sinceMs >= 0 && sinceMs < DriveWidgetState::kActivityFadeMs)
            {
                m_d3dRenderer.MarkRedrawNeeded();
            }
        }
    }

    // A 2D drive's name roll wants frames for the same reason. Asked of the
    // WIDGETS rather than the states, since the roll's clock lives with the
    // label it is moving.
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        for (const DriveWidget & drive : m_driveChrome)
        {
            if (drive.IsNameRolling (nowMs))
            {
                m_d3dRenderer.MarkRedrawNeeded();
            }
        }
    }

    // The padlock coming or going wants a frame, and it is the one drive cue
    // that nothing else asks for one about: a mount rolls the label, activity
    // fades, a door swings, but write-protecting a disk moves no pixel the
    // machine owns. Left to the next unrelated redraw, the padlock appeared
    // whenever something else happened to repaint -- a theme hover, a
    // resize -- which read as the click not having worked.
    //
    // BOTH PRESENTATIONS, so it sits above the scene branch: the 2D widget
    // paints its own badge, and in the scene the padlock is a glyph on the
    // front of the disk's NAME, which is re-hung below.
    for (int i = 0; i < (int) m_driveWpShown.size(); i++)
    {
        bool  wp = m_driveWidgetState[i].writeProtect.Any();

        if (wp != m_driveWpShown[i])
        {
            m_driveWpShown[i] = wp;
            m_d3dRenderer.MarkRedrawNeeded();
            wpMoved = true;
        }
    }

    // 3D scene drive visuals: activity lamp, door swing, and the padlock,
    // pushed from the same per-drive state the 2D widgets mirror. The scene
    // only rebuilds geometry when a value actually moved.
    if (DeskSceneActive())
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        for (int i = 0; i < 2; i++)
        {
            const DriveWidgetState &  st       = m_driveWidgetState[i];
            float                     t        = std::clamp ((float) (nowMs - st.animationStartTimeMs) /
                                                             (float) DriveWidgetState::kDoorAnimationMs, 0.0f, 1.0f);
            float                     progress = 0.0f;
            bool                      lampOn   = st.motorOn.load    (memory_order_relaxed) ||
                                                 st.diskActive.load (memory_order_relaxed);

            switch (st.doorState)
            {
                case DriveWidgetState::Door::Open:     progress = 1.0f;     break;
                case DriveWidgetState::Door::Opening:  progress = t;        break;
                case DriveWidgetState::Door::Closing:  progress = 1.0f - t; break;
                case DriveWidgetState::Door::Closed:   progress = 0.0f;     break;
            }

            m_deskScene.SetDriveVisuals (i, lampOn, progress, st.writeProtect.Any());
        }

        // A mount or eject changes the basename strip under the drive, and so
        // does write-protecting the disk, since the padlock is a glyph at the
        // head of that name. Neither runs a layout pass, so watch both here
        // and re-hang the labels (with their text measurement) on a change.
        {
            bool  labelsMoved = wpMoved;

            for (int i = 0; i < (int) m_sceneLabelPath.size(); i++)
            {
                std::string  source = m_machine.GetDiskStore().GetSourcePath (6, i);

                if (source != m_sceneLabelPath[i])
                {
                    m_sceneLabelPath[i] = source;
                    labelsMoved         = true;
                }
            }

            if (labelsMoved)
            {
                SyncSceneDriveLabels();
                m_d3dRenderer.MarkRedrawNeeded();
            }
        }
    }

    // Fullscreen drive overlay strip (FR-015): tick the FSM from this
    // frame's observations, apply its capture effects, and compose the slid
    // band the hook will render. The flat themes ride the same FSM with the
    // 2D widgets laid into the band, so fullscreen hides the drives the same
    // way everywhere and brings them back the same way too.
    bool  stripHasDrives = DeskSceneActive()
                         ? DeskSceneDriveCount() > 0
                         : (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();

    if (m_d3dRenderer.IsFullscreen() && stripHasDrives)
    {
        StripInputs   inputs;
        StripEffects  effects;
        POINT         cursor  = {};
        RECT          client  = {};
        int64_t       stripNowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                       std::chrono::steady_clock::now().time_since_epoch()).count();

        GetClientRect (m_hwnd, &client);

        inputs.nowMs = stripNowMs;

        if (GetCursorPos (&cursor) && ScreenToClient (m_hwnd, &cursor) && PtInRect (&client, cursor))
        {
            inputs.pointerAtBottomEdge = cursor.y >= client.bottom - m_scaler.ToPx (s_kStripEdgeZoneDp);
            inputs.pointerOverStrip    = m_stripState.Mode() != StripMode::Hidden &&
                                         PtInRect (&m_stripRectPx, cursor);
        }

        inputs.hotkey        = m_stripHotkeyPending;
        m_stripHotkeyPending = false;

        inputs.pinned        = m_stripBrowseOpen || m_driveTooltip.IsVisible();

        // LIVE, not Active: Mouse mode being CONFIGURED is not the guest
        // owning the pointer. At a BASIC prompt in Mouse mode the host
        // cursor is the only pointer there is, and the bottom edge must
        // summon the strip -- Active gated the reveal off for the whole
        // session on a machine whose mouse is built in.
        inputs.guestPointer  = m_paddleCaptured    ? GuestPointerMode::Paddle
                             : IsGuestMouseLive()    ? GuestPointerMode::Mouse
                             :                       GuestPointerMode::None;
        inputs.anyDriveActive = anyDriveLive;

        effects = m_stripState.Tick (inputs);

        if (effects.releaseCapture)
        {
            if (m_paddleCaptured)
            {
                StopPaddleCapture();
            }
            else
            {
                m_stripSuppressGuestMouse = true;
            }
        }

        if (effects.restoreCapture == GuestPointerMode::Paddle)
        {
            StartPaddleCapture();
        }
        else if (effects.restoreCapture == GuestPointerMode::Mouse)
        {
            m_stripSuppressGuestMouse = false;
        }

        // The band slides up from the bottom edge: only the top
        // `progress * height` sliver is on-screen mid-animation. The flat
        // widgets' band is the windowed drive bar's height; the scene's is
        // the row its drives compose into.
        {
            float  progress = m_stripState.SlideProgress (stripNowMs);
            int    bandH    = DeskSceneActive() ? m_scaler.ToPx (s_kStripBandDp)
                                                : m_scaler.ToPx (m_driveBarThicknessDp);

            if (progress > 0.0f)
            {
                m_stripRectPx = { 0, client.bottom - (int) (progress * (float) bandH),
                                  client.right, client.bottom - (int) (progress * (float) bandH) + bandH };

                if (DeskSceneActive())
                {
                    HRESULT  hrStrip  = S_OK;
                    RECT     driveRow = {};

                    // The drives get the band LESS the name strip, the way the
                    // windowed drive band reserves it: the disk's name and its
                    // padlock belong under the drive here too, and a row composed
                    // into the whole band would put them off the screen's edge.
                    driveRow         = m_stripRectPx;
                    driveRow.bottom -= m_scaler.ToPx (s_kSceneDriveLabelStripDp + s_kSceneDriveLabelGapDp);

                    // The drive band's calibrated look-down, not the desk's
                    // near-level default: the band angle is what shows the
                    // drives' tops, and the fullscreen strip is the same
                    // drives-only row the windowed band composes.
                    hrStrip = DeskSceneLayout::ComputeStrip (driveRow, m_scaler.GetDpi(),
                                                             DeskSceneDriveCount(),
                                                             m_deskScene.Metrics(), m_stripComp,
                                                             DeskSceneLayout::kDriveBandGazeDownRad);
                    IGNORE_RETURN_VALUE (hrStrip, S_OK);
                }
                else
                {
                    // The flat widgets sit in the band wherever the slide has
                    // put it this frame, bottom-anchored the way the windowed
                    // bar anchors them, over the band's own surface. They paint
                    // after the picture, so they ride over it.
                    m_driveBandSurface.SetBounds (m_stripRectPx);
                    m_driveBandSurface.SetVisible (true);
                    LayoutDriveWidgetsInCommandBar (m_driveChrome, bandH, client.right,
                                                    m_stripRectPx.bottom, m_scaler.GetDpi(), 1.0f,
                                                    ShouldShowExternalDrive() ? 2 : 1);

                    if (!ShouldShowExternalDrive())
                    {
                        m_driveChrome[1].Hide();
                    }
                }
            }
            else
            {
                m_stripRectPx = {};
                m_stripComp   = {};

                if (!DeskSceneActive())
                {
                    m_driveBandSurface.SetVisible (false);
                    m_driveChrome[0].SetVisible (false);
                    m_driveChrome[1].SetVisible (false);
                    m_driveChrome[0].Hide();
                    m_driveChrome[1].Hide();
                }
            }
        }

        // The strip's names ride its slide: re-hung every pass so they track
        // the band on its way in and out, and retire with it. The flat
        // widgets carry their own names.
        if (DeskSceneActive())
        {
            SyncSceneDriveLabels();
        }

        if (m_stripState.Mode() != StripMode::Hidden || m_stripState.ActivityIndicator())
        {
            m_d3dRenderer.MarkRedrawNeeded();
        }
    }
    else
    {
        // Not presenting the strip (windowed, or fullscreen left): never
        // strand a suppressed guest mouse.
        m_stripSuppressGuestMouse = false;
        m_stripRectPx             = {};
    }

    // Keep presenting while the drives' visible state is CHANGING, plus the
    // one frame after it settles so the last change actually reaches the
    // screen. Doors mid-swing vote separately above; this covers the activity
    // lamps.
    //
    // CHANGING, not merely LIVE. The vote used to fire for as long as a motor
    // was energized, and a Disk II motor stays energized until the guest
    // writes $C0E8 -- which plenty of software simply never does once it has
    // loaded. A demo that leaves the drive spinning is faithful hardware
    // behavior, and it pinned Casso at a full 60 fps of nine-pass CRT
    // post-processing forever, over a picture that had not changed in
    // minutes. An LED that is steadily lit is not an animation.
    if (driveSig != m_lastDriveSig || m_driveSigSettling)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    m_driveSigSettling = (driveSig != m_lastDriveSig);
    m_lastDriveSig     = driveSig;

    // //c switch strip: refresh the disk-use LED (drive activity) and the
    // Ctrl-armed reset cue every UI frame so they track live state.
    if (MachineHasCaseSwitches())
    {
        SyncSwitchBarState();
    }

    if (m_disk2DebugPanel != nullptr)
    {
        hr = m_disk2DebugPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_inputDebugPanel != nullptr)
    {
        hr = m_inputDebugPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_printerPanel != nullptr)
    {
        hr = m_printerPanel->RenderFrame();
        IGNORE_RETURN_VALUE (hr, S_OK);
    }

    if (m_mainMenu.IsOpen())
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    // While the modeless Settings sheet is open, force a present every UI
    // frame so live Display edits (brightness / contrast / scanlines / text
    // color) reflect in the emulator instantly. The retired SettingsWindow
    // was rendered inline in this loop each frame, which coupled the
    // emulator's present cadence to the settings edits; the standalone
    // sheet decoupled it, so between framebuffer changes (e.g. a cursor
    // blink) a CRT-param edit would otherwise wait for the next
    // NeedsPresent trigger and appear laggy.
    if (m_settingsSheet != nullptr)
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    // An open toolbar picker previews live, so it needs the same treatment:
    // a highlight change alters the chrome or the picture, and without a
    // forced present the preview would wait for the next unrelated redraw.
    SyncToolbarState();

    if (m_toolbar.IsMenuOpen())
    {
        m_d3dRenderer.MarkRedrawNeeded();
    }

    // Drive the chrome tooltip dwell timers (joystick button, toolbar,
    // //c switch strip, drive widgets); each shows / hides its popup once
    // the open / close delay elapses after a hover.
    {
        int64_t  nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                             std::chrono::steady_clock::now().time_since_epoch()).count();

        m_toolbarTooltip.Tick   (nowMs);
        m_switchBarTooltip.Tick (nowMs);
        m_driveTooltip.Tick     (nowMs);
        m_captionTooltip.Tick   (nowMs);

        // An open menu's submenu waits out the system's show delay before it
        // opens, and the pointer resting on the row produces no messages, so
        // a present is requested every frame one is armed, as for the compass.
        if (m_mainMenu.WantsTick() || m_toolbar.WantsTick())
        {
            m_mainMenu.TickMenus (nowMs);
            m_toolbar.TickMenus  (nowMs);

            m_d3dRenderer.MarkRedrawNeeded();
        }

        // A HELD COMPASS ARROW REPEATS, and a held arrow produces no messages
        // to wake this loop -- the pointer is not moving, which is the very
        // condition the repeat exists for. So it votes for a present the
        // whole time it is held, not only on the frames it fires: without
        // that the loop parks and the repeat stops between steps.
        if (m_sceneCompass.WantsTick())
        {
            m_sceneCompass.Tick (nowMs);

            m_d3dRenderer.MarkRedrawNeeded();
        }
    }

    // Refresh the printer status LED; marks a redraw itself on a change so
    // a static screen (e.g. a pending page at the BASIC prompt) repaints.
    UpdatePrinterStatus();

    // Auto-open the print preview when a print begins and stream the strip
    // into it live as the guest prints (non-destructive snapshot).
    UpdatePrinterPreview();

    didPresent = m_d3dRenderer.NeedsPresent (framebufferDirtyThisFrame);


    if (didPresent)
    {
        // Drive the host paint pump for this frame. Stage the emulator
        // framebuffer for the before-present hook, then request a
        // synchronous WM_PAINT: the host clears, the hook composites the
        // framebuffer, the chrome paints on top, and the host presents.
        m_pendingFramebuffer = framebufferDirtyThisFrame ? m_uiFramebuffer.data() : nullptr;
        InvalidateRect (m_hwnd, nullptr, FALSE);
        UpdateWindow   (m_hwnd);
    }

    return didPresent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishFramebuffer
//
//  Copies the freshly-rendered CPU framebuffer into the UI-visible
//  framebuffer under m_framebufferMutex and wakes the UI thread. Skips the whole
//  handoff when the frame is byte-identical to the one last published, so a
//  static screen stops driving the CRT post-process + Present at 60 Hz.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PublishFramebuffer()
{
    HRESULT  hr = S_OK;
    BOOL     ok = FALSE;



    // THE UPSTREAM GATE ANSWERS A DIFFERENT QUESTION. RunCpuThreadFrame asks
    // whether the picture COULD have changed -- a write landed in a display
    // page, the mode moved, the flash phase flipped -- and it is deliberately
    // conservative, because guessing wrong the other way drops a frame the
    // user was waiting on.
    //
    // The flash phase is the one that matters here. It flips about four times
    // a second whatever is on screen, so a full-screen hi-res picture with no
    // text on it at all re-rasterized and republished at 3.7 Hz forever --
    // byte for byte the same image every time. Downstream that was enough to
    // keep resetting the persistence settle counter, and Casso ran the
    // nine-pass CRT chain at a full 60 fps over a still image indefinitely.
    //
    // So the handoff is gated on the RESULT rather than the prediction: a
    // frame identical to the one already published is not published again.
    // The compare is one pass over ~840 KB, and it only runs when the cheap
    // gate upstream already thought something moved.
    {
        lock_guard<mutex>  lock (m_framebufferMutex);

        bool  same = m_uiFramebuffer.size() == m_cpuFramebuffer.size()
                     && memcmp (m_uiFramebuffer.data(), m_cpuFramebuffer.data(),
                                m_cpuFramebuffer.size() * sizeof (uint32_t)) == 0;

        BAIL_OUT_IF (same, S_OK);

        m_uiFramebuffer    = m_cpuFramebuffer;
        m_framebufferReady = true;
    }

    if (m_frameReadyEvent != nullptr)
    {
        ok = SetEvent (m_frameReadyEvent);
        CWRA (ok);
    }

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShouldPublishFrame
//
//  Presentation-side pacing. Below Maximum speed the CPU thread is already
//  paced to one frame per vsync by its waitable timer, so every frame is
//  published. At Maximum speed emulation is unthrottled, so gate the
//  rasterize/publish to a ~60 Hz wall-clock cadence.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ShouldPublishFrame()
{
    SpeedMode  speed = m_cpuManager.GetSpeedMode();



    //  The clock lives in FrameClock, which is what makes the decision
    //  testable without a test having to wait for real time to pass.
    return m_frameClock.ShouldPublish (speed == SpeedMode::Maximum, s_kMaxSpeedPublishIntervalUs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeVideoModeSig
//
//  Packs every soft-switch that changes what the renderer produces into a
//  small integer: the base graphics/mixed/page2/hi-res selects plus the //e
//  80STORE / 80COL / ALTCHARSET / double-hi-res bits. Any change re-renders.
//  Mirrors exactly the inputs MachineManager::SelectVideoMode reads.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t EmulatorShell::ComputeVideoModeSig()
{
    uint32_t                  sig = 0;
    Apple2eSoftSwitchBank *   iie = m_machine.GetRefs().iieSoftSwitches;



    if (m_machine.GetRefs().softSwitches != nullptr)
    {
        sig |= m_machine.GetRefs().softSwitches->IsGraphicsMode() ? 0x01u : 0u;
        sig |= m_machine.GetRefs().softSwitches->IsMixedMode()    ? 0x02u : 0u;
        sig |= m_machine.GetRefs().softSwitches->IsPage2()        ? 0x04u : 0u;
        sig |= m_machine.GetRefs().softSwitches->IsHiresMode()    ? 0x08u : 0u;

        if (iie != nullptr)
        {
            sig |= iie->Is80Store()     ? 0x10u : 0u;
            sig |= iie->Is80ColMode()   ? 0x20u : 0u;
            sig |= iie->IsAltCharSet()  ? 0x40u : 0u;
            sig |= iie->IsDoubleHiRes() ? 0x80u : 0u;
        }
    }

    return sig;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeFlashOn
//
//  Derives the text flash/cursor-blink phase from emulated time (total CPU
//  cycles), toggling every 16 emulated frames as the real VBL-driven blink
//  does. Computed independently of rendering so the gate can re-render on a
//  toggle without the flash freezing when frames are skipped.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::ComputeFlashOn()
{
    uint64_t  cyclesPerToggle = 16ull * m_cyclesPerFrame;
    bool      flashOn         = true;   // no clock yet: show the glyph



    if (m_machine.GetCpu() != nullptr && cyclesPerToggle != 0)
    {
        flashOn = ((m_machine.GetCpu()->GetTotalCycles() / cyclesPerToggle) & 1ull) == 0;
    }

    return flashOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeColorSig
//
//  Folds the monitor color mode and the color-monitor text color into one
//  value so a live change (View menu / Settings) re-renders the frame.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t EmulatorShell::ComputeColorSig()
{
    uint64_t  mode = (uint64_t) m_colorMode.load (memory_order_acquire);
    uint64_t  argb = (uint64_t) m_colorMonitorTextArgb.load (memory_order_acquire);



    return (mode << 32) | argb;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowNotice
//
//  Show a notice over the picture for a few seconds. UI thread only.
//
//  The text arrives already composed -- by CaptureOutcome::DescribeResult or
//  WriteProtectChange::DescribeResult -- which is deliberate: every branch of
//  what to say is decided in core where a test can reach it, and this
//  function chooses no wording.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ShowNotice (const std::wstring & text)
{
    int64_t   nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                          std::chrono::steady_clock::now().time_since_epoch()).count();



    m_notice.Show (text, nowMs);

    SyncNotice();

    m_d3dRenderer.MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PostNotice
//
//  Hand a notice to the window from any thread. The notice is Dxui and Dxui
//  asserts UI-thread affinity, so a caller on the CPU thread cannot show it
//  directly. With no window there is nothing to show it over, and the notice
//  is dropped: it only confirms a change the indicators already show.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PostNotice (const std::wstring & text)
{
    wstring *  carried = nullptr;



    if (m_hwnd == nullptr)
    {
        return;
    }

    carried = new (std::nothrow) wstring (text);

    if (carried != nullptr && !PostMessageW (m_hwnd, WM_APP_SHOW_NOTICE, 0,
                                             reinterpret_cast<LPARAM> (carried)))
    {
        delete carried;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncNotice
//
//  Lay the notice out while it is live, and drop it once it expires.
//
//  ACROSS THE TOP, UNDER EVERYTHING DOCKED THERE. The chrome at the top of
//  the window is where this window puts what it has to say about itself, and
//  a screenshot's filename is exactly that -- the one part of a capture that
//  is about the application rather than about the machine. Put over the
//  picture it lands on whatever the user just photographed, and read as a
//  caption on it.
//
//  IT OVERLAYS RATHER THAN DOCKS, which is the one way it differs from the
//  pointer-capture bar beside it. That bar tracks a state and is worth the
//  height it takes from the picture; this one is up for four seconds, and a
//  band that appears and vanishes on a timer would reflow the machine twice
//  for every screenshot. So it hangs UNDER the last docked band and covers a
//  strip of picture, dimmed by a scrim rather than hidden behind a panel.
//
//  Separate from the pointer-capture bar, not a reuse of it: a screenshot
//  taken with the paddle captured must not replace the words telling the user
//  how to get their cursor back.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncNotice()
{
    RECT                 client = {};
    RECT                 rc     = {};
    IDxuiTextRenderer *  text   = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    float                width  = 0.0f;
    int64_t              nowMs  = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                      std::chrono::steady_clock::now().time_since_epoch()).count();



    if (!m_notice.IsShowing (nowMs) || m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        m_notice.SetVisible (false);
        return;
    }

    width = (float) (client.right - client.left);

    rc.left  = client.left;
    rc.right = client.right;
    rc.top   = ComputeTopOverlayEdgePx (client);

    //  Measured where there is a renderer to ask; the estimate is the
    //  fallback for the frames before the renderer exists.
    m_notice.SetDpi (m_scaler.GetDpi());

    rc.bottom = rc.top + (LONG) ((text != nullptr)
                                 ? m_notice.GetMeasuredHeightPx (*text, width, m_scaler)
                                 : m_notice.GetPreferredHeightPx (width, m_scaler));

    m_notice.Layout     (rc, m_scaler);
    m_notice.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ComputeTopOverlayEdgePx
//
//  Where the picture starts, for something that wants to hang over the top of
//  it without landing on the chrome.
//
//  IT ASKS THE BANDS RATHER THAN ADDING THEM UP. Which bands are at the top,
//  and which of those are showing, varies by theme, by fullscreen and by
//  whether the mouse is currently captured -- a count kept here would be a
//  second copy of the dock's arithmetic, and the copy is the one that goes
//  wrong. The lowest bottom edge among the bands that are up IS the answer,
//  and it stays the answer when a band is added.
//
//  Fullscreen has no bands at all: the toolbar reveals itself over the
//  picture and the pointer-capture bar hangs beneath it, and both are in the
//  list for exactly that case.
//
////////////////////////////////////////////////////////////////////////////////

LONG EmulatorShell::ComputeTopOverlayEdgePx (const RECT & client) const
{
    const IDxuiControl * const  bands[] = { &m_mainMenu,
                                            &m_toolbar,
                                            &m_changeBanner,
                                            &m_standInBarSurface,
                                            &m_standInBar };
    LONG                        top     = client.top;
    RECT                        rc      = {};



    for (const IDxuiControl * band : bands)
    {
        rc = band->GetBounds();

        if (band->IsVisible() && rc.bottom > rc.top
            && rc.top < client.bottom && rc.bottom > top)
        {
            top = rc.bottom;
        }
    }

    return top;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetStandInOverlaysHidden
//
//  HIDE WHAT DESCRIBES THE APPLICATION; CAPTURE WHAT DESCRIBES THE MACHINE.
//
//  The compass is a control, the two readouts are diagnostics, and the
//  pointer-capture bar is a transient piece of state -- none of them are part
//  of the machine on the desk, and each can sit inside the viewport where a
//  Scene capture would otherwise collect it.
//
//  A useful side effect: a scene capture no longer depends on which
//  diagnostics happen to be switched on, so two captures of the same view are
//  the same image.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetStandInOverlaysHidden (bool hidden)
{
    if (hidden)
    {
        m_sceneCompass.SetVisible     (false);
        m_fpsReadout.SetVisible       (false);
        m_sceneViewReadout.SetVisible (false);
        //  The pointer-capture bar is docked chrome in a window, and a
        //  Scene capture takes the viewport, so there it is already out of
        //  frame. In FULLSCREEN there are no bands and the bar hangs off the
        //  top edge, inside the picture -- which is the case this covers.
        m_standInBar.SetVisible        (false);
        m_standInBarSurface.SetVisible (false);

        //  Including this one. Two captures inside the notice's few seconds
        //  would otherwise photograph the first one's filename.
        m_notice.SetVisible (false);
    }
    else
    {
        //  Restored by the layout pass that owns each one, rather than by
        //  remembering four booleans here -- the pose readout and the frame
        //  rate are driven by prefs, the compass by whether a scene is up,
        //  and the banner by whether the mouse is captured. Re-deriving is
        //  what keeps this from disagreeing with them.
        LayoutSceneCompass();
        SyncFrameRateReadout();
        SyncSceneViewReadout();
        SyncStandInBanner();
        SyncNotice();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ServiceCaptureRequest
//
//  Called from both paint hooks. Fills the pending capture if this is the
//  point in the frame the plan asked for, and does nothing otherwise.
//
//  Read failures are swallowed here on purpose: `captured` stays false and
//  TakeScreenshot reports it once the frame is over. A paint hook is not a
//  place to raise anything -- it runs inside the host's pump, with a frame
//  half-built.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ServiceCaptureRequest (CapturePoint atPoint)
{
    HRESULT   hr = S_OK;



    if (!m_pendingCapture.armed || m_pendingCapture.captured)
    {
        return;
    }

    if (m_pendingCapture.at != atPoint)
    {
        return;
    }

    if (m_pendingCapture.from == CaptureSource::PictureTarget)
    {
        hr = m_d3dRenderer.CaptureSceneTargetRegion (m_pendingCapture.regionPx, m_pendingCapture.image);
    }
    else
    {
        hr = m_d3dRenderer.CaptureBackBufferRegion (m_pendingCapture.regionPx, m_pendingCapture.image);
    }

    m_pendingCapture.captured = SUCCEEDED (hr);

    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildScreenshotFacts
//
//  Gathers what a screenshot can say about itself. Collecting only -- WHICH of
//  these a given mode actually emits is the composer's decision, and it is
//  made in core where a test can reach it.
//
//  Every value here already has an owner elsewhere and is reused rather than
//  re-derived: the version string is the one the WOZ creator stamp uses, the
//  monitor key is the one the user's CRT overrides are filed under, and the
//  pose comes from the same formatter as the on-screen readout. That is what
//  keeps a screenshot's account of itself from drifting away from the rest of
//  the application.
//
////////////////////////////////////////////////////////////////////////////////

ScreenshotFacts EmulatorShell::BuildScreenshotFacts (ScreenshotMode mode, const SYSTEMTIME & when) const
{
    ScreenshotFacts          facts;
    TIME_ZONE_INFORMATION    tz     = {};
    DWORD                    tzKind = 0;
    size_t                   mIndex = 0;
    const CrtParams &        crt    = m_d3dRenderer.GetCrtParams();



    facts.mode               = mode;
    facts.versionString      = string ("Casso ") + VERSION_STRING;
    facts.when               = when;
    facts.machineDisplayName = m_machine.GetConfig().name;

    //  The offset the timestamp is expressed in. GetTimeZoneInformation
    //  reports Bias as minutes to ADD to local time to reach UTC, which is the
    //  opposite sign from the one RFC 1123 prints, and daylight time carries
    //  its own extra bias on top.
    tzKind = GetTimeZoneInformation (&tz);

    if (tzKind != TIME_ZONE_ID_INVALID)
    {
        LONG   bias = tz.Bias + ((tzKind == TIME_ZONE_ID_DAYLIGHT) ? tz.DaylightBias : tz.StandardBias);

        facts.utcOffsetMinutes = (int) -bias;
    }

    //  The key the user's CRT overrides are filed under, taken from the cache
    //  the render path already keeps rather than re-resolved: resolving the
    //  monitor costs a path lookup, a file read and a JSON parse.
    mIndex = (size_t) m_colorMode.load (std::memory_order_acquire);

    if (mIndex < kCrtModeCount)
    {
        facts.monitorKey = m_crtOverrideKeys[mIndex];
    }

    if (DeskSceneActive())
    {
        facts.hasScenePose  = true;
        facts.orbitYawRad   = m_sceneView.orbitYawRad;
        facts.orbitPitchRad = m_sceneView.orbitPitchRad;
        facts.zoom          = m_sceneView.zoom;
        facts.panX          = m_sceneView.panX;
        facts.panY          = m_sceneView.panY;
    }

    facts.crt.brightness        = crt.brightness;
    facts.crt.contrast          = crt.contrast;
    facts.crt.gamma             = crt.gamma;
    facts.crt.scanlineIntensity = crt.scanlineIntensity;
    facts.crt.bloomStrength     = crt.bloomStrength;
    facts.crt.bloomRadius       = crt.bloomRadius;
    facts.crt.colorBleedWidth   = crt.colorBleedWidth;
    facts.crt.persistence       = crt.persistence;

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TakeScreenshot
//
//  Resolve what the user asked for, get the pixels, deliver them, say what
//  happened.
//
//  THE PIXELS COME FROM ONE OF TWO PLACES, and which one decides the shape of
//  this function. A Raw capture is a memcpy out of the framebuffer and needs
//  no frame at all. The other two live on the GPU in a back buffer that
//  FLIP_DISCARD throws away at Present, so they can only be read from inside a
//  paint -- which is why this arms a request, drives one synchronous paint
//  through the ordinary WM_PAINT path, and collects the result afterwards.
//
//  Driving the paint rather than waiting for the next natural one keeps the
//  whole thing synchronous from the caller's point of view: the notice appears
//  in the same turn the user pressed the button.
//
//  The overlays are hidden across that paint and restored immediately, which
//  the user sees as a shutter.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::TakeScreenshot()
{
    HRESULT                     hr          = S_OK;
    ScreenshotPlanInputs        inputs;
    ScreenshotPlan              plan;
    ScreenshotCapture::Sources  sources;
    CaptureOutcome              outcome;
    CapturedImage               image;
    vector<MetadataEntry>       textChunks;
    PWSTR                       picturesRaw = nullptr;
    HRESULT                     hrPictures  = S_OK;
    SYSTEMTIME                  now         = {};



    GetLocalTime (&now);

    hrPictures = SHGetKnownFolderPath (FOLDERID_Pictures, 0, nullptr, &picturesRaw);

    if (SUCCEEDED (hrPictures))
    {
        inputs.defaultPicturesFolder = fs::path (picturesRaw);
    }

    inputs.mode            = ScreenshotModeToken::Parse (m_globalPrefs.screenshotMode);
    inputs.saveFile        = m_globalPrefs.screenshotSaveFile;
    inputs.folder          = fs::path (m_globalPrefs.screenshotFolder);
    //  THE TWO RECTS ARE NOT THE SAME THING, and m_viewportBoundsPx is not
    //  either of them under a desk scene. That member is the DxuiViewport
    //  panel's bounds, which the scene layout puts on the GLASS -- measured,
    //  594x378 inside a 1582x1116 client. Feeding it to both inputs made Scene
    //  and Crt capture one identical crop of the monitor's face.
    //
    //  Scene   the whole area the scene is drawn into, which the composition
    //          owns; the same rect the compass is inset from.
    //  Crt     where the picture actually landed. With a desk scene that is a
    //          sub-rect of the offscreen target, recorded by the renderer as
    //          it drew; without one the chain composited straight into the
    //          back buffer at its target bounds.
    if (DeskSceneActive())
    {
        inputs.viewportPx = m_deskScene.Composition().viewportPx;
        inputs.picturePx  = m_d3dRenderer.GetScenePictureRect();
    }
    else
    {
        inputs.viewportPx = m_viewportBoundsPx;
        inputs.picturePx  = m_d3dRenderer.GetTargetBounds();

        //  AND THE DRIVES, which in a flat theme are not in the viewport at
        //  all -- they are widgets in a band docked under it, where the desk
        //  scene models them inside the picture area. Full scene left them
        //  out, which made it identical to Screen only in every flat theme.
        //  The band's surface runs from its top to the bottom of the client,
        //  so the union takes the switch bar between them as well: those
        //  switches are the machine's too.
        if (m_driveBandSurface.IsVisible())
        {
            inputs.machineChromePx = m_driveBandSurface.GetBounds();
        }
    }

    inputs.framebufferSize = { kFramebufferWidth, kFramebufferHeight };
    inputs.deskSceneActive = DeskSceneActive();
    inputs.windowMinimized = (IsIconic (m_hwnd) != FALSE);
    inputs.when            = now;

    plan = ScreenshotPlan::Resolve (inputs,
               [] (const fs::path & p) { std::error_code e; return fs::exists (p, e); });

    textChunks = ScreenshotMetadata::Compose (BuildScreenshotFacts (inputs.mode, now));


    sources.hwnd             = m_hwnd;
    sources.renderer         = &m_d3dRenderer;
    sources.clipboard        = m_clipboardManager.get();
    sources.framebuffer      = m_uiFramebuffer.empty() ? nullptr : m_uiFramebuffer.data();
    sources.framebufferMutex = &m_framebufferMutex;
    sources.framebufferSize  = { kFramebufferWidth, kFramebufferHeight };

    if (plan.refusal == CaptureRefusal::None)
    {
        if (plan.source == CaptureSource::Framebuffer)
        {
            hr = ScreenshotCapture::AcquirePixels (plan, sources, image);

            IGNORE_RETURN_VALUE (hr, S_OK);
        }
        else
        {
            m_pendingCapture          = PendingCapture();
            m_pendingCapture.armed    = true;
            m_pendingCapture.from     = plan.source;
            m_pendingCapture.regionPx = plan.sourceRectPx;
            m_pendingCapture.at       = (inputs.mode == ScreenshotMode::Scene)
                                        ? CapturePoint::AfterChrome
                                        : CapturePoint::AfterPicture;

            SetStandInOverlaysHidden (true);

            m_d3dRenderer.MarkRedrawNeeded();
            InvalidateRect (m_hwnd, nullptr, FALSE);
            UpdateWindow   (m_hwnd);

            SetStandInOverlaysHidden (false);

            if (m_pendingCapture.captured)
            {
                image = std::move (m_pendingCapture.image);
            }

            m_pendingCapture = PendingCapture();
        }
    }

    hr = ScreenshotCapture::Deliver (plan, sources, textChunks, image, outcome);

    IGNORE_RETURN_VALUE (hr, S_OK);

    ShowNotice (CaptureOutcome::DescribeResult (outcome));

    if (picturesRaw != nullptr)
    {
        CoTaskMemFree (picturesRaw);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFramebuffer
//
//  Rasterize one emulated video frame into the CPU-side framebuffer: pick the
//  active mode, render it, overlay mixed-mode text, then tint.
//
//  Text color is pushed in rather than baked into the renderer because the
//  same glyph raster serves every monitor type. Color monitors get white text
//  directly; the monochrome modes leave the renderer's green in place and let
//  the tint pass below recolor the entire frame to the chosen phosphor, so
//  green / amber / white monitors need no separate glyph path.
//
//  Flash state is likewise pushed in from emulated time instead of being
//  self-advanced by Render, so a blinking cursor keeps its phase across the
//  frames the render-skip gate drops.
//
//  The dirty-row text cache is force-invalidated in exactly two cases, and
//  both are correctness, not tuning:
//
//    monochrome mode  the tint below is not idempotent, so a row that was
//                     skipped keeps its previous tinted value and darkens a
//                     little more every frame
//    mode change      the buffer last held graphics or another text width, so
//                     no cached row describes what is on screen
//
//  Steady color text hits neither, which is the case that matters -- it lets
//  AppleTextMode redraw only the rows that changed.
//
//  Render is handed a null videoRam so it reads through MemoryBus rather than
//  the CPU's memory array. Only the bus page table reflects live MMU banking
//  ($0400-$07FF and $2000-$3FFF switching between main and aux under 80STORE
//  with PAGE2 / HIRES); the //e MMU re-points those pages at buffers the
//  RamDevice owns, so reading the CPU array directly would show main memory
//  while the guest is displaying aux.
//
//  Mixed mode overlays rows 20-23 through the same RenderRowRange entry point
//  on both the 40- and 80-column renderers, so the split screen is one code
//  path with a width choice rather than two duplicated ones (FR-017a/FR-020).
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::RenderFramebuffer()
{
    ColorMode  color   = m_colorMode.load (memory_order_acquire);
    bool       flashOn = ComputeFlashOn();



    // Nothing to render before a machine is built, or after one is torn
    // down. The modes are created and cleared together, so text40 answers
    // for all of them and every use below can go straight to the refs.
    if (m_machine.GetRefs().text40 == nullptr)
    {
        return;
    }

    // A color monitor renders text white; the monochrome monitors keep the
    // text renderer's green here and the post-render tint below recolors the
    // whole frame to the selected phosphor. Flash state is pushed in from
    // emulated time (Render no longer self-advances it) so the blink
    // survives the render-skip gate.
    {
        uint32_t textOnColor = (color == ColorMode::Color)
                                   ? m_colorMonitorTextArgb.load (memory_order_acquire)
                                   : s_kMonoSourceTextBgra;

        m_machine.GetRefs().text40->SetOnColor    (textOnColor);
        m_machine.GetRefs().text40->SetFlashState (flashOn);

        m_machine.GetRefs().text80->SetOnColor    (textOnColor);
        m_machine.GetRefs().text80->SetFlashState (flashOn);

        // Both graphics modes decode from the dots differently per monitor,
        // so they need the monitor type rather than a tint of one decode.
        // In both cases the color decode has already discarded what a
        // monochrome monitor would show -- DHR collapses each 4-dot cell to
        // one palette entry, and hi-res folds the half-dot shift into a
        // color pair -- so no amount of post-tinting brings it back.
        bool monoMonitor = (color != ColorMode::Color);

        m_machine.GetRefs().hiRes->SetMonochrome       (monoMonitor);
        m_machine.GetRefs().doubleHiRes->SetMonochrome (monoMonitor);
    }

    m_machineBuilder->SelectVideoMode();

    // Dirty-row text cache: force a full re-raster when reusing last frame's
    // rows would be unsafe. (1) A monochrome color mode applies a
    // non-idempotent tint over the whole framebuffer below, so a row we skipped
    // would darken every frame. (2) A change of active mode means the buffer
    // last held graphics / another mode, so no text row can be trusted. Steady
    // color text hits neither and lets AppleTextMode redraw only changed rows.
    {
        bool forceFullText = (color != ColorMode::Color)
                          || (m_machine.GetRefs().activeVideoMode != m_prevActiveVideoMode);

        if (forceFullText)
        {
            m_machine.GetRefs().text40->InvalidateCache();
            m_machine.GetRefs().text80->InvalidateCache();
        }
    }

    m_prevActiveVideoMode = m_machine.GetRefs().activeVideoMode;

    if (m_machine.GetRefs().activeVideoMode != nullptr)
    {
        // Pass nullptr for videoRam so the renderer reads through MemoryBus.
        // The bus's page table reflects the current MMU banking state
        // (main vs aux for $0400-$07FF / $2000-$3FFF under 80STORE+PAGE2/HIRES);
        // CPU memory[] alone does not, since the //e MMU re-points pages at
        // the RamDevice / aux RAM buffers it owns.
        m_machine.GetRefs().activeVideoMode->Render (nullptr,
                                   m_cpuFramebuffer.data(),
                                   kFramebufferWidth,
                                   kFramebufferHeight);
    }

    // Mixed mode: overlay text on the bottom 4 rows (rows 20-23) via the
    // composed renderer (FR-017a / FR-020). When 80COL is active on the //e
    // we route through Apple80ColTextMode::RenderRowRange; otherwise through
    // AppleTextMode::RenderRowRange. Both share a single composed code path
    // (no branched duplicated render logic).
    if (m_machine.GetSoftSwitchMirror().mixedMode && m_machine.GetSoftSwitchMirror().graphicsMode)
    {
        static constexpr int kMixedFirstRow = 20;
        static constexpr int kMixedLastRow  = 24;

        bool  use80Col = m_machine.GetRefs().iieSoftSwitches != nullptr
                      && m_machine.GetRefs().iieSoftSwitches->Is80ColMode();

        if (use80Col)
        {
            m_machine.GetRefs().text80->SetPage2 (false);
            m_machine.GetRefs().text80->RenderRowRange (kMixedFirstRow, kMixedLastRow,
                                           nullptr,
                                           m_cpuFramebuffer.data(),
                                           kFramebufferWidth,
                                           kFramebufferHeight);
        }
        else
        {
            m_machine.GetRefs().text40->SetPage2 (m_machine.GetSoftSwitchMirror().page2);
            m_machine.GetRefs().text40->RenderRowRange (kMixedFirstRow, kMixedLastRow,
                                           nullptr,
                                           m_cpuFramebuffer.data(),
                                           kFramebufferWidth,
                                           kFramebufferHeight);
        }
    }

    // Apply monochrome tint via Video/MonochromeTint.h helpers (kept
    // out-of-line in CassoEmuCore so the BGRA arithmetic is unit-
    // testable independent of the Win32 shell).
    if (color != ColorMode::Color)
    {
        for (auto & pixel : m_cpuFramebuffer)
        {
            switch (color)
            {
                case ColorMode::GreenMono:
                    pixel = Casso::Video::TintGreenMono (pixel);
                    break;

                case ColorMode::AmberMono:
                    pixel = Casso::Video::TintAmberMono (pixel);
                    break;

                case ColorMode::WhiteMono:
                    pixel = Casso::Video::TintWhiteMono (pixel);
                    break;

                default:
                    break;
            }
        }
    }
}
