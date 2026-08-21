#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Dxui3DRenderer
//
//  A deliberately scoped 3D path for Dxui's D3D11 pipeline: one MVP constant
//  buffer, one textured+tinted shader pair, one dynamic vertex buffer, and a
//  single dynamic content texture (plus a built-in 1x1 white for untextured
//  geometry). Consumers submit world-space triangles with UVs and a per-vertex
//  tint (which doubles as baked lighting); the renderer transforms them by the
//  caller's matrix and composites premultiplied source-over into whatever
//  render target is currently bound -- the same compositing contract as
//  DxuiPainter, so a scene drawn from a window's before-present hook layers
//  correctly under the panel-tree paint.
//
//  This is intentionally NOT an engine: no depth buffer (submit back-to-front,
//  painter's algorithm), no scene graph, no materials. It exists to render
//  small procedural set-pieces -- first the printer panel's ImageWriter +
//  curled-fanfold-paper scene (FR-032, research R-017) -- and is the primitive
//  the drive widgets can adopt when they move to true 3D.
//
//  All state is set on every Draw, mirroring DxuiPainter::End, so interleaving
//  with the painter / text renderer requires no state save/restore etiquette.
//
////////////////////////////////////////////////////////////////////////////////

class Dxui3DRenderer
{
public:
    struct Vertex
    {
        float  x, y, z;         // position, in whatever space the mvp expects
        float  u, v;            // content-texture coordinates (ignored when tinted-only)
        float  r, g, b, a;      // material tint, multiplied with the texture
        float  nx, ny, nz;      // normal, same space as the position
        float  er, eg, eb;      // emissive, ADDED after shading

        // Surface finish, 0 smooth .. 1 fully pebbled. Perturbs the normal
        // with position-hashed noise before shading, so a molded-in texture
        // catches light per bump instead of being painted on.
        //
        // Evaluated in the vertex's OWN space, which for the desk scene is
        // model space, so the grain is welded to the part: a drive the user
        // drags carries its finish with it rather than swimming through it.
        float  pebble = 0.0f;
    };

    // Per-pixel lighting for subsequent draws.
    //
    // A ZERO NORMAL MEANS UNLIT: the shader passes tint*texture straight
    // through. That is what lets glass, lamp lenses, and every mesh built
    // before this existed keep working untouched -- a value-initialized
    // Vertex is unlit by construction -- while still needing only the one
    // shader pair that serves textured and tinted geometry alike.
    //
    // Emissive is separate from tint because it must NOT be modulated by
    // room light: the drive lamp's glow is baked with occlusion at load
    // (a pixel shader cannot trace those rays cheaply), and it lands inside
    // a dark notch. Folded into the tint it would be dimmed by the very
    // shading it is supposed to overpower.
    //
    // Positions are whatever space the vertices are in -- the desk scene
    // keeps each device in its own model coordinates and transforms the
    // lights to match, so this is set per device before its draw.
    struct Lighting
    {
        float  light0[3]    = { 0.0f, 0.0f, 0.0f };
        float  light1[3]    = { 0.0f, 0.0f, 0.0f };
        float  eye[3]       = { 0.0f, 0.0f, 0.0f };
        float  refDist      = 1.0f;    // distance lit at full strength
        float  span         = 0.84f;
        float  gain         = 1.0f;    // drives the rolloff's knee
        float  specStrength = 0.0f;    // 0 disables the highlight entirely
        float  specPower    = 48.0f;

        // HEMISPHERE ambient, not one flat number. A room bounces light off
        // its ceiling and off the desk, and those are not the same color or
        // strength, so which way a surface faces decides what it picks up
        // even where no lamp reaches it. A single floor value gave every
        // unlit face the identical tone, which is why a downward-facing
        // flank of molded relief had nowhere to sit but flush with the
        // surface beside it.
        float  ambientUp[3]   = { 0.16f, 0.16f, 0.16f };
        float  ambientDown[3] = { 0.16f, 0.16f, 0.16f };

        // The device's own lamp as a REAL light rather than baked spill.
        // Radiates into the hemisphere its lens faces, so the housing around
        // a recessed lamp stays dark because those faces point away from it,
        // not because anything traced a ray. Zero color disables it.
        float  lampPos[3]   = { 0.0f, 0.0f, 0.0f };
        float  lampDir[3]   = { 0.0f, -1.0f, 0.0f };   // the lens's facing
        float  lampColor[3] = { 0.0f, 0.0f, 0.0f };
        float  lampRefDist  = 22.0f;
        float  lampRange    = 130.0f;

