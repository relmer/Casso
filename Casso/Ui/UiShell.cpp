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

            PaintDragDropOverlay (visual, localTheme, bottomInset, barTop);

            if (m_navLayer != nullptr)
            {
                m_navLayer->PaintDropdown (m_painter, m_text, visual, localTheme);
            }
        }

        if (settingsVisible)
        {
            // The SettingsPanel owns its own backdrop dim and fades it
            // with the preview state machine; no second scrim here.
            m_settingsPanel->Layout (m_viewportWidthPx, m_viewportHeightPx, m_scaler);
            m_settingsPanel->Paint (m_painter, m_text);
        }
    }

    IGNORE_RETURN_VALUE (hr, m_painter.End (rtv));
    IGNORE_RETURN_VALUE (hr, m_text.EndDraw());
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintDragDropOverlay
//
//  Visual feedback while an OLE drag is in progress over the window:
//
//   * Unsupported file type: full-viewport dim. The OS already paints
//     the not-allowed cursor; the dim signals "we noticed, this isn't
//     going anywhere".
//
//   * Supported disk image: dim everything except the drive widget
//     rects, then paint an amber glow border on each drive. The drive
//     currently under the cursor gets a brighter / thicker glow so the
//     user can see exactly where the drop will land.
//
////////////////////////////////////////////////////////////////////////////////

