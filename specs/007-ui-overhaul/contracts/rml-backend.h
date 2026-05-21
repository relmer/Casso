// Contract: Casso/Ui/RmlBackend_D3D11.h
//
// This file documents the binding interface between Casso's existing
// D3DRenderer and the new RmlUi integration. It is a CONTRACT, not the
// final source — the implementation lives in Casso/Ui/RmlBackend_D3D11.{h,cpp}
// and MUST satisfy every signature and invariant declared here.
//
// All functions follow the project EHM pattern (HRESULT return, single
// Error: label exit) except where the underlying RmlUi callback signature
// requires void; in those cases the implementation wraps internal work in
// helper functions that DO follow EHM.

#pragma once

// (In real code these come through Pch.h, never directly.)
// #include <RmlUi/Core/RenderInterface.h>
// #include <d3d11.h>


namespace Casso::Ui
{
    //
    // Init / shutdown
    //

    // Initialize against the existing D3D11 device owned by D3DRenderer.
    // MUST NOT create a second device, swap chain, or DXGI factory.
    // pDevice and pContext are non-owning; lifetime is managed by D3DRenderer.
    HRESULT RmlBackend_Initialize (
        ID3D11Device         * pDevice,
        ID3D11DeviceContext  * pContext,
        UINT                   viewportWidthPx,
        UINT                   viewportHeightPx);

    // Release all GPU resources owned by the backend (textures, buffers,
    // shaders, blend/raster/depth states).
    void    RmlBackend_Shutdown ();

    // Handle viewport resize. Cheap; called from WM_SIZE path.
    HRESULT RmlBackend_Resize (UINT widthPx, UINT heightPx);

    // Handle D3D device-lost. The implementation MUST be able to
    // re-create every GPU resource it owns. Caller passes the new device.
    HRESULT RmlBackend_OnDeviceRestored (
        ID3D11Device         * pNewDevice,
        ID3D11DeviceContext  * pNewContext);


    //
    // Per-frame integration with D3DRenderer
    //
    // Frame order (enforced by EmulatorShell::Render):
    //   1. D3DRenderer::UploadAndPresent draws the emulated framebuffer.
    //   2. CrtPostProcess::Run runs the optional CRT effect chain.
    //   3. UiShell::Render calls Rml::Context::Update + Render, which calls
    //      back into the methods below.
    //   4. D3DRenderer::Present swaps the chain.
    //

    // Begin a frame of Rml rendering. Caches/clears any per-frame state.
    HRESULT RmlBackend_BeginFrame ();

    // End a frame of Rml rendering. Restores any pipeline state we mutated.
    HRESULT RmlBackend_EndFrame ();
}


//
// Rml::RenderInterface implementation contract.
//
// The class Casso::Ui::RmlRenderInterface_D3D11 inherits from
// Rml::RenderInterface and overrides:
//
//   void  RenderGeometry            (Rml::Vertex *, int, int *, int,
//                                    Rml::TextureHandle, const Rml::Vector2f &);
//   Rml::CompiledGeometryHandle
//         CompileGeometry           (Rml::Vertex *, int, int *, int,
//                                    Rml::TextureHandle);
//   void  RenderCompiledGeometry    (Rml::CompiledGeometryHandle,
//                                    const Rml::Vector2f &);
//   void  ReleaseCompiledGeometry   (Rml::CompiledGeometryHandle);
//   void  EnableScissorRegion       (bool);
//   void  SetScissorRegion          (int, int, int, int);
//   bool  LoadTexture               (Rml::TextureHandle &, Rml::Vector2i &,
//                                    const Rml::String &);
//   bool  GenerateTexture           (Rml::TextureHandle &, const Rml::byte *,
//                                    const Rml::Vector2i &);
//   void  ReleaseTexture            (Rml::TextureHandle);
//   void  SetTransform              (const Rml::Matrix4f *);
//
// INVARIANTS:
// * No method allocates D3D resources outside Init / OnDeviceRestored.
//   GenerateTexture and CompileGeometry MAY allocate; LoadTexture MAY load
//   from disk via the IFileSystem provided to UiShell at init time.
// * All texture loads MUST go through Casso's IFileSystem abstraction —
//   NEVER through CreateFile / Win32 directly — so unit tests can mock.
// * No exception escapes any override; all internal failures become
//   logged warnings + safe-fallback (transparent texture, empty geometry).
// * Coordinate system: top-left origin, pixels, matches Rml's default.
// * Blend mode: premultiplied alpha, source-over.
