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
    bool                       haveMeshes  = false;



    haveMeshes = !monitorMesh.empty() && !driveMesh.empty();
    CBRA (haveMeshes);

    hr = m_deskScene.LoadModels (monitor.sceneKind, monitorMesh, driveMesh);
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
//  (the resources are compiled into the exe), so the guards assert; the shell
//  then simply leaves m_deskSceneReady false and the 2D chrome carries on.
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
    DeskRegionBox  doorBoxes[s_kSceneDriveMax] = {};



    m_deskScene.BuildTiltedMonitorWorld (m_deskScene.Composition(), tiltWorld);
    m_deskScene.MonitorModel().BoundsMin (monLo);
    m_deskScene.MonitorModel().BoundsMax (monHi);
    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
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
                                         doorBoxes);
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
    DeskRegionBox  doorBoxes[s_kSceneDriveMax]  = {};



    m_deskScene.DriveModel().BoundsMin (drvLo);
    m_deskScene.DriveModel().BoundsMax (drvHi);
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
                                         doorBoxes);
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

    slack = std::max (0.0f, m_sceneView.zoom - 1.0f);

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
    IDxuiTextRenderer *           text    = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    bool                          visible = DeskSceneActive() && (!fs || onStrip);
    float                         fontDip = s_kSceneDriveLabelFontDip;
    // The strip has nothing in front of its drives, so it keeps the chrome
    // label; the desk hands its names to the scene instead.
    bool                          inScene = visible && !onStrip;
    std::array<std::wstring, 2>   names;
    int                           halfW   = m_scaler.ToPx (s_kSceneDriveLabelWidthDp) / 2;
    int                           stripH  = m_scaler.ToPx (s_kSceneDriveLabelStripDp);
    int                           gapPx   = m_scaler.ToPx (s_kSceneDriveLabelGapDp);
    SIZE                          cellPx  = { halfW * 2, stripH };



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
        if (!name.empty())
        {
            rc.left   = comp.driveLabelPx[i].x - halfW;
            rc.right  = comp.driveLabelPx[i].x + halfW;
            rc.top    = comp.driveLabelPx[i].y + gapPx;
            rc.bottom = rc.top + stripH;

            if (text != nullptr)
            {
                // The same DIP-to-pixel the widget itself paints at, so the
                // width this truncates to is the width it renders.
                float  px = fontDip * (float) m_scaler.GetDpi() / 96.0f;

                name = TruncateToWidth (name, (float) (rc.right - rc.left),
                                        [text, px] (std::wstring_view run) -> float
                {
                    float    w  = 0.0f;
                    float    h  = 0.0f;
                    HRESULT  hr = text->MeasureString (std::wstring (run).c_str(), px,
                                                       DxuiTheme::kBodyFace, w, h);

                    return SUCCEEDED (hr) ? w : 0.0f;
                });
            }
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

    if (inScene)
    {
        SyncSceneDiskLabelQuads (names, cellPx, gapPx);
    }
    else
    {
        ClearSceneDiskLabels();
    }
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

void EmulatorShell::SyncSceneDiskLabelQuads (const std::array<std::wstring, 2> & names,
                                             const SIZE                        & cellPx,
                                             int                                 gapPx)
{
    const DeskSceneComposition &  comp = m_deskScene.Composition();
    IDxuiTextRenderer *           text = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    UINT                          texW = 0;
    UINT                          texH = 0;



    if (text == nullptr || (names[0].empty() && names[1].empty()))
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

        if (names[i].empty() ||
            !DeskSceneLayout::TryMakeDriveLabelQuad (comp, i, cellPx, gapPx, corners))
        {
            m_deskScene.SetDiskLabel (i, nullptr, nullptr, nullptr);
            continue;
        }

        // Against the texture's REAL size, not the size the bake asked for.
        // The renderer grows that texture and never shrinks it, so the cells
        // usually cover only part of it and a 0..1 mapping would stretch
        // whatever else is still in there across the name.
        uv[0] = 0.0f;
        uv[1] = (float) (i * cellPx.cy)       / (float) texH;
        uv[2] = (float) cellPx.cx             / (float) texW;
        uv[3] = (float) ((i + 1) * cellPx.cy) / (float) texH;

        m_deskScene.SetDiskLabel (i, m_sceneDiskLabelSrv, corners, uv);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  EmulatorShell::TryBakeSceneDiskLabels
//
//  Draws both names into one off-screen texture, stacked, and keeps the view.
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

bool EmulatorShell::TryBakeSceneDiskLabels (const std::array<std::wstring, 2> & names,
                                            const SIZE                        & cellPx)
{
    // Baked white, which is what the chrome label has always defaulted to;
    // the glow behind it is what separates it from the case.
    constexpr uint32_t           kLabelArgb = 0xFFFFFFFF;
    IDxuiTextRenderer         *  text       = (m_host != nullptr) ? m_host->GetTextRenderer() : nullptr;
    ID3D11ShaderResourceView  *  srv        = nullptr;
    float                        fontPx     = 0.0f;
    HRESULT                      hr         = S_OK;



    if (text == nullptr || cellPx.cx <= 0 || cellPx.cy <= 0)
    {
        return false;
    }

    hr = text->BeginDrawToTexture ((UINT) cellPx.cx, (UINT) (cellPx.cy * 2));

    if (FAILED (hr))
    {
        return false;
    }

    // The same DIP-to-pixel the chrome label paints at, which is also the
    // size the truncation was measured against.
    fontPx = s_kSceneDriveLabelFontDip * (float) m_scaler.GetDpi() / (float) s_kBaseDpi;

    for (int i = 0; i < (int) names.size(); i++)
    {
        if (names[i].empty())
        {
            continue;
        }

        DxuiShadowedText::PaintShadowed (*text, names[i].c_str(),
                                         0.0f, (float) (i * cellPx.cy),
                                         (float) cellPx.cx, (float) cellPx.cy,
                                         kLabelArgb, fontPx, DxuiTheme::kBodyFace,
                                         DxuiTextHAlign::Center, DxuiTextVAlign::Center,
                                         DxuiShadowedText::kGlowReachPx);
    }

    hr = text->EndDrawToTexture (&srv);

    if (FAILED (hr) || srv == nullptr)
    {
        return false;
    }

    m_sceneDiskLabelSrv  = srv;
    m_sceneDiskLabelText = names;
    m_sceneDiskLabelCell = cellPx;

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
