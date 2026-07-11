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
        float  x, y, z;      // world-space position
        float  u, v;         // content-texture coordinates (ignored when tinted-only)
        float  r, g, b, a;   // tint * baked light, multiplied with the texture
    };

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

    // Prepare (and clear) a depth buffer matching the currently bound render
    // target, for draws submitted with `depthTest == true`. Call once at the
    // start of a scene frame, AFTER the caller's render target is bound --
    // the depth texture is sized by querying it. Loaded meshes need real
    // depth testing (their triangles arrive in arbitrary order); the
    // hand-built painter's-algorithm batches keep passing false.
    HRESULT  BeginDepthPass ();

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
                            bool             depthTest = false);

private:
    HRESULT  CreateShaders      ();
    HRESULT  CreatePipelineState ();
    HRESULT  EnsureVertexBuffer (size_t requiredVerts);

    ID3D11Device        *             m_device  = nullptr;   // non-owning
    ID3D11DeviceContext *             m_context = nullptr;   // non-owning

    ComPtr<ID3D11VertexShader>        m_vs;
    ComPtr<ID3D11PixelShader>         m_ps;
    ComPtr<ID3D11InputLayout>         m_layout;
    ComPtr<ID3D11Buffer>              m_vertexBuffer;
    ComPtr<ID3D11Buffer>              m_mvpBuffer;
    ComPtr<ID3D11BlendState>          m_blendState;
    ComPtr<ID3D11RasterizerState>     m_rasterState;
    ComPtr<ID3D11DepthStencilState>   m_depthState;       // depth off (painter's algorithm)
    ComPtr<ID3D11DepthStencilState>   m_depthStateTest;   // LESS test + write (meshes)
    ComPtr<ID3D11SamplerState>        m_sampler;

    ComPtr<ID3D11Texture2D>           m_depthTex;
    ComPtr<ID3D11DepthStencilView>    m_depthDsv;
    int                               m_depthWidth  = 0;
    int                               m_depthHeight = 0;

    ComPtr<ID3D11Texture2D>           m_contentTex;
    ComPtr<ID3D11ShaderResourceView>  m_contentSrv;
    ComPtr<ID3D11Texture2D>           m_whiteTex;
    ComPtr<ID3D11ShaderResourceView>  m_whiteSrv;

    size_t                            m_vertexBufferCapacity = 0;
    int                               m_contentWidth         = 0;
    int                               m_contentHeight        = 0;
};
