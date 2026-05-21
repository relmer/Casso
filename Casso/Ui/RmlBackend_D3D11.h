#pragma once

#include "Pch.h"




// The Casso PCH publishes this alias at global scope; redeclare it
// here so consumers compiled against a leaner PCH (the UnitTest
// project) still see it. C++ permits redeclaration of an identical
// alias template at the same scope.
#ifndef CASSO_COMPTR_ALIAS_DECLARED
#define CASSO_COMPTR_ALIAS_DECLARED
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
#endif





////////////////////////////////////////////////////////////////////////////////
//
//  RmlBackend_D3D11
//
//  Concrete Rml::RenderInterface that targets the SAME ID3D11Device
//  owned by D3DRenderer. Never creates a second device, swap chain,
//  or DXGI factory.
//
//  RmlUi 6.x funnels every draw through CompileGeometry: there is no
//  immediate-mode RenderGeometry fast path on the interface. We mirror
//  that — CompileGeometry allocates one immutable VB + one immutable
//  IB on the heap (D3D11_USAGE_IMMUTABLE) and ReleaseGeometry frees
//  them. RenderGeometry binds those buffers, uploads the per-draw
//  translation into the constant buffer, and issues DrawIndexed.
//
//  Coordinate system: top-left origin, pixels, matching RmlUi's
//  defaults. The projection matrix is rebuilt on every Resize().
//
//  Blend state: premultiplied-alpha, source-over; cached once at
//  init and rebound on every BeginFrame.
//
//  Scissor: rasterizer state has ScissorEnable=TRUE; EnableScissorRegion
//  toggles between "scissor = current rect" and "scissor = full
//  viewport". SetScissorRegion applies via RSSetScissorRects.
//
//  Textures: identified by `TextureHandle = uintptr_t`. The backend
//  uses the handle as a raw pointer to an ID3D11ShaderResourceView,
//  keeps a ComPtr alive in m_textures keyed by that same pointer, and
//  bumps the refcount before handing the handle back. ReleaseTexture
//  drops the ComPtr.
//
//  Read-through hook integration: D3DRenderer calls the bound
//  per-frame hook between the emulator blit and Present. The hook,
//  installed by UiShell, calls Rml::Context::Update + Render, which
//  funnels through the methods below.
//
////////////////////////////////////////////////////////////////////////////////

class IFileSystem;


class RmlBackend_D3D11 : public Rml::RenderInterface
{
public:
    RmlBackend_D3D11();
    ~RmlBackend_D3D11() override;

    // Non-owning. pDevice/pContext lifetimes belong to D3DRenderer.
    // pFs is used for LoadTexture() so unit tests can substitute an
    // InMemoryFileSystem. May be null in which case LoadTexture
    // returns 0 (which causes RmlUi to render the document without
    // that image, but everything else still works).
    HRESULT Initialize (
        ID3D11Device         * pDevice,
        ID3D11DeviceContext  * pContext,
        UINT                   viewportWidthPx,
        UINT                   viewportHeightPx,
        IFileSystem          * pFs);

    void    Shutdown();

    HRESULT Resize (UINT widthPx, UINT heightPx);

    // Called by UiShell::Render() before walking the Rml render
    // queue. Snapshots whatever pipeline state we are about to
    // clobber so it can be restored in EndFrame.
    HRESULT BeginFrame();
    HRESULT EndFrame   ();

    // ---- Rml::RenderInterface ----

    Rml::CompiledGeometryHandle CompileGeometry (
        Rml::Span<const Rml::Vertex>  vertices,
        Rml::Span<const int>          indices) override;

    void RenderGeometry (
        Rml::CompiledGeometryHandle  geometry,
        Rml::Vector2f                translation,
        Rml::TextureHandle           texture) override;

