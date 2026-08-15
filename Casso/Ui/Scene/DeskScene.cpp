#include "Pch.h"

#include "Ui/Scene/DeskScene.h"

#include "Render/SceneCamera.h"





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

HRESULT DeskScene::LoadModels (DeskDeviceKind       monitorKind,
                               const std::string  & monitorObj, const std::string & monitorMtl,
                               const std::string  & driveObj,   const std::string & driveMtl)
{
    HRESULT   hr = S_OK;



    hr = m_monitor.Load (monitorKind, monitorObj, monitorMtl);
    CHRA (hr);

    hr = m_drive.Load (DeskDeviceKind::DiskII, driveObj, driveMtl);
    CHRA (hr);

    // The per-drive badge text, stamped onto the model's badge plaque
    // (plaque 13..52 x 64..74 mm): the number differs per drive, so the
    // text lives here, not in the shared model.
    {
        constexpr float   kLabelCellMm = 0.85f;
        constexpr float   kLabelLeftMm = 15.2f;
        constexpr float   kLabelTopZMm = 80.4f;   // on the badge plaque (71.4 .. 82.6)
        constexpr float   kLabelFrontY = -1.9f;
        constexpr float   kLabelRgb[3] = { 0.150f, 0.140f, 0.130f };

        m_driveLabelVerts[0].clear();
        m_driveLabelVerts[1].clear();

        DeskSceneModel::StampText (m_driveLabelVerts[0], "DRIVE 1", kLabelLeftMm,
                                   kLabelTopZMm, kLabelCellMm, kLabelFrontY, kLabelRgb);
        DeskSceneModel::StampText (m_driveLabelVerts[1], "DRIVE 2", kLabelLeftMm,
                                   kLabelTopZMm, kLabelCellMm, kLabelFrontY, kLabelRgb);
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
    BuildLampGlow (m_monitor, kMonitorGlowRgb, m_monitorGlowVerts);
    BuildLampGlow (m_drive,   kDriveGlowRgb,   m_driveGlowVerts);

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

    m_driveWp[drive] = writeProtected;

    if (m_doorProgress[drive] < 0.0f ||
        std::abs (doorProgress - m_doorProgress[drive]) > kDoorProgressEps)
    {
        m_doorProgress[drive] = doorProgress;
        m_driveDoorVerts[drive].clear();   // rebuilt lazily in Render
    }
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
        outline (bx0, bx1, bz0, bz1, 0.0f,       bandRing);
        outline (ox0, ox1, oz0, oz1, radius,     openRing);

        // The tube ring: band -> glass edge, ON the surface (no lift).
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
    BuildGlassSheen (surface);

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

void DeskScene::BuildGlassSheen (const CurvedDisplaySurface & surface)
{
    float   cx     = (surface.x0 + surface.x1) * 0.5f;
    float   cz     = (surface.z0 + surface.z1) * 0.5f;
    float   apexY  = surface.baseY - CurvedDisplayMath::MaxSag (surface);
    float   ctr[3] = { cx, apexY + surface.radius, cz };
    float   eye[3] = { cx, apexY - kSheenEyeMm, cz };
    float   ll     = std::sqrt (kSheenLight[0] * kSheenLight[0] +
                                kSheenLight[1] * kSheenLight[1] +
                                kSheenLight[2] * kSheenLight[2]);



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
                    h[i] = toEye[i] / el + kSheenLight[i] / ll;
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

        hr = m_renderer.DrawStatic (m_driveOpaqueMesh[m_driveActive[drive] ? 1 : 0],
                                    m_drive.OpaqueVerts (m_driveActive[drive]).data(),
                                    m_drive.OpaqueVerts (m_driveActive[drive]).size(),
                                    m_geometryRev, mvp, false, viewport, true);
        CHRA (hr);

        // Door assembly: a rotated copy of the model's cached verts, rebuilt
        // only when SetDriveVisuals moved the progress.
        if (m_driveDoorVerts[drive].empty() && !m_drive.DoorVerts().empty())
        {
            float   progress = std::clamp (m_doorProgress[drive], 0.0f, 1.0f);
            float   pivotY   = 0.0f;
            float   pivotZ   = 0.0f;

            m_drive.DoorPivot (pivotY, pivotZ);
            DeskSceneModel::RotateDoorVerts (m_drive.DoorVerts(), pivotY, pivotZ,
                                             progress * kDoorOpenRad, m_driveDoorVerts[drive]);
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

        if (!m_driveLabelVerts[drive].empty())
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
    auto  vertexAt = [&] (const GlowBand & band, float angle) -> Dxui3DRenderer::Vertex
    {
        float  color[3] = {};

        for (int c = 0; c < 3; c++)
        {
            color[c] = (rgb[c] + (1.0f - rgb[c]) * band.whiteMix) * band.alpha;
        }

        return Dxui3DRenderer::Vertex
        {
            lamp.center[0] + radiusX * band.radiusScale * std::cos (angle),
            y,
            lamp.center[2] + radiusZ * band.radiusScale * std::sin (angle),
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

    hr = DrawShadows (strip, viewport, false);
    CHRA (hr);

    hr = DrawDrives (strip, viewport);
    CHRA (hr);

    hr = DrawLampGlows (strip, viewport, false);
    CHRA (hr);

Error:
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
//  DeskScene::Render
//
//  Matrix multiplies and draw submission only -- geometry was cached at load
//  or on the change that dirtied it. Depth-tested throughout: the devices are
//  solid bodies, and the glass sits proud of the monitor's recess.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::Render (ID3D11RenderTargetView   * dstRtv,
                           ID3D11ShaderResourceView * displaySrv,
                           const CrtUvRect          & displayUv,
                           int                        displayW,
                           int                        displayH)
{
    HRESULT          hr        = S_OK;
    D3D11_VIEWPORT   viewport  = {};
    float            mvp[16]   = {};
    bool             uvChanged = false;
    HRESULT          hrEnd     = S_OK;



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

    viewport.TopLeftX = (float) m_comp.viewportPx.left;
    viewport.TopLeftY = (float) m_comp.viewportPx.top;
    viewport.Width    = (float) (m_comp.viewportPx.right - m_comp.viewportPx.left);
    viewport.Height   = (float) (m_comp.viewportPx.bottom - m_comp.viewportPx.top);
    viewport.MaxDepth = 1.0f;

    // Everything from here to EndMultisampledScene lands in the offscreen
    // multisampled target: the case silhouette, the reveal groove, the molded
    // icon's ridges and the bezel's radii are all geometry edges, which is
    // exactly what the resolve smooths.
    hr = m_renderer.BeginMultisampledScene();
    CHRA (hr);

    hr = m_renderer.BeginDepthPass();
    CHRA (hr);

    hr = DrawShadows (m_comp, viewport, true);
    CHRA (hr);

    // Opaque bodies: monitor, then each placed drive.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    hr = m_renderer.DrawStatic (m_monitorOpaqueMesh[m_powerLampOn ? 1 : 0],
                                m_monitor.OpaqueVerts (m_powerLampOn).data(),
                                m_monitor.OpaqueVerts (m_powerLampOn).size(),
                                m_geometryRev, mvp, false, viewport, true);
    CHRA (hr);

    hr = DrawDrives (m_comp, viewport);
    CHRA (hr);

    // The tube (dark, untextured), then the picture band floating on it,
    // sampling the CRT output.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    if (!m_glassVerts.empty())
    {
        hr = m_renderer.DrawStatic (m_glassMesh, m_glassVerts.data(), m_glassVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    if (!m_pictureVerts.empty() && displaySrv != nullptr)
    {
        m_renderer.SetContentSrv (displaySrv);

        hr = m_renderer.DrawStatic (m_pictureMesh, m_pictureVerts.data(), m_pictureVerts.size(), m_geometryRev,
                                       mvp, true, viewport, true);

        m_renderer.SetContentSrv (nullptr);
        CHRA (hr);
    }

    if (!m_maskVerts.empty())
    {
        hr = m_renderer.DrawStatic (m_maskMesh, m_maskVerts.data(), m_maskVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    // The reflection goes on last of the tube's layers and over the mask,
    // because a real faceplate reflects the room across its whole face, dark
    // border included. Depth tested but not written: it is light, and the
    // bezel in front of it must still win.
    if (!m_sheenVerts.empty())
    {
        hr = m_renderer.DrawStatic (m_sheenMesh, m_sheenVerts.data(), m_sheenVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true, false);
        CHRA (hr);
    }

    if (!m_monitorLampVerts.empty())
    {
        hr = m_renderer.DrawStatic (m_monitorLampMesh, m_monitorLampVerts.data(), m_monitorLampVerts.size(), m_geometryRev,
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    hr = DrawLampGlows (m_comp, viewport, true);
    CHRA (hr);

Error:
    // Resolve and composite on EVERY exit, not just the clean one: a failure
    // between Begin and End would otherwise strand the detour armed, and
    // every later draw would land in an offscreen target nobody composites --
    // the scene would simply vanish. A no-op when Begin declined.
    hrEnd = m_renderer.EndMultisampledScene();
    IGNORE_RETURN_VALUE (hrEnd, S_OK);

    return hr;
}
