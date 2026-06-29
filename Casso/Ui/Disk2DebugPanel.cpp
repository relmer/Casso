#include "Pch.h"

#include "Disk2DebugPanel.h"

#include "Chrome/CassoTheme.h"
#include "Chrome/TitleBar.h"

#include "../DebugDialogProjection.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName  = L"Casso.Disk2Debug.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle = L"Casso - Disk ][ debug";

    constexpr int      s_kPreferredWidthDip  = 960;
    constexpr int      s_kPreferredHeightDip = 600;
    constexpr UINT     s_kSwapBufferCount     = 2;

    constexpr LPCWSTR  s_kpszTrackFilterLabel  = L"Track:";
    constexpr LPCWSTR  s_kpszSectorFilterLabel = L"Sector:";
    constexpr LPCWSTR  s_kpszTrackQtFilterLabel = L"Quarter-track:";

    constexpr float    s_kLabelFontDip = 13.0f;

    constexpr LPCWSTR  s_kpszEventCheckLabels[kEventTypeCheckCount] =
    {
        L"Motor", L"HeadStep", L"HeadBump", L"AddrMark",
        L"Read",  L"Write",    L"Door",     L"DriveSel",
    };

    constexpr LPCWSTR  s_kpszAudioSubLabels[kAudioSubCheckCount] =
    {
        L"Started", L"Restarted", L"Continued", L"Silent",
    };

    constexpr LPCWSTR  s_kpszDriveOptionLabels[kDriveRadioCount] =
    {
        L"All", L"Drive 1", L"Drive 2",
    };

    constexpr LPCWSTR  s_kpszRawQtLabel    = L"Quarter-track steps";
    constexpr LPCWSTR  s_kpszPauseLabel    = L"Pause";
    constexpr LPCWSTR  s_kpszResumeLabel   = L"Resume";
    constexpr LPCWSTR  s_kpszClearLabel    = L"Clear";
    constexpr LPCWSTR  s_kpszAudioLabel    = L"All";
    constexpr LPCWSTR  s_kpszInvalidLabel  = L"Invalid";
    constexpr LPCWSTR  s_kpszTrackInvalidPrefix  = L"Invalid track: ";
    constexpr LPCWSTR  s_kpszSectorInvalidPrefix = L"Invalid sector: ";
    constexpr LPCWSTR  s_kpszDriveFilterLabel    = L"Drive:";
    constexpr LPCWSTR  s_kpszDiskEventsLabel     = L"Disk events:";
    constexpr LPCWSTR  s_kpszAudioEventsLabel    = L"Audio events:";

    constexpr LPCWSTR  s_kpszEventCheckTips[kEventTypeCheckCount] =
    {
        L"Motor spin-up / spin-down transitions",
        L"Stepper head moves between tracks",
        L"Head bumps against track 0 stop",
        L"Address-field reads (track / sector / volume)",
        L"Data-field sector reads",
        L"Data-field sector writes",
        L"Disk-inserted / disk-ejected events",
        L"Soft-switch drive selection (Drive 1 vs Drive 2)",
    };

    constexpr LPCWSTR  s_kpszAudioSubTips[kAudioSubCheckCount] =
    {
        L"Audio loop started",
        L"Audio loop restarted with new parameters",
        L"Audio loop continued without retrigger",
        L"Audio loop silenced (with reason)",
    };

    constexpr LPCWSTR  s_kpszDriveRadioTips[kDriveRadioCount] =
    {
        L"Show events from all drives",
        L"Show only events targeting Drive 1",
        L"Show only events targeting Drive 2",
    };

    constexpr LPCWSTR  s_kpszAudioMasterTip = L"Master toggle for all audio-event categories below";
    constexpr LPCWSTR  s_kpszRawQtTip       = L"Show every quarter-track head step (verbose)";
    constexpr LPCWSTR  s_kpszTrackEditTip   = L"Filter rows to a single track (blank = all)";
    constexpr LPCWSTR  s_kpszSectorEditTip  = L"Filter rows to a single sector (blank = all)";


    void ArgbToFloat4 (uint32_t argb, float (& outRgba)[4]) noexcept
    {
        outRgba[0] = (float) ((argb >> 16) & 0xFFu) / 255.0f;
        outRgba[1] = (float) ((argb >>  8) & 0xFFu) / 255.0f;
        outRgba[2] = (float) ((argb      ) & 0xFFu) / 255.0f;
        outRgba[3] = (float) ((argb >> 24) & 0xFFu) / 255.0f;
    }



    // Builds the "Invalid track: tok1, tok2" detail label by slicing
    // the rejected UTF-16 spans out of the original expression. If the
    // edit parsed cleanly, returns an empty string. Defensive about
    // bad spans so an out-of-range index can't crash the dialog.
    std::wstring BuildInvalidLabel (
        LPCWSTR                                                  prefix,
        const std::wstring                                     & expr,
        const std::vector<TrackSectorPredicate::RejectedSpan> & spans)
    {
        std::wstring  result;

        if (spans.empty()) { return result; }

        result = prefix;
        for (size_t i = 0; i < spans.size(); ++i)
        {
            int  beginIdx = spans[i].beginUtf16;
            int  endIdx   = spans[i].endUtf16;
            if (beginIdx < 0)                       { beginIdx = 0; }
            if (endIdx   > (int) expr.size())       { endIdx   = (int) expr.size(); }
            if (endIdx  <= beginIdx)                { continue; }
            if (i > 0)                              { result += L", "; }
            result.append (expr, (size_t) beginIdx, (size_t) (endIdx - beginIdx));
        }
        return result;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2DebugPanel
//
////////////////////////////////////////////////////////////////////////////////

Disk2DebugPanel::Disk2DebugPanel()
{
    // Register each owned widget into the panel's child list via Adopt
    // so they participate in the IDxuiControl tree (Bounds, Visible,
    // focus, parent pointers). The widgets remain Disk2DebugPanel-owned
    // members; Adopt is non-owning. The chrome shell still drives
    // input/paint through the bespoke IChromedPanelContent shims;
    // collapsing the duality is deferred to a follow-up session that
    // also threads a popup host through to the column-menu / tooltip.
    Adopt (m_trackFilterLabel);
    Adopt (m_sectorFilterLabel);
    Adopt (m_driveFilterLabel);
    Adopt (m_diskEventsLabel);
    Adopt (m_audioEventsLabel);
    Adopt (m_trackInvalidLabel);
    Adopt (m_sectorInvalidLabel);
    for (DxuiCheckbox & check : m_eventChecks)
    {
        Adopt (check);
    }
    Adopt (m_audioMasterCheck);
    for (DxuiCheckbox & check : m_audioSubChecks)
    {
        Adopt (check);
    }
    Adopt (m_rawQtCheck);
    Adopt (m_driveRadio);
    Adopt (m_trackEdit);
    Adopt (m_sectorEdit);
    Adopt (m_pauseButton);
    Adopt (m_clearButton);
    Adopt (m_eventList);
    Adopt (m_tooltip);
    Adopt (m_columnMenu);

    m_uptimeAnchor = std::chrono::steady_clock::now();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~Disk2DebugPanel
//
////////////////////////////////////////////////////////////////////////////////

Disk2DebugPanel::~Disk2DebugPanel()
{
    Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
//  Registers the window class for this panel content type, then asks
//  the chrome shell to stand up the host HWND bound to this content.
//  Idempotent -- a second call while already open is a no-op.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme    * theme)
{
    HRESULT  hr = S_OK;



    BAIL_OUT_IF (m_window.IsOpen(), S_OK);

    CBRAEx (hInstance, E_INVALIDARG);
    CBRAEx (device,    E_INVALIDARG);
    CBRAEx (context,   E_INVALIDARG);

    m_device  = device;
    m_context = context;
    m_theme   = theme;

    hr = m_window.RegisterClass (hInstance, s_kpszClassName);
    CHRA (hr);

    hr = m_window.Create (hwndOwner, this, device, context, theme);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Show
//
//  Brings the host window to the front. Lifecycle assumes Create has
//  already succeeded; ShowWindow on a null HWND is a no-op anyway.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Show()
{
    m_window.Activate();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Hide()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_HIDE);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Destroy
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Destroy()
{
    m_window.Destroy();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RenderFrame
//
//  Public render entry point invoked by the host frame loop. Delegates
//  to the chrome shell which composites our content (currently just a
//  themed background clear) under its title bar.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::RenderFrame()
{
    return m_window.Render();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  IChromedPanelContent override -- invoked by the chrome shell during
//  its own Render. T044 lands the panel body as a flat themed clear;
//  T046+ layers in widget rendering atop this.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::Render()
{
    HRESULT                  hr            = S_OK;
    float                    clearColor[4] = { 0.08f, 0.08f, 0.08f, 1.0f };
    ComPtr<ID3D11Texture2D>  backBuffer;
    ComPtr<IDXGISurface>     surface;
    ChromeVisualState        visual        = {};
    D3D11_VIEWPORT           vp            = {};


    BAIL_OUT_IF (m_swapChain == nullptr || m_rtv == nullptr || m_context == nullptr, S_OK);

    DrainAndProject();

    if (!m_text.IsTargetBound())
    {
        hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
        CHRA (hr);

        hr = backBuffer.As (&surface);
        CHRA (hr);

        hr = m_text.BindBackBuffer (surface.Get(), m_dpi, m_dpi);
        CHRA (hr);
    }

    if (m_theme != nullptr)
    {
        ArgbToFloat4 (m_theme->titleBarBottomArgb, clearColor);
        clearColor[3] = 1.0f;
    }

    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width    = (float) m_widthPx;
    vp.Height   = (float) m_heightPx;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;

    m_context->RSSetViewports (1, &vp);
    m_context->OMSetRenderTargets (1, m_rtv.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView (m_rtv.Get(), clearColor);

    hr = m_painter.Begin (m_widthPx, m_heightPx);
    CHRA (hr);

    hr = m_text.BeginDrawOffscreen();
    CHRA (hr);

    visual.dpi = m_dpi;
    if (m_titleBar != nullptr && m_theme != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, *m_theme);
    }

    for (auto & cb : m_eventChecks)
    {
        cb.Paint (m_painter, m_text);
    }
    m_audioMasterCheck.Paint (m_painter, m_text);
    for (auto & cb : m_audioSubChecks)
    {
        cb.Paint (m_painter, m_text);
    }
    m_driveRadio.Paint   (m_painter, m_text);
    m_rawQtCheck.Paint   (m_painter, m_text);

    m_trackFilterLabel.Paint  (m_painter, m_text);
    m_sectorFilterLabel.Paint (m_painter, m_text);
    m_driveFilterLabel.Paint  (m_painter, m_text);
    m_diskEventsLabel.Paint   (m_painter, m_text);
    m_audioEventsLabel.Paint  (m_painter, m_text);
    m_trackInvalidLabel.Paint (m_painter, m_text);
    m_sectorInvalidLabel.Paint(m_painter, m_text);

    m_trackEdit.Paint  (m_painter, m_text);
    m_sectorEdit.Paint (m_painter, m_text);

    if (m_theme != nullptr)
    {
        m_pauseButton.Paint (m_painter, m_text, *m_theme);
        m_clearButton.Paint (m_painter, m_text, *m_theme);
    }

    m_eventList.Paint (m_painter, m_text);

    m_tooltip.Tick  (NowMs());

    // Overlays (tooltip + column menu) must composite ABOVE every
    // underlying widget. Both the geometry painter (DxuiPainter) and
    // the text renderer (DxuiTextRenderer) batch their draws, so
    // without an explicit flush all text would composite on top of
    // all geometry — including the tooltip's opaque background. Flush
    // both pipelines now so the next Begin/Paint cycle lands cleanly
    // on top of everything painted so far.
    if (m_tooltip.IsVisible() || m_columnMenu.IsVisible())
    {
        hr = m_painter.End (m_rtv.Get());
        CHRA (hr);
        hr = m_text.EndDrawComposite();
        CHRA (hr);
        hr = m_painter.Begin (m_widthPx, m_heightPx);
        CHRA (hr);
        hr = m_text.BeginDrawOffscreen();
        CHRA (hr);
    }

    m_tooltip.Paint (m_painter, m_text);

    m_columnMenu.Paint (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDrawComposite();
    CHRA (hr);

    // If the D2D target was lost partway through this frame (the
    // swap-chain buffers were invalidated by a live resize), EndDraw
    // unbinds the target and the frame we just built is incomplete --
    // some text was flushed before the loss, the rest was dropped.
    // Presenting it would flash partially-painted content (blank rows,
    // missing buttons). Skip Present so the last good frame stays on
    // screen; the next Render rebinds and repaints cleanly.
    if (m_text.IsTargetBound())
    {
        hr = m_swapChain->Present (1, 0);
        CHRA (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SetTheme (const CassoTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowClassName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR Disk2DebugPanel::GetWindowClassName() const
{
    return s_kpszClassName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR Disk2DebugPanel::GetWindowTitle() const
{
    return s_kpszWindowTitle;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostCreated
//
//  Stands up the swap chain bound to the host HWND. Title-bar pointer
//  and theme are remembered so SetChromeTheme calls land on the same
//  TitleBar instance the shell painted into.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::OnHostCreated (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi,
    TitleBar             * titleBar,
    const CassoTheme    * theme)
{
    HRESULT  hr = S_OK;


    CBRAEx (hwnd,    E_INVALIDARG);
    CBRAEx (device,  E_INVALIDARG);
    CBRAEx (context, E_INVALIDARG);

    m_hwnd     = hwnd;
    m_device   = device;
    m_context  = context;
    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_dpi      = dpi;
    m_titleBar = titleBar;
    m_theme    = theme;

    hr = EnsureSwapChain();
    CHRA (hr);

    hr = m_painter.Initialize (device, context);
    CHRA (hr);

    hr = m_text.Initialize (device);
    CHRA (hr);

    ConfigureWidgets();
    RecomputeLayout();

    // Route the column right-click menu through the host popup pool so
    // it renders as a real top-level popup (not clipped to the panel).
    m_columnMenu.SetPopupHost (m_window.PopupHost());

    // Same for hover tooltips so the balloon can escape the panel edge
    // and occlude whatever is behind it.
    m_tooltip.SetPopupHost (m_window.PopupHost());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostDestroyed
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnHostDestroyed()
{
    // Release any live popup back to the pool and drop the host pointer
    // before the host (and its pool) are destroyed in OnDestroy.
    m_columnMenu.Hide();
    m_columnMenu.SetPopupHost (nullptr);

    m_tooltip.HideImmediate();
    m_tooltip.SetPopupHost (nullptr);

    m_text.UnbindBackBuffer();
    m_text.Shutdown();
    m_painter.Shutdown();
    ReleaseRenderTargets();
    m_swapChain.Reset();
    m_hwnd     = nullptr;
    m_titleBar = nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostResize
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::OnHostResize (int widthPx, int heightPx, UINT dpi)
{
    HRESULT  hr = S_OK;


    m_widthPx  = std::max (1, widthPx);
    m_heightPx = std::max (1, heightPx);
    m_dpi      = dpi;

    BAIL_OUT_IF (m_swapChain == nullptr, S_OK);

    m_text.UnbindBackBuffer();
    ReleaseRenderTargets();

    hr = m_swapChain->ResizeBuffers (s_kSwapBufferCount,
                                     (UINT) m_widthPx,
                                     (UINT) m_heightPx,
                                     DXGI_FORMAT_B8G8R8A8_UNORM,
                                     0);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

    RecomputeLayout();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetChromeTheme
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SetChromeTheme (TitleBar * titleBar, const CassoTheme * theme)
{
    m_titleBar = titleBar;
    m_theme    = theme;
    RecomputeLayout();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PreferredClientSize
//
//  T044 lands a fixed default size; T046 will replace this with a
//  layout-driven calculation once control families exist.
//
////////////////////////////////////////////////////////////////////////////////

SIZE Disk2DebugPanel::PreferredClientSize (UINT dpi) const
{
    SIZE  size = {};


    size.cx = MulDiv (s_kPreferredWidthDip,  (int) dpi, 96);
    size.cy = MulDiv (s_kPreferredHeightDip, (int) dpi, 96);
    return size;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonDown
//
//  Routes mouse-down to whichever widget owns the hit point. First-hit
//  wins; widgets earlier in the dispatch order get priority.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnLButtonDown (int x, int y)
{
    bool  handled    = false;
    int   newFocus   = -1;


    if (m_columnMenu.IsVisible())
    {
        if (m_columnMenu.OnLButtonDown (x, y)) { return; }
    }

    for (size_t i = 0; i < m_eventChecks.size(); ++i)
    {
        if (m_eventChecks[i].OnLButtonDown (x, y))
        {
            handled  = true;
            newFocus = (int) i;
            break;
        }
    }
    if (!handled && m_audioMasterCheck.OnLButtonDown (x, y))
    {
        handled  = true;
        newFocus = 8;
    }
    if (!handled)
    {
        for (size_t i = 0; i < m_audioSubChecks.size(); ++i)
        {
            if (m_audioSubChecks[i].OnLButtonDown (x, y))
            {
                handled  = true;
                newFocus = 9 + (int) i;
                break;
            }
        }
    }
    if (!handled && m_rawQtCheck.OnLButtonDown (x, y))
    {
        handled  = true;
        newFocus = 16;
    }
    if (!handled && m_driveRadio.OnLButtonDown (x, y))
    {
        handled  = true;
        newFocus = 13;
    }
    if (!handled && m_trackEdit.OnLButtonDown (x, y))
    {
        handled  = true;
        newFocus = 14;
    }
    if (!handled && m_sectorEdit.OnLButtonDown (x, y))
    {
        handled  = true;
        newFocus = 15;
    }

    if (m_pauseButton.HitTest (x, y))
    {
        m_pauseButton.SetMouse (x, y, true);
        handled  = true;
        newFocus = 17;
    }
    if (!handled && m_clearButton.HitTest (x, y))
    {
        m_clearButton.SetMouse (x, y, true);
        handled  = true;
        newFocus = 18;
    }

    if (handled)
    {
        SetFocusIndex (newFocus);
    }

    if (!handled)
    {
        // Scrollbar thumb drag has priority over column-resize and
        // row-hit, so the user can grab the thumb without accidentally
        // clicking the row beneath it. Clicks on the track (not the
        // thumb) page-scroll toward the click; the end arrows nudge by
        // one row.
        int  relX = x - m_layout.listView.left;
        int  relY = y - m_layout.listView.top;
        if (m_eventList.HitTestScrollbarArrowUp (relX, relY))
        {
            m_eventList.ScrollByRows (-1);
            handled = true;
        }
        else if (m_eventList.HitTestScrollbarArrowDown (relX, relY))
        {
            m_eventList.ScrollByRows (1);
            handled = true;
        }
        else if (m_eventList.HitTestScrollbarThumb (relX, relY))
        {
            m_eventList.BeginThumbDrag (relY);
            SetCapture (m_hwnd);
            handled = true;
        }
        else if (m_eventList.HitTestScrollbarTrack (relX, relY))
        {
            m_eventList.PageFromTrackClick (relY);
            handled = true;
        }
    }

    if (!handled)
    {
        // Column-resize drag has priority over header-sort: if the
        // user grabbed a resize handle on a header right-edge, we
        // start the drag here and the sort hit-test below is skipped.
        int  relX       = x - m_layout.listView.left;
        int  relY       = y - m_layout.listView.top;
        int  tolPx      = MulDiv (4, (int) m_dpi, 96);
        int  resizeCol  = m_eventList.HitTestColumnResize (relX, relY, tolPx);
        if (resizeCol >= 0)
        {
            m_resizeColumn       = resizeCol;
            m_resizeStartXPx     = x;
            m_resizeStartWidthPx = m_eventList.GetColumnEffectiveWidthPx ((size_t) resizeCol);
            SetCapture (m_hwnd);
            handled              = true;

            int  visIdx = m_eventList.GetVisibleIndexOfColumn ((size_t) resizeCol);
            if (visIdx >= 0)
            {
                SetFocusIndex (19 + 2 * visIdx + 1);
            }
        }
    }

    if (!handled)
    {
        // Click on listview body selects a row; click on the header
        // sorts that column (first click ascending, subsequent click
        // on the same column flips direction).
        int  relX = x - m_layout.listView.left;
        int  relY = y - m_layout.listView.top;
        int  hit  = m_eventList.HitTestRow (relX, relY);
        if (hit >= 0)
        {
            int  N = m_eventList.GetVisibleColumnCount();
            if (hit < (int) m_filteredIndices.size())
            {
                m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) hit];
            }
            if (N > 0)
            {
                SetFocusIndex (19 + 2 * N - 1);
            }
            ApplyListSelection();
        }
        else
        {
            int  col = m_eventList.HitTestHeaderColumn (relX, relY);
            if (col >= 0)
            {
                SortByColumn (col);
                int  visIdx = m_eventList.GetVisibleIndexOfColumn ((size_t) col);
                if (visIdx >= 0)
                {
                    SetFocusIndex (19 + 2 * visIdx);
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnLButtonUp (int x, int y)
{
    if (m_eventList.IsThumbDragging())
    {
        m_eventList.EndThumbDrag();
        ReleaseCapture();
        return;
    }

    if (m_resizeColumn >= 0)
    {
        m_resizeColumn = -1;
        ReleaseCapture();
    }

    if (m_columnMenu.IsVisible())
    {
        if (m_columnMenu.OnLButtonUp (x, y)) { return; }
    }

    for (auto & cb : m_eventChecks)        { cb.OnLButtonUp (x, y); }
    m_audioMasterCheck.OnLButtonUp (x, y);
    for (auto & cb : m_audioSubChecks)     { cb.OnLButtonUp (x, y); }
    m_rawQtCheck.OnLButtonUp   (x, y);
    m_driveRadio.OnLButtonUp   (x, y);
    m_trackEdit.OnLButtonUp    (x, y);
    m_sectorEdit.OnLButtonUp   (x, y);

    bool  pauseDown = m_pauseButton.HitTest (x, y);
    bool  clearDown = m_clearButton.HitTest (x, y);

    m_pauseButton.SetMouse (x, y, false);
    m_clearButton.SetMouse (x, y, false);

    if (pauseDown)
    {
        m_pauseButton.Click();
    }
    if (clearDown)
    {
        m_clearButton.Click();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseMove
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMouseMove (int x, int y)
{
    if (m_eventList.IsThumbDragging())
    {
        int  relY = y - m_layout.listView.top;
        m_eventList.UpdateThumbDrag (relY);
        return;
    }

    if (m_resizeColumn >= 0)
    {
        int  deltaPx  = x - m_resizeStartXPx;
        int  newWidth = m_resizeStartWidthPx + deltaPx;
        int  minPx    = MulDiv (24, (int) m_dpi, 96);
        if (newWidth < minPx) { newWidth = minPx; }
        m_eventList.SetColumnOverrideWidthPx ((size_t) m_resizeColumn, newWidth);
        return;
    }

    if (m_columnMenu.IsVisible())
    {
        m_columnMenu.OnMouseMove (x, y);
        m_tooltip.RequestHide (NowMs());
        return;
    }

    for (auto & cb : m_eventChecks)        { cb.SetMouseHover (x, y); }
    m_audioMasterCheck.SetMouseHover (x, y);
    for (auto & cb : m_audioSubChecks)     { cb.SetMouseHover (x, y); }
    m_rawQtCheck.SetMouseHover (x, y);
    m_driveRadio.SetMouseHover (x, y);
    m_trackEdit.SetMouseHover  (x, y);
    m_sectorEdit.SetMouseHover (x, y);

    m_pauseButton.SetMouse (x, y, m_pauseButton.HitTest (x, y) && (GetKeyState (VK_LBUTTON) & 0x8000));
    m_clearButton.SetMouse (x, y, m_clearButton.HitTest (x, y) && (GetKeyState (VK_LBUTTON) & 0x8000));

    int  relX = x - m_layout.listView.left;
    int  relY = y - m_layout.listView.top;
    int  hit  = m_eventList.HitTestRow (relX, relY);
    m_eventList.SetHoveredRow (hit);

    UpdateTooltip (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnMouseWheel
//
//  Wheel up scrolls back in history (older events); wheel down scrolls
//  toward the tail. Mouse position is panel-relative; we forward to
//  the list unconditionally because the trackpad / wheel input from
//  the whole panel should drive the only scrollable widget.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMouseWheel (int x, int y, int delta)
{
    (void) x;
    (void) y;

    m_eventList.ScrollByWheelDelta (delta, 3);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnRButtonDown
//
//  Right-click inside the list-view header strip surfaces a themed
//  popup menu of column-visibility toggles. Anywhere else, right-click
//  is currently a no-op.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnRButtonDown (int x, int y)
{
    int  relX        = x - m_layout.listView.left;
    int  relY        = y - m_layout.listView.top;
    int  headerH     = m_eventList.GetHeaderHeightPx();
    int  listWidthPx = m_layout.listView.right - m_layout.listView.left;


    if (!m_eventList.IsHeaderShown())                                { return; }
    if (relX < 0 || relX >= listWidthPx)                          { return; }
    if (relY < 0 || relY >= headerH)                              { return; }

    ShowColumnMenu (x, y);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSetCursor
//
//  Cursor override for the panel. Returns IDC_SIZEWE while a column
//  resize is active or when the cursor is parked on a header-edge
//  resize handle; returns nullptr (default arrow) otherwise.
//
////////////////////////////////////////////////////////////////////////////////

HCURSOR Disk2DebugPanel::OnSetCursor (int x, int y)
{
    int  relX  = x - m_layout.listView.left;
    int  relY  = y - m_layout.listView.top;
    int  tolPx = MulDiv (4, (int) m_dpi, 96);

    if (m_resizeColumn >= 0)
    {
        return LoadCursorW (nullptr, IDC_SIZEWE);
    }
    if (m_eventList.HitTestColumnResize (relX, relY, tolPx) >= 0)
    {
        return LoadCursorW (nullptr, IDC_SIZEWE);
    }
    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShowColumnMenu
//
//  Builds a popup menu item for each column with the current
//  visibility as the check state and anchors it at the click point.
//  Selection callback is wired in ConfigureWidgets and flips the
//  selected column's visibility, then re-runs layout so width / sort
//  reflect the change.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ShowColumnMenu (int anchorX, int anchorY)
{
    std::vector<DxuiPopupMenu::Item>  items;
    RECT                          host = { 0, 0, m_widthPx, m_heightPx };


    items.reserve (m_eventList.GetColumnCount());

    for (size_t i = 0; i < m_eventList.GetColumnCount(); ++i)
    {
        DxuiPopupMenu::Item  item;
        item.label   = m_eventList.GetColumnAt (i).title;
        item.checked = m_eventList.IsColumnVisible (i);
        items.push_back (std::move (item));
    }

    m_columnMenu.Show (anchorX, anchorY, std::move (items), m_text, host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
//  ClearAllWidgetFocus
//
//  Drops focus on every focusable widget. SetFocusIndex calls this
//  before re-applying focus to the new owner.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ClearAllWidgetFocus()
{
    m_pauseButton.SetFocused      (false);
    m_clearButton.SetFocused      (false);
    for (auto & cb : m_eventChecks)     { cb.SetFocused (false); }
    m_audioMasterCheck.SetFocused (false);
    for (auto & cb : m_audioSubChecks)  { cb.SetFocused (false); }
    m_rawQtCheck.SetFocused       (false);
    m_driveRadio.SetFocused       (false);
    m_trackEdit.SetFocused        (false);
    m_sectorEdit.SetFocused       (false);
    m_eventList.SetListFocused          (false);
    m_eventList.SetFocusedHeaderColumn  (-1);
    m_eventList.SetFocusedDividerColumn (-1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DynamicStopCount / TotalStopCount
//
//  The dynamic-stop count is 2 * VisibleColumnCount (one header stop +
//  one right-divider stop per visible column), where the last divider
//  slot is repurposed as the list-body stop (i.e. the last visible
//  column has no divider; the very last dynamic stop is the list).
//  So total dynamic stops = 2N for N visible columns (N headers,
//  N-1 dividers, 1 list).
//
////////////////////////////////////////////////////////////////////////////////

int Disk2DebugPanel::DynamicStopCount () const
{
    return 2 * m_eventList.GetVisibleColumnCount();
}

int Disk2DebugPanel::TotalStopCount () const
{
    return 19 + DynamicStopCount();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFocusIndex
//
//  Single source of truth for which widget owns the keyboard. The
//  index space (Z-pattern, top-to-bottom / left-to-right):
//
//      0..7       m_eventChecks[0..7]
//      8          m_audioMasterCheck
//      9..12      m_audioSubChecks[0..3]
//      13         m_driveRadio
//      14         m_trackEdit
//      15         m_sectorEdit
//      16         m_rawQtCheck
//      17         m_pauseButton
//      18         m_clearButton
//      19..       per-visible-column header / divider stops, then list
//
//  Out-of-range values (including -1) clear all focus.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SetFocusIndex (int index)
{
    int  total = TotalStopCount();


    ClearAllWidgetFocus();

    if (index < 0 || index >= total)
    {
        m_focusIndex = -1;
        return;
    }

    m_focusIndex = index;

    if (index <= 7)
    {
        m_eventChecks[(size_t) index].SetFocused (true);
        return;
    }
    if (index == 8)  { m_audioMasterCheck.SetFocused (true); return; }
    if (index <= 12) { m_audioSubChecks[(size_t) (index - 9)].SetFocused (true); return; }
    if (index == 13) { m_driveRadio.SetFocused  (true); return; }
    if (index == 14) { m_trackEdit.SetFocused   (true); return; }
    if (index == 15) { m_sectorEdit.SetFocused  (true); return; }
    if (index == 16) { m_rawQtCheck.SetFocused  (true); return; }
    if (index == 17) { m_pauseButton.SetFocused (true); return; }
    if (index == 18) { m_clearButton.SetFocused (true); return; }

    // Dynamic per-column stop.
    int  d = index - 19;
    int  N = m_eventList.GetVisibleColumnCount();
    if (N <= 0 || d < 0 || d >= 2 * N)
    {
        m_focusIndex = -1;
        return;
    }

    if (d == 2 * N - 1)
    {
        m_eventList.SetListFocused (true);
        ApplyListSelection();
        return;
    }

    if ((d & 1) == 0)
    {
        int  absCol = m_eventList.GetNthVisibleColumnIndex (d / 2);
        if (absCol >= 0) { m_eventList.SetFocusedHeaderColumn (absCol); }
    }
    else
    {
        int  absCol = m_eventList.GetNthVisibleColumnIndex ((d - 1) / 2);
        if (absCol >= 0) { m_eventList.SetFocusedDividerColumn (absCol); }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FocusCycle
//
//  Tab (+1) / Shift+Tab (-1) advance the focus index with wrap-around
//  over the full 0..TotalStopCount()-1 range. From the unfocused
//  state, Tab lands on 0 and Shift+Tab lands on the last stop.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::FocusCycle (int direction)
{
    int  total = TotalStopCount();
    int  next  = 0;


    if (total <= 0) { return; }

    if (m_focusIndex < 0)
    {
        next = (direction >= 0) ? 0 : (total - 1);
    }
    else
    {
        next = m_focusIndex + direction;
        while (next < 0)        { next += total; }
        while (next >= total)   { next -= total; }
    }

    SetFocusIndex (next);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
//  Routes the key to the currently-focused widget only. The Tab key
//  always cycles focus; the column popup, when visible, captures
//  everything else.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnKey (WPARAM vk)
{
    bool  shiftDown = (GetKeyState (VK_SHIFT) & 0x8000) != 0;


    if (m_columnMenu.IsVisible())  { return m_columnMenu.OnKey (vk); }

    if (vk == VK_TAB)
    {
        FocusCycle (shiftDown ? -1 : 1);
        return true;
    }

    if (m_focusIndex < 0) { return false; }

    if (m_focusIndex >= 19)
    {
        int  d = m_focusIndex - 19;
        int  N = m_eventList.GetVisibleColumnCount();
        if (N <= 0 || d < 0 || d >= 2 * N) { return false; }

        if (d == 2 * N - 1)
        {
            int  rowCount = m_eventList.GetRowCount();
            int  cur      = m_eventList.GetSelectedRow();
            int  cap      = m_eventList.GetVisibleRowCapacity();
            int  next     = cur;

            if (rowCount <= 0) { return false; }

            switch (vk)
            {
                case VK_UP:    next = (cur <= 0) ? 0 : (cur - 1); break;
                case VK_DOWN:  next = (cur < 0) ? 0 : ((cur + 1 >= rowCount) ? (rowCount - 1) : (cur + 1)); break;
                case VK_HOME:  next = 0;                          break;
                case VK_END:   next = rowCount - 1;               break;
                case VK_PRIOR: next = (cur < 0) ? 0 : std::max (0, cur - std::max (1, cap)); break;
                case VK_NEXT:  next = (cur < 0) ? 0 : std::min (rowCount - 1, cur + std::max (1, cap)); break;
                default:       return false;
            }
            m_eventList.SetSelectedRow (next);
            OnListSelectionMoved();
            return true;
        }

        if ((d & 1) == 0)
        {
            if (vk == VK_SPACE || vk == VK_RETURN)
            {
                OnHeaderSortKey();
                return true;
            }
            return false;
        }

        if (vk == VK_LEFT)  { OnDividerResizeKey (-1); return true; }
        if (vk == VK_RIGHT) { OnDividerResizeKey (+1); return true; }
        return false;
    }

    if (m_focusIndex <= 7) { return m_eventChecks[(size_t) m_focusIndex].OnKey (vk); }
    switch (m_focusIndex)
    {
        case 8:  return m_audioMasterCheck.OnKey (vk);
        case 13: return m_driveRadio.OnKey       (vk);
        case 14: return m_trackEdit.OnKey        (vk);
        case 15: return m_sectorEdit.OnKey       (vk);
        case 16: return m_rawQtCheck.OnKey       (vk);
        case 17: return m_pauseButton.OnKey      (vk);
        case 18: return m_clearButton.OnKey      (vk);
        default: break;
    }
    if (m_focusIndex >= 9 && m_focusIndex <= 12)
    {
        return m_audioSubChecks[(size_t) (m_focusIndex - 9)].OnKey (vk);
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyListSelection
//
//  Resolves m_listSelectedEventIndex (an absolute index into m_events)
//  against the current m_filteredIndices and pushes the corresponding
//  visible-row index into the DxuiListView. If the previously-selected
//  event is no longer visible under the current filter, snap to the
//  previous still-visible row (or the next one if there is no
//  previous). If neither exists, clear the selection.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ApplyListSelection()
{
    if (m_listSelectedEventIndex < 0 || m_filteredIndices.empty())
    {
        m_listSelectedEventIndex = -1;
        m_eventList.SetSelectedRow (-1);
        return;
    }

    size_t  target = (size_t) m_listSelectedEventIndex;
    auto    it     = std::lower_bound (m_filteredIndices.begin(),
                                       m_filteredIndices.end(),
                                       target);

    if (it != m_filteredIndices.end() && *it == target)
    {
        m_eventList.SetSelectedRow ((int) (it - m_filteredIndices.begin()));
        return;
    }

    if (it != m_filteredIndices.begin())
    {
        auto prev = it - 1;
        m_listSelectedEventIndex = (int) *prev;
        m_eventList.SetSelectedRow ((int) (prev - m_filteredIndices.begin()));
        return;
    }

    if (it != m_filteredIndices.end())
    {
        m_listSelectedEventIndex = (int) *it;
        m_eventList.SetSelectedRow ((int) (it - m_filteredIndices.begin()));
        return;
    }

    m_listSelectedEventIndex = -1;
    m_eventList.SetSelectedRow (-1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnListSelectionMoved
//
//  Mirrors the DxuiListView's new selected-row index back into our
//  persistent event-index identity so it survives filter/sort
//  rebuilds.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnListSelectionMoved()
{
    int  row = m_eventList.GetSelectedRow();


    if (row < 0 || (size_t) row >= m_filteredIndices.size())
    {
        m_listSelectedEventIndex = -1;
        return;
    }
    m_listSelectedEventIndex = (int) m_filteredIndices[(size_t) row];
}





////////////////////////////////////////////////////////////////////////////////
//
//  SortByColumn
//
//  Toggles descending when the same column is re-sorted; otherwise
//  switches to ascending sort on the new column. Rebuilds rows and
//  preserves selection.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::SortByColumn (int absCol)
{
    if (absCol == m_sortColumn)
    {
        m_sortDescending = !m_sortDescending;
    }
    else
    {
        m_sortColumn     = absCol;
        m_sortDescending = false;
    }
    m_eventList.SetSortIndicator (m_sortColumn, m_sortDescending);
    RebuildFilteredIndices();
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHeaderSortKey
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnHeaderSortKey()
{
    if (m_focusIndex < 19) { return; }
    int  d      = m_focusIndex - 19;
    int  absCol = m_eventList.GetNthVisibleColumnIndex (d / 2);
    if (absCol < 0) { return; }
    SortByColumn (absCol);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnDividerResizeKey
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnDividerResizeKey (int direction)
{
    if (m_focusIndex < 19) { return; }
    int  d      = m_focusIndex - 19;
    int  absCol = m_eventList.GetNthVisibleColumnIndex ((d - 1) / 2);
    int  stepPx = MulDiv (8,  (int) m_dpi, 96);
    int  minPx  = MulDiv (24, (int) m_dpi, 96);
    if (absCol < 0) { return; }

    int  curPx  = m_eventList.GetColumnEffectiveWidthPx ((size_t) absCol);
    int  newPx  = curPx + direction * stepPx;
    if (newPx < minPx) { newPx = minPx; }
    m_eventList.SetColumnOverrideWidthPx ((size_t) absCol, newPx);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::OnChar (wchar_t ch)
{
    if (m_trackEdit.OnChar  (ch)) { return true; }
    if (m_sectorEdit.OnChar (ch)) { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Accept
//
//  The panel is non-modal and has no commit semantics, so Enter is a
//  no-op (matches legacy Disk2DebugDialog behaviour).
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Accept()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Esc / WM_CLOSE / titlebar close all hide the panel rather than
//  destroying it, matching the legacy dialog: re-opening keeps the
//  filter state and the event ring populated.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Cancel()
{
    Hide();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsContentActive
//
//  Always true while the host is up -- Cancel hides the window without
//  asking the shell to destroy it, so the chrome shell must not tear
//  down the HWND on Cancel.
//
////////////////////////////////////////////////////////////////////////////////

bool Disk2DebugPanel::IsContentActive() const
{
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EnsureSwapChain
//
//  Creates a flip-sequential swap chain on the host HWND if one is
//  not already attached. Uses straight-HWND swap chain (no DComp) --
//  the panel has no transparency / overlap requirements that would
//  need composition.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::EnsureSwapChain()
{
    HRESULT                hr           = S_OK;
    ComPtr<IDXGIDevice>    dxgiDevice;
    ComPtr<IDXGIAdapter>   dxgiAdapter;
    ComPtr<IDXGIFactory2>  dxgiFactory;
    DXGI_SWAP_CHAIN_DESC1  desc         = {};



    BAIL_OUT_IF (m_swapChain != nullptr, S_OK);

    CBRA (m_device);
    CBRA (m_hwnd);

    hr = m_device->QueryInterface (IID_PPV_ARGS (&dxgiDevice));
    CHRA (hr);

    hr = dxgiDevice->GetAdapter (&dxgiAdapter);
    CHRA (hr);

    hr = dxgiAdapter->GetParent (IID_PPV_ARGS (&dxgiFactory));
    CHRA (hr);

    desc.Width            = (UINT) m_widthPx;
    desc.Height           = (UINT) m_heightPx;
    desc.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.Stereo           = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount      = s_kSwapBufferCount;
    desc.Scaling          = DXGI_SCALING_STRETCH;
    desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    desc.AlphaMode        = DXGI_ALPHA_MODE_IGNORE;
    desc.Flags            = 0;

    hr = dxgiFactory->CreateSwapChainForHwnd (m_device,
                                              m_hwnd,
                                              &desc,
                                              nullptr,
                                              nullptr,
                                              &m_swapChain);
    CHRA (hr);

    hr = dxgiFactory->MakeWindowAssociation (m_hwnd, DXGI_MWA_NO_ALT_ENTER);
    CHRA (hr);

    hr = CreateBackBufferRtv();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateBackBufferRtv
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Disk2DebugPanel::CreateBackBufferRtv()
{
    HRESULT                       hr         = S_OK;
    ComPtr<ID3D11Texture2D>       backBuffer;



    CBRA (m_swapChain);
    CBRA (m_device);

    hr = m_swapChain->GetBuffer (0, IID_PPV_ARGS (&backBuffer));
    CHRA (hr);

    hr = m_device->CreateRenderTargetView (backBuffer.Get(), nullptr, &m_rtv);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseRenderTargets
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ReleaseRenderTargets()
{
    if (m_context != nullptr)
    {
        ID3D11RenderTargetView *  nullRtv = nullptr;
        m_context->OMSetRenderTargets (1, &nullRtv, nullptr);
    }
    m_rtv.Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RecomputeLayout
//
//  Recomputes the cached PanelLayoutSlots whenever the panel's client
//  size or DPI changes. Slots are positioned below the chrome title bar
//  so they don't overlap it. Once slot rectangles are known, label
//  widgets are re-anchored to the appropriate slots.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RecomputeLayout()
{
    int  titleHeight = 0;


    if (m_titleBar != nullptr)
    {
        titleHeight = m_titleBar->GetTitleHeight();
    }

    m_layout = ComputeDisk2DebugPanelLayout (m_widthPx, m_heightPx, titleHeight, m_dpi);

    LayoutWidgets();
}





////////////////////////////////////////////////////////////////////////////////
//
//  LayoutWidgets
//
//  Reapplies rect / DPI / theme color to every widget owned by the
//  panel. Called whenever the layout slots or theme change.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::LayoutWidgets()
{
    uint32_t  textArgb     = 0xFFE8EEF4;
    uint32_t  invalidArgb  = 0xFFFF6666;


    if (m_theme != nullptr)
    {
        textArgb = m_theme->navItemTextArgb;
    }

    m_trackFilterLabel.SetText        (m_filter.trackFilterRawQt ? s_kpszTrackQtFilterLabel : s_kpszTrackFilterLabel);
    m_trackFilterLabel.SetRect        (m_layout.trackFilterLabel);
    m_trackFilterLabel.SetDpi         (m_dpi);
    m_trackFilterLabel.SetFontSizeDip (s_kLabelFontDip);
    m_trackFilterLabel.SetColorArgb   (textArgb);
    m_trackFilterLabel.SetHAlign      (DxuiTextRenderer::HAlign::Right);
    m_trackFilterLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_sectorFilterLabel.SetText        (s_kpszSectorFilterLabel);
    m_sectorFilterLabel.SetRect        (m_layout.sectorFilterLabel);
    m_sectorFilterLabel.SetDpi         (m_dpi);
    m_sectorFilterLabel.SetFontSizeDip (s_kLabelFontDip);
    m_sectorFilterLabel.SetColorArgb   (textArgb);
    m_sectorFilterLabel.SetHAlign      (DxuiTextRenderer::HAlign::Right);
    m_sectorFilterLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_trackInvalidLabel.SetText        (BuildInvalidLabel (s_kpszTrackInvalidPrefix, m_trackEdit.Text(), m_filter.trackFilter.RejectedSpans()).c_str());
    m_trackInvalidLabel.SetRect        (m_layout.trackInvalidLabel);
    m_trackInvalidLabel.SetDpi         (m_dpi);
    m_trackInvalidLabel.SetFontSizeDip (s_kLabelFontDip);
    m_trackInvalidLabel.SetColorArgb   (invalidArgb);
    m_trackInvalidLabel.SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_trackInvalidLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_sectorInvalidLabel.SetText        (BuildInvalidLabel (s_kpszSectorInvalidPrefix, m_sectorEdit.Text(), m_filter.sectorFilter.RejectedSpans()).c_str());
    m_sectorInvalidLabel.SetRect        (m_layout.sectorInvalidLabel);
    m_sectorInvalidLabel.SetDpi         (m_dpi);
    m_sectorInvalidLabel.SetFontSizeDip (s_kLabelFontDip);
    m_sectorInvalidLabel.SetColorArgb   (invalidArgb);
    m_sectorInvalidLabel.SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_sectorInvalidLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        m_eventChecks[i].SetRect (m_layout.eventTypeChecks[i]);
        m_eventChecks[i].SetDpi  (m_dpi);
    }

    m_audioMasterCheck.SetRect (m_layout.audioMasterCheck);
    m_audioMasterCheck.SetDpi  (m_dpi);

    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        m_audioSubChecks[i].SetRect (m_layout.audioSubChecks[i]);
        m_audioSubChecks[i].SetDpi  (m_dpi);
    }

    m_rawQtCheck.SetRect (m_layout.rawQtCheck);
    m_rawQtCheck.SetDpi  (m_dpi);

    m_driveFilterLabel.SetText        (s_kpszDriveFilterLabel);
    m_driveFilterLabel.SetRect        (m_layout.driveFilterLabel);
    m_driveFilterLabel.SetDpi         (m_dpi);
    m_driveFilterLabel.SetFontSizeDip (s_kLabelFontDip);
    m_driveFilterLabel.SetColorArgb   (textArgb);
    m_driveFilterLabel.SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_driveFilterLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_diskEventsLabel.SetText        (s_kpszDiskEventsLabel);
    m_diskEventsLabel.SetRect        (m_layout.diskEventsLabel);
    m_diskEventsLabel.SetDpi         (m_dpi);
    m_diskEventsLabel.SetFontSizeDip (s_kLabelFontDip);
    m_diskEventsLabel.SetColorArgb   (textArgb);
    m_diskEventsLabel.SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_diskEventsLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    m_audioEventsLabel.SetText        (s_kpszAudioEventsLabel);
    m_audioEventsLabel.SetRect        (m_layout.audioEventsLabel);
    m_audioEventsLabel.SetDpi         (m_dpi);
    m_audioEventsLabel.SetFontSizeDip (s_kLabelFontDip);
    m_audioEventsLabel.SetColorArgb   (textArgb);
    m_audioEventsLabel.SetHAlign      (DxuiTextRenderer::HAlign::Left);
    m_audioEventsLabel.SetVAlign      (DxuiTextRenderer::VAlign::Center);

    // DxuiRadioGroup expects rects in its option records.
    std::vector<DxuiRadioOption>  driveOpts;
    for (int i = 0; i < kDriveRadioCount; i++)
    {
        DxuiRadioOption  opt;
        opt.rect  = m_layout.driveRadios[i];
        opt.label = s_kpszDriveOptionLabels[i];
        driveOpts.push_back (std::move (opt));
    }
    m_driveRadio.SetOptions  (std::move (driveOpts));
    m_driveRadio.SetDpi      (m_dpi);
    // Re-apply selection after SetOptions: ConfigureWidgets calls
    // SetSelected before LayoutWidgets has supplied any options, which
    // makes the initial SetSelected a no-op (out-of-range clamps to -1).
    m_driveRadio.SetSelected (m_filter.driveFilter);

    m_trackEdit.SetRect  (m_layout.trackEdit);
    m_trackEdit.SetDpi   (m_dpi);
    m_trackEdit.SetTheme (m_theme);
    m_trackEdit.SetHwnd  (m_hwnd);

    m_sectorEdit.SetRect  (m_layout.sectorEdit);
    m_sectorEdit.SetDpi   (m_dpi);
    m_sectorEdit.SetTheme (m_theme);
    m_sectorEdit.SetHwnd  (m_hwnd);

    m_pauseButton.Layout (m_layout.pauseButton);
    m_pauseButton.SetDpi (m_dpi);
    m_clearButton.Layout (m_layout.clearButton);
    m_clearButton.SetDpi (m_dpi);

    m_eventList.SetRect  (m_layout.listView);
    m_eventList.SetDpi   (m_dpi);
    m_eventList.SetTheme (m_theme);

    m_columnMenu.SetDpi   (m_dpi);
    m_columnMenu.SetTheme (m_theme);

    m_tooltip.SetDpi      (m_dpi);
    m_tooltip.SetViewportSize (m_widthPx, m_heightPx);
}


////////////////////////////////////////////////////////////////////////////////
//
//  ConfigureWidgets
//
//  Wires labels, initial state, and change callbacks onto every widget.
//  Called once after device init; layout (rect / DPI) is reapplied per
//  resize / theme change via LayoutWidgets.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ConfigureWidgets()
{
    static const std::array<uint32_t, kEventTypeCheckCount> s_kCheckBits =
    {
        FilterState::kEventCatMotor,    FilterState::kEventCatHeadStep,
        FilterState::kEventCatHeadBump, FilterState::kEventCatAddrMark,
        FilterState::kEventCatRead,     FilterState::kEventCatWrite,
        FilterState::kEventCatDoor,     FilterState::kEventCatDriveSelect,
    };


    for (int i = 0; i < kEventTypeCheckCount; i++)
    {
        m_eventChecks[i].SetLabel    (s_kpszEventCheckLabels[i]);
        m_eventChecks[i].SetChecked  ((m_filter.eventTypeMask & s_kCheckBits[i]) != 0);
        uint32_t  bit = s_kCheckBits[i];
        m_eventChecks[i].SetOnChange ([this, bit] (bool checked)
        {
            if (checked) { m_filter.eventTypeMask |=  bit; }
            else         { m_filter.eventTypeMask &= ~bit; }
            OnFilterChanged();
        });
    }

    m_audioMasterCheck.SetLabel    (s_kpszAudioLabel);
    m_audioMasterCheck.SetChecked  (m_filter.audioMaster);
    m_audioMasterCheck.SetOnChange ([this] (bool checked)
    {
        m_filter.audioMaster = checked;
        for (auto & cb : m_audioSubChecks) { cb.SetEnabled (checked); }
        OnFilterChanged();
    });

    bool * const  s_kAudioSubBackers[kAudioSubCheckCount] =
    {
        &m_filter.audioStarted, &m_filter.audioRestarted,
        &m_filter.audioContinued, &m_filter.audioSilent,
    };

    for (int i = 0; i < kAudioSubCheckCount; i++)
    {
        m_audioSubChecks[i].SetLabel    (s_kpszAudioSubLabels[i]);
        m_audioSubChecks[i].SetChecked  (*s_kAudioSubBackers[i]);
        m_audioSubChecks[i].SetEnabled  (m_filter.audioMaster);
        bool * backer = s_kAudioSubBackers[i];
        m_audioSubChecks[i].SetOnChange ([this, backer] (bool checked)
        {
            *backer = checked;
            OnFilterChanged();
        });
    }

    m_rawQtCheck.SetLabel    (s_kpszRawQtLabel);
    m_rawQtCheck.SetChecked  (m_filter.trackFilterRawQt);
    m_rawQtCheck.SetOnChange ([this] (bool checked)
    {
        m_filter.trackFilterRawQt = checked;
        OnTrackEditChanged();
        OnFilterChanged();
        LayoutWidgets();
    });

    m_driveRadio.SetSelected (m_filter.driveFilter);
    m_driveRadio.SetOnChange ([this] (int newIndex)
    {
        m_filter.driveFilter = newIndex;
        OnFilterChanged();
    });

    m_trackEdit.SetMaxLength  (32);
    m_trackEdit.SetOnChange   ([this] (const std::wstring &) { OnTrackEditChanged(); OnFilterChanged(); });

    m_sectorEdit.SetMaxLength (32);
    m_sectorEdit.SetOnChange  ([this] (const std::wstring &) { OnSectorEditChanged(); OnFilterChanged(); });

    m_pauseButton.SetLabel (s_kpszPauseLabel);
    m_pauseButton.SetClick ([this] ()
    {
        m_paused = !m_paused;
        UpdatePauseLabel();
    });

    m_clearButton.SetLabel (s_kpszClearLabel);
    m_clearButton.SetClick ([this] () { ClearEvents(); });

    std::vector<DxuiListView::Column>  cols;
    cols.push_back ({ L"Time",   0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Uptime", 0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Cycle",  0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Drive",  0, false, DxuiTextRenderer::HAlign::Right });
    cols.push_back ({ L"Event",  0, false, DxuiTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Detail", 0, true,  DxuiTextRenderer::HAlign::Left  });
    m_eventList.SetColumns    (std::move (cols));
    m_eventList.SetShowHeader (true);

    m_columnMenu.SetOnSelect ([this] (int index)
    {
        if (index < 0 || index >= (int) m_eventList.GetColumnCount()) { return; }
        m_eventList.SetColumnVisible ((size_t) index, !m_eventList.IsColumnVisible ((size_t) index));
        LayoutWidgets();
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrainAndProject
//
//  Per-frame pull: drain the ring into the deque (with dropped-count
//  synthetic EventsLost), rebuild filtered index list, push visible
//  rows into the list view. Pause skips the drain so producer events
//  continue accumulating but the display freezes.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::DrainAndProject()
{
    uint32_t  dropped = 0;
    int64_t   ticks   = 0;


    if (m_resetAnchorPending.exchange (false, std::memory_order_acq_rel))
    {
        // A reset (Ctrl+R / power-cycle) was requested from the CPU
        // thread. Apply the staged Uptime anchor and clear the event
        // list HERE, on the render thread, so m_events, m_filteredIndices
        // and the DxuiListView rows are only ever touched by one thread.
        ticks = m_pendingAnchorTicks.load (std::memory_order_acquire);

        m_uptimeAnchor = std::chrono::steady_clock::time_point (std::chrono::steady_clock::duration (ticks));
        ClearEvents();
    }

    if (m_paused)
    {
        return;
    }

    dropped = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);
    DebugDialogProjection::DrainAndProject (m_ring, m_events, dropped, m_uptimeAnchor);

    RebuildFilteredIndices();
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RebuildFilteredIndices()
{
    m_filteredIndices.clear();
    m_filteredIndices.reserve (m_events.size());

    for (size_t i = 0; i < m_events.size(); i++)
    {
        if (MatchesFilter (m_events[i], m_filter))
        {
            m_filteredIndices.push_back (i);
        }
    }

    if (m_sortColumn < 0)
    {
        return;
    }

    const std::deque<Disk2EventDisplay> &  events = m_events;
    int                                     col    = m_sortColumn;
    bool                                    desc   = m_sortDescending;

    auto cmpStr = [] (const wchar_t * a, const wchar_t * b) -> int
    {
        return wcscmp (a, b);
    };

    auto cmpCycle = [] (const wchar_t * a, const wchar_t * b) -> int
    {
        // Cycle strings carry thousands separators and no leading zeros,
        // so length-then-lex is equivalent to numeric ordering.
        size_t  la = wcslen (a);
        size_t  lb = wcslen (b);
        if (la != lb) { return (la < lb) ? -1 : 1; }
        return wcscmp (a, b);
    };

    std::stable_sort (m_filteredIndices.begin(),
                      m_filteredIndices.end(),
                      [&] (size_t ia, size_t ib) -> bool
    {
        const Disk2EventDisplay &  ea = events[ia];
        const Disk2EventDisplay &  eb = events[ib];
        int                         c  = 0;

        switch (col)
        {
            case 0: c = cmpStr   (ea.wallStr.data(),   eb.wallStr.data());   break;
            case 1: c = cmpStr   (ea.uptimeStr.data(), eb.uptimeStr.data()); break;
            case 2: c = cmpCycle (ea.cycleStr.data(),  eb.cycleStr.data());  break;
            case 3:
                if (ea.drive != eb.drive) { c = (ea.drive < eb.drive) ? -1 : 1; }
                break;
            case 4:
            {
                std::wstring_view  la = DebugDialogProjection::EventLabel (ea.category, ea.type);
                std::wstring_view  lb = DebugDialogProjection::EventLabel (eb.category, eb.type);
                c = la.compare (lb);
                break;
            }
            case 5: c = ea.detail.compare (eb.detail); break;
            default: break;
        }

        if (c == 0)
        {
            // Fall back to chronological (insertion) order so equal
            // keys keep a stable, predictable arrangement.
            return ia < ib;
        }
        return desc ? (c > 0) : (c < 0);
    });
}





////////////////////////////////////////////////////////////////////////////////
//
//  PushListViewRows
//
//  Manual virtualization: only push the rows that fit visibly within
//  the DxuiListView slot. Walking from the tail keeps the most recent
//  events visible, matching the legacy auto-tail behavior.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::PushListViewRows()
{
    size_t  total = m_filteredIndices.size();
    size_t  cap   = m_events.size();
    std::vector<std::vector<DxuiListView::Cell>>  rows;


    rows.reserve (total);

    for (size_t k = 0; k < total; k++)
    {
        size_t  idx = m_filteredIndices[k];
        if (idx >= cap) { continue; }
        const Disk2EventDisplay & e = m_events[idx];

        std::vector<DxuiListView::Cell>  row;
        row.push_back ({ std::wstring (e.wallStr.data()),   false });
        row.push_back ({ std::wstring (e.uptimeStr.data()), false });
        row.push_back ({ std::wstring (e.cycleStr.data()),  false });

        wchar_t  driveBuf[8] = {};
        if (e.drive == Disk2EventDisplay::kFieldNotApplicable)
        {
            row.push_back ({ L"", false });
        }
        else
        {
            swprintf_s (driveBuf, L"%d", e.drive + 1);
            row.push_back ({ std::wstring (driveBuf), false });
        }

        std::wstring_view  label = DebugDialogProjection::EventLabel (e.category, e.type);
        row.push_back ({ std::wstring (label), false });
        row.push_back ({ e.detail, false });

        rows.push_back (std::move (row));
    }

    m_eventList.SetRows (std::move (rows));
    m_eventList.UpdateAutoFitFromRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::PublishToRing (const Disk2Event & e)
{
    Disk2Event  stamped = e;

    if (m_cycleCounter != nullptr)
    {
        stamped.cycle = *m_cycleCounter;
    }

    if (!m_ring.TryPush (stamped))
    {
        m_droppedSinceLastDrain.fetch_add (1, std::memory_order_relaxed);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MakeStampedEvent
//
////////////////////////////////////////////////////////////////////////////////

Disk2Event Disk2DebugPanel::MakeStampedEvent (EventCategory cat, Disk2EventType type) const noexcept
{
    Disk2Event  e = {};

    e.category = cat;
    e.type     = type;
    e.drive    = (int8_t) m_currentDrive;

    return e;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnFilterChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnFilterChanged()
{
    RebuildFilteredIndices();
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnTrackEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnTrackEditChanged()
{
    m_filter.trackFilter = TrackSectorPredicate::Parse (m_trackEdit.Text(),
                                                        TrackSectorPredicate::Mode::Track,
                                                        m_filter.trackFilterRawQt);
    m_trackEditValid = m_filter.trackFilter.RejectedSpans().empty();
    m_trackInvalidLabel.SetText (BuildInvalidLabel (s_kpszTrackInvalidPrefix, m_trackEdit.Text(), m_filter.trackFilter.RejectedSpans()).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSectorEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnSectorEditChanged()
{
    m_filter.sectorFilter = TrackSectorPredicate::Parse (m_sectorEdit.Text(),
                                                         TrackSectorPredicate::Mode::Sector);
    m_sectorEditValid = m_filter.sectorFilter.RejectedSpans().empty();
    m_sectorInvalidLabel.SetText (BuildInvalidLabel (s_kpszSectorInvalidPrefix, m_sectorEdit.Text(), m_filter.sectorFilter.RejectedSpans()).c_str());
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePauseLabel
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::UpdatePauseLabel()
{
    m_pauseButton.SetLabel (m_paused ? s_kpszResumeLabel : s_kpszPauseLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearEvents
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::ClearEvents()
{
    constexpr uint32_t  kClearDrainBatchSize = 64;
    Disk2Event         scratch[kClearDrainBatchSize] = {};
    uint32_t            drained                       = 0;


    m_droppedSinceLastDrain.store (0, std::memory_order_release);
    do
    {
        drained = m_ring.Drain (scratch, kClearDrainBatchSize);
    }
    while (drained > 0);

    m_events.clear();
    m_filteredIndices.clear();
    m_currentDrive = 0;
    m_listSelectedEventIndex = -1;
    m_eventList.ResetAutoFit();
    PushListViewRows();
    ApplyListSelection();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequestResetAnchor
//
//  Thread-safe reset entry point for the CPU/reset thread. Stages the
//  new Uptime anchor and raises a pending-reset flag; DrainAndProject
//  applies the anchor and clears the event list on the render thread,
//  keeping the event deque and DxuiListView rows single-threaded.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::RequestResetAnchor (std::chrono::steady_clock::time_point anchor) noexcept
{
    m_pendingAnchorTicks.store (anchor.time_since_epoch().count(), std::memory_order_release);
    m_resetAnchorPending.store (true, std::memory_order_release);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDisk2EventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnMotorCommandOn ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOn);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorEngaged ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorEngaged);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorCommandOff ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorCommandOff);
    PublishToRing (e);
}
void Disk2DebugPanel::OnMotorDisengaged ()
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::MotorDisengaged);
    PublishToRing (e);
}

void Disk2DebugPanel::OnHeadStep (int prevQt, int newQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadStep);
    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;
    PublishToRing (e);
}

void Disk2DebugPanel::OnHeadBump (int atQt)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::HeadBump);
    e.payload.bump.atQt = atQt;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAddressMark (int track, int sector, int volume)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::AddrMark);
    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataRead);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DataWrite);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDriveSelect (int drive)
{
    m_currentDrive = drive;
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DriveSelect);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDiskInserted (int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DiskInserted);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

void Disk2DebugPanel::OnDiskEjected (int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Controller, Disk2EventType::DiskEjected);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::OnAudioStarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioRestarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioRestarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioContinued (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioContinued);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioSilent (SoundKind kind, int drive, SilentReason reason)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioSilent);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = reason;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioLoopStarted (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioLoopStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void Disk2DebugPanel::OnAudioLoopStopped (SoundKind kind, int drive)
{
    Disk2Event  e = MakeStampedEvent (EventCategory::Audio, Disk2EventType::AudioLoopStopped);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}




////////////////////////////////////////////////////////////////////////////////
//
//  NowMs
//
//  Wall-clock-ish millisecond stamp for tooltip dwell timing. Uses
//  steady_clock so a system clock adjustment can't make a tooltip
//  hide millennia in the future.
//
////////////////////////////////////////////////////////////////////////////////

int64_t Disk2DebugPanel::NowMs() const
{
    auto  delta = std::chrono::steady_clock::now() - m_uptimeAnchor;
    return std::chrono::duration_cast<std::chrono::milliseconds> (delta).count();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateTooltip
//
//  Walks the filter / drive / edit widgets and shows the appropriate
//  tooltip for whichever the cursor is over. Tooltips dwell-open after
//  ~500ms of stable hover (DxuiTooltip widget enforces it) and hide as soon
//  as the cursor leaves all known targets.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::UpdateTooltip (int x, int y)
{
    int64_t  now = NowMs();

    for (size_t i = 0; i < m_eventChecks.size(); ++i)
    {
        if (m_eventChecks[i].HitTest (x, y))
        {
            m_tooltip.RequestShow (m_eventChecks[i].Rect(), s_kpszEventCheckTips[i], now);
            return;
        }
    }

    if (m_audioMasterCheck.HitTest (x, y))
    {
        m_tooltip.RequestShow (m_audioMasterCheck.Rect(), s_kpszAudioMasterTip, now);
        return;
    }

    for (size_t i = 0; i < m_audioSubChecks.size(); ++i)
    {
        if (m_audioSubChecks[i].HitTest (x, y))
        {
            m_tooltip.RequestShow (m_audioSubChecks[i].Rect(), s_kpszAudioSubTips[i], now);
            return;
        }
    }

    if (m_rawQtCheck.HitTest (x, y))
    {
        m_tooltip.RequestShow (m_rawQtCheck.Rect(), s_kpszRawQtTip, now);
        return;
    }

    int  driveHit = m_driveRadio.HitTest (x, y);
    if (driveHit >= 0 && driveHit < (int) m_driveRadio.Options().size())
    {
        m_tooltip.RequestShow (m_driveRadio.Options()[driveHit].rect,
                               s_kpszDriveRadioTips[driveHit],
                               now);
        return;
    }

    if (m_trackEdit.HitTest (x, y))
    {
        m_tooltip.RequestShow (m_trackEdit.Rect(), s_kpszTrackEditTip, now);
        return;
    }

    if (m_sectorEdit.HitTest (x, y))
    {
        m_tooltip.RequestShow (m_sectorEdit.Rect(), s_kpszSectorEditTip, now);
        return;
    }

    m_tooltip.RequestHide (now);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Layout (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Layout(RECT, scaler) for the
//  IDxuiControl tree. The chrome shell drives this panel's bespoke
//  RecomputeLayout / LayoutWidgets pipeline directly via
//  OnHostResize, so the adapter is intentionally a no-op. It exists
//  so an IDxuiControl-tree walk targeting the panel does not abort
//  on the pure virtual.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    UNREFERENCED_PARAMETER (boundsDip);
    UNREFERENCED_PARAMETER (scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Paint (IDxuiControl adapter)
//
//  Bridges DxuiPanel's pure-virtual Paint(IDxuiPainter, ...). The
//  chrome shell drives this panel's bespoke Render via the
//  IChromedPanelContent path, which composes against its own owned
//  m_painter / m_text. The adapter is a no-op for the same reason
//  Layout above is: the unified Dxui dispatch path does not yet
//  reach the chrome-hosted panel, so this hook stays inert.
//
////////////////////////////////////////////////////////////////////////////////

void Disk2DebugPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    UNREFERENCED_PARAMETER (painter);
    UNREFERENCED_PARAMETER (text);
    UNREFERENCED_PARAMETER (theme);
}