        // Shadow lookup, one matrix per room light. Each takes THIS draw's
        // vertex positions straight to that light's clip space (row-vector:
        // clip = v * M), so geometry submitted in its own model space needs
        // no world position in the shader -- the caller folds the device's
        // placement into the matrix, which is also what lets one shared map
        // hold every device and give inter-object shadows.
        //
        // Zero texel disables shadowing outright, which is how every scene
        // that never calls BeginShadowPass keeps working untouched.
        float  shadowMatrix[2][16] = {};
        float  shadowTexel    = 0.0f;      // 1 / map edge; 0 disables
        float  shadowBias     = 0.0f;      // in light-clip depth units
        float  shadowStrength = 1.0f;      // 0 lets a shadowed face keep its light

        // The device lamp's own occlusion. It needs one because facing the
        // lens is not the same as seeing it: the notch floor in front of the
        // power button points straight at the lamp and takes its light, while
        // the button sits squarely in the way. Which map to read is a slot
        // rather than a fixed register because the lamp is a property of the
        // DEVICE -- the monitor's and the drive's are different lamps in
        // different model spaces, so each carries its own.
        float  lampShadow[16]  = {};
        float  lampShadowTexel = 0.0f;   // 0 leaves the lamp unshadowed
        float  lampShadowBias  = 0.0f;
        int    lampShadowSlot  = -1;

        // The pebble finish's grain: how far apart the bumps sit, in the
        // vertices' own units (mm here), and how hard each tilts the normal.
        // Only vertices carrying a nonzero `pebble` are affected at all.
        // The COARSEST octave's cell size; two finer ones ride on it. Smaller
        // again, because at 0.85 the cells were still resolvable as cells --
        // the grain read as a pattern with a size rather than as a surface.
        float  pebblePitchMm = 0.60f;

        // How hard the grain tilts the normal.
        float  pebbleAmount  = 0.30f;

        // How dark a bump's own underside goes. This is the term that makes
        // relief read AS relief: cavity darkening says "this spot is low",
        // but only a horizon test says "this spot is low because something
        // beside it is in the way", and that shadow is the cue the eye
        // actually uses. Zero leaves the finish shadowless.
        float  pebbleShadow  = 0.55f;

        // How much the grain DARKENS its own pits. This is what reads as
        // depth: a matte near-black surface returns almost the same value
        // however its normal is tilted, so bending normals alone left the
        // finish flat no matter how hard it was pushed. Less of the room
        // reaches the bottom of a dimple than its rim, and that difference is
        // the whole cue. Zero leaves the finish shading-only.
        float  pebbleCavity  = 0.55f;
    };

    void  SetLighting (const Lighting & lighting)  { m_lighting = lighting; }

    Dxui3DRenderer  () = default;
    ~Dxui3DRenderer ();

    // Non-owning device/context, same lifetime contract as DxuiPainter.
    HRESULT  Initialize (ID3D11Device * device, ID3D11DeviceContext * context);
    void     Shutdown   ();

    bool     IsInitialized () const { return m_device != nullptr; }

    // Upload premultiplied-BGRA pixels into the content texture (recreated on
    // size change, Map/WRITE_DISCARD otherwise). Triangles drawn with
    // `textured == true` sample it; call again only when the content changes.
    HRESULT  UpdateContentTexture (const uint32_t * bgra, int width, int height);

    // Adopt an externally-owned SRV as the content texture instead of the
    // CPU-upload path -- the desk scene hands over the CRT chain's finished
    // output so a GPU-resident image never round-trips through system memory.
    // While set (non-null, non-owning, caller-guaranteed lifetime across the
    // frame), textured draws sample it in preference to the uploaded texture;
    // pass null to fall back.
    void     SetContentSrv        (ID3D11ShaderResourceView * srv) { m_externalSrv = srv; }

    // The CPU-uploaded texture's view, for callers that want to hand it back
    // as an explicit content SRV rather than relying on the fallback.
    ID3D11ShaderResourceView *  ContentSrv () const { return m_contentSrv.Get(); }

    // Prepare (and clear) a depth buffer matching the currently bound render
    // target, for draws submitted with `depthTest == true`. Call once at the
    // start of a scene frame, AFTER the caller's render target is bound --
    // the depth texture is sized by querying it. Loaded meshes need real
    // depth testing (their triangles arrive in arbitrary order); the
    // hand-built painter's-algorithm batches keep passing false.
    HRESULT  BeginDepthPass ();

