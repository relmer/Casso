#include "Pch.h"

#include "Shell/EmulatorShell.h"
#include "Shell/EmulatorShellInternal.h"
#include "AssetBootstrap.h"
#include "Config/MonitorCatalog.h"
#include "Config/MachineInputPrefs.h"
#include "Config/CrtPresets.h"
#include "Config/CrtResolver.h"
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
#include "Render/SceneCamera.h"
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
#include "Ui/Dialogs/DialogBodyContent.h"
#include "Ui/Dialogs/MessageDialog.h"
#include "Ui/Dialogs/SalvageDialogContent.h"
#include "Ui/Settings/SettingsPanelState.h"
#include "Ui/Settings/SettingsSheet.h"   // TEMP (T162 3a dev trigger)
#include "Seams/Win32IntentChannel.h"
#include "Devices/Disk/PreservedCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplySavedBezelTilt
//
//  Restores the tilt this MONITOR was left at. Keyed by the monitor rather
//  than by the machine, because the tilt is a property of the thing standing
//  on the desk: put the same tube in front of another machine and it is still
//  angled the way it was left.
//
//  A monitor nobody has touched has no entry, which reads as square-on -- and
//  the setter clamps whatever it finds, so a file carrying a tilt from a
//  bezel with more travel cannot push this one through its frame.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplySavedBezelTilt()
{
    const MonitorSpec &  monitor = ResolveMonitorForCurrentMachine();
    auto                 found   = m_globalPrefs.monitorTilt.find (std::string (monitor.configName));
    float                radians = (found != m_globalPrefs.monitorTilt.end()) ? found->second : 0.0f;



    m_deskScene.SetBezelTilt (radians);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PersistBezelTilt
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::PersistBezelTilt()
{
    const MonitorSpec &  monitor = ResolveMonitorForCurrentMachine();
    float                radians = m_deskScene.BezelTiltRad();



    // A SQUARE-ON MONITOR IS THE ABSENCE OF AN ENTRY, not an entry reading
    // zero. The file is meant to carry only the monitors the user has
    // actually moved, and a reset that wrote a zero would leave the entry
    // there forever, saying the user had posed this tube when they had put
    // it back.
    if (radians == 0.0f)
    {
        m_globalPrefs.monitorTilt.erase (std::string (monitor.configName));
    }
    else
    {
        m_globalPrefs.monitorTilt[std::string (monitor.configName)] = radians;
    }

    if (m_userConfigStore != nullptr)
    {
        HRESULT  hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);

        IGNORE_RETURN_VALUE (hr, S_OK);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadDeskSceneModelsForMachine
//
//  The desk wears what the machine wore. The //c gets its platinum Monitor //c
//  over the matching 5.25 drives; everything else gets the beige Monitor II
//  over Disk IIs. Pairing across the families is what reads wrong -- a //c
//  monitor standing on Disk IIs is two eras of Apple industrial design in one
//  stack, in two different shades of case plastic.
//
//  Called again on a machine switch, so the stack changes with the machine.
//  Reloading rebuilds every cached mesh (glow discs, contact shadows, badge
//  stamps), which is why the scene's own state is re-pushed afterward.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::LoadDeskSceneModelsForMachine()
{
    // WHICH MONITOR IS A PROPERTY OF THE MACHINE'S CONFIG, not a question
    // asked about its name. The drives still follow the machine, because a
    // //c's drives are part of the machine rather than of what it is plugged
    // into.
    HRESULT                    hr          = S_OK;
    bool                       isC         = MachineHasBuiltInDrive();
    const MonitorSpec &        monitor     = ResolveMonitorForCurrentMachine();
    std::span<const uint8_t>   monitorMesh = PrinterPanel::LoadBinaryResource (monitor.meshResourceId);
    std::span<const uint8_t>   driveMesh   = PrinterPanel::LoadBinaryResource (isC ? IDR_MODEL_DISK2C_MESH
                                                                                   : IDR_MODEL_DISKII_MESH);
    std::span<const uint8_t>   recorderMesh;
    bool                       haveMeshes  = false;
    bool                       haveTape    = false;



    haveMeshes = !monitorMesh.empty() && !driveMesh.empty();
    CBRA (haveMeshes);

    // The cassette recorder sits beside the stack only on a machine with
    // cassette jacks to plug it into.
    if (MachineHasCassettePort())
    {
        recorderMesh = PrinterPanel::LoadBinaryResource (IDR_MODEL_CASSETTE_RECORDER_MESH);
        haveTape     = !recorderMesh.empty();
        CBRA (haveTape);
    }

    hr = m_deskScene.LoadModels (monitor.sceneKind, monitorMesh, driveMesh, recorderMesh);
    CHRA (hr);

    m_deskSceneMachineIsC = isC;

    // The monitor that just loaded brings its own tilt with it.
    ApplySavedBezelTilt();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InitializeDeskScene
//
//  Loads the model pair the ACTIVE MACHINE wore and stands the scene renderer
//  up on the host device. Missing or unparseable model text is a build defect
//  (the resources are compiled into the exe), so the guards assert; the caller
//  then leaves m_deskSceneReady false and runs FallBackFromDeskScene.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT EmulatorShell::InitializeDeskScene()
{
    HRESULT   hr = S_OK;



    hr = m_deskScene.Initialize (m_host->GetDevice(), m_host->GetContext());
    CHRA (hr);

    hr = LoadDeskSceneModelsForMachine();
    CHRA (hr);

    // A powered monitor's lamp is lit for as long as the machine exists;
    // drive activity arrives per frame from the drive state sync.
    m_deskScene.SetPowerLampOn (true);

    ApplySceneAntiAliasing();

    {
        wchar_t   debugValue[8] = {};

        if (GetEnvironmentVariableW (L"CASSO_SCENE_DEBUG", debugValue, ARRAYSIZE (debugValue)) > 0)
        {
            m_deskSceneDebug = _wtoi (debugValue);
        }
    }

    // THE POSE READOUT, READ BACK IN. The readout on the picture exists so a
    // screenshot says where it was taken from; this is the other half, so that
    // pose can be flown to exactly instead of hunted for with the wheel. A
    // fault that only shows past sixty degrees of yaw is not reachable by
    // guesswork, and "I could not reproduce it" is the wrong answer when the
    // reporter told you the angle.
    //
    // Same five numbers the readout prints, same order, comma separated:
    //
    //     CASSO_SCENE_POSE=yaw,pitch,zoom,panX,panY[,bezelTilt]
    //
    // Degrees in, radians stored, because degrees are what the readout shows.
    // Absent or unparseable leaves the composed pose alone. The bezel's lean
    // is optional and last: it is not part of the orbit, but a fault that
    // only shows while the bezel is tilted needs it reproducible too.
    {
        wchar_t   poseValue[128] = {};

        if (GetEnvironmentVariableW (L"CASSO_SCENE_POSE", poseValue, ARRAYSIZE (poseValue)) > 0)
        {
            float  yawDeg   = 0.0f;
            float  pitchDeg = 0.0f;
            float  zoom     = 1.0f;
            float  panX     = 0.0f;
            float  panY     = 0.0f;
            float  tiltDeg  = 0.0f;
            int    got      = swscanf_s (poseValue, L"%f,%f,%f,%f,%f,%f",
                                         &yawDeg, &pitchDeg, &zoom, &panX, &panY,
                                         &tiltDeg);

            if (got >= 2)
            {
                m_sceneView.orbitYawRad   = yawDeg * 3.14159265f / 180.0f;
                m_sceneView.orbitPitchRad = pitchDeg * 3.14159265f / 180.0f;
                m_sceneView.zoom          = (got >= 3 && zoom > 0.0f) ? zoom : 1.0f;
                m_sceneView.panX          = (got >= 4) ? panX : 0.0f;
                m_sceneView.panY          = (got >= 5) ? panY : 0.0f;

                // The bezel leans independently of the orbit, and a fault
                // that only shows while it is leaning needs it reproducible
                // too. Optional, so a five-value pose still reads.
                if (got >= 6)
                {
                    m_deskScene.SetBezelTilt (tiltDeg * 3.14159265f / 180.0f);
                }

                InvalidateSceneComposition();
            }
        }
    }

    m_deskSceneReady = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::FallBackFromDeskScene
//
//  A skeuomorphic theme draws its drives only as 3D objects in the desk
//  scene, so with the scene unavailable it would show no drives at all. The
//  failure is recorded and the active theme applied again, which swaps in a
//  compact theme; every later theme change goes through the same check.
//
//  The user's chosen theme stays as it was, in the theme manager and in the
//  saved preferences, so a launch where the scene loads shows it again.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::FallBackFromDeskScene()
{
    std::string  themeName = m_globalPrefs.activeTheme;



    m_deskSceneFailed = true;

    if (m_themeManager != nullptr)
    {
        themeName = m_themeManager->GetActiveThemeName();
    }

    ApplyChromeThemeByName (themeName);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplyChromeThemeByName
//
//  Builds the chrome theme for a theme name and applies it, standing in a
//  compact theme when the named one needs the desk scene and the scene has
//  failed. The notice says why the window does not look the way the user set
//  it; without it the swap would pass for a theme setting that was lost.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplyChromeThemeByName (const std::string & themeName)
{
    bool  needsDeskScene = !CassoTheme::MakeByName (themeName).compactDrives;



    m_chromeTheme = CassoTheme::MakeByName (themeName, !m_deskSceneFailed);
    ApplyThemeToChrome (m_chromeTheme);

    if (needsDeskScene && m_deskSceneFailed)
    {
        ShowNotice (kpszDeskSceneFallbackNotice);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::EnsureSceneCalibration
//
//  Builds the CASSO_SCENE_DEBUG=2 stripe texture: back-buffer sized, with a
//  pattern in the fitted picture region expressed in FRAMEBUFFER columns --
//  red at emulated column 0, green at the last column, white every 8th, blue
//  rows top and bottom. What survives to the screen tells exactly how the
//  glass maps texels.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::EnsureSceneCalibration (const RECT & fittedRect)
{
    HRESULT                  hr     = S_OK;
    int                      bbW    = m_d3dRenderer.GetBackBufferWidth();
    int                      bbH    = m_d3dRenderer.GetBackBufferHeight();
    D3D11_TEXTURE2D_DESC     desc   = {};
    D3D11_SUBRESOURCE_DATA   init   = {};
    std::vector<uint32_t>    pixels;



    BAIL_OUT_IF (bbW <= 0 || bbH <= 0, S_OK);
    BAIL_OUT_IF (m_sceneCalibTex != nullptr && EqualRect (&m_sceneCalibRect, &fittedRect), S_OK);

    pixels.assign ((size_t) bbW * bbH, 0xFF000000);

    for (LONG y = fittedRect.top; y < fittedRect.bottom && y < bbH; y++)
    {
        for (LONG x = fittedRect.left; x < fittedRect.right && x < bbW; x++)
        {
            int        fbx   = MulDiv ((int) (x - fittedRect.left), kFramebufferWidth,
                                       (int) (fittedRect.right - fittedRect.left));
            int        fby   = MulDiv ((int) (y - fittedRect.top), kFramebufferHeight,
                                       (int) (fittedRect.bottom - fittedRect.top));
            uint32_t   color = 0xFF000000;

            if (fbx == 0)                              { color = 0xFFFF0000; }
            else if (fbx == kFramebufferWidth - 1)     { color = 0xFF00FF00; }
            else if (fby <= 1 || fby >= kFramebufferHeight - 2) { color = 0xFF4080FF; }
            else if ((fbx % 8) == 0)                   { color = 0xFFFFFFFF; }

            pixels[(size_t) y * bbW + x] = color;
        }
    }

    m_sceneCalibTex.Reset();
    m_sceneCalibSrv.Reset();

    desc.Width            = (UINT) bbW;
    desc.Height           = (UINT) bbH;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    init.pSysMem     = pixels.data();
    init.SysMemPitch = (UINT) bbW * 4;

    hr = m_host->GetDevice()->CreateTexture2D (&desc, &init, m_sceneCalibTex.GetAddressOf());
    CHR (hr);

    hr = m_host->GetDevice()->CreateShaderResourceView (m_sceneCalibTex.Get(), nullptr,
                                                        m_sceneCalibSrv.GetAddressOf());
    CHR (hr);

    m_sceneCalibRect = fittedRect;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DeskSceneHit
//
//  One frame of truth: resolves against the composition the scene last
//  rendered with, so hover, clicks, and pixels can never disagree with what
//  is on screen. With the monitor opted out the composition holds drives
//  alone, so the glass is excluded -- the flat picture is hit-tested by its
//  viewport rect on the classic paths, as it was before the scene existed.
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult EmulatorShell::DeskSceneHit (int xPx, int yPx) const
{
    float          tiltWorld[16]               = {};
    float          monLo[3]                    = {};
    float          monHi[3]                    = {};
    float          drvLo[3]                    = {};
    float          drvHi[3]                    = {};
    float          recLo[3]                    = {};
    float          recHi[3]                    = {};
    DeskRegionBox  doorBoxes[s_kSceneDriveMax] = {};



    m_deskScene.BuildTiltedMonitorWorld (m_deskScene.Composition(), tiltWorld);
    m_deskScene.MonitorModel().BoundsMin (monLo);
    m_deskScene.MonitorModel().BoundsMax (monHi);
    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
    m_deskScene.RecorderModel().BoundsMin (recLo);
    m_deskScene.RecorderModel().BoundsMax (recHi);
    BuildDriveDoorBoxes (doorBoxes);

    return DeskSceneHitTester::Classify (m_deskScene.Composition(),
                                         m_deskScene.MonitorModel().Surface(),
                                         m_deskScene.DriveModel().RegionBoxes(),
                                         (float) xPx,
                                         (float) yPx,
                                         kFramebufferWidth,
                                         kFramebufferHeight,
                                         CrtMonitorActive(),
                                         &m_deskScene.MonitorModel().TiltGrips(),
                                         tiltWorld,
                                         monLo, monHi, drvLo, drvHi,
                                         doorBoxes,
                                         m_deskScene.HasRecorder() ? recLo : nullptr,
                                         m_deskScene.HasRecorder() ? recHi : nullptr,
                                         m_deskScene.HasRecorder() ? m_deskScene.RecorderModel().KeyBoxes() : nullptr,
                                         DeskSceneModel::kRecorderKeyCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BuildDriveDoorBoxes
//
//  THE DOOR IS THE ONE PART OF A DRIVE THAT MOVES, so its click target is
//  built per frame from where the door actually is rather than read off the
//  model's fixed region list. The //c's latch travels up clear of the lid
//  when it opens, which put the very part a user is reaching for outside
//  every box the case owns -- and the target was small to begin with, since
//  the slot band alone is about seven millimeters tall.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BuildDriveDoorBoxes (DeskRegionBox (& out)[s_kSceneDriveMax]) const
{
    for (int drive = 0; drive < s_kSceneDriveMax; drive++)
    {
        float  lo[3] = {};
        float  hi[3] = {};

        out[drive]        = DeskRegionBox {};
        out[drive].region = DriveWidgetRegion::Eject;

        if (!m_deskScene.DoorHitBox (drive, lo, hi))
        {
            continue;
        }

        memcpy (out[drive].boxMin, lo, sizeof (lo));
        memcpy (out[drive].boxMax, hi, sizeof (hi));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::StripHit
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult EmulatorShell::StripHit (int xPx, int yPx) const
{
    float          drvLo[3]                     = {};
    float          drvHi[3]                     = {};
    float          recLo[3]                     = {};
    float          recHi[3]                     = {};
    DeskRegionBox  doorBoxes[s_kSceneDriveMax]  = {};



    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
    m_deskScene.RecorderModel().BoundsMin (recLo);
    m_deskScene.RecorderModel().BoundsMax (recHi);
    BuildDriveDoorBoxes (doorBoxes);

    return DeskSceneHitTester::Classify (m_stripComp,
                                         m_deskScene.MonitorModel().Surface(),
                                         m_deskScene.DriveModel().RegionBoxes(),
                                         (float) xPx,
                                         (float) yPx,
                                         kFramebufferWidth,
                                         kFramebufferHeight,
                                         false,
                                         nullptr, nullptr, nullptr, nullptr,
                                         drvLo, drvHi,
                                         doorBoxes,
                                         m_deskScene.HasRecorder() ? recLo : nullptr,
                                         m_deskScene.HasRecorder() ? recHi : nullptr,
                                         m_deskScene.HasRecorder() ? m_deskScene.RecorderModel().KeyBoxes() : nullptr,
                                         DeskSceneModel::kRecorderKeyCount);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::RecorderHit
//
//  In fullscreen the recorder exists only on the strip, and only while the
//  strip is up; the desk composition there holds the glass alone.
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult EmulatorShell::RecorderHit (int xPx, int yPx) const
{
    POINT  pt = { xPx, yPx };



    if (!m_d3dRenderer.IsFullscreen())
    {
        return DeskSceneHit (xPx, yPx);
    }

    if (m_stripRectPx.bottom > m_stripRectPx.top && PtInRect (&m_stripRectPx, pt))
    {
        return StripHit (xPx, yPx);
    }

    return SceneHitResult {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::InvalidateSceneComposition
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::InvalidateSceneComposition()
{
    RECT  client = {};



    if (m_hwnd == nullptr || !GetClientRect (m_hwnd, &client))
    {
        return;
    }

    UpdateViewportLayout (client.right - client.left, client.bottom - client.top);
    m_d3dRenderer.MarkRedrawNeeded();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ClampSceneView
//
//  Keeps the framing somewhere a user can get back from.
//
//  Pan is bounded by how much slack the zoom actually created: at 2x the
//  scene is twice the viewport, so one viewport-width of offset is exactly
//  enough to reach any edge and no more. At 1x there is no slack, so the pan
//  is dropped outright -- the fitted composition already fits, and an offset
//  there can only move it somewhere worse.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ClampSceneView()
{
    float  slack = 0.0f;



    m_sceneView.zoom = std::clamp (m_sceneView.zoom, s_kSceneZoomMin, s_kSceneZoomMax);

    // Some room even at the fitted zoom: a turn can swing part of the desk out
    // of frame, and panning is how it comes back. Half the viewport either way,
    // plus whatever zooming in adds.
    slack = s_kScenePanFloorNdc + std::max (0.0f, m_sceneView.zoom - 1.0f);

    m_sceneView.panX = std::clamp (m_sceneView.panX, -slack, slack);

    // DOWNWARD, THE DRIVE IS NOT THE BOTTOM OF THE SCENE. The mounted
    // image's name hangs below the drive, outside the composed bounds the
    // slack is measured from, so at the pan limit the drive is at the edge
    // and its name is past it -- unreachable, however far you drag.
    //
    // Clamping the name into the viewport instead was the wrong cure: it
    // detaches the label from the thing it names. Give the pan the strip's
    // own height as extra room and the name stays where it belongs.
    {
        RECT   vp    = m_deskScene.Composition().viewportPx;
        int    vh    = vp.bottom - vp.top;
        float  extra = 0.0f;

        if (vh > 0)
        {
            extra = 2.0f * (float) m_scaler.ToPx (s_kSceneDriveLabelStripDp +
                                                  s_kSceneDriveLabelGapDp) / (float) vh;
        }

        m_sceneView.panY = std::clamp (m_sceneView.panY, -slack - extra, slack + extra);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ZoomSceneAt
//
//  Zooms about a client point rather than about the viewport center, so what
//  is under the cursor stays under it. Center-anchored zoom would push the
//  part being inspected toward an edge exactly as it grew big enough to see.
//
//  The pan solve falls out of the same mapping the projection uses:
//
//      ndc = zoom * u + pan          (u == the un-framed NDC of a point)
//
//  Holding the cursor's u fixed across a zoom by k gives
//
//      pan' = c - k * (c - pan)
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ZoomSceneAt (POINT clientPt, float factor)
{
    // The box the composition was SOLVED into -- not the client rect and not
    // the glass rect. NDC is defined against this one, so anchoring the zoom
    // to anything else would drift the cursor off its target the further the
    // chrome pushed the scene around.
    RECT   box    = m_deskScene.Composition().viewportPx;
    float  width  = (float) (box.right - box.left);
    float  height = (float) (box.bottom - box.top);
    float  cx     = 0.0f;
    float  cy     = 0.0f;
    float  before = m_sceneView.zoom;
    float  k      = 1.0f;



    if (width <= 0.0f || height <= 0.0f)
    {
        return;
    }

    // Client point -> NDC. Y flips: client grows downward, NDC upward.
    cx = ((float) (clientPt.x - box.left) / width)  * 2.0f - 1.0f;
    cy = 1.0f - ((float) (clientPt.y - box.top) / height) * 2.0f;

    m_sceneView.zoom = std::clamp (before * factor, s_kSceneZoomMin, s_kSceneZoomMax);

    // The REALIZED ratio, not the requested one -- at a clamp the two differ,
    // and anchoring on the request would slide the scene under a cursor that
    // is no longer zooming.
    k = (before > 0.0f) ? (m_sceneView.zoom / before) : 1.0f;

    m_sceneView.panX = cx - k * (cx - m_sceneView.panX);
    m_sceneView.panY = cy - k * (cy - m_sceneView.panY);

    ClampSceneView();
    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ResetSceneView
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ResetSceneView()
{
    // THE BEZEL LEANS TOO, and it is part of how the scene is posed even
    // though it does not live in DeskSceneView -- it is a property of the
    // MODEL rather than of the camera. Reset has to put back everything the
    // user can move, or the one control that says "start over" leaves the
    // monitor still tipped and has to be followed by hand.
    bool  tilted = m_deskScene.BezelTiltRad() != 0.0f;



    if (m_sceneView.IsIdentity() && !tilted)
    {
        return;
    }

    m_sceneView = DeskSceneView {};

    m_deskScene.SetBezelTilt (0.0f);

    // AND THE RESET IS PERSISTED, because the tilt it undoes was. Every other
    // thing this function puts back lives only for the run, so reset had no
    // reason to touch a file -- but the bezel was saved the moment the user
    // let go of it, and leaving that entry behind made "start over" last until
    // the next launch and no further.
    if (tilted)
    {
        PersistBezelTilt();
    }

    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::DeskSceneDriveCount
//
//  The same gates the 2D widgets use: no Disk II controller means no drives
//  at all; a //c with the external drive disconnected shows only the
//  internal one.
//
//  The controller check stays first and stays separate from the config. A
//  machine can declare drive ports it cannot use -- the //c builds its IWM in
//  code from a banked-ROM test rather than from a slot -- so "is there a
//  controller" and "what is attached to it" are two questions, and answering
//  the second alone would put drives on a machine that has nowhere to run
//  them.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::DeskSceneDriveCount() const
{
    bool  hasDisk = (m_diskManager != nullptr) && m_diskManager->HasSlot6Controller();



    if (!hasDisk)
    {
        return 0;
    }

    // The //c's drives are not carded -- one is soldered in and the second
    // hangs off the back-panel disk port -- so its slot list says nothing
    // about them and the internal drive is always there.
    if (m_machine.GetConfig().slots.empty())
    {
        return ShouldShowExternalDrive() ? 2 : 1;
    }

    // A card with every port empty reports zero, which is the point of being
    // able to detach a drive at all.
    return m_machine.GetConfig().AttachedDiskIiDriveCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneViewReadout
//
//  The scene pose -- orbit, zoom and pan -- written across the middle of the
//  picture.
//
//  IT EXISTS TO MAKE A SCREENSHOT SELF-DESCRIBING. A render fault in the desk
//  scene is usually only visible through a narrow window of angles, and an
//  image does not carry the pose it was taken from -- so reproducing one means
//  guessing, and a wrong guess reads as "I cannot see the problem" when the
//  truth is "I am not looking from where you were". With the five numbers that
//  fully determine the view printed on the picture, any screenshot can be
//  restored exactly.
//
//  DEGREES, not the radians the view actually stores: these are for a person
//  to read off an image and say back. One decimal is 0.0017 rad, far finer
//  than any angle a fault survives.
//
//  ON THE MONITOR'S PROJECTED BOUNDS, whose center lands on the glass, and NOT
//  on glassRectPx despite that being the rect named for the job. Measured at
//  the composed pose, glassRectPx came back 854,875..1821,1489 -- a rect whose
//  bottom half is the monitor's base and the tops of both drives. Whatever it
//  is tracking, it is not the CRT, so anchoring here would put the pose on the
//  desk. monitorRectPx is the one chrome already lays out against and it lands
//  where the monitor does.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneViewReadout()
{
    const DeskSceneComposition &  comp      = m_deskScene.Composition();
    RECT                          rc        = {};
    wchar_t                       text[128] = {};
    bool                          posed     = comp.monitorRectPx.right > comp.monitorRectPx.left &&
                                              comp.monitorRectPx.bottom > comp.monitorRectPx.top;



    if (!m_globalPrefs.showSceneView || m_host == nullptr || !posed || !DeskSceneActive())
    {
        m_sceneViewReadout.SetVisible (false);
        return;
    }

    {
        LONG  cx = (comp.monitorRectPx.left + comp.monitorRectPx.right) / 2;
        LONG  cy = (comp.monitorRectPx.top + comp.monitorRectPx.bottom) / 2;
        LONG  hw = m_scaler.ToPx (s_kScenePoseWidthDp) / 2;
        LONG  hh = m_scaler.ToPx (s_kScenePoseHeightDp) / 2;

        rc.left   = cx - hw;
        rc.right  = cx + hw;
        rc.top    = cy - hh;
        rc.bottom = cy + hh;
    }

    //  ONE FORMATTER, shared with the screenshot metadata entry. A pose read
    //  out of a file and one read off the picture have to be the same text or
    //  they do not restore the same view, and two format strings in two files
    //  is how they stop being.
    {
        string   pose = ScreenshotMetadata::FormatScenePose (m_sceneView.orbitYawRad,
                                                             m_sceneView.orbitPitchRad,
                                                             m_sceneView.zoom,
                                                             m_sceneView.panX,
                                                             m_sceneView.panY);

        m_sceneViewReadout.SetText (wstring (pose.begin(), pose.end()).c_str());
    }


    m_sceneViewReadout.SetFontSizeDip (DxuiShadowedText::kFontDip);
    m_sceneViewReadout.SetAlign       (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
    m_sceneViewReadout.SetDpi         (m_scaler.GetDpi());
    m_sceneViewReadout.Layout         (rc, m_scaler);
    m_sceneViewReadout.SetVisible     (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::LayoutSceneCompass
//
//  The compass sits in the scene viewport's BOTTOM-RIGHT corner, inset far
//  enough that it reads as furniture of the window rather than part of the
//  machines. Hidden wherever the scene is not the thing on screen --
//  fullscreen shows the picture, the 2D paths have no scene to turn.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::LayoutSceneCompass()
{
    RECT   vp       = m_deskScene.Composition().viewportPx;
    LONG   sidePx   = m_scaler.ToPx (72);
    LONG   marginPx = m_scaler.ToPx (10);
    bool   show     = DeskSceneActive() && !m_d3dRenderer.IsFullscreen() &&
                      (vp.right - vp.left) > sidePx * 3;
    RECT   rc       = {};



    if (!show)
    {
        m_sceneCompass.SetVisible (false);
        return;
    }

    rc.right  = vp.right  - marginPx;
    rc.bottom = vp.bottom - marginPx;
    rc.left   = rc.right  - sidePx;
    rc.top    = rc.bottom - sidePx;

    m_sceneCompass.SetDpi     (m_scaler.GetDpi());
    m_sceneCompass.SetRect    (rc);
    m_sceneCompass.SetVisible (true);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneDriveChrome
//
//  The scene owns the drives: the 2D widgets hide (still syncing state for
//  the //c switch strip and the door FSM), the drag-drop hit registry is
//  rebuilt from the composition's projected drive bounds so dropping a disk
//  image on a 3D drive keeps mounting into that drive, and each drive's
//  basename label is re-hung under those same bounds.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneDriveChrome()
{
    const DeskSceneComposition &  comp = m_deskScene.Composition();



    SyncSceneDriveLabels();
    LayoutSceneCompass();

    // INVISIBLE, not merely collapsed. Hide() only empties the bounds, and a
    // visible panel with empty bounds is one stray Layout away from painting:
    // a resize arranges the docked bands before this runs, so the retired 2D
    // widgets flashed along the bottom edge for a frame under the 3D drives.
    m_driveChrome[0].SetVisible (false);
    m_driveChrome[1].SetVisible (false);
    m_driveChrome[0].Hide();
    m_driveChrome[1].Hide();

    m_uiShell.GetHitTester().Clear();

    for (int i = 0; i < comp.driveCount; i++)
    {
        if (comp.driveRectPx[i].right > comp.driveRectPx[i].left)
        {
            m_uiShell.GetHitTester().Register (DxuiHitRect { comp.driveRectPx[i], DxuiHitSlot::Custom, i });
        }
    }

    RegisterTapeDropTarget();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneDriveLabels
//
//  Hangs the mounted image's basename in a strip under each 3D drive, where
//  the 2D widget's label sat -- the name belongs on screen, not buried in a
//  hover tooltip. The strip spans the drive's projected width, so the layout
//  reserves its height above (both scene branches shrink the rect they
//  compose into by exactly that much) and the drives never sit on it.
//
//  Ellipsized through the shared pure truncation helper against the real text
//  measurement, so a long name ends in a single ellipsis instead of wrapping
//  out of the strip. The tooltip still carries the full name.
//
//  Fullscreen shows no labels: the picture owns the client and the drives are
//  only briefly on screen in the overlay strip, which has its own tooltip.
//
//  A theme with no 3D drives shows none either, and that has to be asked
//  rather than inferred from the composition. The composition is not cleared
//  when the theme changes, so its drive rects stay valid and the labels went
//  on hanging under drives that were no longer drawn, beside the flat
//  widgets' own labels.
//
//  Windowed, the labels ride the ORBIT: they re-hang under each drive's
//  projected bounds on every composition pass, so they stay legible from
//  whatever angle the inspection orbit is showing rather than vanishing the
//  moment the camera moves.
//
//  ON THE DESK THE NAME IS SCENE GEOMETRY, in the strip it stays chrome, and
//  what decides is whether anything can get in front of the drive. Orbit the
//  desk and the monitor comes between the camera and a drive, so the name
//  has to be something the depth buffer can cut. The overlay strip is a bare
//  row with nothing in front of it, so a chrome label there is occluded by
//  nothing and costs no texture.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneDriveLabels()
{
    // FULLSCREEN LABELS THE STRIP'S DRIVES, not the desk's: the overlay is
    // the only place drives appear there, and a drive worth revealing is
    // worth naming. Its composition is the strip's, and the labels come and
    // go with the slide.
    bool                          fs      = m_d3dRenderer.IsFullscreen();
    bool                          onStrip = fs && m_stripRectPx.bottom > m_stripRectPx.top &&
                                            m_stripComp.driveCount > 0;
    const DeskSceneComposition &  comp    = onStrip ? m_stripComp : m_deskScene.Composition();
    bool                          visible = DeskSceneActive() && (!fs || onStrip);
    float                         fontDip = s_kSceneDriveLabelFontDip;
    // The strip has nothing in front of its drives, so it keeps the chrome
    // label; the desk hands its names to the scene instead.
    bool                                          inScene   = visible && !onStrip;
    std::array<std::wstring, 2>                   names;
    std::array<std::wstring, s_kSceneLabelCount>  fullNames;
    int                                           halfW     = GetSceneLabelHalfWidthPx (comp);
    int                                           stripH    = m_scaler.ToPx (s_kSceneDriveLabelStripDp);
    int                                           gapPx     = m_scaler.ToPx (s_kSceneDriveLabelGapDp);
    SIZE                                          cellPx    = { halfW * 2, stripH };



    for (int i = 0; i < (int) m_sceneDriveLabel.size(); i++)
    {
        std::wstring &  name = names[i];
        RECT            rc   = {};

        if (visible && i < comp.driveCount && comp.driveRectPx[i].right > comp.driveRectPx[i].left)
        {
            name = std::filesystem::path (m_machine.GetDiskStore().GetSourcePath (6, i)).filename().wstring();

            // THE PADLOCK RIDES THE NAME, not the drive. It had been a brass
            // badge stamped on the faceplate -- on a case whose whole job is
            // to look like 1983 hardware, and no Disk II ever wore one. Here
            // it is what it actually is: a fact about the MOUNTED IMAGE,
            // sitting beside that image's name.
            //
            // Ahead of the truncation on purpose. Truncation eats the TAIL,
            // so a badge at the head survives however long the name is, and
            // nothing downstream has to keep it out of the ellipsis by hand.
            if (!name.empty() && m_driveWidgetState[i].writeProtect.Any())
            {
                name = std::wstring (s_kpszLock) + L" " + name;
            }
        }

        // A FIXED TYPE SIZE, NOT SCENE GEOMETRY. Standing the name on the
        // desk let it foreshorten and scale with the pose, which reads well
        // until the desk is small -- and then the one thing on screen whose
        // whole job is to be READ is the thing too small to read. Worse,
        // zoomed in it left the viewport entirely and could not be panned
        // back, because it hung off the drive rather than off the window.
        //
        // So it is chrome again: the same size wherever the scene is posed,
        // hung off the drive's projected anchor -- one model point rather
        // than the drive's swelling bounds, so it rides the orbit rigidly.
        //
        // The desk bakes the whole name and scrolls it under the pointer; only
        // the chrome label is cut short to fit.
        fullNames[i] = name;

        if (!name.empty())
        {
            rc.left   = comp.driveLabelPx[i].x - halfW;
            rc.right  = comp.driveLabelPx[i].x + halfW;
            rc.top    = comp.driveLabelPx[i].y + gapPx;
            rc.bottom = rc.top + stripH;
        }

        // On the strip a long name scrolls under the pointer, as on the desk,
        // whose baked names keep their own periods.
        if (onStrip)
        {
            SetStripLabelMarquee (m_sceneDriveLabel[i], i, name, rc);
        }

        m_sceneDriveLabel[i].SetText        (name);
        m_sceneDriveLabel[i].SetFontSizeDip (fontDip);
        m_sceneDriveLabel[i].SetAlign       (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        m_sceneDriveLabel[i].SetDpi         (m_scaler.GetDpi());
        m_sceneDriveLabel[i].Layout         (rc, m_scaler);
        m_sceneDriveLabel[i].SetVisible     (!name.empty() && !inScene);

        // THE RECT STAYS HONEST EITHER WAY. It anchors the write-protect
        // tooltip, and the quad covers exactly these pixels, so the hover
        // target lands on the name whichever way the name was drawn.
        m_sceneDriveLabelRect[i] = name.empty() ? RECT{} : rc;
    }

    // THE RECORDER'S LABELS ARE THE DRIVES' LABELS: the same bake, the same
    // halo, the same quads, the same scroll under the pointer -- its tape name,
    // the counter under it, and the name of the key under the pointer.
    if (visible && comp.hasRecorder != 0 && m_deskScene.HasRecorder() && MachineHasCassettePort())
    {
        TapeDeckView  view = GetTapeView();

        fullNames[s_kSceneTapeNameCell] = TapeDeckWidget::GetDisplayName (view);

        if (view.transport != TapeTransport::Empty)
        {
            fullNames[s_kSceneCounterCell] = TapeDeckWidget::FormatTime (view.positionSeconds) + L" / " +
                                             TapeDeckWidget::FormatTime (view.lengthSeconds);
        }

        if (m_recorderHoverKey >= 0 && m_recorderHoverKey < (int) DeskSceneModel::kRecorderKeyCount)
        {
            fullNames[s_kSceneKeyCell] = TapeDeckWidget::GetControlLabel ((size_t) m_recorderHoverKey);
        }
    }

    SyncStripTapeLabels (comp, onStrip, fullNames);

    if (inScene)
    {
        SyncSceneDiskLabelQuads (fullNames, cellPx, gapPx);
    }
    else
    {
        ClearSceneDiskLabels();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncStripTapeLabels
//
//  The recorder's names on the fullscreen strip, where they are chrome as
//  the drives' are: the tape name under the recorder's front edge with the
//  counter under it, and the name of the key under the pointer under that
//  key. Hidden everywhere else, where the desk bakes them instead.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncStripTapeLabels (const DeskSceneComposition                       & comp,
                                         bool                                               onStrip,
                                         const std::array<std::wstring, s_kSceneLabelCount> & names)
{
    static constexpr int  kCells[3] = { s_kSceneTapeNameCell, s_kSceneCounterCell, s_kSceneKeyCell };
    int                   halfW     = GetSceneLabelHalfWidthPx (comp);
    int                   stripH    = m_scaler.ToPx (s_kSceneDriveLabelStripDp);
    int                   gapPx     = m_scaler.ToPx (s_kSceneDriveLabelGapDp);



    for (size_t i = 0; i < m_stripTapeLabel.size(); i++)
    {
        int           cell      = kCells[i];
        int           key       = (cell == s_kSceneKeyCell) ? m_recorderHoverKey : -1;
        std::wstring  name      = onStrip ? names[(size_t) cell] : std::wstring();
        float         anchor[3] = {};
        float         screen[2] = {};
        RECT          rc        = {};

        if (!name.empty() && GetRecorderLabelAnchor (comp, key, anchor) &&
            SceneCamera::ProjectToScreen (comp.viewProj, anchor, comp.viewportPx, screen))
        {
            rc.left   = (LONG) screen[0] - halfW;
            rc.right  = (LONG) screen[0] + halfW;
            rc.top    = (LONG) screen[1] + gapPx + ((cell == s_kSceneCounterCell) ? stripH : 0);
            rc.bottom = rc.top + stripH;
        }
        else
        {
            name.clear();
        }

        if (onStrip)
        {
            SetStripLabelMarquee (m_stripTapeLabel[i], cell, name, rc);
        }

        m_stripTapeLabel[i].SetText        (name);
        m_stripTapeLabel[i].SetFontSizeDip (s_kSceneDriveLabelFontDip);
        m_stripTapeLabel[i].SetAlign       (DxuiTextHAlign::Center, DxuiTextVAlign::Center);
        m_stripTapeLabel[i].SetDpi         (m_scaler.GetDpi());
        m_stripTapeLabel[i].Layout         (rc, m_scaler);
        m_stripTapeLabel[i].SetVisible     (!name.empty());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetStripLabelMarquee
//
//  A strip label whose name is too long for its rect is shown whole and
//  scrolled while the pointer is on it, by the same clock and the same
//  measure the desk's baked names use, so the two read alike. One that fits
//  is the ordinary centered line.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetStripLabelMarquee (DxuiShadowedText & label, int cell, const std::wstring & name, const RECT & rc)
{
    HRESULT              hr     = E_FAIL;
    IDxuiTextRenderer *  text   = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    float                fontPx = s_kSceneDriveLabelFontDip * (float) m_scaler.GetDpi() / (float) s_kBaseDpi;
    float                textW  = 0.0f;
    float                textH  = 0.0f;
    float                period = 0.0f;
    int64_t              nowMs  = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                                      std::chrono::steady_clock::now().time_since_epoch()).count();



    if (text != nullptr && !name.empty())
    {
        hr = text->MeasureString (name.c_str(), fontPx, DxuiTheme::kBodyFace, textW, textH);
    }

    if (SUCCEEDED (hr) && textW > (float) (rc.right - rc.left) - 2.0f * DxuiShadowedText::kGlowReachPx)
    {
        period = ceilf (textW + (float) m_scaler.ToPx (s_kSceneLabelScrollGapDp));
    }

    m_sceneDiskLabelPeriod[(size_t) cell] = period;

    label.SetMarquee (period, (period > 0.0f) ? GetSceneLabelScrollPx (cell, nowMs) : 0.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdateSceneLabelHover
//
//  Which desk drive the pointer is on, by its face or its name, so that
//  drive's name can scroll. Returns whether that changed.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::UpdateSceneLabelHover (int x, int y, int64_t nowMs)
{
    // In fullscreen the names are the strip's, while it is up.
    bool                          fs      = m_d3dRenderer.IsFullscreen();
    bool                          onStrip = fs && m_stripRectPx.bottom > m_stripRectPx.top;
    const DeskSceneComposition &  comp    = onStrip ? m_stripComp : m_deskScene.Composition();
    POINT                         pt      = { x, y };
    int                           hovered = -1;
    bool                          changed = false;



    if (DeskSceneActive() && (!fs || onStrip))
    {
        for (int i = 0; i < comp.driveCount && i < (int) m_sceneDriveLabelRect.size(); i++)
        {
            if (PtInRect (&comp.driveRectPx[i], pt) || PtInRect (&m_sceneDriveLabelRect[i], pt))
            {
                hovered = i;
                break;
            }
        }

        if (hovered < 0 && (PtInRect (&comp.recorderRectPx, pt) || PtInRect (&m_sceneTapeNameRect, pt)))
        {
            hovered = s_kSceneTapeNameCell;
        }
    }

    changed = hovered != m_sceneLabelHover;

    if (changed)
    {
        m_sceneLabelHover   = hovered;
        m_sceneLabelHoverMs = nowMs;
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetSceneLabelScrollPx
//
//  How far along its double bake a desk name's window has slid: zero for a
//  name that fits or a drive the pointer is not on.
//
////////////////////////////////////////////////////////////////////////////////

float EmulatorShell::GetSceneLabelScrollPx (int drive, int64_t nowMs)
{
    float    period   = m_sceneDiskLabelPeriod[(size_t) drive];
    float    speed    = (float) m_scaler.ToPx (1) * s_kSceneLabelScrollDipPerSec;
    int64_t  scrollMs = 0;



    if (drive != m_sceneLabelHover || period <= 0.0f || speed <= 0.0f)
    {
        return 0.0f;
    }

    // Finished and still under the pointer: go again after the hold.
    scrollMs = (int64_t) (period / speed * 1000.0f);

    if (nowMs - (m_sceneLabelHoverMs + scrollMs) >= s_kSceneLabelScrollHoldMs)
    {
        m_sceneLabelHoverMs = nowMs;
    }

    return TapeDeckWidget::GetMarqueeOffset (nowMs, m_sceneLabelHoverMs, period, speed);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncRecorderKeys
//
//  Stands the desk recorder's keys as they are latched (LatchRecorderKeys):
//  RECORD, REW, FF and PLAY stay down from the press until STOP, EJECT or a
//  reset, whatever the tape does meanwhile. STOP and EJECT dip for a moment
//  when clicked. Returns whether a key is still moving.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::SyncRecorderKeys (int64_t nowMs)
{
    constexpr float                                       kTravelMm = 6.0f;   // how far a key goes down
    constexpr int64_t                                     kDipMs    = 160;   // held down long enough to reach the bottom
    constexpr size_t                                      kRecord   = 0, kRewind = 1, kForward = 2, kPlay = 3;
    TapeDeckView                                          view      = GetTapeView();
    TapeTransport                                         transport = view.transport;
    std::array<float, DeskSceneModel::kRecorderKeyCount>  depths    = {};
    bool                                                  dipping   = false;



    // A reset, a power cycle or an empty deck lets every key back up, and so
    // does a STOP or EJECT once it has reached the bottom of its stroke.
    if (m_machine.GetTapeResetCount() != m_seenTapeResets || transport == TapeTransport::Empty ||
        (m_recorderReleaseAtMs != 0 && nowMs >= m_recorderReleaseAtMs))
    {
        m_seenTapeResets      = m_machine.GetTapeResetCount();
        m_recorderReleaseAtMs = 0;
        m_recorderKeyLatched.fill (false);
    }

    // A wind key lets go by itself once the deck has wound to the end it was
    // winding toward and stopped there.
    if (m_recorderKeyLatched[kRewind] && transport != TapeTransport::Rewinding && view.positionSeconds <= 0.0)
    {
        m_recorderKeyLatched[kRewind] = false;
    }

    if (m_recorderKeyLatched[kForward] && transport != TapeTransport::FastForwarding &&
        view.positionSeconds >= view.lengthSeconds)
    {
        m_recorderKeyLatched[kForward] = false;
    }

    dipping = m_recorderReleaseAtMs != 0 || m_recorderHeldKey >= 0;

    for (size_t key = 0; key < depths.size(); key++)
    {
        depths[key] = (m_recorderKeyLatched[key] || (int) key == m_recorderHeldKey) ? kTravelMm : 0.0f;
    }

    for (size_t key = 0; key < depths.size(); key++)
    {
        int64_t  since = nowMs - m_recorderKeyDipMs[key];

        if (m_recorderKeyDipMs[key] != 0 && since >= 0 && since < kDipMs)
        {
            depths[key] = kTravelMm;
            dipping     = true;
        }
    }

    // A KEY IS PUSHED DOWN AND SPRINGS BACK. Going down it starts slow and
    // speeds up to the bottom, as under a finger; coming up it returns fast
    // and at an even speed, as a spring sends it.
    {
        constexpr float  kDownMs  = (float) s_kRecorderKeyDownMs;
        constexpr float  kUpMs    = 35.0f;     // a full stroke back up
        float            elapsed  = (m_recorderKeyStepMs == 0) ? 0.0f : clamp ((float) (nowMs - m_recorderKeyStepMs), 0.0f, 100.0f);

        m_recorderKeyStepMs = nowMs;

        for (size_t key = 0; key < depths.size(); key++)
        {
            float &    shown = m_recorderKeyShownMm[key];
            int64_t &  start = m_recorderKeyDownMs[key];

            if (depths[key] > 0.0f)
            {
                float  p = 0.0f;

                if (start == 0)
                {
                    start                      = nowMs;
                    m_recorderKeyDownFrom[key] = shown;
                }

                p     = min (1.0f, (float) (nowMs - start) / kDownMs);
                shown = m_recorderKeyDownFrom[key] + (kTravelMm - m_recorderKeyDownFrom[key]) * p * p;
            }
            else
            {
                start = 0;
                shown = max (0.0f, shown - kTravelMm * elapsed / kUpMs);
            }

            dipping = dipping || shown != depths[key];
        }
    }

    // THE DOOR stands open with no tape in -- after Eject, or before the
    // first tape -- and closes over one. It eases both ways, and the cassette
    // behind it goes and comes with the tape.
    {
        constexpr float  kOpenMs   = 320.0f;
        constexpr float  kOpenRad  = 35.0f * 3.14159265f / 180.0f;
        bool             isEmpty   = transport == TapeTransport::Empty;
        float            elapsed   = (m_recorderLidStepMs == 0) ? 0.0f : clamp ((float) (nowMs - m_recorderLidStepMs), 0.0f, 100.0f);
        float            p         = 0.0f;

        m_recorderLidStepMs = nowMs;
        m_recorderLidOpen   = clamp (m_recorderLidOpen + (isEmpty ? 1.0f : -1.0f) * elapsed / kOpenMs, 0.0f, 1.0f);
        p                   = m_recorderLidOpen * m_recorderLidOpen * (3.0f - 2.0f * m_recorderLidOpen);
        dipping             = dipping || (m_recorderLidOpen > 0.0f && m_recorderLidOpen < 1.0f);

        m_deskScene.SetRecorderLid (kOpenRad * p, !isEmpty);
    }

    m_deskScene.SetRecorderKeyDepths (m_recorderKeyShownMm);

    return dipping;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneTapeLabel
//
//  The desk recorder's tape name, with its counter under it, hung below the
//  recorder's front edge as the drives' names hang below theirs: the name
//  opens the picker and the counter the position dialog. Re-hung every frame,
//  since the counter runs and the orbit moves the recorder.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneTapeLabel()
{
    // In fullscreen the recorder is on the strip, while the strip is up.
    bool                          fs        = m_d3dRenderer.IsFullscreen();
    bool                          onStrip   = fs && m_stripRectPx.bottom > m_stripRectPx.top;
    const DeskSceneComposition &  comp      = onStrip ? m_stripComp : m_deskScene.Composition();
    bool                          visible   = DeskSceneActive() && (!fs || onStrip) &&
                                              comp.hasRecorder != 0 && m_deskScene.HasRecorder() &&
                                              MachineHasCassettePort();
    TapeDeckView                  view      = GetTapeView();
    float                         anchor[3] = {};
    float                         screen[2] = {};
    int                           halfW     = GetSceneLabelHalfWidthPx (comp);
    int                           stripH    = m_scaler.ToPx (s_kSceneDriveLabelStripDp);
    int                           gapPx     = m_scaler.ToPx (s_kSceneDriveLabelGapDp);
    std::wstring                  shown;



    m_sceneTapeNameRect    = {};
    m_sceneTapeCounterRect = {};

    // Where the name and counter stand on screen, for the clicks on them.
    if (visible && GetRecorderLabelAnchor (comp, -1, anchor) &&
        SceneCamera::ProjectToScreen (comp.viewProj, anchor, comp.viewportPx, screen))
    {
        m_sceneTapeNameRect    = { (LONG) screen[0] - halfW, (LONG) screen[1] + gapPx,
                                   (LONG) screen[0] + halfW, (LONG) screen[1] + gapPx + stripH };
        m_sceneTapeCounterRect = { m_sceneTapeNameRect.left,  m_sceneTapeNameRect.bottom,
                                   m_sceneTapeNameRect.right, m_sceneTapeNameRect.bottom + stripH };
    }

    // The labels themselves are baked with the drives' names, so any change
    // to what they say -- the counter ticking, a key under the pointer --
    // re-runs that bake.
    if (visible)
    {
        shown = TapeDeckWidget::GetDisplayName (view) + L"|" +
                TapeDeckWidget::FormatTime (view.positionSeconds) + L"|" +
                std::to_wstring (m_recorderHoverKey) + L"|" + std::to_wstring ((int) view.transport);
    }

    if (shown != m_sceneTapeLabelShown)
    {
        m_sceneTapeLabelShown = shown;
        SyncSceneDriveLabels();
        m_d3dRenderer.MarkRedrawNeeded();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetRecorderLabelAnchor
//
//  The world point a recorder label hangs from: the middle of the recorder's
//  front edge at the desk for its tape name (key -1), or the front of a key
//  for that key's name.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::GetRecorderLabelAnchor (const DeskSceneComposition & comp, int key, float anchor[3])
{
    float  lo[3]    = {};
    float  hi[3]    = {};
    float  model[3] = {};



    if (key < 0)
    {
        m_deskScene.RecorderModel().BoundsMin (lo);
        m_deskScene.RecorderModel().BoundsMax (hi);

        model[0] = (lo[0] + hi[0]) * 0.5f;
        model[1] = lo[1];
        model[2] = lo[2];
    }
    else if (key < (int) DeskSceneModel::kRecorderKeyCount)
    {
        const float *  box = m_deskScene.RecorderModel().KeyBoxes() + key * 6;

        model[0] = (box[0] + box[3]) * 0.5f;
        model[1] = box[1];
        model[2] = box[2];
    }
    else
    {
        return false;
    }

    return SceneCamera::TransformPoint (comp.recorderWorld, model, anchor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetSceneLabelHalfWidthPx
//
//  Half the width every name under the devices gets: the full label width,
//  but never more than half the space between two neighbors' names, so a
//  small scene -- a narrow window, the fullscreen strip -- cannot run one
//  name into the next. What no longer fits scrolls under the pointer.
//
////////////////////////////////////////////////////////////////////////////////

int EmulatorShell::GetSceneLabelHalfWidthPx (const DeskSceneComposition & comp)
{
    std::vector<float>  centers;
    float               anchor[3] = {};
    float               screen[2] = {};
    int                 halfW     = m_scaler.ToPx (s_kSceneDriveLabelWidthDp) / 2;
    int                 margin    = m_scaler.ToPx (s_kSceneLabelNeighborGapDp);



    for (int i = 0; i < comp.driveCount; i++)
    {
        centers.push_back ((float) comp.driveLabelPx[i].x);
    }

    if (comp.hasRecorder != 0 && m_deskScene.HasRecorder() && GetRecorderLabelAnchor (comp, -1, anchor) &&
        SceneCamera::ProjectToScreen (comp.viewProj, anchor, comp.viewportPx, screen))
    {
        centers.push_back (screen[0]);
    }

    std::sort (centers.begin(), centers.end());

    for (size_t i = 1; i < centers.size(); i++)
    {
        halfW = std::min (halfW, (int) ((centers[i] - centers[i - 1]) / 2.0f) - margin);
    }

    return std::max (halfW, margin);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TryMakeSceneLabelQuad
//
//  Where a baked label stands: a drive's under the drive, the recorder's tape
//  name under its front edge with the counter under that, and a key's name
//  under the key.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryMakeSceneLabelQuad (const DeskSceneComposition & comp, int cell, const SIZE & cellPx,
                                           int gapPx, float corners[4][3])
{
    float  anchor[3] = {};



    if (cell < s_kSceneTapeNameCell)
    {
        return DeskSceneLayout::TryMakeDriveLabelQuad (comp, cell, cellPx, gapPx, corners);
    }

    if (comp.hasRecorder == 0 ||
        !GetRecorderLabelAnchor (comp, (cell == s_kSceneKeyCell) ? m_recorderHoverKey : -1, anchor))
    {
        return false;
    }

    return DeskSceneLayout::TryMakeLabelQuad (comp, anchor, cellPx,
                                              (cell == s_kSceneCounterCell) ? gapPx + cellPx.cy : gapPx,
                                              corners);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SyncSceneDiskLabelQuads
//
//  Puts both names in the scene: one baked texture, two camera-facing quads.
//
//  THE TEXTURE IS BAKED ON A CHANGE, THE QUADS ARE SOLVED EVERY PASS. A name
//  changes when a disk is mounted; the quad changes whenever the camera
//  moves, because holding a constant pixel size at a moving distance is
//  exactly what it is for.
//
//  The two cells are stacked in one texture and each quad takes its own half
//  through its uv rect. Both halves are the same size, so drive 1's cell is
//  simply the second one down.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SyncSceneDiskLabelQuads (const std::array<std::wstring, s_kSceneLabelCount> & names,
                                             const SIZE                                         & cellPx,
                                             int                                                  gapPx)
{
    const DeskSceneComposition  & comp  = m_deskScene.Composition();
    IDxuiTextRenderer           * text  = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    UINT                          texW  = 0;
    UINT                          texH  = 0;
    int64_t                       nowMs = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now().time_since_epoch()).count();



    if (text == nullptr || std::all_of (names.begin(), names.end(), [] (const std::wstring & n) { return n.empty(); }))
    {
        ClearSceneDiskLabels();
        return;
    }

    if (names != m_sceneDiskLabelText ||
        cellPx.cx != m_sceneDiskLabelCell.cx || cellPx.cy != m_sceneDiskLabelCell.cy)
    {
        if (!TryBakeSceneDiskLabels (names, cellPx))
        {
            ClearSceneDiskLabels();
            return;
        }
    }

    text->GetDrawToTextureSize (texW, texH);

    if (m_sceneDiskLabelSrv == nullptr || texW == 0 || texH == 0)
    {
        ClearSceneDiskLabels();
        return;
    }

    for (int i = 0; i < (int) names.size(); i++)
    {
        float  corners[4][3] = {};
        float  uv[4]         = {};

        if (names[i].empty() || !TryMakeSceneLabelQuad (comp, i, cellPx, gapPx, corners))
        {
            m_deskScene.SetDiskLabel (i, nullptr, nullptr, nullptr);
            continue;
        }

        // Against the texture's REAL size, not the size the bake asked for.
        // The renderer grows that texture and never shrinks it, so the cells
        // usually cover only part of it and a 0..1 mapping would stretch
        // whatever else is still in there across the name.
        //
        // A scrolling name slides its window along the double bake.
        {
            float  scroll = GetSceneLabelScrollPx (i, nowMs);

            float  top = (float) GetSceneLabelCellTopPx (i, cellPx);

            uv[0] = scroll                        / (float) texW;
            uv[1] = top                           / (float) texH;
            uv[2] = (scroll + (float) cellPx.cx)  / (float) texW;
            uv[3] = (top + (float) cellPx.cy)     / (float) texH;
        }

        m_deskScene.SetDiskLabel (i, m_sceneDiskLabelSrv, corners, uv);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::GetSceneLabelCellTopPx
//
//  Where a label's cell starts in the baked texture. The cells are stacked
//  with a glow's reach of empty texture above and below each: a name's glow
//  spills past its cell, and with the cells touching that spill showed in
//  the next cell down as a hard dark band along its top.
//
////////////////////////////////////////////////////////////////////////////////

LONG EmulatorShell::GetSceneLabelCellTopPx (int cell, const SIZE & cellPx)
{
    LONG  pad = (LONG) ceilf (DxuiShadowedText::kGlowReachPx);



    return cell * (cellPx.cy + 2 * pad) + pad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TryBakeSceneDiskLabels
//
//  Draws every label into one off-screen texture, stacked, and keeps the view.
//
//  ONE TEXTURE FOR THE PAIR because the text renderer owns exactly one: its
//  view is replaced by the next BeginDrawToTexture, so baking a label per
//  drive leaves the first drive pointing at the second drive's name. Stacking
//  the cells is what makes a single bake serve both.
//
//  Painted with the same static, color and glow reach the chrome label uses,
//  so moving a name into the scene does not restyle it.
//
//  THE SHADOW IS BAKED IN, not painted over the scene afterwards. The name is
//  geometry now and can be occluded; a halo laid on in screen space would
//  stay flat on the glass while the text it belongs to went behind the case.
//
////////////////////////////////////////////////////////////////////////////////

bool EmulatorShell::TryBakeSceneDiskLabels (const std::array<std::wstring, s_kSceneLabelCount> & names,
                                            const SIZE                                         & cellPx)
{
    // Baked white, which is what the chrome label has always defaulted to;
    // the glow behind it is what separates it from the case.
    constexpr uint32_t           kLabelArgb = 0xFFFFFFFF;
    IDxuiTextRenderer         *  text       = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    ID3D11ShaderResourceView  *  srv        = nullptr;
    float                        fontPx     = 0.0f;
    LONG                         bakeW      = cellPx.cx;
    HRESULT                      hr         = S_OK;



    if (text == nullptr || cellPx.cx <= 0 || cellPx.cy <= 0)
    {
        return false;
    }

    // The same DIP-to-pixel the chrome label paints at.
    fontPx = s_kSceneDriveLabelFontDip * (float) m_scaler.GetDpi() / (float) s_kBaseDpi;

    // A NAME TOO LONG FOR ITS CELL IS BAKED WHOLE, TWICE, a gap apart, so the
    // quad can slide its window along it and come back to the start without a
    // seam -- the flat widget's marquee, done in texture coordinates so
    // scrolling never re-bakes. The texture widens to hold it.
    for (int i = 0; i < (int) names.size(); i++)
    {
        float    textW     = 0.0f;
        float    textH     = 0.0f;
        HRESULT  hrMeasure = E_FAIL;

        m_sceneDiskLabelPeriod[i] = 0.0f;

        if (!names[i].empty())
        {
            hrMeasure = text->MeasureString (names[i].c_str(), fontPx, DxuiTheme::kBodyFace, textW, textH);
        }

        if (SUCCEEDED (hrMeasure) && textW > (float) cellPx.cx - 2.0f * DxuiShadowedText::kGlowReachPx)
        {
            m_sceneDiskLabelPeriod[i] = ceilf (textW + (float) m_scaler.ToPx (s_kSceneLabelScrollGapDp));
            bakeW = max (bakeW, cellPx.cx + (LONG) m_sceneDiskLabelPeriod[i]);
        }
    }

    hr = text->BeginDrawToTexture ((UINT) bakeW, (UINT) GetSceneLabelCellTopPx ((int) names.size(), cellPx));

    if (FAILED (hr))
    {
        return false;
    }

    for (int i = 0; i < (int) names.size(); i++)
    {
        float  period = m_sceneDiskLabelPeriod[i];
        float  glow   = DxuiShadowedText::kGlowReachPx;

        if (names[i].empty())
        {
            continue;
        }

        if (period <= 0.0f)
        {
            DxuiShadowedText::PaintShadowed (*text, names[i].c_str(),
                                             0.0f, (float) GetSceneLabelCellTopPx (i, cellPx),
                                             (float) cellPx.cx, (float) cellPx.cy,
                                             kLabelArgb, fontPx, DxuiTheme::kBodyFace,
                                             DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                                             DxuiShadowedText::kGlowReachPx);
            continue;
        }

        for (float x : { glow, glow + period })
        {
            DxuiShadowedText::PaintShadowed (*text, names[i].c_str(),
                                             x, (float) GetSceneLabelCellTopPx (i, cellPx),
                                             period, (float) cellPx.cy,
                                             kLabelArgb, fontPx, DxuiTheme::kBodyFace,
                                             DxuiTextHAlign::Left, DxuiTextVAlign::Center,
                                             DxuiShadowedText::kGlowReachPx);
        }
    }

    hr = text->EndDrawToTexture (&srv);

    if (FAILED (hr) || srv == nullptr)
    {
        return false;
    }

    m_sceneDiskLabelSrv  = srv;
    m_sceneDiskLabelText = names;
    m_sceneDiskLabelCell = cellPx;

    // Redrawn into the same texture, so the scene's cached picture has to be
    // told -- or the counter only moves when something else, such as the
    // camera, redraws the scene.
    m_deskScene.OnLabelsRebaked();

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ClearSceneDiskLabels
//
//  Retires both quads and forgets what was baked, so the next name that needs
//  one bakes rather than reusing a texture drawn for a different cell.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ClearSceneDiskLabels()
{
    for (int i = 0; i < (int) m_sceneDiskLabelText.size(); i++)
    {
        m_deskScene.SetDiskLabel (i, nullptr, nullptr, nullptr);
        m_sceneDiskLabelText[i].clear();
    }

    m_sceneDiskLabelSrv  = nullptr;
    m_sceneDiskLabelCell = SIZE {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::SetSceneAntiAliasing
//
//  The scene's multisampling, in samples (1 / 2 / 4). Applies to the very next
//  frame -- the renderer drops its offscreen targets and rebuilds them at the
//  new count -- so the user sees the trade they just made without restarting.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::SetSceneAntiAliasing (int samples)
{
    HRESULT  hr     = S_OK;
    int      wanted = (samples >= 4) ? 4 : ((samples >= 2) ? 2 : 1);



    BAIL_OUT_IF (m_globalPrefs.sceneAntiAliasing == wanted, S_OK);

    m_globalPrefs.sceneAntiAliasing = wanted;

    ApplySceneAntiAliasing();

    if (m_userConfigStore != nullptr)
    {
        hr = m_userConfigStore->SaveAll (m_globalPrefs, m_uiFs);
    }
    else
    {
        hr = m_globalPrefs.Save (m_machine.GetAssetBaseDir(), m_uiFs);
    }

    IGNORE_RETURN_VALUE (hr, S_OK);

    m_d3dRenderer.MarkRedrawNeeded();

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::ApplySceneAntiAliasing
//
//  Pushes the stored count at every renderer that draws the scene. Separate
//  from the setter because startup has to apply it too, without re-saving the
//  file it was just read from.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::ApplySceneAntiAliasing()
{
    m_deskScene.SetSampleCount ((UINT) m_globalPrefs.sceneAntiAliasing);
}





// (LayoutPrinterIndicator deleted: the standalone printer indicator is
// retired -- the toolbar's Printer button carries the status LED now.)





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::PanSceneByNotch
//
//  A touchpad slide, in wheel notches, moved into the scene's pan.
//
//  DECLINED AT 1x, exactly as the touch pan is: with the scene framed to fit,
//  there is nowhere to pan to, and claiming the message would only take the
//  slide away from whatever else might want it. Returning NotHandled leaves
//  it to be scrolled by something that can.
//
//  THE SIGNS MATCH THE TOUCH DRAG, not the scrollbar. Windows' own convention
//  for a wheel is that the viewport follows the fingers, so content appears to
//  go the other way; a DRAG is the opposite bargain -- the content is what you
//  have hold of, and it goes where your fingers go, the way it does on a map.
//  Since this gesture exists to be the touchpad's version of the one-finger
//  touch pan, it copies that one's relationship to the hand.
//
//  Which took two goes, because the reasoning above does not settle the sign
//  on its own: it says the scene follows the fingers, and then you still have
//  to know which way Windows reports fingers going. It reports a downward
//  slide as a NEGATIVE vertical delta and a rightward slide as a POSITIVE
//  horizontal one -- opposite senses, on the same hand movement -- so one of
//  the two axes was always going to come out backward from a single rule.
//
////////////////////////////////////////////////////////////////////////////////

DxuiMessageResult EmulatorShell::PanSceneByNotch (float notch, bool horizontal)
{
    if (m_sceneView.zoom <= 1.0f)
    {
        return DxuiMessageResult::NotHandled;
    }

    if (horizontal)
    {
        m_sceneView.panX -= notch * s_kScenePanStep;
    }
    else
    {
        m_sceneView.panY -= notch * s_kScenePanStep;
    }

    ClampSceneView();
    InvalidateSceneComposition();

    return DxuiMessageResult::Handled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OrbitSceneBy / BeginSceneOrbit / UpdateSceneOrbit
//
//  The inspection orbit: the camera swings about its gaze target so every
//  side of the devices can be looked at. The signs follow the touch drag's
//  bargain, the same one the pan keeps -- the CONTENT goes where the fingers
//  go. Dragging right pushes the stack's front to the right, which shows its
//  left flank, which is the eye swinging the OTHER way; dragging down tips
//  the top toward the viewer, which is the eye rising. Hence yaw takes the
//  negative of the drag and pitch the positive.
//
//  Yaw wraps rather than clamps -- spinning past the back and around is the
//  point. Pitch is bounded here only loosely, against unbounded wind-up while
//  pinned; the REAL elevation clamp lives in the layout, on the total, where
//  the seat's own baseline is known.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::OrbitSceneBy (float yawRad, float pitchRad)
{
    constexpr float  kTwoPi = 6.2831853f;



    m_sceneView.orbitYawRad += yawRad;

    if (m_sceneView.orbitYawRad > 3.1415927f)
    {
        m_sceneView.orbitYawRad -= kTwoPi;
    }
    else if (m_sceneView.orbitYawRad < -3.1415927f)
    {
        m_sceneView.orbitYawRad += kTwoPi;
    }

    m_sceneView.orbitPitchRad = std::clamp (m_sceneView.orbitPitchRad + pitchRad,
                                            -s_kOrbitPitchLimit, s_kOrbitPitchLimit);

    InvalidateSceneComposition();
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::BeginSceneOrbit
//
//  Arms an orbit drag at the press: everything after is absolute from this
//  anchor, so the drag tracks the pointer exactly and cannot creep.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::BeginSceneOrbit (int x, int y)
{
    m_sceneOrbiting      = true;
    m_sceneOrbitMoved    = false;
    m_sceneOrbitStartPx  = POINT { x, y };
    m_sceneOrbitStartYaw = m_sceneView.orbitYawRad;
    m_sceneOrbitStartPit = m_sceneView.orbitPitchRad;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::OrbitRadPerPx
//
//  Radians per pixel of drag, FROM THE VIEWPORT, not a constant. The
//  coordinates the handlers see are DPI-scaled, so a fixed radians-per-pixel
//  was twice as touchy at 200% as at 100% -- a forty-degree drag came out
//  eighty, and the first captures of this feature were of poses nobody had
//  asked for. Tying the sweep to the viewport's width makes the same hand
//  motion the same turn on every monitor: a drag across the window is
//  s_kOrbitDragSweepRad, wherever it happens.
//
////////////////////////////////////////////////////////////////////////////////

float EmulatorShell::OrbitRadPerPx() const
{
    RECT   box = m_deskScene.Composition().viewportPx;
    float  w   = (float) (box.right - box.left);



    return s_kOrbitDragSweepRad / (std::max) (w, 200.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::UpdateSceneOrbit
//
//  Absolute from the press's anchor, like the pan: a long drag tracks the
//  cursor exactly and cannot creep.
//
////////////////////////////////////////////////////////////////////////////////

void EmulatorShell::UpdateSceneOrbit (int x, int y)
{
    float  radPerPx = OrbitRadPerPx();



    m_sceneView.orbitYawRad   = m_sceneOrbitStartYaw
                              - (float) (x - m_sceneOrbitStartPx.x) * radPerPx;
    m_sceneView.orbitPitchRad = std::clamp (
        m_sceneOrbitStartPit + (float) (y - m_sceneOrbitStartPx.y) * radPerPx,
        -s_kOrbitPitchLimit, s_kOrbitPitchLimit);

    InvalidateSceneComposition();
}
