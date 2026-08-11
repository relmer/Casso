#pragma once

#include "Pch.h"

#include "CrtPostProcess.h"
#include "Render/Dxui3DRenderer.h"
#include "Ui/Scene/DeskSceneLayout.h"
#include "Ui/Scene/DeskSceneModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskScene
//
//  D3D submission for the desk scene: cached geometry over Dxui3DRenderer,
//  drawn from the main window's before-present hook on the shared device. The
//  thin, untestable edge -- every decision it draws (layout, sub-meshes, UVs,
//  hit results) is computed by the testable classes it consumes.
//
//  Geometry policy (research R9): model vertex data is cached at load; the
//  small mutable copies (glass with the display's UV subrect applied, lamps
//  with their on/off tint) are rebuilt only when their inputs change. Per
//  frame the scene only multiplies matrices and issues draws. The host's
//  theme backdrop stays visible around the devices -- the scene clears
//  nothing.
//
//  Draw order per frame: BeginDepthPass, opaque devices (depth), textured
//  glass (depth), lamps (depth). The renderer's full-state-per-draw contract
//  means no interaction with the Dxui painter that runs after the hook.
//
////////////////////////////////////////////////////////////////////////////////

class DeskScene
{
public:
    HRESULT  Initialize (ID3D11Device * device, ID3D11DeviceContext * context);
    void     Shutdown   ();

    bool     IsInitialized () const { return m_renderer.IsInitialized(); }
    bool     HasModels     () const { return m_modelsLoaded; }

    // Parses both device models from OBJ/MTL text (embedded resources,
    // decoded by the shell).
    HRESULT  LoadModels (const std::string & monitorObj, const std::string & monitorMtl,
                         const std::string & driveObj,   const std::string & driveMtl);

    const DeskSceneModel &  MonitorModel () const { return m_monitor; }
    const DeskSceneModel &  DriveModel   () const { return m_drive; }

    // The measured metrics DeskSceneLayout composes with.
    DeskSceneMetrics  Metrics () const;

    // The current composition (from DeskSceneLayout::Compute); cached so
    // Render and the shell's hit tests share one frame of truth.
    void                          SetComposition (const DeskSceneComposition & comp) { m_comp = comp; }
    const DeskSceneComposition &  Composition    () const { return m_comp; }

    // Lamp states; a change dirties only the small lamp vertex copies.
    void  SetPowerLampOn   (bool on);
    void  SetDriveActivity (int drive, bool active);

    // Draws the scene into `dstRtv` (bound here -- the CRT offscreen pass
    // that runs just before leaves ITS target bound, so relying on ambient
    // state would draw the monitor into the display texture). `displaySrv`
    // is the CRT chain's offscreen output; `displayUv` the subrect of it the
    // picture occupies (ComputeUvRectForFit); `displayW/H` the emulated grid,
    // for the picture's aspect-fit band on the glass.
    HRESULT  Render (ID3D11RenderTargetView   * dstRtv,
                     ID3D11ShaderResourceView * displaySrv,
                     const CrtUvRect          & displayUv,
                     int                        displayW,
                     int                        displayH);

    // Debug aid: outlines a client-px rect in the given color (2px bars),
    // drawn over the scene. Gated by the shell's CASSO_SCENE_DEBUG env var.
    void  DrawDebugRect (const RECT & rectPx, int backBufferW, int backBufferH, uint32_t argb);

    // Lamp tint applied when a lamp is unlit (multiplies the base color).
    static constexpr float  kLampOffDim = 0.22f;

private:
    void  RebuildGlassUvs   (const CrtUvRect & displayUv, int displayW, int displayH);
    void  RebuildLampVerts  ();

    static void  TintInto (const std::vector<Dxui3DRenderer::Vertex> & base,
                           float                                       factor,
                           std::vector<Dxui3DRenderer::Vertex>       & out);

    Dxui3DRenderer          m_renderer;
    ID3D11DeviceContext   * m_context      = nullptr;   // non-owning
    DeskSceneModel          m_monitor;
    DeskSceneModel          m_drive;
    DeskSceneComposition    m_comp;
    bool                    m_modelsLoaded = false;

    std::vector<Dxui3DRenderer::Vertex>   m_glassVerts;         // UVs remapped to displayUv
    CrtUvRect                             m_glassUv;
    bool                                  m_glassUvDirty    = true;

    std::vector<Dxui3DRenderer::Vertex>   m_monitorLampVerts;
    std::vector<Dxui3DRenderer::Vertex>   m_driveLampVerts[2];
    bool                                  m_powerLampOn     = false;
    bool                                  m_driveActive[2]  = {};
    bool                                  m_lampsDirty      = true;
};
