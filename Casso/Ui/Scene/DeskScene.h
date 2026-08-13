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

    // Per-drive visual state, pushed each UI frame: activity lamp, door
    // openness (0 closed .. 1 open, from the drive's door FSM), and the
    // write-protect padlock. Only an actual change dirties geometry.
    void  SetDriveVisuals  (int drive, bool lampOn, float doorProgress, bool writeProtected);

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

    // Draws only the drive devices under an alternate composition -- the
    // fullscreen overlay strip. Shares the cached door/lamp/padlock geometry
    // and state with the main render; its own depth pass, so it overlays
    // whatever the frame already holds.
    HRESULT  RenderStrip (ID3D11RenderTargetView * dstRtv, const DeskSceneComposition & strip);

    // Debug aid: outlines a client-px rect in the given color (2px bars),
    // drawn over the scene. Gated by the shell's CASSO_SCENE_DEBUG env var.
    void  DrawDebugRect (const RECT & rectPx, int backBufferW, int backBufferH, uint32_t argb);

    // Lamp tint applied when a lamp is unlit (multiplies the base color).
    static constexpr float  kLampOffDim = 0.22f;

    // Lamp glow: concentric bands seated on the lens, drawn only while lit.
    // A lens rendered as flat shaded geometry reads as colored plastic no
    // matter how saturated the tint. Two things sell "lit": a core that
    // blows out toward WHITE (an emitter overruns its own hue at the middle
    // -- a uniformly saturated disc always reads as paint), and light
    // spilling PAST the lens onto the housing around it. Premultiplied
    // source-over (the renderer's only blend) is indistinguishable from
    // additive here because everything it covers is dark housing. Drawn
    // depth-test-free after the scene, so the transparent rim never writes
    // depth over the device behind it.
    //
    // Each band is { radius as a multiple of the lens radius, alpha, how far
    // the color is pushed toward white }, interpolated across the fan.
    struct GlowBand
    {
        float  radiusScale;
        float  alpha;
        float  whiteMix;
    };

    static constexpr GlowBand  kGlowProfile[] =
    {
        { 0.00f, 0.95f, 0.80f },   // blown-out core
        { 0.45f, 0.80f, 0.42f },
        { 1.00f, 0.42f, 0.00f },   // the lens rim: full hue
        { 1.90f, 0.14f, 0.00f },   // spill onto the housing
        { 3.40f, 0.00f, 0.00f },
    };

    static constexpr float  kGlowLiftMm        = 0.35f;
    static constexpr int    kGlowSegments      = 24;
    static constexpr float  kMonitorGlowRgb[3] = { 0.400f, 1.000f, 0.520f };
    static constexpr float  kDriveGlowRgb[3]   = { 1.000f, 0.260f, 0.180f };

    // Contact shadow: what grounds a device. Without one every model reads as
    // pasted onto the backdrop rather than standing on a desk -- the scene has
    // no floor geometry, but the eye infers a surface from the shadow alone,
    // which is the whole trick. Drawn in the ground plane before the bodies,
    // so the device paints over the part of it that is underneath.
    //
    // The margin is where the penumbra lives: darkest at the silhouette, gone
    // by the outer edge. It is ANISOTROPIC because the camera is, and equal
    // margins do not read as equal on screen. The scene's gaze is shallow, so
    // the depth axis (front-to-back) foreshortens hard -- a margin there
    // becomes a thin smear under the front lip -- while the side margins keep
    // their full width and turn into dark vertical bands standing beside the
    // device. Sides therefore stay tight and depth runs long.
    static constexpr float  kShadowMarginSideMm  = 9.0f;
    static constexpr float  kShadowMarginDepthMm = 34.0f;
    static constexpr float  kShadowAlpha         = 0.70f;
    static constexpr float  kShadowMidStop       = 0.30f;   // fraction of the margin
    static constexpr float  kShadowMidAlpha      = 0.30f;
    static constexpr int    kShadowCornerSegs    = 4;

    // The picture sub-mesh: its own curved grid spanning exactly the band
    // (mesh edge == band edge, UVs a clean 0..1 span), floated off the tube
    // sheet by more than the tube's worst chord sag -- the two meshes
    // tessellate the same sphere at different resolutions, and a lift inside
    // the chord error lets coarse tube triangles poke through the picture as
    // dark petals.
    static constexpr int    kPictureGridCols = 24;
    static constexpr int    kPictureGridRows = 18;
    static constexpr float  kPictureLiftMm   = 0.45f;

    // The unlit tube's tint -- near-black with the faint green of period
    // glass.
    static constexpr float  kTubeTint[3]     = { 0.020f, 0.035f, 0.028f };

    // The tube mask: a rounded-corner opening over the glass, the way a real
    // CRT's faceplate rounds off. The opening sits kMaskPadMm OUTSIDE the
    // picture band along the edges, but a tangent corner arc pinches toward
    // the band diagonal: its clearance from the raster corner is only
    // radius - (radius - pad) * sqrt(2). Pad 4 / radius 10 leaves ~1.5mm at
    // the corners -- past the raster and most of its bloom, so text and
    // graphics never clip and the rounding reads only where bright content
    // glows into the corners. kMaskPadMm must stay below CurvedDisplayMath::
    // kBandInsetMm: a pad past the band's own glass inset clamps the opening
    // to the glass edge, and the clamped tangent arc swings into the raster.
    // The mask rides just a hair above the picture: the two never overlap in
    // plan (the opening stays kMaskPadMm outside the band), so the lift only
    // needs to clear the tube -- and any real gap above the picture casts
    // the mask's silhouette inward over it along grazing corner rays,
    // eating the picture's corners as small dark divets.
    static constexpr float  kMaskPadMm       = 4.0f;
    static constexpr float  kMaskRadiusMm    = 10.0f;
    static constexpr float  kMaskLiftMm      = 0.48f;
    static constexpr int    kMaskArcSegments = 8;
    static constexpr float  kMaskTint[3]     = { 0.012f, 0.020f, 0.016f };

    // Drive door swing: fully open lifts the door bar this far off the
    // faceplate (a touch past 60 degrees, like the real drive at rest), and
    // progress deltas below the epsilon skip the vertex rebuild.
    static constexpr float  kDoorOpenRad     = 1.15f;
    static constexpr float  kDoorProgressEps = 1.0f / 256.0f;