    // Multisampled scene pass. Between these two calls the scene draws into
    // an offscreen MSAA target instead of the caller's render target; End
    // resolves it and composites the result back.
    //
    // The detour exists because a flip-model swap chain CANNOT be
    // multisampled -- DXGI requires SampleDesc.Count == 1 on it -- so the
    // only way to antialias is to render somewhere else and resolve.
    //
    // The offscreen target starts fully transparent and the scene layers into
    // it exactly as it would have layered onto the destination: this
    // renderer blends premultiplied source-over, and premultiplied
    // compositing is associative, so compositing the finished layer over the
    // destination gives the same pixels as drawing straight onto it. No copy
    // of the destination is needed going in.
    //
    // Call Begin AFTER binding the destination -- it sizes the offscreen
    // target by querying it, the same contract BeginDepthPass keeps. When the
    // device will not multisample at this count Begin quietly does nothing
    // and the scene draws straight to the destination; End then has nothing
    // to composite and is equally quiet, so callers pair them unconditionally.
    HRESULT  BeginMultisampledScene ();
    HRESULT  EndMultisampledScene   ();

    // Depth-only pass from one room light. Between Begin and End every draw
    // writes ONLY depth, into that light's shadow map, using whatever mvp the
    // caller passes -- which has to land the geometry in the same clip space
    // the matching Lighting::shadowMatrix does, or the lookup misses.
    //
    // Submit every caster in the scene between one pair, in whatever spaces
    // they live in: the map is shared, so a device shadows its neighbors as
    // readily as itself. Call before the color pass; the maps persist until
    // the next Begin on that slot, so a still scene pays nothing to redraw.
    // `texels` sizes THIS slot's map. The room lights cover the whole desk and
    // want a generous one; a device lamp reaches only inside its own housing,
    // so it resolves the same detail from a fraction of the memory.
    HRESULT  BeginShadowPass (int slot, UINT texels);
    void     EndShadowPass   ();

    static constexpr int   kShadowMaps        = 4;   // 0,1 room lights; 2,3 device lamps
    static constexpr int   kShadowLights      = 2;   // ...of which these are directional
    static constexpr UINT  kDefaultShadowSize = 2048;

    // Lay a premultiplied texture over the whole bound target as one quad --
    // what EndMultisampledScene does with its resolve, exposed because the
    // desk scene composites a cached plate the same way.
    HRESULT  CompositeFullTarget (ID3D11ShaderResourceView * srv, int width, int height);

    // Crop every subsequent draw to `rect` (target pixels); null clears it.
    // For keeping a scene inside a sub-rect that something else may cover --
    // shrinking the viewport instead would re-map the projection and squash
    // the scene rather than crop it.
    void     SetScissor (const RECT * rect);

    // Antialiasing for the scene pass, in SAMPLES: 1 disables the offscreen
    // detour entirely, 2 and 4 arm it. What it costs is very much a property
    // of the machine -- on an integrated GPU the whole scene is rasterized
    // into a target this many times over, out of the same memory bandwidth
    // the CPU is using -- so the user owns the number, not this file.
    // Unsupported counts fall back to drawing unantialiased, exactly as a
    // device that will not multisample at all does.
    void     SetSceneSampleCount (UINT samples);
    UINT     SceneSampleCount    () const { return m_sampleCount; }

    static constexpr UINT  kDefaultSceneSampleCount = 4;

    // Transform `verts` by row-major `mvp` (row-vector convention: clip = v * M)
    // and draw as a triangle list into the currently bound render target,
    // restricted to `viewportPx`. `textured` selects the content texture;
    // untextured geometry samples opaque white, so the tint IS the color.
    // `depthTest` binds the BeginDepthPass buffer (test + write, LESS).
    HRESULT  DrawTriangles (const Vertex   * verts,
                            size_t           vertexCount,
                            const float      mvp[16],
                            bool             textured,
                            const D3D11_VIEWPORT & viewportPx,
                            bool             depthTest  = false,
                            bool             depthWrite = true);

    // A vertex array parked on the GPU between frames -- one per array, held
    // by whoever owns the geometry. See DrawStatic.
    struct StaticMesh
    {
        ComPtr<ID3D11Buffer>  buffer;
        size_t                vertexCount = 0;
        uint32_t              revision    = 0;
    };

