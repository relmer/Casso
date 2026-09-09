#include "Pch.h"

#include "Ui/Scene/DeskScene.h"

#include "Render/SceneCamera.h"




// The room's two ceiling fixtures in WORLD space -- the one basis every
// device's placement is expressed in, so it is the only place they can live
// if shading and shadowing are to agree about where the light is.
//
// World is the desk's axes remapped by MakeDeviceWorld: desk x stays x, desk
// z (up) becomes world y, and desk y (back) becomes world -z. World y = 0 is
// the desk surface, which is where the drives stand.
static constexpr float   s_kRoomLightsWorld[2][3] =
{
    { -610.0f, 1524.0f, 450.0f },     // 2 ft left, 5 ft up, slightly forward
    { 2134.0f, 1524.0f, 450.0f },     // 7 ft right, same height
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::Initialize (ID3D11Device * device, ID3D11DeviceContext * context)
{
    m_context = context;

    return m_renderer.Initialize (device, context);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::Shutdown()
{
    m_renderer.Shutdown();
    m_context      = nullptr;
    m_modelsLoaded = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::LoadModels
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::LoadModels (DeskDeviceKind             monitorKind,
                               std::span<const uint8_t>   monitorMesh,
                               std::span<const uint8_t>   driveMesh)
{
    DeskDeviceKind  driveKind = (monitorKind == DeskDeviceKind::Monitor2c)
                                ? DeskDeviceKind::Disk2c : DeskDeviceKind::DiskII;
    HRESULT         hr        = S_OK;



    hr = m_monitor.Load (monitorKind, monitorMesh);
    CHRA (hr);

    // The drive that comes with the monitor. They are never mixed -- the //c
    // stands over its platinum 5.25s and the //e over Disk IIs -- so pairing
    // them here beats making every caller say it twice and disagree once.
    hr = m_drive.Load (driveKind, driveMesh);
    CHRA (hr);

    // The per-drive legend: the number differs per drive, so it is stamped
    // here rather than into the shared model. Everything about how it is set
    // -- its margin, size, color and finish -- belongs with the rest of the
    // faceplate furniture and stays in the model.
    //
    // ONLY THE DISK II WEARS ONE. The //c drive has no drive-number legend on
    // the real hardware, and stamping the Disk II's landed it off the top of
    // a face 20 mm shorter -- printing it on the monitor standing on the
    // drive rather than on the drive.
    m_driveLabelVerts[0].clear();
    m_driveLabelVerts[1].clear();

    if (driveKind == DeskDeviceKind::DiskII)
    {
        DeskSceneModel::StampDriveLabel (m_driveLabelVerts[0], 1);
        DeskSceneModel::StampDriveLabel (m_driveLabelVerts[1], 2);
    }

    BuildDerivedGeometry();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::AdoptModelsFrom
//
//  Takes another scene's already-parsed models instead of parsing the same
//  text again. A DeskSceneModel is pure CPU vertex data -- nothing in it
//  belongs to a device -- so a second scene on a DIFFERENT device can share
//  it outright, and only the derived geometry has to be rebuilt.
//
//  This is what keeps the settings sheet cheap to open. Parsing costs a
//  moment; the lamp's occlusion bake, which traces a ray from the lens to
//  every nearby face, costs considerably more, and paying it a second time
//  stalled the Theme tab visibly on the way in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::AdoptModelsFrom (const DeskScene & other)
{
    HRESULT  hr = S_OK;



    CBRA (other.m_modelsLoaded);

    m_monitor = other.m_monitor;
    m_drive   = other.m_drive;

    m_driveLabelVerts[0] = other.m_driveLabelVerts[0];
    m_driveLabelVerts[1] = other.m_driveLabelVerts[1];

    BuildDerivedGeometry();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildDerivedGeometry
//
//  Everything cached FROM the models: state-free glow discs and the contact
//  shadows, plus the dirty flags that make the per-frame copies rebuild.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildDerivedGeometry()
{
    // THE POSED DOORS BELONG TO THE MODEL THAT WAS POSED, and the model has
    // just been replaced. They are cached until the door's progress moves --
    // which a machine switch does not move, since both machines' doors are
    // equally shut -- so the old drive's door survived the swap and drew
    // over the new one: the //c's lever and its keycap-gray latch printed on
    // a Disk II's faceplate, which is exactly as wrong as it sounds. The
    // progress goes with them so the next pose rebuilds from the new
    // geometry rather than matching against a stale value.
    for (int drive = 0; drive < 2; drive++)
    {
        m_driveDoorVerts[drive].clear();
        m_doorProgress[drive] = -1.0f;
    }

    BuildLampGlow (m_monitor, kMonitorGlowRgb, m_monitorGlowVerts);
    BuildLampGlow (m_drive,   DriveGlowRgb (m_drive.Kind()), m_driveGlowVerts);

    BuildContactShadow (m_monitor, kMonitorShadowMarginSideMm, kMonitorShadowMarginDepthMm,
                        m_monitorShadowVerts);
    BuildContactShadow (m_drive,   kShadowMarginSideMm,        kShadowMarginDepthMm,
                        m_driveShadowVerts);

    m_modelsLoaded = true;
    m_glassUvDirty = true;
    m_lampsDirty   = true;

    TouchGeometry();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::Metrics
//
////////////////////////////////////////////////////////////////////////////////

DeskSceneMetrics DeskScene::Metrics() const
{
    DeskSceneMetrics   metrics;



    m_monitor.BoundsMin (metrics.monitorMin);
    m_monitor.BoundsMax (metrics.monitorMax);
    m_drive.BoundsMin   (metrics.driveMin);
    m_drive.BoundsMax   (metrics.driveMax);

    metrics.glass         = m_monitor.Surface();
    metrics.monitorFrontY = m_monitor.FrontPlaneY();
    metrics.driveFrontY   = m_drive.FrontPlaneY();
    metrics.driveDoorClearMm = m_drive.DoorFrontClearanceMm();
    metrics.driveDoorRiseMm  = m_drive.DoorRiseMm();

    // The room the contact shadows need on the floor, so the containment
    // solve keeps them inside the picture instead of clipping them away.
    metrics.monitorPadSideMm  = kMonitorShadowMarginSideMm;
    metrics.monitorPadDepthMm = kMonitorShadowMarginDepthMm;
    metrics.drivePadSideMm    = kShadowMarginSideMm;
    metrics.drivePadDepthMm   = kShadowMarginDepthMm;

    return metrics;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SetPowerLampOn
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SetPowerLampOn (bool on)
{
    if (m_powerLampOn != on)
    {
        m_powerLampOn = on;
        m_lampsDirty  = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SetDriveActivity
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SetDriveActivity (int drive, bool active)
{
    if (drive >= 0 && drive < 2 && m_driveActive[drive] != active)
    {
        m_driveActive[drive] = active;
        m_lampsDirty         = true;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SetDriveVisuals
//
//  The per-frame state push. The door rebuild is deferred to Render (models
//  guaranteed loaded there); this only records the target progress when it
//  actually moved.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SetDriveVisuals (int drive, bool lampOn, float doorProgress, bool writeProtected)
{
    if (drive < 0 || drive >= 2)
    {
        return;
    }

    SetDriveActivity (drive, lampOn);

    if (m_driveWp[drive] != writeProtected)
    {
        m_driveWp[drive] = writeProtected;
        InvalidatePlate();                 // the padlock appears or goes
    }

    if (m_doorProgress[drive] < 0.0f ||
        std::abs (doorProgress - m_doorProgress[drive]) > kDoorProgressEps)
    {
        m_doorProgress[drive] = doorProgress;
        m_driveDoorVerts[drive].clear();   // rebuilt lazily in Render
        InvalidatePlate();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SetModelLighting
//
//  The room's two ceiling fixtures, in the coordinates of the model about to
//  be drawn.
//
//  The ramp constants match what the CPU bake used, so this is a change of
//  SHADING QUALITY and not of overall brightness: same 0.16 floor over 0.84
//  span, same 1524 mm reference distance. What differs is the curve's top
//  end. The bake clamped with min(1, sum), which collapsed every face past
//  the threshold onto a single value and flattened relief wherever two
//  nearby point lights summed over 1; this rolls off exponentially instead,
//  so a face can keep getting brighter without ever landing on the ceiling
//  its neighbor is already stuck against. The gain is set so the knee sits
//  where the clamp used to: 1 - exp(-1.6) is about 0.8, so a face that
//  previously pinned at full span now reads near it and still has somewhere
//  left to go.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SetModelLighting (const DeskSceneModel & model,
                                  const float            world[16],
                                  bool                   lampOn,
                                  const float            lampRgb[3])
{
    Dxui3DRenderer::Lighting   lighting;
    float                      toModel[16] = {};



    // Where the fixtures are for THIS device, solved from its placement
    // rather than assumed. The model used to carry its own copy, offset by
    // its own x-center, which quietly lit every device as though it stood on
    // the desk's center line: both drives took identical light no matter
    // where the row put them. That was invisible while nothing cast a
    // shadow, and it stops being invisible the moment something does -- a
    // face can shade lit while the shadow map, which works in real world
    // space, says it is occluded. One basis for both, or they disagree.
    if (SceneCamera::Inverse44 (world, toModel))
    {
        SceneCamera::TransformPoint (toModel, s_kRoomLightsWorld[0], lighting.light0);
        SceneCamera::TransformPoint (toModel, s_kRoomLightsWorld[1], lighting.light1);
    }

    // The shadow lookup goes from THIS device's model space straight to the
    // light's clip space, so its placement rides in the matrix and the pixel
    // shader never needs a world position it does not have.
    if (m_shadowsReady)
    {
        for (int k = 0; k < 2; k++)
        {
            SceneCamera::Mul44 (world, m_lightVp[k], lighting.shadowMatrix[k]);
        }

        lighting.shadowTexel    = 1.0f / (float) kShadowMapTexels;
        lighting.shadowBias     = kShadowBias;

        // The strip draws the drives with nothing above them: the shadow
        // maps were rendered with the monitor in place, and applying them
        // there prints a phantom of a device that is not on screen.
        lighting.shadowStrength = m_stripPass ? 0.0f : kShadowStrength;
    }

    lighting.refDist = DeskSceneModel::kLightRefMm;
    lighting.span    = DeskSceneModel::kShadeSpan;
    lighting.gain    = 2.05f;

    // Every model shares the same axis convention -- X right, Y back, Z up --
    // so one direction serves them all: the seated eye is in front (-Y) and
    // about ten degrees above, looking down.
    lighting.eye[0] =  0.0f;
    lighting.eye[1] = -0.985f;
    lighting.eye[2] =  0.174f;

    // Molded plastic has a broad soft sheen, and without it relief on a face
    // viewed near head-on has nothing to read by: the flanks foreshorten to
    // a pixel or two at this camera angle, while a highlight on the rounded
    // edge stays put. A wide lobe, because a matte case is not a mirror.
    lighting.specStrength = 0.22f;
    lighting.specPower    = 20.0f;

    // Ceiling bounce above, desk bounce below -- and BOTH generous, because
    // two ceiling fixtures alone cannot light a room. A vertical front face
    // catches only about a fifth of a lamp directly overhead (N.L ~ 0.2), so
    // a scene lit by those two lights and a token 0.16 floor rendered a
    // cream-colored case as mid-gray. What actually lights the front of a
    // machine on a desk is the room: walls, ceiling, window, the desk
    // itself. That is what this stands in for, so it carries real weight
    // rather than just keeping shadows off pure black.
    //
    // Slightly warm above and warmer still below, since the bounce picks up
    // the desk's own color on the way back up.
    //
    // SPREAD FURTHER APART THAN THEY WERE, around the same midpoint. The
    // shader takes ambient as lerp(down, up, n.z), so how far apart these two
    // sit IS how much a shadowed surface can still say about which way it
    // faces -- and it was barely anything: a vertical wall landed at the
    // midpoint, 85% of the flat lid beside it.
    //
    // That is what made the //c's lid ribs go thin under the monitor. A
    // groove reads by the contrast between its two walls, the direct term
    // supplies that contrast, and inside a shadow the direct term is mostly
    // gone -- leaving the ribs to be drawn by a 15% ambient difference, which
    // is a hairline where the lit ones are bands. Widening the spread is the
    // fix that does not touch the shadow: relief keeps its modeling in the
    // dark, exactly as relief does in a real room, where what fills a shadow
    // is sky and bounce and not a uniform gray.
    //
    // The MIDPOINT is held, so nothing facing the viewer changes value at
    // all. Only the difference between up-facing and down-facing grows.
    lighting.ambientUp[0]   = 0.359f;
    lighting.ambientUp[1]   = 0.350f;
    lighting.ambientUp[2]   = 0.331f;
    lighting.ambientDown[0] = 0.208f;
    lighting.ambientDown[1] = 0.200f;
    lighting.ambientDown[2] = 0.181f;

    // The device's own lamp, as a light rather than baked spill. It sits at
    // the lens and radiates the way the lens faces (-Y, toward the viewer),
    // so the housing behind it stays dark because those faces point away --
    // no rays traced, no second copy of the body baked with the glow burnt
    // into its vertices.
    if (lampOn && lampRgb != nullptr && !model.Lamps().empty())
    {
        const DeskLampAnchor & anchor = model.Lamps().front();

        // The shader weighs each surface by dot(n, L), so a lamp coplanar with
        // the faceplate lies in that plane and lights none of it. Something
        // has to stand the source off the face.
        //
        // THE PART CANNOT ALWAYS BE THAT SOMETHING. It was, for a while, on
        // the reasoning that a protrusion is a fact about the part and so the
        // part should carry it -- which is true of the Disk II, whose LED is a
        // genuine 5 mm dome. It is false of the //c family, whose indicators
        // are matte plastic windows lying flush in the panel. Asking those to
        // protrude meant modeling a bump that is not on the hardware, to solve
        // a problem that was never geometry's.
        //
        // So the source stands off the lens's own FACE, always, by a fixed
        // amount that belongs to the light and not to the part.
        //
        // It used to measure that standoff from the lens's BACK face and take
        // whichever came out further forward, so as not to shove a domed LED
        // that already protrudes. That worked only while every lens had some
        // thickness in front of the panel. Make one truly flush -- which is
        // what the //c family's are -- and the back-face rule hands back the
        // panel plane itself, which lights nothing, and the indicator goes
        // dark in its own glow. A rule that fails on the case it exists to
        // serve is the wrong rule.
        lighting.lampPos[0] = anchor.center[0];
        lighting.lampPos[1] = anchor.frontY - kLampLightStandoffMm;
        lighting.lampPos[2] = anchor.center[2];

        lighting.lampDir[0] =  0.0f;
        lighting.lampDir[1] = -1.0f;
        lighting.lampDir[2] =  0.0f;

        // Which KIND of emitter this is. Only the Disk II has a domed LED;
        // every other lamp in the scene is a flat window lying flush in its
        // panel, and a flat window does not light the panel.
        lighting.lampWrap = (model.Kind() == DeskDeviceKind::DiskII)
                            ? kLampWrapDome : kLampWrapFlush;

        // The lamp's light is its color times an INTENSITY, because the two
        // are different things and only one of them was being supplied. The
        // shader scales the lamp term by the receiving surface's own base
        // color, and the faceplate is matte black at 0.10 -- so a lamp of
        // "brightness 1" put 1.5/255 of red onto the plastic beside it, which
        // is to say nothing. A working LED is a small source but a fierce
        // one; the number it needs is nowhere near unity.
        //
        // The GLOW geometry keeps the unscaled color on purpose: the bulb is
        // already at full brightness and multiplying it too would only clip
        // it to a white blob.
        for (int i = 0; i < 3; i++)
        {
            lighting.lampColor[i] = lampRgb[i] * kLampLightGain;
            lighting.lampCap[i]   = lampRgb[i];
        }

        // Which lamp map: the monitor's and the drive's are different lamps in
        // different model spaces. Nothing needs folding into the matrix here,
        // because the map was drawn in the very space this draw submits.
        {
            bool  isMonitor = (&model == &m_monitor);
            int   which     = isMonitor ? 0 : 1;

            if (m_lampVpValid[which])
            {
                memcpy (lighting.lampShadow, m_lampVp[which], sizeof (lighting.lampShadow));

                lighting.lampShadowSlot  = isMonitor ? kLampSlotMonitor : kLampSlotDrive;
                lighting.lampShadowTexel = 1.0f / (float) kLampShadowTexels;
                lighting.lampShadowBias  = kLampShadowBias;

                // The throw stops inside the cone the frustum inscribes, so the
                // lamp never lights a surface the map has no opinion about. Set
                // WITH the map and not without it: an unshadowed lamp has no
                // boundary to hide and keeps its full reach.
                lighting.lampConeCosOuter = std::cos (kLampConeOuterDeg * 3.14159265f / 180.0f);
                lighting.lampConeCosInner = std::cos (kLampConeInnerDeg * 3.14159265f / 180.0f);
            }
        }
    }

    m_renderer.SetLighting (lighting);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::TintInto
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::TintInto (const std::vector<Dxui3DRenderer::Vertex> & base,
                          float                                       factor,
                          std::vector<Dxui3DRenderer::Vertex>       & out)
{
    out = base;

    for (Dxui3DRenderer::Vertex & v : out)
    {
        v.r *= factor;
        v.g *= factor;
        v.b *= factor;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::RebuildGlassUvs
//
//  The model's glass UVs span [0,1] over the sheet; the display lands in a
//  subrect of the offscreen CRT texture, so the cached copy linearly remaps
//  into that subrect. Rebuilt only when the subrect changes (a resize).
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::RebuildGlassUvs (const CrtUvRect & displayUv, int displayW, int displayH)
{
    const CurvedDisplaySurface &  surface = m_monitor.Surface();
    float                         bandU0  = 0.0f;
    float                         bandV0  = 0.0f;
    float                         bandU1  = 1.0f;
    float                         bandV1  = 1.0f;



    // The picture is a curved grid spanning exactly the aspect-fit band
    // (same band as the input math), so its mesh boundary IS the picture
    // boundary -- no interpolation ever crosses the picture's texture edge,
    // which is what used to either clamp-smear the outermost columns into
    // the tube margins or (with a black guard) let the CRT chain's
    // neighbor-sampling passes dim them.
    CurvedDisplayMath::ComputePictureBand (surface, displayW, displayH,
                                           bandU0, bandV0, bandU1, bandV1);

    m_pictureVerts.clear();
    m_pictureVerts.reserve ((size_t) kPictureGridCols * kPictureGridRows * 6);

    for (int row = 0; row < kPictureGridRows; row++)
    {
        for (int col = 0; col < kPictureGridCols; col++)
        {
            Dxui3DRenderer::Vertex   corner[4] = {};
            int                      steps[4][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

            for (int i = 0; i < 4; i++)
            {
                float  fu    = (float) (col + steps[i][0]) / (float) kPictureGridCols;
                float  fv    = (float) (row + steps[i][1]) / (float) kPictureGridRows;
                float  gu    = bandU0 + fu * (bandU1 - bandU0);
                float  gv    = bandV0 + fv * (bandV1 - bandV0);
                float  pt[3] = {};

                CurvedDisplayMath::ModelPointFromUv (surface, gu, gv, pt);

                corner[i].x = pt[0];
                corner[i].y = pt[1] - kPictureLiftMm;
                corner[i].z = pt[2];
                corner[i].u = displayUv.u0 + fu * (displayUv.u1 - displayUv.u0);
                corner[i].v = displayUv.v0 + fv * (displayUv.v1 - displayUv.v0);
                corner[i].r = corner[i].g = corner[i].b = corner[i].a = 1.0f;
            }

            m_pictureVerts.push_back (corner[0]);
            m_pictureVerts.push_back (corner[1]);
            m_pictureVerts.push_back (corner[2]);
            m_pictureVerts.push_back (corner[0]);
            m_pictureVerts.push_back (corner[2]);
            m_pictureVerts.push_back (corner[3]);
        }
    }

    // The band again, as a DEPTH STAMP: the same triangles with every color
    // zeroed, so drawing them under premultiplied source-over changes no
    // pixel and writes only depth. RenderPlate stamps this into the front
    // plate's depth before re-drawing the case, which is what lets the case
    // occlude the live picture without the cavity behind it doing the same.
    m_pictureDepthVerts = m_pictureVerts;

    for (Dxui3DRenderer::Vertex & v : m_pictureDepthVerts)
    {
        v.r = v.g = v.b = v.a = 0.0f;
    }

    // The tube ring and the mask: dark rings riding the sag surface. The
    // tube covers band -> glass edge AT the surface -- deliberately a ring,
    // not the model's full sheet: geometry under the picture is what used to
    // poke through it at grazing corner angles, so none exists. The mask
    // covers (rounded opening) -> glass edge just above the picture,
    // rounding the corners the way a real faceplate does.
    //
    // Both stop on the glass's ROUNDED outline. See kGlassEdgeRadiusMm: a
    // square outer corner pushes a lifted wedge past the bezel's rounded
    // mouth, which reads as a second screen behind the first.
    {
        float  gx0    = surface.x0;
        float  gx1    = surface.x1;
        float  gz0    = surface.z0;
        float  gz1    = surface.z1;
        float  bx0    = gx0 + bandU0 * (gx1 - gx0);
        float  bx1    = gx0 + bandU1 * (gx1 - gx0);
        float  bz1    = gz1 - bandV0 * (gz1 - gz0);
        float  bz0    = gz1 - bandV1 * (gz1 - gz0);
        float  ox0    = std::max (bx0 - kMaskPadMm, gx0);
        float  ox1    = std::min (bx1 + kMaskPadMm, gx1);
        float  oz0    = std::max (bz0 - kMaskPadMm, gz0);
        float  oz1    = std::min (bz1 + kMaskPadMm, gz1);
        float  radius = std::min ({ kMaskRadiusMm, (ox1 - ox0) * 0.5f, (oz1 - oz0) * 0.5f });
        float  edgeR  = std::min ({ kGlassEdgeRadiusMm, (gx1 - gx0) * 0.5f, (gz1 - gz0) * 0.5f });
        float  under  = std::min ({ kTubeUnderlapMm, (bx1 - bx0) * 0.25f, (bz1 - bz0) * 0.25f });

        struct RingPoint { float x; float z; };

        constexpr float                        kHalfPi = 1.5707963f;

        std::vector<Dxui3DRenderer::Vertex> *  target  = nullptr;
        float                                  lift    = 0.0f;
        const float                          * tint    = kTubeTint;

        auto surfacePoint = [&] (float x, float z, Dxui3DRenderer::Vertex & v)
        {
            float   u     = (x - surface.x0) / (surface.x1 - surface.x0);
            float   w     = (surface.z1 - z) / (surface.z1 - surface.z0);
            float   pt[3] = {};

            CurvedDisplayMath::ModelPointFromUv (surface, u, w, pt);

            v = {};
            v.x = pt[0];
            v.y = pt[1] - lift;
            v.z = pt[2];
            v.r = tint[0];
            v.g = tint[1];
            v.b = tint[2];
            v.a = 1.0f;
        };

        auto pushTri = [&] (float x0, float z0, float x1, float z1, float x2, float z2)
        {
            Dxui3DRenderer::Vertex   v = {};

            surfacePoint (x0, z0, v);  target->push_back (v);
            surfacePoint (x1, z1, v);  target->push_back (v);
            surfacePoint (x2, z2, v);  target->push_back (v);
        };

        // A rounded rectangle walked counter-clockwise in a fixed structure:
        // four quarter arcs and four sides, always the same sample counts. Two
        // outlines of different size and radius therefore correspond point for
        // point, and the ring stretched between them cannot twist. A zero
        // radius simply collapses its arc onto the corner.
        auto outline = [] (float x0, float x1, float z0, float z1, float r,
                           std::vector<RingPoint> & out)
        {
            const float   arcs[4][3] = { { x1 - r, z1 - r, 0.0f              },   // top-right
                                         { x0 + r, z1 - r, kHalfPi           },   // top-left
                                         { x0 + r, z0 + r, kHalfPi * 2.0f    },   // bottom-left
                                         { x1 - r, z0 + r, kHalfPi * 3.0f    } }; // bottom-right
            const float   sides[4][4] = { { x1 - r, z1, x0 + r, z1 },             // top
                                          { x0, z1 - r, x0, z0 + r },             // left
                                          { x0 + r, z0, x1 - r, z0 },             // bottom
                                          { x1, z0 + r, x1, z1 - r } };           // right

            out.clear();

            for (int k = 0; k < 4; k++)
            {
                // Half-open: each segment leaves its end point to the next one,
                // so the walk closes on itself without doubled samples.
                for (int i = 0; i < kRingArcSegments; i++)
                {
                    float   a = arcs[k][2] + kHalfPi * (float) i / (float) kRingArcSegments;

                    out.push_back ({ arcs[k][0] + r * std::cos (a),
                                     arcs[k][1] + r * std::sin (a) });
                }

                for (int i = 0; i < kRingSideSegments; i++)
                {
                    float   t = (float) i / (float) kRingSideSegments;

                    out.push_back ({ sides[k][0] + (sides[k][2] - sides[k][0]) * t,
                                     sides[k][1] + (sides[k][3] - sides[k][1]) * t });
                }
            }
        };

        // The ring between two such outlines. Subdivided ACROSS as well as
        // along: the sphere bulges millimeters over spans this long -- far
        // past any lift -- so a single quad from outline to outline would
        // slice below the layer beneath and let it poke through as dark
        // petals. Every sample lands back on the sphere, leaving a chord
        // error well under the lift.
        auto pushRing = [&] (const std::vector<RingPoint> & inner,
                             const std::vector<RingPoint> & outer)
        {
            for (size_t i = 0; i < outer.size(); i++)
            {
                size_t   j = (i + 1) % outer.size();

                for (int k = 0; k < kRingCrossSegments; k++)
                {
                    float       t0 = (float) k / (float) kRingCrossSegments;
                    float       t1 = (float) (k + 1) / (float) kRingCrossSegments;

                    auto mix = [] (const RingPoint & a, const RingPoint & b, float t) -> RingPoint
                    { return { a.x + (b.x - a.x) * t, a.z + (b.z - a.z) * t }; };

                    RingPoint   oi = mix (outer[i], inner[i], t0);
                    RingPoint   oj = mix (outer[j], inner[j], t0);
                    RingPoint   ii = mix (outer[i], inner[i], t1);
                    RingPoint   ij = mix (outer[j], inner[j], t1);

                    pushTri (oi.x, oi.z, ii.x, ii.z, ij.x, ij.z);
                    pushTri (oi.x, oi.z, ij.x, ij.z, oj.x, oj.z);
                }
            }
        };

        std::vector<RingPoint>   edgeRing;
        std::vector<RingPoint>   bandRing;
        std::vector<RingPoint>   openRing;

        outline (gx0, gx1, gz0, gz1, edgeR,      edgeRing);
        outline (bx0 + under, bx1 - under, bz0 + under, bz1 - under, 0.0f, bandRing);
        outline (ox0, ox1, oz0, oz1, radius,     openRing);

        // The tube ring: band -> glass edge, ON the surface (no lift), its
        // inner edge run back under the picture by kTubeUnderlapMm so the
        // picture's lift has something behind it.
        m_glassVerts.clear();
        target = &m_glassVerts;
        lift   = 0.0f;
        tint   = kTubeTint;

        pushRing (bandRing, edgeRing);

        // The mask ring: rounded opening -> glass edge, floated past the
        // picture's lift.
        m_maskVerts.clear();
        target = &m_maskVerts;
        lift   = kMaskLiftMm;
        tint   = kMaskTint;

        pushRing (openRing, edgeRing);
    }

    m_glassUv      = displayUv;
    BuildGlassSheen (surface, m_bezelTiltRad);

    m_glassUvDirty = false;

    TouchGeometry();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildGlassSheen
//
//  The room, reflected in the faceplate. Without it the tube's curvature is
//  invisible: sag is a DEPTH change, and a couple of centimeters of it at
//  arm's length is a two percent change in distance the eye cannot read. In
//  a photograph of a real CRT the curvature is carried almost entirely by
//  the reflection sliding across the glass, so the shape needs a highlight
//  to bend before it can be seen bending.
//
//  A Blinn half-vector term over the sphere's own normals, premultiplied and
//  drawn translucent over the picture. It rides the same surface the picture
//  does, so it tracks any change to the radius for free.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildGlassSheen (const CurvedDisplaySurface & surface, float tiltRad)
{
    float   cx     = (surface.x0 + surface.x1) * 0.5f;
    float   cz     = (surface.z0 + surface.z1) * 0.5f;
    float   apexY  = surface.baseY - CurvedDisplayMath::MaxSag (surface);
    float   ctr[3] = { cx, apexY + surface.radius, cz };
    float   eye[3] = { cx, apexY - kSheenEyeMm, cz };
    float   lgt[3] = { kSheenLight[0], kSheenLight[1], kSheenLight[2] };



    // THE ROOM DOES NOT TILT WITH THE TUBE. These verts ride the tilted
    // transform at draw time, so anything built here in model space turns
    // with the glass -- including, before this, the light and the viewer,
    // which froze the glare onto the face like a decal. Rotating both by the
    // INVERSE tilt puts them where the fixed room lands in tilted model
    // space, so at draw time they come out stationary and the highlight
    // slides across the face as the tube turns under it -- the one visible
    // proof that the glass is really moving.
    {
        float  c  = std::cos (tiltRad);
        float  s  = std::sin (tiltRad);
        float  ga = tiltRad * kSheenTiltGlareGain;
        float  gc = std::cos (ga);
        float  gs = std::sin (ga);
        float  py = m_monitor.TiltPivotY();
        float  pz = m_monitor.TiltPivotZ();
        float  dy = eye[1] - py;
        float  dz = eye[2] - pz;
        float  ly = lgt[1];
        float  lz = lgt[2];

        eye[1] = py + dy * c - dz * s;
        eye[2] = pz + dy * s + dz * c;

        // The light takes the exaggerated angle -- see kSheenTiltGlareGain.
        lgt[1] = ly * gc - lz * gs;
        lgt[2] = ly * gs + lz * gc;
    }

    float   ll     = std::sqrt (lgt[0] * lgt[0] +
                                lgt[1] * lgt[1] +
                                lgt[2] * lgt[2]);



    m_sheenVerts.clear();

    if (surface.radius <= 0.0f || ll <= 0.0f)
    {
        return;
    }

    // Coverage on the rounded outline, as a signed distance: zero outside it,
    // one a fade-width inside. Standard rounded-box distance -- push the point
    // in by the straight extent, and what is left is measured against the
    // corner radius.
    auto  coverage = [&] (float x, float z) -> float
    {
        float   halfX = (surface.x1 - surface.x0) * 0.5f;
        float   halfZ = (surface.z1 - surface.z0) * 0.5f;
        float   r     = std::min ({ kGlassEdgeRadiusMm, halfX, halfZ });
        float   qx    = std::max (std::abs (x - cx) - (halfX - r), 0.0f);
        float   qz    = std::max (std::abs (z - cz) - (halfZ - r), 0.0f);
        float   d     = std::sqrt (qx * qx + qz * qz) - r;

        return std::min (std::max (-d / kSheenFadeMm, 0.0f), 1.0f);
    };

    auto  vertexAt = [&] (int col, int row) -> Dxui3DRenderer::Vertex
    {
        Dxui3DRenderer::Vertex   v     = {};
        float                    pt[3] = {};
        float                    n[3]  = {};
        float                    h[3]  = {};
        float                    nl    = 0.0f;
        float                    hl    = 0.0f;
        float                    spec  = 0.0f;

        CurvedDisplayMath::ModelPointFromUv (surface,
                                             (float) col / (float) kPictureGridCols,
                                             (float) row / (float) kPictureGridRows, pt);

        // Outward normal: away from the sphere's center, which sits one
        // radius behind the apex.
        for (int i = 0; i < 3; i++)
        {
            n[i] = pt[i] - ctr[i];
        }

        nl = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

        if (nl > 0.0f)
        {
            float  toEye[3] = { eye[0] - pt[0], eye[1] - pt[1], eye[2] - pt[2] };
            float  el       = std::sqrt (toEye[0] * toEye[0] + toEye[1] * toEye[1] +
                                         toEye[2] * toEye[2]);

            if (el > 0.0f)
            {
                for (int i = 0; i < 3; i++)
                {
                    h[i] = toEye[i] / el + lgt[i] / ll;
                }

                hl = std::sqrt (h[0] * h[0] + h[1] * h[1] + h[2] * h[2]);
            }

            if (hl > 0.0f)
            {
                float  d = (n[0] * h[0] + n[1] * h[1] + n[2] * h[2]) / (nl * hl);

                spec = (d > 0.0f) ? std::pow (d, kSheenExponent) * kSheenStrength : 0.0f;
            }
        }

        spec *= coverage (pt[0], pt[2]);

        v.x = pt[0];
        v.y = pt[1] - kSheenLiftMm;
        v.z = pt[2];
        v.r = v.g = v.b = spec;      // premultiplied: white light at alpha
        v.a = spec;

        return v;
    };

    m_sheenVerts.reserve ((size_t) kPictureGridCols * kPictureGridRows * 6);

    for (int row = 0; row < kPictureGridRows; row++)
    {
        for (int col = 0; col < kPictureGridCols; col++)
        {
            m_sheenVerts.push_back (vertexAt (col,     row));
            m_sheenVerts.push_back (vertexAt (col + 1, row));
            m_sheenVerts.push_back (vertexAt (col + 1, row + 1));
            m_sheenVerts.push_back (vertexAt (col,     row));
            m_sheenVerts.push_back (vertexAt (col + 1, row + 1));
            m_sheenVerts.push_back (vertexAt (col,     row + 1));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SceneBoundsWorld
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SceneBoundsWorld (const DeskSceneComposition & comp,
                                  float                        lo[3],
                                  float                        hi[3]) const
{
    auto  accumulate = [&lo, &hi] (const DeskSceneModel & model, const float world[16])
    {
        float  mn[3] = {};
        float  mx[3] = {};

        model.BoundsMin (mn);
        model.BoundsMax (mx);

        // All eight corners, because the model->world remap swaps axes: the
        // box's min corner is not the world box's min corner.
        for (int c = 0; c < 8; c++)
        {
            float  pt[3]  = { (c & 1) ? mx[0] : mn[0],
                              (c & 2) ? mx[1] : mn[1],
                              (c & 4) ? mx[2] : mn[2] };
            float  out[3] = {};

            if (SceneCamera::TransformPoint (world, pt, out))
            {
                for (int a = 0; a < 3; a++)
                {
                    lo[a] = (std::min) (lo[a], out[a]);
                    hi[a] = (std::max) (hi[a], out[a]);
                }
            }
        }
    };



    lo[0] = lo[1] = lo[2] =  FLT_MAX;
    hi[0] = hi[1] = hi[2] = -FLT_MAX;

    accumulate (m_monitor, comp.monitorWorld);

    for (int drive = 0; drive < comp.driveCount; drive++)
    {
        accumulate (m_drive, comp.driveWorld[drive]);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::FillLampShadow
//
//  Depth of one device's own body as seen from its lamp, in that device's model
//  space -- so the matrix the shader gets needs no placement folded in, unlike
//  the room lights' shared world-space maps.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::FillLampShadow (const DeskSceneModel       & model,
                                   int                          slot,
                                   Dxui3DRenderer::StaticMesh & mesh,
                                   uint32_t                     revision,
                                   float                        outVp[16])
{
    HRESULT          hr       = S_OK;
    D3D11_VIEWPORT   vp       = {};
    float            view[16] = {};
    float            proj[16] = {};
    float            eye[3]   = {};
    float            at[3]    = {};



    {
        bool  hasLamp = !model.Lamps().empty();

        CBREx (hasLamp, E_UNEXPECTED);
    }

    {
        const DeskLampAnchor &  anchor = model.Lamps().front();

        // AT THE LIGHT, not at the lens. The shader stands the source off the
        // lens face by kLampLightStandoffMm, and a shadow map rendered from a
        // different point than the light it shadows for is a map that
        // disagrees with its own lighting -- worst of all when the camera sits
        // in the very surface it is depth-testing, which is grazing geometry
        // and comes back as a ring of acne hugging the lamp.
        eye[0] = anchor.center[0];
        eye[1] = anchor.frontY - kLampLightStandoffMm;
        eye[2] = anchor.center[2];

        // Looking the way the lens faces, which is -Y toward the viewer.
        at[0] = eye[0];
        at[1] = eye[1] - 100.0f;
        at[2] = eye[2];
    }

    // Up is the model's +Z here, NOT the fixed (0,1,0) LookAtRH assumes: in
    // model space +Y is BACK, which is exactly where this camera looks.
    {
        const float  up[3] = { 0.0f, 0.0f, 1.0f };

        SceneCamera::LookAtUpRH (eye, at, up, view);
    }

    SceneCamera::PerspectiveFovRH (kLampShadowFovDeg * 3.14159265f / 180.0f, 1.0f,
                                    0.5f, Dxui3DRenderer::Lighting{}.lampRange, proj);
    SceneCamera::Mul44            (view, proj, outVp);

    hr = m_renderer.BeginShadowPass (slot, kLampShadowTexels);
    CHRA (hr);

    vp.Width    = (float) kLampShadowTexels;
    vp.Height   = (float) kLampShadowTexels;
    vp.MaxDepth = 1.0f;

    hr = m_renderer.DrawStatic (mesh, model.OpaqueVerts().data(), model.OpaqueVerts().size(),
                                revision, outVp, false, vp, true);

    m_renderer.EndShadowPass();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::RenderShadowMaps
//
//  One depth pass per room light, every device drawn into the same map so the
//  shadows are the scene's and not each device's own.
//
//  The frustum is solved from the scene's bounding SPHERE rather than its box:
//  a box's extent depends on which way the light looks at it, so a
//  box-derived fov breathes as the composition changes and the shadow's
//  resolution breathes with it. The sphere gives one number that holds.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::RenderShadowMaps (const DeskSceneComposition & comp,
                                     const D3D11_VIEWPORT       & viewport)
{
    HRESULT  hr        = S_OK;
    float    lo[3]     = {};
    float    hi[3]     = {};
    float    center[3] = {};
    float    radius    = 0.0f;



    m_shadowsReady = false;

    SceneBoundsWorld (comp, lo, hi);

    CBREx (hi[0] >= lo[0], E_UNEXPECTED);

    for (int a = 0; a < 3; a++)
    {
        center[a] = (lo[a] + hi[a]) * 0.5f;
    }

    radius = 0.5f * std::sqrt ((hi[0] - lo[0]) * (hi[0] - lo[0]) +
                               (hi[1] - lo[1]) * (hi[1] - lo[1]) +
                               (hi[2] - lo[2]) * (hi[2] - lo[2]));

    for (int k = 0; k < 2; k++)
    {
        float  view[16] = {};
        float  proj[16] = {};
        float  dx       = s_kRoomLightsWorld[k][0] - center[0];
        float  dy       = s_kRoomLightsWorld[k][1] - center[1];
        float  dz       = s_kRoomLightsWorld[k][2] - center[2];
        float  dist     = std::sqrt (dx * dx + dy * dy + dz * dz);
        float  fovY     = 0.0f;
        float  mvp[16]  = {};

        if (dist <= radius * 1.01f)
        {
            continue;
        }

        // Just wide enough to hold the sphere, with a little air so a texel
        // at the very edge is never the one being read.
        fovY = 2.0f * std::asin ((std::min) (0.99f, radius / dist)) * 1.08f;

        SceneCamera::LookAtRH         (s_kRoomLightsWorld[k], center, view);
        SceneCamera::PerspectiveFovRH (fovY, 1.0f, (std::max) (1.0f, dist - radius),
                                        dist + radius, proj);
        SceneCamera::Mul44            (view, proj, m_lightVp[k]);

        hr = m_renderer.BeginShadowPass (k, kShadowMapTexels);
        CHRA (hr);

        SceneCamera::Mul44 (comp.monitorWorld, m_lightVp[k], mvp);

        hr = m_renderer.DrawStatic (m_monitorOpaqueMesh,
                                    m_monitor.OpaqueVerts().data(),
                                    m_monitor.OpaqueVerts().size(),
                                    m_geometryRev, mvp, false, viewport, true);

        for (int drive = 0; drive < comp.driveCount && SUCCEEDED (hr); drive++)
        {
            SceneCamera::Mul44 (comp.driveWorld[drive], m_lightVp[k], mvp);

            hr = m_renderer.DrawStatic (m_driveOpaqueMesh,
                                        m_drive.OpaqueVerts().data(),
                                        m_drive.OpaqueVerts().size(),
                                        m_geometryRev, mvp, false, viewport, true);
        }

        m_renderer.EndShadowPass();
        CHRA (hr);
    }

    // The lamps get their own, one per device MODEL: both drives share the
    // drive map because they are the same geometry seen from the same lamp in
    // the same model space, which is only true because the map is not in world
    // space the way the room lights are.
    {
        HRESULT  hrMonitorLamp = FillLampShadow (m_monitor, kLampSlotMonitor,
                                                  m_monitorOpaqueMesh, m_geometryRev,
                                                  m_lampVp[0]);
        HRESULT  hrDriveLamp   = FillLampShadow (m_drive, kLampSlotDrive,
                                                  m_driveOpaqueMesh, m_geometryRev,
                                                  m_lampVp[1]);

        m_lampVpValid[0] = SUCCEEDED (hrMonitorLamp);
        m_lampVpValid[1] = SUCCEEDED (hrDriveLamp);
    }

    m_shadowsReady = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::DrawDrives
//
//  The drive pass both presentations share: opaque body, the door assembly
//  (rotated lazily to the current progress), the write-protect padlock, and
//  the activity lamp -- per placed drive of whichever composition is drawing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::DrawDrives (const DeskSceneComposition & comp, const D3D11_VIEWPORT & viewport)
{
    HRESULT   hr      = S_OK;
    float     mvp[16] = {};



    for (int drive = 0; drive < comp.driveCount; drive++)
    {
        SceneCamera::Mul44 (comp.driveWorld[drive], comp.viewProj, mvp);

        // Lighting is set PER DEVICE, not once per frame: each model keeps
        // its own coordinates and Load() put the room's fixtures into them,
        // so a drive at the right of the desk sees the same two ceiling
        // lights from a different place than the monitor above it does.
        SetModelLighting (m_drive, comp.driveWorld[drive], m_driveActive[drive],
                          DriveGlowRgb (m_drive.Kind()));

        hr = m_renderer.DrawStatic (m_driveOpaqueMesh,
                                    m_drive.OpaqueVerts().data(),
                                    m_drive.OpaqueVerts().size(),
                                    m_geometryRev, mvp, false, viewport, true);
        CHRA (hr);

        // Door assembly: a rotated copy of the model's cached verts, rebuilt
        // only when SetDriveVisuals moved the progress.
        if (m_driveDoorVerts[drive].empty() && !m_drive.DoorVerts().empty())
        {
            // The MODEL poses it, because the two drives do not move the same
            // way -- one turns about a pole inside the drive and the other
            // travels back and then up -- and a caller that picks for itself
            // picks the motion it happens to know.
            m_drive.PoseDoor (std::clamp (m_doorProgress[drive], 0.0f, 1.0f),
                              m_driveDoorVerts[drive]);
        }

        if (!m_driveDoorVerts[drive].empty())
        {
            hr = m_renderer.DrawTriangles (m_driveDoorVerts[drive].data(), m_driveDoorVerts[drive].size(),
                                           mvp, false, viewport, true);
            CHRA (hr);
        }

        if (m_driveWp[drive] && !m_drive.PadlockVerts().empty())
        {
            hr = m_renderer.DrawStatic (m_padlockMesh, m_drive.PadlockVerts().data(), m_drive.PadlockVerts().size(), m_geometryRev,
                                           mvp, false, viewport, true);
            CHRA (hr);
        }

        // The drive number is a STICKER, not part of the drive. A real Disk II
        // leaves the factory without one; an owner adds them to tell a pair
        // apart, which is a problem only a pair has. One drive is unambiguous
        // and wears none, so the scene stops inventing a label for it.
        if (comp.driveCount > 1 && !m_driveLabelVerts[drive].empty())
        {
            hr = m_renderer.DrawStatic (m_labelMesh[drive], m_driveLabelVerts[drive].data(), m_driveLabelVerts[drive].size(), m_geometryRev,
                                           mvp, false, viewport, true);
            CHRA (hr);
        }

        if (!m_driveLampVerts[drive].empty())
        {
            hr = m_renderer.DrawStatic (m_driveLampMesh[drive], m_driveLampVerts[drive].data(), m_driveLampVerts[drive].size(), m_geometryRev,
                                           mvp, false, viewport, true);
            CHRA (hr);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildTiltMatrix
//
//  A rigid rotation about the X-axis line at (pivotY, pivotZ), in the row-
//  vector convention the rest of the scene's matrices use.
//
//  The signs match RotateDoorVerts, which is the other place in this scene
//  that turns something about a horizontal hinge: a point above the pivot
//  goes BACK as the angle grows, so a positive tilt tips the bezel's top away
//  from the viewer and brings its chin forward.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildTiltMatrix (float angleRad, float pivotY, float pivotZ, float out[16])
{
    float  c = std::cos (angleRad);
    float  s = std::sin (angleRad);



    out[0]  = 1.0f;  out[1]  = 0.0f;  out[2]  = 0.0f;  out[3]  = 0.0f;
    out[4]  = 0.0f;  out[5]  = c;     out[6]  = -s;    out[7]  = 0.0f;
    out[8]  = 0.0f;  out[9]  = s;     out[10] = c;     out[11] = 0.0f;

    // The pivot, carried through the rotation and put back: p' = (p - v)R + v.
    out[12] = 0.0f;
    out[13] = pivotY - (pivotY * c + pivotZ * s);
    out[14] = pivotZ - (pivotY * -s + pivotZ * c);
    out[15] = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildTiltedMonitorWorld
//
//  The monitor's placement with the bezel's tilt folded in front of it. Every
//  consumer of the tilting assembly goes through here, so the mesh, its
//  shadow and the glass hit test cannot end up disagreeing about where the
//  tube is pointing.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildTiltedMonitorWorld (const DeskSceneComposition & comp, float out[16]) const
{
    float  tilt[16] = {};



    if (m_bezelTiltRad == 0.0f)
    {
        memcpy (out, comp.monitorWorld, 16 * sizeof (float));
        return;
    }

    BuildTiltMatrix (m_bezelTiltRad, m_monitor.TiltPivotY(), m_monitor.TiltPivotZ(), tilt);
    SceneCamera::Mul44 (tilt, comp.monitorWorld, out);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildLampGlow
//
//  Concentric bands from kGlowProfile, fanned in the lens plane: the inner
//  band is a triangle fan off the center, the rest are quad rings. Bands
//  rather than one linear fan because a single gradient reads as a flat
//  translucent disc with a visible edge -- the knees are what make it look
//  like light instead of a decal.
//
//  Premultiplied throughout (the renderer composites source-over), so every
//  color channel is already scaled by its own alpha.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildLampGlow (const DeskSceneModel                & model,
                               const float                           rgb[3],
                               std::vector<Dxui3DRenderer::Vertex> & out)
{
    constexpr int  kBandCount = (int) (sizeof (kGlowProfile) / sizeof (kGlowProfile[0]));



    out.clear();

    if (model.Lamps().empty() ||
        model.Lamps()[0].radiusX <= 0.0f || model.Lamps()[0].radiusZ <= 0.0f)
    {
        return;
    }

    const DeskLampAnchor &  lamp    = model.Lamps()[0];
    float                   y       = lamp.frontY - kGlowLiftMm;
    float                   slopeX  = 0.0f;
    float                   slopeZ  = 0.0f;
    float                   lo[3]   = {};
    float                   hi[3]   = {};
    float                   outer   = kGlowProfile[(int) (sizeof (kGlowProfile) / sizeof (kGlowProfile[0])) - 1].radiusScale;
    float                   radiusX = lamp.radiusX;
    float                   radiusZ = lamp.radiusZ;

    // Keep the halo inside the housing it shines from. Depth testing hides
    // the part that falls on the case, but past the case's silhouette there
    // is nothing to hide behind, and a lamp sunk in the power notch would
    // otherwise leave a speck of light floating in the air beside the
    // cabinet. The reach is measured to the nearest edge of the model, with
    // margin for the rounded corner the flat bound does not know about.
    model.BoundsMin (lo);
    model.BoundsMax (hi);

    if (outer > 0.0f)
    {
        radiusX = (std::min) (radiusX, (std::min) (lamp.center[0] - lo[0], hi[0] - lamp.center[0])
                                       * kGlowEdgeMargin / outer);
        radiusZ = (std::min) (radiusZ, (std::min) (lamp.center[2] - lo[2], hi[2] - lamp.center[2])
                                       * kGlowEdgeMargin / outer);
    }

    // Elliptical, following the lens's own proportions: a circular glow over
    // the tall narrow rhombus of the //c power indicator reads as a light
    // behind a round hole rather than as that lens lit up. A round lamp has
    // equal half-extents and gets a circle for free.
    // THE DISC LIES IN THE PANEL, not in a plane of its own. A flat disc on a
    // sloping panel is sliced by it, and the cut is a straight line at the
    // height where the panel overtakes the disc's plane -- which is precisely
    // the hard horizontal edge the //c monitor's halo had across it, two
    // millimeters above the lens. Solving the panel's plane for y at each
    // vertex leaves nothing to slice.
    if (std::fabs (lamp.facing[1]) > 1e-3f)
    {
        slopeX = -lamp.facing[0] / lamp.facing[1];
        slopeZ = -lamp.facing[2] / lamp.facing[1];
    }

    auto  vertexAt = [&] (const GlowBand & band, float angle) -> Dxui3DRenderer::Vertex
    {
        float  color[3] = {};
        float  dx       = radiusX * band.radiusScale * std::cos (angle);
        float  dz       = radiusZ * band.radiusScale * std::sin (angle);

        for (int c = 0; c < 3; c++)
        {
            color[c] = (rgb[c] + (1.0f - rgb[c]) * band.whiteMix) * band.alpha;
        }

        return Dxui3DRenderer::Vertex
        {
            lamp.center[0] + dx,
            y + slopeX * dx + slopeZ * dz,
            lamp.center[2] + dz,
            0.0f, 0.0f,
            color[0], color[1], color[2], band.alpha
        };
    };

    for (int i = 0; i < kGlowSegments; i++)
    {
        float  a0 = 6.2831853f * (float) i       / (float) kGlowSegments;
        float  a1 = 6.2831853f * (float) (i + 1) / (float) kGlowSegments;

        out.push_back (vertexAt (kGlowProfile[0], 0.0f));
        out.push_back (vertexAt (kGlowProfile[1], a0));
        out.push_back (vertexAt (kGlowProfile[1], a1));

        for (int band = 1; band < kBandCount - 1; band++)
        {
            const GlowBand &  inner = kGlowProfile[band];
            const GlowBand &  outer = kGlowProfile[band + 1];

            out.push_back (vertexAt (inner, a0));
            out.push_back (vertexAt (outer, a0));
            out.push_back (vertexAt (outer, a1));

            out.push_back (vertexAt (inner, a0));
            out.push_back (vertexAt (outer, a1));
            out.push_back (vertexAt (inner, a1));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::BuildContactShadow
//
//  A filled core over the ground footprint, then a penumbra skirt: four side
//  bands and four rounded corners, each running from the silhouette at full
//  darkness out to nothing. Two alpha stops across the margin rather than one,
//  for the same reason the glow has bands -- a linear ramp reads as a gray
//  gradient, while a fast-then-slow falloff reads as a shadow.
//
//  Model space, so the composition's world matrix places it with the device.
//  Black premultiplied: the color channels are zero at every alpha.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::BuildContactShadow (const DeskSceneModel                & model,
                                    float                                 marginSideMm,
                                    float                                 marginDepthMm,
                                    std::vector<Dxui3DRenderer::Vertex> & out)
{
    float   boundsMin[3] = {};
    float   lo[2]        = {};
    float   hi[2]        = {};



    out.clear();

    model.BoundsMin    (boundsMin);
    model.FootprintMin (lo);
    model.FootprintMax (hi);

    if (lo[0] >= hi[0] || lo[1] >= hi[1])
    {
        return;
    }

    float  z = boundsMin[2];

    auto  vertexAt = [&] (float x, float y, float alpha) -> Dxui3DRenderer::Vertex
    {
        return Dxui3DRenderer::Vertex { x, y, z, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, alpha };
    };

    auto  quad = [&] (float x0, float y0, float a0, float x1, float y1, float a1,
                      float x2, float y2, float a2, float x3, float y3, float a3)
    {
        out.push_back (vertexAt (x0, y0, a0));
        out.push_back (vertexAt (x1, y1, a1));
        out.push_back (vertexAt (x2, y2, a2));

        out.push_back (vertexAt (x0, y0, a0));
        out.push_back (vertexAt (x2, y2, a2));
        out.push_back (vertexAt (x3, y3, a3));
    };

    // Core: the footprint itself, mostly hidden by the device standing on it
    // but visible under overhangs and between the feet.
    quad (lo[0], lo[1], kShadowAlpha, hi[0], lo[1], kShadowAlpha,
          hi[0], hi[1], kShadowAlpha, lo[0], hi[1], kShadowAlpha);

    // The penumbra's two stops, as a fraction of whichever axis's margin.
    const float  stops[3][2] =
    {
        { 0.0f,           kShadowAlpha    },
        { kShadowMidStop, kShadowMidAlpha },
        { 1.0f,           0.0f            },
    };

    for (int band = 0; band < 2; band++)
    {
        float  f0 = stops[band][0],     a0 = stops[band][1];
        float  f1 = stops[band + 1][0], a1 = stops[band + 1][1];
        float  s0 = f0 * marginSideMm,  s1 = f1 * marginSideMm;
        float  d0 = f0 * marginDepthMm, d1 = f1 * marginDepthMm;

        quad (lo[0], lo[1] - d0, a0, hi[0], lo[1] - d0, a0,        // front (-Y)
              hi[0], lo[1] - d1, a1, lo[0], lo[1] - d1, a1);
        quad (lo[0], hi[1] + d1, a1, hi[0], hi[1] + d1, a1,        // back (+Y)
              hi[0], hi[1] + d0, a0, lo[0], hi[1] + d0, a0);
        quad (lo[0] - s1, lo[1], a1, lo[0] - s0, lo[1], a0,        // left
              lo[0] - s0, hi[1], a0, lo[0] - s1, hi[1], a1);
        quad (hi[0] + s0, lo[1], a0, hi[0] + s1, lo[1], a1,        // right
              hi[0] + s1, hi[1], a1, hi[0] + s0, hi[1], a0);
    }

    // Corners: a fan per corner, swept through the same two bands so the
    // penumbra turns the corner instead of squaring off. The sweep rides the
    // per-axis margins, so it traces an ellipse quadrant, not a circle.
    const float  corners[4][4] =
    {
        { lo[0], lo[1], -1.0f, -1.0f },
        { hi[0], lo[1],  1.0f, -1.0f },
        { hi[0], hi[1],  1.0f,  1.0f },
        { lo[0], hi[1], -1.0f,  1.0f },
    };

    for (int c = 0; c < 4; c++)
    {
        float  cx = corners[c][0];
        float  cy = corners[c][1];

        for (int band = 0; band < 2; band++)
        {
            float  f0 = stops[band][0],     a0 = stops[band][1];
            float  f1 = stops[band + 1][0], a1 = stops[band + 1][1];

            for (int s = 0; s < kShadowCornerSegs; s++)
            {
                float  t0  = 1.5707963f * (float) s       / (float) kShadowCornerSegs;
                float  t1  = 1.5707963f * (float) (s + 1) / (float) kShadowCornerSegs;
                float  ux0 = corners[c][2] * std::cos (t0) * marginSideMm;
                float  uy0 = corners[c][3] * std::sin (t0) * marginDepthMm;
                float  ux1 = corners[c][2] * std::cos (t1) * marginSideMm;
                float  uy1 = corners[c][3] * std::sin (t1) * marginDepthMm;

                quad (cx + ux0 * f0, cy + uy0 * f0, a0,
                      cx + ux1 * f0, cy + uy1 * f0, a0,
                      cx + ux1 * f1, cy + uy1 * f1, a1,
                      cx + ux0 * f1, cy + uy0 * f1, a1);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::DrawShadows
//
//  Before the bodies and without depth: the shadow lies in the ground plane
//  the devices stand on, so writing depth from a transparent skirt would punch
//  a hole in whatever the device draws next.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::DrawShadows (const DeskSceneComposition & comp,
                                const D3D11_VIEWPORT       & viewport,
                                bool                         includeMonitor)
{
    HRESULT   hr      = S_OK;
    float     mvp[16] = {};



    if (includeMonitor && !m_monitorShadowVerts.empty())
    {
        SceneCamera::Mul44 (comp.monitorWorld, comp.viewProj, mvp);

        hr = m_renderer.DrawStatic (m_monitorShadowMesh, m_monitorShadowVerts.data(), m_monitorShadowVerts.size(), m_geometryRev,
                                       mvp, false, viewport, false);
        CHRA (hr);
    }

    for (int drive = 0; drive < comp.driveCount && !m_driveShadowVerts.empty(); drive++)
    {
        SceneCamera::Mul44 (comp.driveWorld[drive], comp.viewProj, mvp);

        hr = m_renderer.DrawStatic (m_driveShadowMesh, m_driveShadowVerts.data(), m_driveShadowVerts.size(), m_geometryRev,
                                       mvp, false, viewport, false);
        CHRA (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::DrawLampGlows
//
//  Runs last in a frame, testing depth but never writing it: a glow is light,
//  not a solid, so it must not leave its transparent rim in the depth buffer
//  -- but it DOES have to hide behind the housing, or the power lamp's halo
//  floats over the case corner instead of shining out of the notch its lens
//  is actually sunk into.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::DrawLampGlows (const DeskSceneComposition & comp,
                                  const D3D11_VIEWPORT       & viewport,
                                  bool                         includeMonitor)
{
    HRESULT   hr      = S_OK;
    float     mvp[16] = {};



    if (includeMonitor && m_powerLampOn && !m_monitorGlowVerts.empty())
    {
        SceneCamera::Mul44 (comp.monitorWorld, comp.viewProj, mvp);

        hr = m_renderer.DrawStatic (m_monitorGlowMesh, m_monitorGlowVerts.data(), m_monitorGlowVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true, false);
        CHRA (hr);
    }

    for (int drive = 0; drive < comp.driveCount && !m_driveGlowVerts.empty(); drive++)
    {
        if (!m_driveActive[drive])
        {
            continue;
        }

        SceneCamera::Mul44 (comp.driveWorld[drive], comp.viewProj, mvp);

        hr = m_renderer.DrawStatic (m_driveGlowMesh, m_driveGlowVerts.data(), m_driveGlowVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true, false);
        CHRA (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::RenderStrip
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::RenderStrip (ID3D11RenderTargetView * dstRtv, const DeskSceneComposition & strip)
{
    HRESULT          hr       = S_OK;
    D3D11_VIEWPORT   viewport = {};



    BAIL_OUT_IF (!m_modelsLoaded || strip.driveCount <= 0, S_OK);
    BAIL_OUT_IF (strip.viewportPx.right <= strip.viewportPx.left, S_OK);
    BAIL_OUT_IF (dstRtv == nullptr || m_context == nullptr, S_OK);

    m_context->OMSetRenderTargets (1, &dstRtv, nullptr);

    if (m_lampsDirty)
    {
        RebuildLampVerts();
    }

    viewport.TopLeftX = (float) strip.viewportPx.left;
    viewport.TopLeftY = (float) strip.viewportPx.top;
    viewport.Width    = (float) (strip.viewportPx.right - strip.viewportPx.left);
    viewport.Height   = (float) (strip.viewportPx.bottom - strip.viewportPx.top);
    viewport.MaxDepth = 1.0f;

    // Its own depth pass: the strip overlays the finished frame.
    hr = m_renderer.BeginDepthPass();
    CHRA (hr);

    m_stripPass = true;

    hr = DrawShadows (strip, viewport, false);
    CHRA (hr);

    hr = DrawDrives (strip, viewport);
    CHRA (hr);

    hr = DrawLampGlows (strip, viewport, false);
    CHRA (hr);

Error:
    m_stripPass = false;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::RebuildLampVerts
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::RebuildLampVerts()
{
    TintInto (m_monitor.LampVerts(), m_powerLampOn ? 1.0f : kLampOffDim, m_monitorLampVerts);
    TintInto (m_drive.LampVerts(),   m_driveActive[0] ? 1.0f : kLampOffDim, m_driveLampVerts[0]);
    TintInto (m_drive.LampVerts(),   m_driveActive[1] ? 1.0f : kLampOffDim, m_driveLampVerts[1]);

    m_lampsDirty = false;

    TouchGeometry();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::FillViewportBlack
//
//  One opaque black quad over the whole viewport, at the far depth so it
//  depth-tests away nothing else in the scene. This is the glass-only
//  presentation's background: the plate is a LAYER and clears transparent, so
//  without it the theme's backdrop is visible wherever the tube does not
//  cover.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::FillViewportBlack (const D3D11_VIEWPORT & viewport)
{
    HRESULT                 hr           = S_OK;
    float                   identity[16] = {};
    Dxui3DRenderer::Vertex  quad[6]      = {};



    SceneCamera::Identity44 (identity);

    quad[0] = { -1.0f,  1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    quad[1] = {  1.0f,  1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    quad[2] = {  1.0f, -1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    quad[3] = { -1.0f,  1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    quad[4] = {  1.0f, -1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };
    quad[5] = { -1.0f, -1.0f, 1.0f, 0, 0, 0.0f, 0.0f, 0.0f, 1.0f };

    hr = m_renderer.DrawTriangles (quad, 6, identity, false, viewport, false);
    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::DrawDebugRect
//
//  Four thin bars in clip space (identity MVP) over a full-backbuffer
//  viewport -- layout diagnosis only.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::DrawDebugRect (const RECT & rectPx, int backBufferW, int backBufferH, uint32_t argb)
{
    HRESULT         hr           = S_OK;
    D3D11_VIEWPORT  viewport     = {};
    float           r            = (float) ((argb >> 16) & 0xFF) / 255.0f;
    float           g            = (float) ((argb >> 8) & 0xFF) / 255.0f;
    float           b            = (float) (argb & 0xFF) / 255.0f;
    float           identity[16] = {};



    if (backBufferW <= 0 || backBufferH <= 0)
    {
        return;
    }

    SceneCamera::Identity44 (identity);

    viewport.Width    = (float) backBufferW;
    viewport.Height   = (float) backBufferH;
    viewport.MaxDepth = 1.0f;

    {
        RECT   bars[4] = { { rectPx.left, rectPx.top, rectPx.right, rectPx.top + 2 },
                           { rectPx.left, rectPx.bottom - 2, rectPx.right, rectPx.bottom },
                           { rectPx.left, rectPx.top, rectPx.left + 2, rectPx.bottom },
                           { rectPx.right - 2, rectPx.top, rectPx.right, rectPx.bottom } };

        for (const RECT & bar : bars)
        {
            float                   x0      = (float) bar.left / (float) backBufferW * 2.0f - 1.0f;
            float                   x1      = (float) bar.right / (float) backBufferW * 2.0f - 1.0f;
            float                   y0      = 1.0f - (float) bar.top / (float) backBufferH * 2.0f;
            float                   y1      = 1.0f - (float) bar.bottom / (float) backBufferH * 2.0f;
            Dxui3DRenderer::Vertex  quad[6] = {};

            quad[0] = { x0, y0, 0.5f, 0, 0, r, g, b, 1.0f };
            quad[1] = { x1, y0, 0.5f, 0, 0, r, g, b, 1.0f };
            quad[2] = { x1, y1, 0.5f, 0, 0, r, g, b, 1.0f };
            quad[3] = { x0, y0, 0.5f, 0, 0, r, g, b, 1.0f };
            quad[4] = { x1, y1, 0.5f, 0, 0, r, g, b, 1.0f };
            quad[5] = { x0, y1, 0.5f, 0, 0, r, g, b, 1.0f };

            hr = m_renderer.DrawTriangles (quad, 6, identity, false, viewport, false);
            IGNORE_RETURN_VALUE (hr, S_OK);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::RenderPlate
//
//  Draws everything EXCEPT the picture into the cached plate: the desk, the
//  case, the bezel, the drives, the tube ring, the mask, the sheen, the lamps
//  and their glows. All of it is furniture -- it changes when a model loads,
//  the window resizes or a lamp lights, and not otherwise -- so it is drawn
//  into a texture and kept, and the frames in between just lay that texture
//  down again.
//
//  This is where the multisampling happens, which is the point: the resolve
//  used to be paid sixty times a second to antialias edges that had not moved
//  since the last time. Measured at ~20 points of GPU on this machine.
//
//  THE GLASS-ONLY PRESENTATION skips most of that. Fullscreen draws the tube
//  and nothing else, on black, so the case, the bezel, the power lamp and the
//  ground shadows are all left out. The camera sits at whatever distance
//  shows the whole picture, and on a screen wider than the glass that leaves
//  space beside the tube; filling it with a case cropped top and bottom looks
//  worse than filling it with black. What remains is the tube, the faceplate
//  mask and the sheen -- the layers the raster itself sits in.
//
//  The front plate's occluder pass exists only to put the case back in front
//  of the raster, so with no case drawn that whole pass is skipped too.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::RenderPlate (const D3D11_VIEWPORT & viewport, int width, int height)
{
    HRESULT                  hr           = S_OK;
    HRESULT                  hrEnd        = S_OK;
    float                    mvp[16]      = {};
    float                    clear[4]     = { 0.0f, 0.0f, 0.0f, 0.0f };
    bool                     glassOnly    = m_comp.glassOnly != 0;
    ID3D11RenderTargetView * rawPlate     = nullptr;



    hr = EnsurePlateTarget (width, height);
    CHRA (hr);

    // Shadows first, and OUTSIDE the multisampled detour: a depth-only pass
    // wants neither the MSAA target nor the scene's own depth buffer, and
    // filling the maps before anything samples them is the whole ordering
    // requirement.
    hr = RenderShadowMaps (m_comp, viewport);
    CHRA (hr);

    // Behind the picture first.
    rawPlate = m_backPlateRtv.Get();
    m_context->OMSetRenderTargets (1, &rawPlate, nullptr);

    // Transparent, not the theme's backdrop: the plate is a LAYER. Whatever
    // the frame already holds shows through wherever the devices do not
    // stand, which is what keeps the desk background the host's business.
    m_context->ClearRenderTargetView (rawPlate, clear);

    hr = m_renderer.BeginMultisampledScene();
    CHRA (hr);

    hr = m_renderer.BeginDepthPass();
    CHRA (hr);

    if (glassOnly)
    {
        FillViewportBlack (viewport);
    }
    else
    {
        hr = DrawShadows (m_comp, viewport, true);
        CHRA (hr);

        // Opaque bodies: monitor, then each placed drive.
        SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

        SetModelLighting (m_monitor, m_comp.monitorWorld, m_powerLampOn, kMonitorGlowRgb);

        hr = m_renderer.DrawStatic (m_monitorOpaqueMesh,
                                    m_monitor.OpaqueVerts().data(),
                                    m_monitor.OpaqueVerts().size(),
                                    m_geometryRev, mvp, false, viewport, true);
        CHRA (hr);

        hr = DrawDrives (m_comp, viewport);
        CHRA (hr);
    }

    // THE TILTING ASSEMBLY, on its own transform. Lit through that same
    // transform too: the shader takes its lights in model space, so handing
    // it the untilted placement would leave the bezel lit as though it had
    // never moved. The matrix is built whether or not the bezel draws,
    // because the tube below is drawn with it.
    {
        float  tiltWorld[16] = {};

        BuildTiltedMonitorWorld (m_comp, tiltWorld);
        SceneCamera::Mul44 (tiltWorld, m_comp.viewProj, mvp);

        SetModelLighting (m_monitor, tiltWorld, m_powerLampOn, kMonitorGlowRgb);

        if (!glassOnly && !m_monitor.TiltableVerts().empty())
        {
            hr = m_renderer.DrawStatic (m_monitorTiltMesh,
                                        m_monitor.TiltableVerts().data(),
                                        m_monitor.TiltableVerts().size(),
                                        m_geometryRev, mvp, false, viewport, true);
            CHRA (hr);
        }
    }

    // The tube (dark, untextured), then the picture band floating on it,
    // sampling the CRT output. It rides the bezel, so it takes the same
    // matrix the assembly just used.
    if (!m_glassVerts.empty())
    {
        hr = m_renderer.DrawStatic (m_glassMesh, m_glassVerts.data(), m_glassVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    // And now what goes IN FRONT of it.
    //
    // Two plates, not one, and this is the seam. Everything above is BEHIND
    // the raster and opaque -- the cavity lining shows straight through the
    // mouth -- so a single plate laid over the picture simply hid it. What
    // follows belongs on top of the raster instead, and gets its own layer.
    //
    // The depth buffer carries over deliberately: BeginDepthPass is NOT
    // called again, so the sheen and the glows still test against the case
    // and bezel that the first half drew.
    hr = m_renderer.EndMultisampledScene();
    CHRA (hr);

    rawPlate = m_frontPlateRtv.Get();
    m_context->OMSetRenderTargets (1, &rawPlate, nullptr);
    m_context->ClearRenderTargetView (rawPlate, clear);

    hr = m_renderer.BeginMultisampledScene();
    CHRA (hr);

    // THE PICTURE'S OCCLUDERS. The picture is live and composites BETWEEN
    // the plates with no depth of its own, which was fine while the camera
    // faced the screen and wrong the moment it could orbit: from behind, the
    // case is all in the back plate and the picture printed straight over
    // it, a raster floating in the middle of a cabinet.
    //
    // So the front plate carries the case wherever the case is NEARER THAN
    // THE PICTURE. First the picture band itself goes in as a depth stamp --
    // its own triangles with every color zeroed, writing depth and nothing
    // else -- and then the opaque bodies are drawn again, depth-tested. The
    // carried-over depth already holds the whole scene, so the re-draw
    // passes exactly on each body's visible surface (LESS_EQUAL, and this
    // pass is why the renderer's depth test is LESS_EQUAL rather than LESS),
    // except where the picture's stamp is now nearer -- the mouth, seen from
    // the front. Behind and beside, the case wins and covers the raster at
    // composite time; head on, the mouth stays open. The cavity never
    // qualifies, because it lies BEHIND the stamp.
    // TWO MATRICES FROM HERE DOWN, and every draw names the one it means.
    // The case's placement and the tube's (case plus tilt) both flow through
    // this stretch, and the first version of the tilt reused one `mvp` local
    // for both: the stamp wrote the tilted matrix into it, and the whole-case
    // re-draw, the mask, the sheen and the power lamp all inherited it -- the
    // entire monitor drawn a SECOND time, tilted, over the untilted one.
    // Panels sheared, the bell doubled, the rear ripped. What rides the tube
    // takes tubeMvp; what is part of the case takes caseMvp; nothing takes
    // "whatever mvp holds right now".
    {
        float  tiltWorld[16] = {};
        float  caseMvp[16]   = {};
        float  tubeMvp[16]   = {};

        BuildTiltedMonitorWorld (m_comp, tiltWorld);
        SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, caseMvp);
        SceneCamera::Mul44 (tiltWorld,           m_comp.viewProj, tubeMvp);

        // With no case drawn there is nothing to place in front of the
        // raster, and the stamp exists only to cut a mouth in that case.
        if (!glassOnly && !m_pictureDepthVerts.empty())
        {
            // The stamp is the picture's own footprint, so it travels with
            // the tube -- left on the case's placement it opens the mouth
            // where the raster used to be and masks it where the raster is.
            hr = m_renderer.DrawStatic (m_pictureDepthMesh, m_pictureDepthVerts.data(),
                                        m_pictureDepthVerts.size(), m_geometryRev,
                                        tubeMvp, false, viewport, true);
            CHRA (hr);

            SetModelLighting (m_monitor, m_comp.monitorWorld, m_powerLampOn, kMonitorGlowRgb);

            hr = m_renderer.DrawStatic (m_monitorOpaqueMesh,
                                        m_monitor.OpaqueVerts().data(),
                                        m_monitor.OpaqueVerts().size(),
                                        m_geometryRev, caseMvp, false, viewport, true, false);
            CHRA (hr);

            // THE BEZEL TOO. This re-draw is what puts the case back IN
            // FRONT of the raster, and when the bezel moved out of the
            // opaque batch it silently left this pass -- so the picture
            // composited straight over it, and from any angle where the two
            // overlap on screen the CRT showed through the bezel's side.
            // Seen from in front nothing looked wrong, which is how it
            // survived a first report of exactly this.
            if (!m_monitor.TiltableVerts().empty())
            {
                SetModelLighting (m_monitor, tiltWorld, m_powerLampOn, kMonitorGlowRgb);

                hr = m_renderer.DrawStatic (m_monitorTiltMesh,
                                            m_monitor.TiltableVerts().data(),
                                            m_monitor.TiltableVerts().size(),
                                            m_geometryRev, tubeMvp, false, viewport, true, false);
                CHRA (hr);
            }

            hr = DrawDrives (m_comp, viewport);
            CHRA (hr);
        }

        // The mask and the sheen are the faceplate's own layers, so they ride
        // the tube.
        if (!m_maskVerts.empty())
        {
            hr = m_renderer.DrawStatic (m_maskMesh, m_maskVerts.data(), m_maskVerts.size(), m_geometryRev,
                                           tubeMvp, false, viewport, true);
            CHRA (hr);
        }

        // The reflection goes on last of the tube's layers and over the mask,
        // because a real faceplate reflects the room across its whole face,
        // dark border included. Depth tested but not written: it is light,
        // and the bezel in front of it must still win.
        if (!m_sheenVerts.empty())
        {
            hr = m_renderer.DrawStatic (m_sheenMesh, m_sheenVerts.data(), m_sheenVerts.size(), m_geometryRev,
                                           tubeMvp, false, viewport, true, false);
            CHRA (hr);
        }

        // The power lamp sits in the case's notch, not on the tube.
        if (!glassOnly && !m_monitorLampVerts.empty())
        {
            hr = m_renderer.DrawStatic (m_monitorLampMesh, m_monitorLampVerts.data(), m_monitorLampVerts.size(), m_geometryRev,
                                           caseMvp, false, viewport, true);
            CHRA (hr);
        }
    }

    // The disk names go on after every opaque body, and before the glows.
    // Anything drawn later that writes depth would simply paint over them:
    // they test depth without writing it, which is what keeps a name from
    // leaving its transparent rectangle in the buffer.
    //
    // Both belong to bodies the glass-only presentation leaves out: the names
    // are anchored to the drives, and the only glow is the power lamp's.
    if (!glassOnly)
    {
        hr = DrawDiskLabels (m_comp, viewport);
        CHRA (hr);

        hr = DrawLampGlows (m_comp, viewport, true);
        CHRA (hr);
    }

Error:
    // Resolve and composite on EVERY exit, not just the clean one: a failure
    // between Begin and End would otherwise strand the detour armed, and
    // every later draw would land in an offscreen target nobody composites --
    // the scene would simply vanish. A no-op when Begin declined.
    hrEnd = m_renderer.EndMultisampledScene();
    IGNORE_RETURN_VALUE (hrEnd, S_OK);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::EnsurePlateTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::EnsurePlateTarget (int width, int height)
{
    HRESULT               hr     = S_OK;
    D3D11_TEXTURE2D_DESC  desc   = {};
    ComPtr<ID3D11Device>  device;



    CBREx (width > 0 && height > 0 && m_context != nullptr, E_INVALIDARG);

    BAIL_OUT_IF (m_backPlateTex != nullptr && m_plateW == width && m_plateH == height, S_OK);

    m_context->GetDevice (device.GetAddressOf());
    CBREx (device != nullptr, E_UNEXPECTED);

    m_backPlateTex.Reset();
    m_backPlateRtv.Reset();
    m_backPlateSrv.Reset();
    m_frontPlateTex.Reset();
    m_frontPlateRtv.Reset();
    m_frontPlateSrv.Reset();

    desc.Width            = (UINT) width;
    desc.Height           = (UINT) height;
    desc.MipLevels        = 1;
    desc.ArraySize        = 1;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage            = D3D11_USAGE_DEFAULT;
    desc.BindFlags        = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D (&desc, nullptr, m_backPlateTex.GetAddressOf());
    CHRA (hr);

    hr = device->CreateRenderTargetView (m_backPlateTex.Get(), nullptr, m_backPlateRtv.GetAddressOf());
    CHRA (hr);

    hr = device->CreateShaderResourceView (m_backPlateTex.Get(), nullptr, m_backPlateSrv.GetAddressOf());
    CHRA (hr);

    hr = device->CreateTexture2D (&desc, nullptr, m_frontPlateTex.GetAddressOf());
    CHRA (hr);

    hr = device->CreateRenderTargetView (m_frontPlateTex.Get(), nullptr, m_frontPlateRtv.GetAddressOf());
    CHRA (hr);

    hr = device->CreateShaderResourceView (m_frontPlateTex.Get(), nullptr, m_frontPlateSrv.GetAddressOf());
    CHRA (hr);

    m_plateW = width;
    m_plateH = height;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::Render
//
//  Two layers now: the picture, which changes, drawn straight into the frame;
//  and the plate, which does not, laid over it. Compositing is associative
//  under premultiplied source-over, so plate-over-picture lands on exactly the
//  pixels drawing them in order did -- the mask and the sheen still sit on the
//  raster, and the bezel still hides what is behind it.
//
//  The picture is the only thing under the plate, and it never reaches beyond
//  the mask's opening, so it needs no depth test of its own: wherever the
//  monitor's body would have occluded it, the plate is opaque.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::Render (ID3D11RenderTargetView   * dstRtv,
                           ID3D11ShaderResourceView * displaySrv,
                           const CrtUvRect          & displayUv,
                           int                        displayW,
                           int                        displayH)
{
    HRESULT                 hr        = S_OK;
    D3D11_VIEWPORT          viewport  = {};
    float                   mvp[16]   = {};
    bool                    uvChanged = false;
    int                     targetW   = 0;
    int                     targetH   = 0;
    PlateKey                key;
    ComPtr<ID3D11Resource>  res;
    ComPtr<ID3D11Texture2D> tex;
    D3D11_TEXTURE2D_DESC    dstDesc   = {};



    BAIL_OUT_IF (!m_modelsLoaded || m_comp.viewportPx.right <= m_comp.viewportPx.left, S_OK);
    BAIL_OUT_IF (dstRtv == nullptr || m_context == nullptr, S_OK);

    // Bind the destination explicitly: the CRT offscreen pass that just ran
    // left the display texture bound, and BeginDepthPass sizes its buffer by
    // querying whatever is bound.
    m_context->OMSetRenderTargets (1, &dstRtv, nullptr);

    uvChanged = m_glassUvDirty ||
                displayUv.u0 != m_glassUv.u0 || displayUv.v0 != m_glassUv.v0 ||
                displayUv.u1 != m_glassUv.u1 || displayUv.v1 != m_glassUv.v1;

    if (uvChanged)
    {
        RebuildGlassUvs (displayUv, displayW, displayH);
    }

    if (m_lampsDirty)
    {
        RebuildLampVerts();
    }

    dstRtv->GetResource (res.GetAddressOf());
    hr = res.As (&tex);
    CHRA (hr);

    tex->GetDesc (&dstDesc);
    targetW = (int) dstDesc.Width;
    targetH = (int) dstDesc.Height;

    CBREx (targetW > 0 && targetH > 0, E_UNEXPECTED);

    viewport.TopLeftX = (float) m_comp.viewportPx.left;
    viewport.TopLeftY = (float) m_comp.viewportPx.top;
    viewport.Width    = (float) (m_comp.viewportPx.right - m_comp.viewportPx.left);
    viewport.Height   = (float) (m_comp.viewportPx.bottom - m_comp.viewportPx.top);
    viewport.MaxDepth = 1.0f;

    // Everything the plate depends on, compared as one blob. A field added to
    // the composition or forgotten here would show as a scene that stops
    // responding, so the comparison is deliberately of the WHOLE struct
    // rather than of the handful of members that seem to matter.
    key.geometryRev = m_geometryRev;
    key.targetW     = targetW;
    key.targetH     = targetH;
    key.samples     = m_renderer.SceneSampleCount();
    key.clip        = m_clipRect;
    key.hasClip     = m_hasClip ? 1 : 0;
    key.comp        = m_comp;

    if (!m_plateValid || memcmp (&key, &m_plateKey, sizeof (key)) != 0)
    {
        hr = RenderPlate (viewport, targetW, targetH);
        CHRA (hr);

        m_plateKey   = key;
        m_plateValid = true;

        m_context->OMSetRenderTargets (1, &dstRtv, nullptr);
    }

    // Back plate, picture, front plate -- the stack in order.
    if (m_backPlateSrv != nullptr)
    {
        hr = m_renderer.CompositeFullTarget (m_backPlateSrv.Get(), m_plateW, m_plateH);
        CHRA (hr);
    }

    if (!m_pictureVerts.empty() && displaySrv != nullptr)
    {
        // The raster is ON the tube. Its mesh is built from the glass surface
        // in model space, so tilting it is the same one transform the tube
        // and the bezel already ride -- without this the picture hangs in the
        // air where the screen used to be while the screen swings away.
        float  tiltWorld[16] = {};

        BuildTiltedMonitorWorld (m_comp, tiltWorld);
        SceneCamera::Mul44 (tiltWorld, m_comp.viewProj, mvp);

        m_renderer.SetContentSrv (displaySrv);

        hr = m_renderer.DrawStatic (m_pictureMesh, m_pictureVerts.data(), m_pictureVerts.size(),
                                    m_geometryRev, mvp, true, viewport, false);

        m_renderer.SetContentSrv (nullptr);
        CHRA (hr);
    }

    if (m_frontPlateSrv != nullptr)
    {
        hr = m_renderer.CompositeFullTarget (m_frontPlateSrv.Get(), m_plateW, m_plateH);
        CHRA (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::SetDiskLabel
//
//  Makes triangles of the billboard's four world corners.
//
//  WORLD SPACE, not the drive's. The quad faces the camera and covers a fixed
//  number of pixels, so it cannot be expressed in the drive's own frame --
//  the drive turns under the orbit and the name must not turn with it. Its
//  placement is solved in DeskSceneLayout, where a test can reach it, and
//  arrives here already positioned.
//
//  The normals stay zero: zero is the renderer's "unlit", and a name should
//  read the same whatever the room lights are doing to the case behind it.
//
////////////////////////////////////////////////////////////////////////////////

void DeskScene::SetDiskLabel (int drive, ID3D11ShaderResourceView * srv, const float corners[4][3],
                              const float uv[4])
{
    // Two triangles over the corner order the layout hands over -- top-left,
    // top-right, bottom-left, bottom-right -- with v running down the
    // texture as the quad runs down the screen. The corner's two bits pick
    // its edge of the caller's sub-rectangle.
    static constexpr int                 kTri[6] = { 0, 1, 2, 2, 1, 3 };
    std::vector<Dxui3DRenderer::Vertex>  next;



    if (drive < 0 || drive >= 2)
    {
        return;
    }

    if (srv != nullptr && corners != nullptr && uv != nullptr)
    {
        for (int index = 0; index < 6; index++)
        {
            Dxui3DRenderer::Vertex   v      = {};
            int                      corner = kTri[index];

            v.x = corners[corner][0];
            v.y = corners[corner][1];
            v.z = corners[corner][2];
            v.u = (corner & 1) ? uv[2] : uv[0];
            v.v = (corner & 2) ? uv[3] : uv[1];
            v.r = 1.0f;  v.g = 1.0f;  v.b = 1.0f;  v.a = 1.0f;

            next.push_back (v);
        }
    }

    // AN UNCHANGED QUAD COSTS NOTHING. This runs on every composition pass,
    // and the plate exists precisely so a still scene is not redrawn -- so
    // invalidating it here unconditionally would retire the cache outright
    // and put the whole desk back on the GPU every frame.
    if (m_diskLabelSrv[drive] == srv && m_diskLabelVerts[drive].size() == next.size() &&
        (next.empty() ||
         memcmp (m_diskLabelVerts[drive].data(), next.data(),
                 next.size() * sizeof (Dxui3DRenderer::Vertex)) == 0))
    {
        return;
    }

    m_diskLabelSrv[drive]   = srv;
    m_diskLabelVerts[drive] = std::move (next);

    // The quad alone, not the scene's furniture: a name that re-places every
    // time the camera moves must not drag two megabytes of case through the
    // upload path with it.
    m_diskLabelRev++;
    InvalidatePlate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene::DrawDiskLabels
//
//  Draws each mounted image's name, depth-tested against the scene the passes
//  before it built.
//
//  THE VERTICES ARE ALREADY IN WORLD SPACE, so the transform is the shared
//  viewProj alone. Every other draw in this file multiplies a device's world
//  matrix in first; this one must not, or the name would turn with the drive
//  and the constant pixel size the layout solved for would be undone.
//
//  Depth TESTED, never WRITTEN. A name is a transparent decal, and writing
//  its rectangle into the buffer would let the blank corners occlude the lamp
//  glows that come after it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::DrawDiskLabels (const DeskSceneComposition & comp, const D3D11_VIEWPORT & viewport)
{
    HRESULT  hr = S_OK;



    for (int drive = 0; drive < comp.driveCount && drive < 2; drive++)
    {
        if (m_diskLabelVerts[drive].empty() || m_diskLabelSrv[drive] == nullptr)
        {
            continue;
        }

        // The content view is swapped in for this one draw and put back:
        // the glass owns it for the rest of the frame.
        m_renderer.SetContentSrv (m_diskLabelSrv[drive]);

        hr = m_renderer.DrawStatic (m_diskLabelMesh[drive],
                                    m_diskLabelVerts[drive].data(),
                                    m_diskLabelVerts[drive].size(),
                                    m_diskLabelRev, comp.viewProj, true, viewport, true, false);

        m_renderer.SetContentSrv (nullptr);
        CHRA (hr);
    }

Error:
    return hr;
}