    void ReleaseGeometry (Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture (
        Rml::Vector2i        & texture_dimensions,
        const Rml::String    & source) override;

    Rml::TextureHandle GenerateTexture (
        Rml::Span<const Rml::byte>  source,
        Rml::Vector2i               source_dimensions) override;

    void ReleaseTexture (Rml::TextureHandle texture) override;

    void EnableScissorRegion (bool enable) override;
    void SetScissorRegion    (Rml::Rectanglei region) override;

    void SetTransform (const Rml::Matrix4f * transform) override;

    // ---- Test introspection ----
    //
    // Exposed for the smoke tests in
    // UnitTest/UiTests/RmlBackendSmokeTests.cpp.
    UINT GetScissorCallCount     () const { return m_scissorCallCount; }
    bool GetLastScissorEnabled   () const { return m_lastScissorEnabled; }
    RECT GetLastScissorRect      () const { return m_lastScissorRect; }
    UINT GetTextureCount         () const { return static_cast<UINT> (m_textures.size()); }

private:
    HRESULT InitializeShaders          ();
    HRESULT InitializePipelineStates   ();
    HRESULT InitializeConstantBuffer   ();
    HRESULT InitializeWhiteTexture     ();

    HRESULT RebuildProjectionMatrix    ();
    HRESULT UpdateConstantBuffer       (Rml::Vector2f translation);

    Rml::TextureHandle RegisterTexture (ComPtr<ID3D11ShaderResourceView> srv);

    static HRESULT LoadImageBytesViaWic (
        const std::vector<Byte>  & bytes,
        std::vector<Byte>        & outRgbaPremultiplied,
        UINT                     & outWidth,
        UINT                     & outHeight);

    static HRESULT CompileHlsl (
        const char           * src,
        size_t                 srcLen,
        const char           * entry,
        const char           * profile,
        ComPtr<ID3DBlob>     & outBlob);

    // Non-owning
    ID3D11Device         * m_device   = nullptr;
    ID3D11DeviceContext  * m_context  = nullptr;
    IFileSystem          * m_fs       = nullptr;

    // Owned GPU resources
    ComPtr<ID3D11VertexShader>    m_vsTextured;
    ComPtr<ID3D11PixelShader>     m_psTextured;
    ComPtr<ID3D11VertexShader>    m_vsUntextured;
    ComPtr<ID3D11PixelShader>     m_psUntextured;

    ComPtr<ID3D11InputLayout>     m_inputLayout;

    ComPtr<ID3D11BlendState>      m_blendState;
    ComPtr<ID3D11RasterizerState> m_rasterizerState;
    ComPtr<ID3D11DepthStencilState> m_depthStencilState;
    ComPtr<ID3D11SamplerState>    m_sampler;

    ComPtr<ID3D11Buffer>          m_constantBuffer;

    // A 1x1 opaque-white texture handed out for "untextured" draws
    // by RmlUi's clip-mask path; the textured shader then multiplies
    // by it harmlessly. We don't use the untextured shader for those.
    ComPtr<ID3D11ShaderResourceView>  m_whiteSrv;

    // Per-handle ownership table for LoadTexture / GenerateTexture.
    // Key is the raw SRV pointer (== TextureHandle returned).
    std::unordered_map<uintptr_t, ComPtr<ID3D11ShaderResourceView>>  m_textures;

    // Per-CompiledGeometryHandle bundle: VB, IB, index count, texture
    // flag. We hand out the address of these heap-allocated structs
    // as the opaque CompiledGeometryHandle.
    struct CompiledGeometry
    {
        ComPtr<ID3D11Buffer>  vb;
        ComPtr<ID3D11Buffer>  ib;
        UINT                  indexCount = 0;
    };

    std::unordered_map<uintptr_t, std::unique_ptr<CompiledGeometry>>  m_geometries;

    // Cached viewport dimensions; rebuilt projection matrix on resize.
    UINT  m_viewportW = 0;
    UINT  m_viewportH = 0;

    // Cached active transform (nullptr == identity). Multiplied into
    // the projection on every UpdateConstantBuffer.
    bool         m_hasTransform = false;
    Rml::Matrix4f m_userTransform;

    // Cached for resize-driven projection rebuild + per-frame state
    // bookkeeping.
    Rml::Matrix4f m_projection;

    // Scissor state. When disabled we still set a scissor (full
    // viewport) since the rasterizer permanently has scissor enabled
    // — RmlUi expects "disabled" to behave like "no clipping".
    bool m_scissorEnabled = false;
    RECT m_scissorRect    = {};

    // Diagnostic counters surfaced via accessors for tests.
    UINT m_scissorCallCount   = 0;
    bool m_lastScissorEnabled = false;
    RECT m_lastScissorRect    = {};

    bool             m_inFrame = false;
};
