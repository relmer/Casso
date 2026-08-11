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

void DeskScene::RebuildGlassUvs (const CrtUvRect & displayUv)
{
    m_glassVerts = m_monitor.GlassVerts();

    for (Dxui3DRenderer::Vertex & v : m_glassVerts)
    {
        v.u = displayUv.u0 + v.u * (displayUv.u1 - displayUv.u0);
        v.v = displayUv.v0 + v.v * (displayUv.v1 - displayUv.v0);
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
//  DeskScene::Render
//
//  Matrix multiplies and draw submission only -- geometry was cached at load
//  or on the change that dirtied it. Depth-tested throughout: the devices are
//  solid bodies, and the glass sits proud of the monitor's recess.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskScene::Render (ID3D11RenderTargetView   * dstRtv,
                           ID3D11ShaderResourceView * displaySrv,
                           const CrtUvRect          & displayUv)
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
        RebuildGlassUvs (displayUv);
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

        if (!m_driveLampVerts[drive].empty())
        {
            hr = m_renderer.DrawTriangles (m_driveLampVerts[drive].data(), m_driveLampVerts[drive].size(),
                                           mvp, false, viewport, true);
            CHRA (hr);
        }
    }

    // The glass, sampling the CRT output through the remapped UVs.
    SceneCamera::Mul44 (m_comp.monitorWorld, m_comp.viewProj, mvp);

    if (!m_glassVerts.empty() && displaySrv != nullptr)
    {
        m_renderer.SetContentSrv (displaySrv);

        hr = m_renderer.DrawTriangles (m_glassVerts.data(), m_glassVerts.size(),
                                       mvp, true, viewport, true);

        m_renderer.SetContentSrv (nullptr);
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