void UiShell::PaintDragDropOverlay (
    const ChromeVisualState & /*visual*/,
    const ChromeTheme       & /*theme*/,
    int                       /*bottomInset*/,
    int                       /*barTop*/)
{
    const uint32_t  s_kDimRejectArgb     = 0xE6000000;   // ~90% black -- file isn't going anywhere
    const uint32_t  s_kDimAcceptArgb     = 0xC8000000;   // ~78% black -- drives still readable through the haze

    int             hovered              = -1;
    bool            accept               = false;
    int             viewportW            = m_viewportWidthPx;
    int             viewportH            = m_viewportHeightPx;



    if (m_dragSource == nullptr || !m_dragSource->IsDragInProgress())
    {
        return;
    }

    accept  = m_dragSource->IsDragAcceptedType();
    hovered = m_dragSource->HoveredTag();

    if (!accept)
    {
        // Unsupported file: dim the whole viewport. OS provides the
        // not-allowed cursor.
        m_painter.FillRect (0.0f, 0.0f,
                            (float) viewportW,
                            (float) viewportH,
                            s_kDimRejectArgb);
        return;
    }

    // Supported file: dim only the regions OUTSIDE drive widget rects,
    // then halo each drive with a soft amber glow.
    if (m_driveWidgets == nullptr)
    {
        m_painter.FillRect (0.0f, 0.0f,
                            (float) viewportW,
                            (float) viewportH,
                            s_kDimAcceptArgb);
        return;
    }

    {
        RECT  d0       = (*m_driveWidgets)[0].BodyRect();
        RECT  d1       = (*m_driveWidgets)[1].BodyRect();
        bool  d0Empty  = (d0.right <= d0.left) || (d0.bottom <= d0.top);
        bool  d1Empty  = (d1.right <= d1.left) || (d1.bottom <= d1.top);
        LONG  bandTop  = 0;
        LONG  bandBot  = 0;
        LONG  leftX    = 0;
        LONG  midL     = 0;
        LONG  midR     = 0;
        LONG  rightX   = 0;

        if (d0Empty && d1Empty)
        {
            m_painter.FillRect (0.0f, 0.0f,
                                (float) viewportW,
                                (float) viewportH,
                                s_kDimAcceptArgb);
            return;
        }

        if (!d0Empty && !d1Empty)
        {
            bandTop = std::min (d0.top,    d1.top);
            bandBot = std::max (d0.bottom, d1.bottom);
        }
        else
        {
            const RECT &  only = d0Empty ? d1 : d0;
            bandTop = only.top;
            bandBot = only.bottom;
        }

        // 1. top strip (above the drive band)
        if (bandTop > 0)
        {
            m_painter.FillRect (0.0f, 0.0f,
                                (float) viewportW,
                                (float) bandTop,
                                s_kDimAcceptArgb);
        }

        // 2. bottom strip (below the drive band)
        if (bandBot < viewportH)
        {
            m_painter.FillRect (0.0f, (float) bandBot,
                                (float) viewportW,
                                (float) (viewportH - bandBot),
                                s_kDimAcceptArgb);
        }

        // 3/4/5. left, between-drives, right within the drive band
        if (d0Empty || d1Empty)
        {
            const RECT &  only = d0Empty ? d1 : d0;
            leftX  = only.left;
            rightX = only.right;

            if (leftX > 0)
            {
                m_painter.FillRect (0.0f, (float) bandTop,
                                    (float) leftX,
                                    (float) (bandBot - bandTop),
                                    s_kDimAcceptArgb);
            }

            if (rightX < viewportW)
            {
                m_painter.FillRect ((float) rightX, (float) bandTop,
                                    (float) (viewportW - rightX),
                                    (float) (bandBot - bandTop),
                                    s_kDimAcceptArgb);
            }
        }
        else
        {
            leftX  = std::min (d0.left,  d1.left);
            midL   = std::min (d0.right, d1.right);
            midR   = std::max (d0.left,  d1.left);
            rightX = std::max (d0.right, d1.right);

            if (leftX > 0)
            {
                m_painter.FillRect (0.0f, (float) bandTop,
                                    (float) leftX,
                                    (float) (bandBot - bandTop),
                                    s_kDimAcceptArgb);
            }

            if (midR > midL)
            {
                m_painter.FillRect ((float) midL, (float) bandTop,
                                    (float) (midR - midL),
                                    (float) (bandBot - bandTop),
                                    s_kDimAcceptArgb);
            }

            if (rightX < viewportW)
            {
                m_painter.FillRect ((float) rightX, (float) bandTop,
                                    (float) (viewportW - rightX),
                                    (float) (bandBot - bandTop),
                                    s_kDimAcceptArgb);
            }
        }

        // Soft radial glow originating BEHIND each drive. Paint a
        // stack of concentric translucent filled circles centered on
        // the drive, outer-to-inner so the brightest layer lands on
        // top. The drive widget itself was already painted earlier;
        // these layers overlay it with low alpha so the drive's own
        // pixels show through while the glow extends well beyond the
        // rectangle. Hovered drive: brighter base RGB + more layers
        // + larger outer radius + higher peak alpha.
        for (int i = 0; i < 2; i++)
        {
            const RECT &  r       = (i == 0) ? d0 : d1;
            bool          empty   = (r.right <= r.left) || (r.bottom <= r.top);
            bool          hot     = (hovered == i);
            int           layers  = hot ? 60 : 36;
            float         peak    = hot ? 24.0f : 14.0f;
            uint32_t      rgb     = hot ? 0x00FFD27Au : 0x00FFB347u;
            float         innerR  = 0.0f;
            float         outerR  = 0.0f;
            float         cx      = 0.0f;
            float         cy      = 0.0f;
            int           layer   = 0;

            if (empty)
            {
                continue;
            }

            cx     = (float) (r.left + r.right)  * 0.5f;
            cy     = (float) (r.top  + r.bottom) * 0.5f;
            innerR = (float) std::max (r.right - r.left, r.bottom - r.top) * 0.5f;
            outerR = innerR * (hot ? 4.5f : 3.0f);

            // Outer-to-inner so the brightest (innermost) layer paints
            // last and lands on top.
            for (layer = layers - 1; layer >= 0; layer--)
            {
                float     t      = (float) layer / (float) (layers - 1);
                float     radius = innerR + (outerR - innerR) * t;
                float     fall   = (1.0f - t) * (1.0f - t);   // quadratic falloff
                uint32_t  alpha  = (uint32_t) std::clamp (peak * fall, 0.0f, 255.0f);
                uint32_t  argb   = (alpha << 24) | rgb;

                m_painter.FillCircleApprox (cx, cy, radius, argb);
            }
        }
    }
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

    if (m_navLayer != nullptr && m_navLayer->IsOpen())
    {
        (void) m_navLayer->HandleKey (vk);
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
    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        return true;
    }

    if (m_navLayer != nullptr && m_navLayer->IsOpen())
    {
        return true;
    }

    return false;
}