private:
    void     RebuildGlassUvs  (const CrtUvRect & displayUv, int displayW, int displayH);
    void     RebuildLampVerts ();
    HRESULT  DrawDrives       (const DeskSceneComposition & comp, const D3D11_VIEWPORT & viewport);
    HRESULT  DrawLampGlows    (const DeskSceneComposition & comp,
                               const D3D11_VIEWPORT       & viewport,
                               bool                         includeMonitor);
    HRESULT  DrawShadows      (const DeskSceneComposition & comp,
                               const D3D11_VIEWPORT       & viewport,
                               bool                         includeMonitor);

    static void  TintInto (const std::vector<Dxui3DRenderer::Vertex> & base,
                           float                                       factor,
                           std::vector<Dxui3DRenderer::Vertex>       & out);

    // Builds the glow disc for a model's first lamp anchor: a two-ring fan
    // (hot center, mid ring, transparent rim) lying in the lens plane, lifted
    // toward the viewer. Empty when the model carries no lamp.
    static void  BuildLampGlow (const DeskSceneModel                & model,
                                const float                           rgb[3],
                                std::vector<Dxui3DRenderer::Vertex> & out);

    // Builds the contact shadow for a model: a filled core over its ground
    // footprint plus a penumbra skirt fading out over kShadowMarginMm, with
    // rounded corners, all in the model's ground plane.
    static void  BuildContactShadow (const DeskSceneModel                & model,
                                     std::vector<Dxui3DRenderer::Vertex> & out);

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

    // Glow discs, built once at load: model-space geometry, so the two
    // drives share one copy and only their world matrices differ.
    std::vector<Dxui3DRenderer::Vertex>   m_monitorGlowVerts;
    std::vector<Dxui3DRenderer::Vertex>   m_driveGlowVerts;

    // Contact shadows, likewise built once in model space.
    std::vector<Dxui3DRenderer::Vertex>   m_monitorShadowVerts;
    std::vector<Dxui3DRenderer::Vertex>   m_driveShadowVerts;
    bool                                  m_powerLampOn     = false;
    bool                                  m_driveActive[2]  = {};
    bool                                  m_lampsDirty      = true;

    // Per-drive badge text ("DRIVE 1" / "DRIVE 2"), stamped onto the shared
    // model's badge plaque at load -- the number differs per drive, so the
    // text cannot live in the model itself.
    std::vector<Dxui3DRenderer::Vertex>   m_driveLabelVerts[2];

    // Door assemblies, rotated copies of the model's cached door verts;
    // progress -1 forces the first build.
    std::vector<Dxui3DRenderer::Vertex>   m_driveDoorVerts[2];
    float                                 m_doorProgress[2] = { -1.0f, -1.0f };
    bool                                  m_driveWp[2]      = {};
};
