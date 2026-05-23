#include "Pch.h"

#include "UiShell.h"

#include "../D3DRenderer.h"






////////////////////////////////////////////////////////////////////////////////
//
//  UiShell
//
////////////////////////////////////////////////////////////////////////////////

UiShell::UiShell()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~UiShell
//
////////////////////////////////////////////////////////////////////////////////

UiShell::~UiShell()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::Initialize (
    D3DRenderer  & renderer,
    HWND           hwnd,
    IFileSystem  * pFs)
{
    HRESULT               hr     = S_OK;
    RECT                  client = {};
    ID3D11Device        * dev    = nullptr;
    ID3D11DeviceContext * ctx    = nullptr;



    CWRA (GetClientRect (hwnd, &client));

    m_hwnd     = hwnd;
    m_widthPx  = std::max<LONG> (1, client.right  - client.left);
    m_heightPx = std::max<LONG> (1, client.bottom - client.top);

    dev = renderer.GetDevice();
    ctx = renderer.GetContext();

    CBRA (dev);
    CBRA (ctx);

    hr = m_backend.Initialize (dev,
                               ctx,
                               static_cast<UINT> (m_widthPx),
                               static_cast<UINT> (m_heightPx),
                               pFs);
    CHRA (hr);

    Rml::SetSystemInterface     (&m_system);
    Rml::SetRenderInterface     (&m_backend);
    Rml::SetFontEngineInterface (&m_fonts);

    if (!Rml::Initialise())
    {
        // The most common reason Initialise fails is a missing font
        // engine; the DWrite engine above is unconditional, so we
        // log + bail so callers can see the failure.
        OutputDebugStringA ("[UiShell] Rml::Initialise returned false\n");
        hr = E_FAIL;
        goto Error;
    }

    m_rmlInitialised = true;

    m_context = Rml::CreateContext ("casso-main",
                                    Rml::Vector2i (m_widthPx, m_heightPx));

    if (m_context == nullptr)
    {
        OutputDebugStringA ("[UiShell] Rml::CreateContext returned null\n");
        hr = E_FAIL;
        goto Error;
    }

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnResize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnResize (int widthPx, int heightPx)
{
    if (widthPx  <= 0) { widthPx  = 1; }
    if (heightPx <= 0) { heightPx = 1; }

    m_widthPx  = widthPx;
    m_heightPx = heightPx;

    m_backend.Resize (static_cast<UINT> (widthPx), static_cast<UINT> (heightPx));

    if (m_context != nullptr)
    {
        m_context->SetDimensions (Rml::Vector2i (widthPx, heightPx));
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Per-frame entry point. Safe to call after Shutdown — becomes a
//  no-op so D3DRenderer's hook can fire harmlessly during the
//  teardown window.
//
//  Called from the D3DRenderer per-frame hook. Order is:
//      1. emulator framebuffer DrawIndexed (already done by caller)
//      2. UiShell::Render -> Context::Update + Context::Render
//      3. swapChain->Present (caller)
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Render()
{
    if (m_context == nullptr)
    {
        return;
    }

    ++m_frameCount;

    m_backend.BeginFrame();

    m_context->Update();
    m_context->Render();

    m_backend.EndFrame();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Shutdown()
{
    if (m_context != nullptr)
    {
        Rml::RemoveContext ("casso-main");
        m_context = nullptr;
    }

    if (m_rmlInitialised)
    {
        Rml::Shutdown();
        m_rmlInitialised = false;
    }

    m_backend.Shutdown();
}
