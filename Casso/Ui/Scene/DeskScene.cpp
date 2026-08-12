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

HRESULT DeskScene::LoadModels (const std::string & monitorObj, const std::string & monitorMtl,
                               const std::string & driveObj,   const std::string & driveMtl)
{
    HRESULT   hr = S_OK;



    hr = m_monitor.Load (DeskDeviceKind::Monitor2c, monitorObj, monitorMtl);
    CHRA (hr);

    hr = m_drive.Load (DeskDeviceKind::DiskII, driveObj, driveMtl);
    CHRA (hr);

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



    // The tube: the model's glass sheet, unlit near-black. The picture is a
    // SEPARATE curved grid spanning exactly the aspect-fit band (same band as
    // the input math), so its mesh boundary IS the picture boundary -- no
    // interpolation ever crosses the picture's texture edge, which is what
    // used to either clamp-smear the outermost columns into the tube margins
    // or (with a black guard) let the CRT chain's neighbor-sampling passes
    // dim them.
    CurvedDisplayMath::ComputePictureBand (surface, displayW, displayH,
                                           bandU0, bandV0, bandU1, bandV1);

    m_glassVerts = m_monitor.GlassVerts();

    for (Dxui3DRenderer::Vertex & v : m_glassVerts)
    {
        v.r = kTubeTint[0];
        v.g = kTubeTint[1];
        v.b = kTubeTint[2];
    }

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

    // The tube mask: a dark ring from the glass edges to a rounded-rect
    // opening kMaskPadMm outside the band -- four trapezoid strips to the
    // square opening plus a smooth arc fan filling each corner between the
    // opening corner and its quarter circle. Every point rides the sag
    // surface, floated past the picture's own lift so depth never ties.
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

        auto surfacePoint = [&] (float x, float z, Dxui3DRenderer::Vertex & v)
        {
            float   u     = (x - surface.x0) / (surface.x1 - surface.x0);
            float   w     = (surface.z1 - z) / (surface.z1 - surface.z0);
            float   pt[3] = {};

            CurvedDisplayMath::ModelPointFromUv (surface, u, w, pt);

            v = {};
            v.x = pt[0];
            v.y = pt[1] - kMaskLiftMm;
            v.z = pt[2];
            v.r = kMaskTint[0];
            v.g = kMaskTint[1];
            v.b = kMaskTint[2];
            v.a = 1.0f;
        };

        auto pushTri = [&] (float x0, float z0, float x1, float z1, float x2, float z2)
        {
            Dxui3DRenderer::Vertex   v = {};

            surfacePoint (x0, z0, v);  m_maskVerts.push_back (v);
            surfacePoint (x1, z1, v);  m_maskVerts.push_back (v);
            surfacePoint (x2, z2, v);  m_maskVerts.push_back (v);
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

        m_maskVerts.clear();

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

    // Opaque bodies: monitor, then each placed drive.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    hr = m_renderer.DrawTriangles (m_monitor.OpaqueVerts().data(), m_monitor.OpaqueVerts().size(),
                                   mvp, false, viewport, true);
    CHRA (hr);

    for (int drive = 0; drive < m_comp.driveCount; drive++)
    {
        SceneCamera::Mul44 (m_comp.driveWorld[drive], m_comp.viewProj, mvp);

        hr = m_renderer.DrawTriangles (m_drive.OpaqueVerts().data(), m_drive.OpaqueVerts().size(),
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

        if (!m_driveLampVerts[drive].empty())
        {
            hr = m_renderer.DrawTriangles (m_driveLampVerts[drive].data(), m_driveLampVerts[drive].size(),
                                           mvp, false, viewport, true);
            CHRA (hr);
        }
    }

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

Error:
    return hr;
}
