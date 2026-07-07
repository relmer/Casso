#include "Pch.h"

#include "UiShell.h"
#include "D3DRenderer.h"
#include "Chrome/MainMenu.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ~UiShell
//
////////////////////////////////////////////////////////////////////////////////

UiShell::~UiShell ()
{
    Shutdown();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Initialize
//
//  Brings up the DWrite text renderer used to measure chrome text. The
//  renderer's D3D device supplies the DWrite/D2D factories; UiShell no
//  longer creates a painter because the chrome draws through the host
//  panel tree.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::Initialize (D3DRenderer * pRenderer)
{
    HRESULT                hr     = S_OK;
    ID3D11Device         * device = nullptr;



    CBRAEx (pRenderer, E_INVALIDARG);

    m_renderer = pRenderer;
    device     = m_renderer->GetDevice();

    CBRA (device);

    hr = m_text.Initialize (device);
    CHRA (hr);

    m_viewportWidthPx  = m_renderer->GetBackBufferWidth();
    m_viewportHeightPx = m_renderer->GetBackBufferHeight();

Error:
    if (FAILED (hr))
    {
        Shutdown();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shutdown
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Shutdown ()
{
    m_hitTest.Clear();
    m_text.Shutdown();
    m_renderer = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnResize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnResize (int viewportWidthPx, int viewportHeightPx, UINT dpi)
{
    m_viewportWidthPx  = viewportWidthPx;
    m_viewportHeightPx = viewportHeightPx;
    m_dpi              = (dpi == 0) ? 96 : dpi;
    m_scaler.SetDpi    (m_dpi);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceLost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnDeviceLost ()
{
    return m_text.OnDeviceLost();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceRestored
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnDeviceRestored ()
{
    HRESULT                hr      = S_OK;
    ID3D11Device         * device  = nullptr;



    CBRA (m_renderer);

    device = m_renderer->GetDevice();
    CBRA (device);

    hr = m_text.OnDeviceRestored (device);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::OnMouseMove (int x, int y, bool leftDown)
{
    m_leftDown = leftDown;



    if (m_mainMenu != nullptr)
    {
        m_mainMenu->HandleMouseMove (x, y);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseLeave
//
//  Clear the chrome painters' hot-button / hover state when the
//  cursor exits the window. Skips the settings panel because it
//  intercepts mouse events while visible and runs its own dismiss
//  flow on outside clicks rather than tracking NC-area leaves.
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::OnMouseLeave ()
{
    m_leftDown = false;

    if (m_mainMenu != nullptr)
    {
        m_mainMenu->ClearHover();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::OnLButtonDown (int x, int y)
{
    m_leftDown = true;



    OnMouseMove (x, y, true);

    if (m_mainMenu != nullptr)
    {
        m_mainMenu->HandleMouseDown (x, y);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::OnLButtonUp (int x, int y)
{
    m_leftDown = false;



    OnMouseMove (x, y, false);

    if (m_mainMenu != nullptr)
    {
        m_mainMenu->HandleMouseUp (x, y);
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HandleKey
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::HandleKey (WPARAM vk)
{
    if (m_mainMenu != nullptr && m_mainMenu->IsOpen())
    {
        (void) m_mainMenu->HandleKey (vk);
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsCapturingInput
//
//  True when an overlay UI (settings panel or open top-level menu) is
//  consuming keystrokes. EmulatorShell::OnChar uses this to suppress
//  the WM_CHAR that Windows generates from a WM_KEYDOWN we already
//  handled — e.g. Enter pressed on the settings OK button should not
//  also reach the //e keyboard as a carriage return.
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::IsCapturingInput () const
{
    if (m_mainMenu != nullptr && m_mainMenu->IsOpen())
    {
        return true;
    }

    return false;
}





