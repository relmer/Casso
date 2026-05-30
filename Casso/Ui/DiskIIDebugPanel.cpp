#include "Pch.h"

#include "DiskIIDebugPanel.h"

#include "Chrome/ChromeTheme.h"
#include "Chrome/TitleBar.h"

#include "../DebugDialogProjection.h"


namespace
{
    constexpr LPCWSTR  s_kpszClassName  = L"Casso.DiskIIDebug.Panel";
    constexpr LPCWSTR  s_kpszWindowTitle = L"Disk II Debug";

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
    constexpr LPCWSTR  s_kpszAudioLabel    = L"Audio";
    constexpr LPCWSTR  s_kpszInvalidLabel  = L"Invalid";

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
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugPanel::DiskIIDebugPanel()
{
    m_uptimeAnchor = std::chrono::steady_clock::now();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~DiskIIDebugPanel
//
////////////////////////////////////////////////////////////////////////////////

DiskIIDebugPanel::~DiskIIDebugPanel()
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

HRESULT DiskIIDebugPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const ChromeTheme    * theme)
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

void DiskIIDebugPanel::Show()
{
    HWND  hwnd = m_window.Hwnd();


    if (hwnd != nullptr)
    {
        ShowWindow (hwnd, SW_SHOW);
        SetForegroundWindow (hwnd);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Hide
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Hide()
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

void DiskIIDebugPanel::Destroy()
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

HRESULT DiskIIDebugPanel::RenderFrame()
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

HRESULT DiskIIDebugPanel::Render()
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

    hr = m_text.BeginDraw();
    CHRA (hr);

    visual.dpi = m_dpi;
    if (m_titleBar != nullptr && m_theme != nullptr)
    {
        m_titleBar->Paint (m_painter, m_text, visual, *m_theme);
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
    m_tooltip.Paint (m_painter, m_text);

    m_columnMenu.Paint (m_painter, m_text);

    hr = m_painter.End (m_rtv.Get());
    CHRA (hr);

    hr = m_text.EndDraw();
    CHRA (hr);

    hr = m_swapChain->Present (1, 0);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::SetTheme (const ChromeTheme * theme)
{
    m_window.SetTheme (theme);
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowClassName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskIIDebugPanel::GetWindowClassName() const
{
    return s_kpszClassName;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetWindowTitle
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR DiskIIDebugPanel::GetWindowTitle() const
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

HRESULT DiskIIDebugPanel::OnHostCreated (
    HWND                   hwnd,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    int                    widthPx,
    int                    heightPx,
    UINT                   dpi,
    TitleBar             * titleBar,
    const ChromeTheme    * theme)
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

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnHostDestroyed
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnHostDestroyed()
{
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

HRESULT DiskIIDebugPanel::OnHostResize (int widthPx, int heightPx, UINT dpi)
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

void DiskIIDebugPanel::SetChromeTheme (TitleBar * titleBar, const ChromeTheme * theme)
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

SIZE DiskIIDebugPanel::PreferredClientSize (UINT dpi) const
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

void DiskIIDebugPanel::OnLButtonDown (int x, int y)
{
    bool  handled = false;


    if (m_columnMenu.IsVisible())
    {
        if (m_columnMenu.OnLButtonDown (x, y)) { return; }
    }

    for (auto & cb : m_eventChecks)
    {
        if (cb.OnLButtonDown (x, y)) { handled = true; break; }
    }
    if (!handled) { handled = m_audioMasterCheck.OnLButtonDown (x, y); }
    if (!handled)
    {
        for (auto & cb : m_audioSubChecks)
        {
            if (cb.OnLButtonDown (x, y)) { handled = true; break; }
        }
    }
    if (!handled) { handled = m_rawQtCheck.OnLButtonDown   (x, y); }
    if (!handled) { handled = m_driveRadio.OnLButtonDown   (x, y); }
    if (!handled) { handled = m_trackEdit.OnLButtonDown    (x, y); }
    if (!handled) { handled = m_sectorEdit.OnLButtonDown   (x, y); }

    if (m_pauseButton.HitTest (x, y))
    {
        m_pauseButton.SetMouse (x, y, true);
        handled = true;
    }
    if (!handled && m_clearButton.HitTest (x, y))
    {
        m_clearButton.SetMouse (x, y, true);
        handled = true;
    }

    if (!handled)
    {
        // Click on listview header sorts by that column; first click on
        // a new column sorts ascending, subsequent clicks on the same
        // column flip direction. Click on row is reserved for selection
        // (not yet wired -- HitTestRow still returns the row index for
        // future use).
        int  relX = x - m_layout.listView.left;
        int  relY = y - m_layout.listView.top;
        int  hit  = m_eventList.HitTestRow (relX, relY);
        if (hit < 0)
        {
            int  col = m_eventList.HitTestHeaderColumn (relX, relY);
            if (col >= 0)
            {
                if (col == m_sortColumn)
                {
                    m_sortDescending = !m_sortDescending;
                }
                else
                {
                    m_sortColumn     = col;
                    m_sortDescending = false;
                }
                m_eventList.SetSortIndicator (m_sortColumn, m_sortDescending);
                RebuildFilteredIndices();
                PushListViewRows();
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnLButtonUp
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnLButtonUp (int x, int y)
{
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

void DiskIIDebugPanel::OnMouseMove (int x, int y)
{
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
//  OnRButtonDown
//
//  Right-click inside the list-view header strip surfaces a themed
//  popup menu of column-visibility toggles. Anywhere else, right-click
//  is currently a no-op.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnRButtonDown (int x, int y)
{
    int  relX        = x - m_layout.listView.left;
    int  relY        = y - m_layout.listView.top;
    int  headerH     = m_eventList.HeaderHeightPx();
    int  listWidthPx = m_layout.listView.right - m_layout.listView.left;


    if (!m_eventList.ShowHeader())                                { return; }
    if (relX < 0 || relX >= listWidthPx)                          { return; }
    if (relY < 0 || relY >= headerH)                              { return; }

    ShowColumnMenu (x, y);
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

void DiskIIDebugPanel::ShowColumnMenu (int anchorX, int anchorY)
{
    std::vector<PopupMenu::Item>  items;
    RECT                          host = { 0, 0, m_widthPx, m_heightPx };


    items.reserve (m_eventList.ColumnCount());

    for (size_t i = 0; i < m_eventList.ColumnCount(); ++i)
    {
        PopupMenu::Item  item;
        item.label   = m_eventList.ColumnAt (i).title;
        item.checked = m_eventList.ColumnVisible (i);
        items.push_back (std::move (item));
    }

    m_columnMenu.Show (anchorX, anchorY, std::move (items), m_text, host);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnKey
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugPanel::OnKey (WPARAM vk)
{
    if (m_columnMenu.IsVisible())  { return m_columnMenu.OnKey (vk); }
    if (m_trackEdit.OnKey (vk))    { return true; }
    if (m_sectorEdit.OnKey (vk))   { return true; }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnChar
//
////////////////////////////////////////////////////////////////////////////////

bool DiskIIDebugPanel::OnChar (wchar_t ch)
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
//  no-op (matches legacy DiskIIDebugDialog behaviour).
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::Accept()
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

void DiskIIDebugPanel::Cancel()
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

bool DiskIIDebugPanel::IsContentActive() const
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

HRESULT DiskIIDebugPanel::EnsureSwapChain()
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

HRESULT DiskIIDebugPanel::CreateBackBufferRtv()
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

void DiskIIDebugPanel::ReleaseRenderTargets()
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

void DiskIIDebugPanel::RecomputeLayout()
{
    int  titleHeight = 0;


    if (m_titleBar != nullptr)
    {
        titleHeight = m_titleBar->GetTitleHeight();
    }

    m_layout = ComputeDiskIIDebugPanelLayout (m_widthPx, m_heightPx, titleHeight, m_dpi);

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

void DiskIIDebugPanel::LayoutWidgets()
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
    m_trackFilterLabel.SetHAlign      (DwriteTextRenderer::HAlign::Right);
    m_trackFilterLabel.SetVAlign      (DwriteTextRenderer::VAlign::Center);

    m_sectorFilterLabel.SetText        (s_kpszSectorFilterLabel);
    m_sectorFilterLabel.SetRect        (m_layout.sectorFilterLabel);
    m_sectorFilterLabel.SetDpi         (m_dpi);
    m_sectorFilterLabel.SetFontSizeDip (s_kLabelFontDip);
    m_sectorFilterLabel.SetColorArgb   (textArgb);
    m_sectorFilterLabel.SetHAlign      (DwriteTextRenderer::HAlign::Right);
    m_sectorFilterLabel.SetVAlign      (DwriteTextRenderer::VAlign::Center);

    m_trackInvalidLabel.SetText        (m_trackEditValid  ? L"" : s_kpszInvalidLabel);
    m_trackInvalidLabel.SetRect        (m_layout.trackInvalidLabel);
    m_trackInvalidLabel.SetDpi         (m_dpi);
    m_trackInvalidLabel.SetFontSizeDip (s_kLabelFontDip);
    m_trackInvalidLabel.SetColorArgb   (invalidArgb);
    m_trackInvalidLabel.SetHAlign      (DwriteTextRenderer::HAlign::Left);
    m_trackInvalidLabel.SetVAlign      (DwriteTextRenderer::VAlign::Center);

    m_sectorInvalidLabel.SetText        (m_sectorEditValid ? L"" : s_kpszInvalidLabel);
    m_sectorInvalidLabel.SetRect        (m_layout.sectorInvalidLabel);
    m_sectorInvalidLabel.SetDpi         (m_dpi);
    m_sectorInvalidLabel.SetFontSizeDip (s_kLabelFontDip);
    m_sectorInvalidLabel.SetColorArgb   (invalidArgb);
    m_sectorInvalidLabel.SetHAlign      (DwriteTextRenderer::HAlign::Left);
    m_sectorInvalidLabel.SetVAlign      (DwriteTextRenderer::VAlign::Center);

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

    // RadioGroup expects rects in its option records.
    std::vector<RadioOption>  driveOpts;
    for (int i = 0; i < kDriveRadioCount; i++)
    {
        RadioOption  opt;
        opt.rect  = m_layout.driveRadios[i];
        opt.label = s_kpszDriveOptionLabels[i];
        driveOpts.push_back (std::move (opt));
    }
    m_driveRadio.SetOptions (std::move (driveOpts));
    m_driveRadio.SetDpi     (m_dpi);

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

void DiskIIDebugPanel::ConfigureWidgets()
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

    std::vector<ListView::Column>  cols;
    cols.push_back ({ L"Wall",   kColWallWidth,   false, DwriteTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Uptime", kColUptimeWidth, false, DwriteTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Cycle",  kColCycleWidth,  false, DwriteTextRenderer::HAlign::Right });
    cols.push_back ({ L"Drive",  kColDriveWidth,  false, DwriteTextRenderer::HAlign::Right });
    cols.push_back ({ L"Event",  kColEventWidth,  false, DwriteTextRenderer::HAlign::Left  });
    cols.push_back ({ L"Detail", 0,               true,  DwriteTextRenderer::HAlign::Left  });
    m_eventList.SetColumns    (std::move (cols));
    m_eventList.SetShowHeader (true);

    m_columnMenu.SetOnSelect ([this] (int index)
    {
        if (index < 0 || index >= (int) m_eventList.ColumnCount()) { return; }
        m_eventList.SetColumnVisible ((size_t) index, !m_eventList.ColumnVisible ((size_t) index));
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

void DiskIIDebugPanel::DrainAndProject()
{
    uint32_t  dropped = 0;


    if (m_paused)
    {
        return;
    }

    dropped = m_droppedSinceLastDrain.exchange (0, std::memory_order_acq_rel);
    DebugDialogProjection::DrainAndProject (m_ring, m_events, dropped, m_uptimeAnchor);

    RebuildFilteredIndices();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  RebuildFilteredIndices
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::RebuildFilteredIndices()
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

    const std::deque<DiskIIEventDisplay> &  events = m_events;
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
        const DiskIIEventDisplay &  ea = events[ia];
        const DiskIIEventDisplay &  eb = events[ib];
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
//  the ListView slot. Walking from the tail keeps the most recent
//  events visible, matching the legacy auto-tail behavior.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::PushListViewRows()
{
    int     slotHeight = m_layout.listView.bottom - m_layout.listView.top;
    int     visible    = m_eventList.RequiredRowsForHeightPx (slotHeight);
    size_t  total      = m_filteredIndices.size();
    size_t  start      = (total > (size_t) visible) ? total - (size_t) visible : 0;
    std::vector<std::vector<ListView::Cell>>  rows;


    rows.reserve (total - start);

    for (size_t k = start; k < total; k++)
    {
        const DiskIIEventDisplay & e = m_events[m_filteredIndices[k]];

        std::vector<ListView::Cell>  row;
        row.push_back ({ std::wstring (e.wallStr.data()),   false });
        row.push_back ({ std::wstring (e.uptimeStr.data()), false });
        row.push_back ({ std::wstring (e.cycleStr.data()),  false });

        wchar_t  driveBuf[8] = {};
        if (e.drive == DiskIIEventDisplay::kFieldNotApplicable)
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
}





////////////////////////////////////////////////////////////////////////////////
//
//  PublishToRing
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::PublishToRing (const DiskIIEvent & e)
{
    DiskIIEvent  stamped = e;

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

DiskIIEvent DiskIIDebugPanel::MakeStampedEvent (EventCategory cat, DiskIIEventType type) const noexcept
{
    DiskIIEvent  e = {};

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

void DiskIIDebugPanel::OnFilterChanged()
{
    RebuildFilteredIndices();
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnTrackEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnTrackEditChanged()
{
    m_filter.trackFilter = TrackSectorPredicate::Parse (m_trackEdit.Text(),
                                                        TrackSectorPredicate::Mode::Track,
                                                        m_filter.trackFilterRawQt);
    m_trackEditValid = m_filter.trackFilter.RejectedSpans().empty();
    m_trackInvalidLabel.SetText (m_trackEditValid ? L"" : s_kpszInvalidLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  OnSectorEditChanged
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnSectorEditChanged()
{
    m_filter.sectorFilter = TrackSectorPredicate::Parse (m_sectorEdit.Text(),
                                                         TrackSectorPredicate::Mode::Sector);
    m_sectorEditValid = m_filter.sectorFilter.RejectedSpans().empty();
    m_sectorInvalidLabel.SetText (m_sectorEditValid ? L"" : s_kpszInvalidLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UpdatePauseLabel
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::UpdatePauseLabel()
{
    m_pauseButton.SetLabel (m_paused ? s_kpszResumeLabel : s_kpszPauseLabel);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ClearEvents
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::ClearEvents()
{
    constexpr uint32_t  kClearDrainBatchSize = 64;
    DiskIIEvent         scratch[kClearDrainBatchSize] = {};
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
    PushListViewRows();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDiskIIEventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnMotorCommandOn ()
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::MotorCommandOn);
    PublishToRing (e);
}
void DiskIIDebugPanel::OnMotorEngaged ()
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::MotorEngaged);
    PublishToRing (e);
}
void DiskIIDebugPanel::OnMotorCommandOff ()
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::MotorCommandOff);
    PublishToRing (e);
}
void DiskIIDebugPanel::OnMotorDisengaged ()
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::MotorDisengaged);
    PublishToRing (e);
}

void DiskIIDebugPanel::OnHeadStep (int prevQt, int newQt)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::HeadStep);
    e.payload.step.prevQt = prevQt;
    e.payload.step.newQt  = newQt;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnHeadBump (int atQt)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::HeadBump);
    e.payload.bump.atQt = atQt;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAddressMark (int track, int sector, int volume)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::AddrMark);
    e.payload.addrMark.track  = track;
    e.payload.addrMark.sector = sector;
    e.payload.addrMark.volume = volume;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnDataMarkRead (int track, int sector, int volume, int byteCount)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::DataRead);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnDataMarkWrite (int track, int sector, int volume, int byteCount)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::DataWrite);
    e.payload.dataMark.track     = track;
    e.payload.dataMark.sector    = sector;
    e.payload.dataMark.volume    = volume;
    e.payload.dataMark.byteCount = byteCount;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnDriveSelect (int drive)
{
    m_currentDrive = drive;
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::DriveSelect);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnDiskInserted (int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::DiskInserted);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnDiskEjected (int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Controller, DiskIIEventType::DiskEjected);
    e.drive               = (int8_t) drive;
    e.payload.drive.drive = drive;
    PublishToRing (e);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IDriveAudioEventSink implementations
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::OnAudioStarted (SoundKind kind, int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAudioRestarted (SoundKind kind, int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioRestarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAudioContinued (SoundKind kind, int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioContinued);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAudioSilent (SoundKind kind, int drive, SilentReason reason)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioSilent);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = reason;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAudioLoopStarted (SoundKind kind, int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioLoopStarted);
    e.drive                = (int8_t) drive;
    e.payload.audio.kind   = kind;
    e.payload.audio.drive  = drive;
    e.payload.audio.reason = SilentReason::DriveAudioDisabled;
    PublishToRing (e);
}

void DiskIIDebugPanel::OnAudioLoopStopped (SoundKind kind, int drive)
{
    DiskIIEvent  e = MakeStampedEvent (EventCategory::Audio, DiskIIEventType::AudioLoopStopped);
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

int64_t DiskIIDebugPanel::NowMs() const
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
//  ~500ms of stable hover (Tooltip widget enforces it) and hide as soon
//  as the cursor leaves all known targets.
//
////////////////////////////////////////////////////////////////////////////////

void DiskIIDebugPanel::UpdateTooltip (int x, int y)
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
