#include "Pch.h"

#include "UiShell.h"
#include "D3DRenderer.h"
#include "Settings/SettingsPanel.h"





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
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::Initialize (D3DRenderer * pRenderer)
{
    HRESULT                hr     = S_OK;
    ID3D11Device         * device = nullptr;
    ID3D11DeviceContext  * ctx    = nullptr;



    CBRAEx (pRenderer, E_INVALIDARG);

    m_renderer = pRenderer;
    device     = m_renderer->GetDevice();
    ctx        = m_renderer->GetContext();

    CBRA (device);
    CBRA (ctx);

    hr = m_painter.Initialize (device, ctx);
    CHRA (hr);

    hr = m_text.Initialize (device);
    CHRA (hr);

    m_viewportWidthPx  = m_renderer->GetBackBufferWidth();
    m_viewportHeightPx = m_renderer->GetBackBufferHeight();
    m_targetDirty      = true;
    m_initialized      = true;

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
    m_anim.ClearTweens();
    m_focus.Clear();
    m_hitTest.Clear();
    m_input.Clear();
    m_text.Shutdown();
    m_painter.Shutdown();
    m_renderer    = nullptr;
    m_initialized = false;
    m_targetDirty = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetChrome
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::SetChrome (
    TitleBar                    * titleBar,
    NavLayer                    * navLayer,
    std::array<DriveWidget, 2>  * driveWidgets,
    const ChromeTheme           * theme)
{
    m_titleBar     = titleBar;
    m_navLayer     = navLayer;
    m_driveWidgets = driveWidgets;
    m_theme        = theme;
}




////////////////////////////////////////////////////////////////////////////////
//
//  SetSettingsPanel
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::SetSettingsPanel (SettingsPanel * settingsPanel)
{
    m_settingsPanel = settingsPanel;
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
    m_targetDirty      = true;

    m_text.UnbindBackBuffer();

    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->Layout (m_viewportWidthPx, m_viewportHeightPx, m_scaler);
    }

    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnDeviceLost
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::OnDeviceLost ()
{
    HRESULT  hrPainter = m_painter.OnDeviceLost();
    HRESULT  hrText    = m_text.OnDeviceLost();



    m_initialized = false;
    m_targetDirty = true;

    if (FAILED (hrPainter))
    {
        return hrPainter;
    }
    return hrText;
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
    ID3D11DeviceContext  * ctx     = nullptr;



    CBRA (m_renderer);

    device = m_renderer->GetDevice();
    ctx    = m_renderer->GetContext();
    CBRA (device);
    CBRA (ctx);

    hr = m_painter.OnDeviceRestored (device, ctx);
    CHRA (hr);

    hr = m_text.OnDeviceRestored (device);
    CHRA (hr);

    m_initialized = true;
    m_targetDirty = true;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  RefreshTextTarget
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UiShell::RefreshTextTarget ()
{
    HRESULT                hr        = S_OK;
    ComPtr<IDXGISurface>   surface;



    CBRA (m_renderer);

    hr = m_renderer->GetBackBufferDxgiSurface (&surface);
    CHRA (hr);

    hr = m_text.BindBackBuffer (surface.Get(), m_dpi, m_dpi);
    CHRA (hr);

    m_targetDirty = false;

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::Render ()
{
    HRESULT                  hr     = S_OK;
    ID3D11RenderTargetView * rtv    = nullptr;
    ChromeVisualState        visual      = {};
    ChromeTheme              localTheme;
    int                      bottomInset = 0;
    int                      barTop      = 0;



    if (!m_initialized || (m_renderer == nullptr))
    {
        return;
    }

    if (m_targetDirty)
    {
        HRESULT  hrRefresh = RefreshTextTarget();

        if (FAILED (hrRefresh))
        {
            return;
        }
    }

    m_viewportWidthPx  = m_renderer->GetBackBufferWidth();
    m_viewportHeightPx = m_renderer->GetBackBufferHeight();
    rtv                = m_renderer->GetBackBufferRtv();
    localTheme         = (m_theme != nullptr) ? *m_theme : ChromeTheme::Skeuomorphic();
    bottomInset        = m_renderer->GetBottomInsetPx();
    barTop             = std::max (0, m_viewportHeightPx - bottomInset);
    visual.dpi         = m_dpi;
    visual.frameIndex  = m_frameIndex++;

    if (rtv == nullptr)
    {
        return;
    }

    hr = m_painter.Begin (m_viewportWidthPx, m_viewportHeightPx);
    if (FAILED (hr))
    {
        return;
    }

    hr = m_text.BeginDraw();
    if (FAILED (hr))
    {
        IGNORE_RETURN_VALUE (hr, m_painter.End (rtv));
        return;
    }

    {
        bool  settingsVisible = (m_settingsPanel != nullptr) && m_settingsPanel->IsVisible();


        if (!settingsVisible)
        {
            if (m_titleBar != nullptr)
            {
                m_titleBar->Paint (m_painter, m_text, visual, localTheme);
            }

            if (m_navLayer != nullptr)
            {
                m_navLayer->PaintStrip (m_painter, m_text, visual, localTheme);
            }

            if (bottomInset > 0)
            {
                m_painter.FillRect (0.0f,
                                    (float) barTop,
                                    (float) m_viewportWidthPx,
                                    (float) (m_viewportHeightPx - barTop),
                                    localTheme.navStripArgb);
            }

            if (m_driveWidgets != nullptr)
            {
                for (DriveWidget & drive : *m_driveWidgets)
                {
                    drive.Paint (m_painter, m_text, visual, localTheme);
                }
            }

            if (m_navLayer != nullptr)
            {
                m_navLayer->PaintDropdown (m_painter, m_text, visual, localTheme);
            }
        }

        if (settingsVisible)
        {
            // Translucent dim layer so the emulator framebuffer behind
            // the panel stays partially visible instead of going black.
            m_painter.FillRect (0.0f,
                                0.0f,
                                (float) m_viewportWidthPx,
                                (float) m_viewportHeightPx,
                                0xA0000000u);
            m_settingsPanel->Layout (m_viewportWidthPx, m_viewportHeightPx, m_scaler);
            m_settingsPanel->Paint (m_painter, m_text);
        }
    }

    IGNORE_RETURN_VALUE (hr, m_painter.End (rtv));
    IGNORE_RETURN_VALUE (hr, m_text.EndDraw());
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::OnMouseMove (int x, int y, bool leftDown)
{
    m_leftDown = leftDown;



    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->OnMouseMove (x, y);
        return true;
    }

    if (m_titleBar != nullptr)
    {
        m_titleBar->SetMousePosition (x, y, leftDown);
    }

    if (m_navLayer != nullptr)
    {
        m_navLayer->HandleMouseMove (x, y);
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::OnLButtonDown (int x, int y)
{
    m_leftDown = true;



    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->OnLButtonDown (x, y);
        return true;
    }

    OnMouseMove (x, y, true);

    if (m_navLayer != nullptr)
    {
        m_navLayer->HandleMouseDown (x, y);
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



    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->OnLButtonUp (x, y);
        return true;
    }

    OnMouseMove (x, y, false);

    if (m_navLayer != nullptr)
    {
        m_navLayer->HandleMouseUp (x, y);
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
    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        (void) m_settingsPanel->OnKey (vk);
        return true;
    }

    if (m_navLayer != nullptr)
    {
        return m_navLayer->HandleKey (vk);
    }

    return false;
}
