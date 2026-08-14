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

    // Glow discs: model-space and state-free, so they are built once here and
    // the draw only decides whether a given lamp is lit.
    BuildLampGlow (m_monitor, kMonitorGlowRgb, m_monitorGlowVerts);
    BuildLampGlow (m_drive,   kDriveGlowRgb,   m_driveGlowVerts);

    BuildContactShadow (m_monitor, kMonitorShadowMarginSideMm, kMonitorShadowMarginDepthMm,
                        m_monitorShadowVerts);
    BuildContactShadow (m_drive,   kShadowMarginSideMm,        kShadowMarginDepthMm,
                        m_driveShadowVerts);

    m_modelsLoaded = true;
    m_glassUvDirty = true;
    m_lampsDirty   = true;

Error:
    return hr;
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

    metrics.glass = m_monitor.Surface();

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

        std::vector<Dxui3DRenderer::Vertex> *  target = nullptr;
        float                                  lift   = 0.0f;
        const float                          * tint   = kTubeTint;

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

        // Strips are long (they span the glass), and the sag sphere bulges
        // millimeters over such spans -- far past any lift -- so a flat
        // corner-to-corner quad would slice below the tube and let it poke
        // through as dark petals. Subdivide each strip patch and put every
        // sample on the sphere; the leftover chord error is far under the
        // lift.
        auto pushPatch = [&] (float ax, float az, float bx, float bz,
                              float cx2, float cz2, float dx, float dz)
        {
            constexpr int   kAlong  = 24;
            constexpr int   kAcross = 3;

            for (int i = 0; i < kAlong; i++)
            {
                for (int j = 0; j < kAcross; j++)
                {
                    float   s0 = (float) i / (float) kAlong;
                    float   s1 = (float) (i + 1) / (float) kAlong;
                    float   t0 = (float) j / (float) kAcross;
                    float   t1 = (float) (j + 1) / (float) kAcross;

                    auto lerp2 = [&] (float s, float t, float & outX, float & outZ)
                    {
                        float   topX = ax + (bx - ax) * s;
                        float   topZ = az + (bz - az) * s;
                        float   botX = dx + (cx2 - dx) * s;
                        float   botZ = dz + (cz2 - dz) * s;

                        outX = topX + (botX - topX) * t;
                        outZ = topZ + (botZ - topZ) * t;
                    };

                    float   p00x = 0.0f, p00z = 0.0f, p10x = 0.0f, p10z = 0.0f;
                    float   p11x = 0.0f, p11z = 0.0f, p01x = 0.0f, p01z = 0.0f;

                    lerp2 (s0, t0, p00x, p00z);
                    lerp2 (s1, t0, p10x, p10z);
                    lerp2 (s1, t1, p11x, p11z);
                    lerp2 (s0, t1, p01x, p01z);

                    pushTri (p00x, p00z, p10x, p10z, p11x, p11z);
                    pushTri (p00x, p00z, p11x, p11z, p01x, p01z);
                }
            }
        };

        // The tube ring: band -> glass edge, ON the surface (no lift).
        m_glassVerts.clear();
        target = &m_glassVerts;
        lift   = 0.0f;
        tint   = kTubeTint;

        pushPatch (gx0, gz1, gx1, gz1, bx1, bz1, bx0, bz1);   // top
        pushPatch (bx0, bz0, bx1, bz0, gx1, gz0, gx0, gz0);   // bottom
        pushPatch (gx0, gz1, bx0, bz1, bx0, bz0, gx0, gz0);   // left
        pushPatch (bx1, bz1, gx1, gz1, gx1, gz0, bx1, bz0);   // right

        // The mask ring: rounded opening -> glass edge, floated past the
        // picture's lift.
        m_maskVerts.clear();
        target = &m_maskVerts;
        lift   = kMaskLiftMm;
        tint   = kMaskTint;

        // Four strips from the glass edges to the square opening.
        pushPatch (gx0, gz1, gx1, gz1, ox1, oz1, ox0, oz1);   // top
        pushPatch (ox0, oz0, ox1, oz0, gx1, gz0, gx0, gz0);   // bottom
        pushPatch (gx0, gz1, ox0, oz1, ox0, oz0, gx0, gz0);   // left
        pushPatch (ox1, oz1, gx1, gz1, gx1, gz0, ox1, oz0);   // right

        // Four corner fans: from each opening corner out to its quarter arc.
        {
            float   corners[4][4] = { { ox0, oz1, 1.0f, -1.0f },     // top-left: center is right+down
                                      { ox1, oz1, -1.0f, -1.0f },    // top-right
                                      { ox1, oz0, -1.0f, 1.0f },     // bottom-right
                                      { ox0, oz0, 1.0f, 1.0f } };    // bottom-left

            for (int k = 0; k < 4; k++)
            {
                float   cx = corners[k][0] + corners[k][2] * radius;   // arc center
                float   cz = corners[k][1] + corners[k][3] * radius;

                for (int i = 0; i < kMaskArcSegments; i++)
                {
                    float  a0  = (float) i / (float) kMaskArcSegments * 1.5707963f;
                    float  a1  = (float) (i + 1) / (float) kMaskArcSegments * 1.5707963f;
                    float  p0x = cx - corners[k][2] * radius * std::cos (a0);
                    float  p0z = cz - corners[k][3] * radius * std::sin (a0);
                    float  p1x = cx - corners[k][2] * radius * std::cos (a1);
                    float  p1z = cz - corners[k][3] * radius * std::sin (a1);

                    pushTri (corners[k][0], corners[k][1], p0x, p0z, p1x, p1z);
                }
            }
        }
    }

    m_glassUv      = displayUv;
    m_glassUvDirty = false;
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

        hr = m_renderer.DrawTriangles (m_drive.OpaqueVerts (m_driveActive[drive]).data(),
                                       m_drive.OpaqueVerts (m_driveActive[drive]).size(),
                                       mvp, false, viewport, true);
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
            hr = m_renderer.DrawTriangles (m_drive.PadlockVerts().data(), m_drive.PadlockVerts().size(),
                                           mvp, false, viewport, true);
            CHRA (hr);
        }

        if (!m_driveLabelVerts[drive].empty())
        {
            hr = m_renderer.DrawTriangles (m_driveLabelVerts[drive].data(), m_driveLabelVerts[drive].size(),
                                           mvp, false, viewport, true);
            CHRA (hr);
        }

        if (!m_driveLampVerts[drive].empty())
        {
            hr = m_renderer.DrawTriangles (m_driveLampVerts[drive].data(), m_driveLampVerts[drive].size(),
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

        hr = m_renderer.DrawTriangles (m_monitorShadowVerts.data(), m_monitorShadowVerts.size(),
                                       mvp, false, viewport, false);
        CHRA (hr);
    }

    for (int drive = 0; drive < comp.driveCount && !m_driveShadowVerts.empty(); drive++)
    {
        SceneCamera::Mul44 (comp.driveWorld[drive], comp.viewProj, mvp);

        hr = m_renderer.DrawTriangles (m_driveShadowVerts.data(), m_driveShadowVerts.size(),
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

        hr = m_renderer.DrawTriangles (m_monitorGlowVerts.data(), m_monitorGlowVerts.size(),
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

        hr = m_renderer.DrawTriangles (m_driveGlowVerts.data(), m_driveGlowVerts.size(),
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

    hr = m_renderer.BeginDepthPass();
    CHRA (hr);

    hr = DrawShadows (m_comp, viewport, true);
    CHRA (hr);

    // Opaque bodies: monitor, then each placed drive.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    hr = m_renderer.DrawTriangles (m_monitor.OpaqueVerts (m_powerLampOn).data(),
                                   m_monitor.OpaqueVerts (m_powerLampOn).size(),
                                   mvp, false, viewport, true);
    CHRA (hr);

    hr = DrawDrives (m_comp, viewport);
    CHRA (hr);

    // The tube (dark, untextured), then the picture band floating on it,
    // sampling the CRT output.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    if (!m_glassVerts.empty())
    {
        hr = m_renderer.DrawTriangles (m_glassVerts.data(), m_glassVerts.size(),
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    if (!m_pictureVerts.empty() && displaySrv != nullptr)
    {
        m_renderer.SetContentSrv (displaySrv);

        hr = m_renderer.DrawTriangles (m_pictureVerts.data(), m_pictureVerts.size(),
                                       mvp, true, viewport, true);

        m_renderer.SetContentSrv (nullptr);
        CHRA (hr);
    }

    if (!m_maskVerts.empty())
    {
        hr = m_renderer.DrawTriangles (m_maskVerts.data(), m_maskVerts.size(),
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    if (!m_monitorLampVerts.empty())
    {
        hr = m_renderer.DrawTriangles (m_monitorLampVerts.data(), m_monitorLampVerts.size(),
                                       mvp, false, viewport, true);
        CHRA (hr);
    }

    hr = DrawLampGlows (m_comp, viewport, true);
    CHRA (hr);

Error:
    return hr;
}
