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

    // The picture sub-mesh: its own curved grid spanning exactly the band
    // (mesh edge == band edge, UVs a clean 0..1 span), floated a hair off
    // the tube sheet so depth never ties.
    static constexpr int    kPictureGridCols = 24;
    static constexpr int    kPictureGridRows = 18;
    static constexpr float  kPictureLiftMm   = 0.15f;

    // The unlit tube's tint -- near-black with the faint green of period
    // glass.
    static constexpr float  kTubeTint[3]     = { 0.020f, 0.035f, 0.028f };

    // The tube mask: a rounded-corner opening over the glass, the way a real
    // CRT's faceplate rounds off. The opening sits kMaskPadMm OUTSIDE the
    // picture band along the edges, but a tangent corner arc pinches toward
    // the band diagonal: its clearance from the raster corner is only
    // radius - (radius - pad) * sqrt(2). Pad 5 / radius 12 leaves ~2.1mm at
    // the corners -- past the raster and most of its bloom, so text and
    // graphics never clip and the rounding reads only where bright content
    // glows into the corners.
    static constexpr float  kMaskPadMm       = 5.0f;
    static constexpr float  kMaskRadiusMm    = 12.0f;
    static constexpr float  kMaskLiftMm      = 0.25f;
    static constexpr int    kMaskArcSegments = 8;
    static constexpr float  kMaskTint[3]     = { 0.012f, 0.020f, 0.016f };

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

    std::vector<Dxui3DRenderer::Vertex>   m_glassVerts;         // the tube: dark, untextured
    std::vector<Dxui3DRenderer::Vertex>   m_pictureVerts;       // band-exact curved grid, textured
    std::vector<Dxui3DRenderer::Vertex>   m_maskVerts;          // rounded-corner tube mask ring
    CrtUvRect                             m_glassUv;
    bool                                  m_glassUvDirty    = true;

    std::vector<Dxui3DRenderer::Vertex>   m_monitorLampVerts;
    std::vector<Dxui3DRenderer::Vertex>   m_driveLampVerts[2];
    bool                                  m_powerLampOn     = false;
    bool                                  m_driveActive[2]  = {};
    bool                                  m_lampsDirty      = true;
};