    // DrawTriangles for geometry that does not change every frame, which in
    // this scene is nearly all of it. `revision` is the caller's own counter,
    // bumped whenever it rebuilds the array; the vertices are re-uploaded only
    // when it moves, so a still frame costs no copying at all. Passing a
    // revision that never changes for an array that does is the one way to get
    // this wrong -- it draws stale geometry, silently.
    HRESULT  DrawStatic (StaticMesh     & mesh,
                         const Vertex   * verts,
                         size_t           vertexCount,
                         uint32_t         revision,
                         const float      mvp[16],
                         bool             textured,
                         const D3D11_VIEWPORT & viewportPx,
                         bool             depthTest  = false,
                         bool             depthWrite = true);

private:
    // The tail both draw paths share, once the vertices are wherever they
    // live: transform, full state set, Draw.
    HRESULT  IssueDraw (ID3D11Buffer             * vertexBuffer,
                        size_t                     vertexCount,
                        const float                mvp[16],
                        ID3D11ShaderResourceView * srv,
                        const D3D11_VIEWPORT     & viewportPx,
                        bool                       useDepth,
                        ID3D11DepthStencilState  * depthState);

    HRESULT  CreateShaders      ();
    HRESULT  CreatePipelineState ();
    HRESULT  EnsureVertexBuffer (size_t requiredVerts);
    HRESULT  EnsureShadowMap    (int slot, UINT texels);
    void     IssueShadowDraw    (ID3D11Buffer * vertexBuffer, size_t vertexCount);

    // One light's depth of the whole scene. Depth-only: no color target is
    // bound while it is being filled, and the SRV is what the color pass
    // reads back.
    struct ShadowMap
    {
        ComPtr<ID3D11Texture2D>           tex;
        ComPtr<ID3D11DepthStencilView>    dsv;
        ComPtr<ID3D11ShaderResourceView>  srv;
        UINT                              size = 0;
    };

    ShadowMap                         m_shadow[kShadowMaps];
    int                               m_shadowSlot = -1;   // >= 0 inside a pass
    ComPtr<ID3D11DepthStencilState>   m_shadowDepthState;
    ComPtr<ID3D11RasterizerState>     m_shadowRasterState;
    ComPtr<ID3D11SamplerState>        m_shadowSampler;
    ComPtr<ID3D11RenderTargetView>    m_shadowSavedRtv;
    ComPtr<ID3D11DepthStencilView>    m_shadowSavedDsv;

    ID3D11Device        *             m_device  = nullptr;   // non-owning
    ID3D11DeviceContext *             m_context = nullptr;   // non-owning

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11PixelShader>         m_ps;
    ComPtr<ID3D11InputLayout>         m_layout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11Buffer>              m_mvpBuffer;
    ComPtr<ID3D11Buffer>              m_lightBuffer;
    Lighting                          m_lighting;
    ComPtr<ID3D11BlendState>          m_blendState;
    ComPtr<ID3D11RasterizerState>     m_rasterState;
    ComPtr<ID3D11RasterizerState>     m_rasterStateScissor;
    RECT                              m_scissor    = {};
    bool                              m_hasScissor = false;
    ComPtr<ID3D11DepthStencilState>   m_depthState;       // depth off (painter's algorithm)
    ComPtr<ID3D11DepthStencilState>   m_depthStateTest;   // LESS test + write (meshes)
    ComPtr<ID3D11DepthStencilState>   m_depthStateReadOnly;   // LESS test, no write (light)
    ComPtr<ID3D11SamplerState>        m_sampler;

    // The multisampled scene detour: an MSAA color target the scene draws
    // into, and the single-sample texture it resolves to before being
    // composited back over the caller's target.
    ComPtr<ID3D11Texture2D>           m_msaaTex;
    ComPtr<ID3D11RenderTargetView>    m_msaaRtv;
    ComPtr<ID3D11Texture2D>           m_resolveTex;
    ComPtr<ID3D11ShaderResourceView>  m_resolveSrv;
    ComPtr<ID3D11RenderTargetView>    m_savedRtv;        // the caller's, while detoured
    int                               m_msaaWidth   = 0;
    int                               m_msaaHeight  = 0;
    bool                              m_inMsaaScene = false;
    UINT                              m_sampleCount = kDefaultSceneSampleCount;

    ComPtr<ID3D11Texture2D>         m_depthTex;
    ComPtr<ID3D11DepthStencilView>  m_depthDsv;
    UINT                            m_depthSamples = 0;
    int                             m_depthWidth   = 0;
    int                             m_depthHeight  = 0;

    ComPtr<ID3D11Texture2D>           m_contentTex;
    ComPtr<ID3D11ShaderResourceView>  m_contentSrv;
    ID3D11ShaderResourceView *        m_externalSrv = nullptr;   // non-owning
    ComPtr<ID3D11Texture2D>           m_whiteTex;
    ComPtr<ID3D11ShaderResourceView>  m_whiteSrv;

    size_t                            m_vertexBufferCapacity = 0;
    int                               m_contentWidth         = 0;
    int                               m_contentHeight        = 0;
};
