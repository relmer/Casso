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
    MainMenu                    * MainMenu,
    std::array<DriveWidget, 2>  * driveWidgets,
    const ChromeTheme           * theme)
{
    m_titleBar     = titleBar;
    m_mainMenu     = MainMenu;
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
    visual.nowMs       = (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
                            std::chrono::steady_clock::now().time_since_epoch()).count();

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
                m_titleBar->Paint (m_painter, m_text, localTheme);
            }

            if (m_mainMenu != nullptr)
            {
                m_mainMenu->PaintStrip (m_painter, m_text, visual, localTheme);
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

            if (m_joystickButton != nullptr)
            {
                m_joystickButton->Paint (m_painter, m_text, localTheme);
            }

            PaintDragDropOverlay (visual, localTheme, bottomInset, barTop);

            if (m_mainMenu != nullptr)
            {
                m_mainMenu->PaintDropdown (m_painter, m_text, visual, localTheme);
            }

            if (m_joystickTooltip != nullptr)
            {
                m_joystickTooltip->SetViewportSize (m_viewportWidthPx, m_viewportHeightPx);
                m_joystickTooltip->Tick (visual.nowMs);
                m_joystickTooltip->Paint (m_painter, m_text);
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

        // Drop-target affordance: leave the drives un-dimmed (they
        // already paint at full brightness vs the ~78% scrim around
        // them). Hovered drive gets a subtle warm tint so the active
        // target reads as the "selected" drop point. Skipping the
        // concentric-circle glow until the dragdrop-light-source
        // follow-up ships the real texture-quad version -- the slice-
        // approximated circles produced visible banding and obscured
        // the drive content under them.
        if (hovered >= 0 && hovered < 2)
        {
            const RECT &  r     = (hovered == 0) ? d0 : d1;
            bool          empty = (r.right <= r.left) || (r.bottom <= r.top);

            if (!empty)
            {
                m_painter.FillRect ((float) r.left,
                                    (float) r.top,
                                    (float) (r.right  - r.left),
                                    (float) (r.bottom - r.top),
                                    0x30FFD27Au);
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

    if (m_titleBar != nullptr)
    {
        m_titleBar->ClearHover();
    }

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



    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->OnLButtonDown (x, y);
        return true;
    }

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



    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        m_settingsPanel->OnLButtonUp (x, y);
        return true;
    }

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
    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        (void) m_settingsPanel->OnKey (vk);
        return true;
    }

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
    if (m_settingsPanel != nullptr && m_settingsPanel->IsVisible())
    {
        return true;
    }

    if (m_mainMenu != nullptr && m_mainMenu->IsOpen())
    {
        return true;
    }

    return false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  IsSettingsCapturing
//
//  True only when the modal settings panel is visible. The chrome
//  keyboard-focus ring (menu titles / buttons / drives) lets the settings
//  panel win keystrokes outright while still owning every other key, so it
//  needs to test the settings panel in isolation from the open-menu state
//  that IsCapturingInput also folds in.
//
////////////////////////////////////////////////////////////////////////////////

bool UiShell::IsSettingsCapturing () const
{
    return m_settingsPanel != nullptr && m_settingsPanel->IsVisible();
}
