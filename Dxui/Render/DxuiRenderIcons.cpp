#include "Pch.h"

#include "Render/DxuiRenderIcons.h"
#include "Render/DxuiSvgPath.h"
#include "DxuiTextRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRenderIcons::DxuiRenderIcons
//
////////////////////////////////////////////////////////////////////////////////

DxuiRenderIcons::DxuiRenderIcons (DxuiTextRenderer & renderer)
    : m_renderer (renderer)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetIconLayerGeometry
//
//  The layer's sub-shapes as one geometry, filled by the winding rule SVG
//  uses, so a ring's inner edge cuts its outer one rather than filling in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::GetIconLayerGeometry (const DxuiVectorIcon & icon, const DxuiVectorIconLayer & layer, ID2D1PathGeometry ** outGeometry)
{
    HRESULT                       hr        = S_OK;
    std::vector<DxuiSvgSubpath>   subpaths;
    ComPtr<ID2D1PathGeometry>     geometry;
    ComPtr<ID2D1GeometrySink>     sink;
    auto                          cached    = m_iconGeometry.find (&layer);
    bool                          parsed    = false;



    if (cached != m_iconGeometry.end())
    {
        *outGeometry = cached->second.Get();
        (*outGeometry)->AddRef();
        return S_OK;
    }

    parsed = DxuiSvgPath::Parse (icon.pathData, subpaths);
    CBRA (parsed);

    hr = m_renderer.m_d2dFactory->CreatePathGeometry (&geometry);
    CHRA (hr);

    hr = geometry->Open (&sink);
    CHRA (hr);

    sink->SetFillMode (D2D1_FILL_MODE_WINDING);

    for (size_t i = 0; i < subpaths.size() && i < 32; i++)
    {
        const DxuiSvgSubpath  & sub = subpaths[i];

        if ((layer.subpaths & (1u << i)) == 0)
        {
            continue;
        }

        sink->BeginFigure (D2D1::Point2F (sub.start.x, sub.start.y), D2D1_FIGURE_BEGIN_FILLED);

        for (const DxuiSvgSegment & seg : sub.segments)
        {
            switch (seg.kind)
            {
                case DxuiSvgSegment::Kind::Cubic:
                    sink->AddBezier (D2D1::BezierSegment (D2D1::Point2F (seg.c1.x, seg.c1.y),
                                                          D2D1::Point2F (seg.c2.x, seg.c2.y),
                                                          D2D1::Point2F (seg.to.x, seg.to.y)));
                    break;

                case DxuiSvgSegment::Kind::Arc:
                    sink->AddArc (D2D1::ArcSegment (D2D1::Point2F (seg.to.x, seg.to.y),
                                                    D2D1::SizeF (seg.radiusX, seg.radiusY),
                                                    seg.rotationDeg,
                                                    seg.clockwise ? D2D1_SWEEP_DIRECTION_CLOCKWISE : D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE,
                                                    seg.largeArc  ? D2D1_ARC_SIZE_LARGE : D2D1_ARC_SIZE_SMALL));
                    break;

                default:
                    sink->AddLine (D2D1::Point2F (seg.to.x, seg.to.y));
                    break;
            }
        }

        sink->EndFigure (sub.closed ? D2D1_FIGURE_END_CLOSED : D2D1_FIGURE_END_OPEN);
    }

    hr = sink->Close();
    CHRA (hr);

    m_iconGeometry[&layer] = geometry;
    *outGeometry = geometry.Detach();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FillVectorIcon
//
//  Each layer in its tone, scaled from the icon's own square to sizeDip at
//  x, y. A layer kept to a band is clipped to it, in the icon's units.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::FillVectorIcon (const DxuiVectorIcon & icon, float xDip, float yDip, float sizeDip, uint32_t foreground, uint32_t accent)
{
    HRESULT                       hr        = S_OK;
    D2D1_MATRIX_3X2_F             saved     = {};
    float                         scale     = 0.0f;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_drawing);
    CBRA (icon.pathData != nullptr && icon.box > 0.0f);

    scale = sizeDip / icon.box;
    m_renderer.m_d2dContext->GetTransform (&saved);
    m_renderer.m_d2dContext->SetTransform (D2D1::Matrix3x2F::Scale (scale, scale) * D2D1::Matrix3x2F::Translation (xDip, yDip) * saved);

    for (size_t i = 0; i < icon.layerCount; i++)
    {
        const DxuiVectorIconLayer     & layer    = icon.layers[i];
        ComPtr<ID2D1PathGeometry>       geometry;
        ComPtr<ID2D1SolidColorBrush>    brush;
        D2D1_COLOR_F                    color    = m_renderer.ColorFromArgb (layer.accent ? accent : foreground);
        bool                            banded   = layer.bandTop != layer.bandBottom;

        hr = GetIconLayerGeometry (icon, layer, &geometry);

        if (FAILED (hr))
        {
            continue;
        }

        color.a *= m_renderer.m_globalAlpha;
        hr = m_renderer.m_d2dContext->CreateSolidColorBrush (color, &brush);

        if (FAILED (hr))
        {
            continue;
        }

        if (banded)
        {
            m_renderer.m_d2dContext->PushAxisAlignedClip (D2D1::RectF (-1.0f, layer.bandTop, icon.box + 1.0f, layer.bandBottom),
                                               D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        }

        m_renderer.m_d2dContext->FillGeometry (geometry.Get(), brush.Get());

        if (banded)
        {
            m_renderer.m_d2dContext->PopAxisAlignedClip();
        }
    }

    m_renderer.m_d2dContext->SetTransform (saved);
    hr = S_OK;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawFramebuffer
//
//  Uploads a BGRA8 CPU pixel buffer into the cached ID2D1Bitmap for its
//  address and draws it into the destination DIP rect with linear
//  interpolation, or nearest-neighbor for a sharp frame. A frame drawn
//  smaller than its pixels is first averaged down to the drawn size, since
//  sampling it makes a dithered picture's stripes beat against the screen's
//  pixels, in color bands that move with every size. The bitmap is created
//  lazily and recreated when the uploaded size changes; otherwise
//  CopyFromMemory rewrites the pixels in place each call. The cache is
//  emptied once it has s_kMaxFramebufferBitmaps buffers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::DrawFramebuffer (
    const uint32_t * srcBgraPixels,
    int              srcWidthPx,
    int              srcHeightPx,
    float            destXDip,
    float            destYDip,
    float            destWidthDip,
    float            destHeightDip,
    bool             sharp)
{
    HRESULT              hr      = S_OK;
    D2D1_RECT_F          dest    = {};
    FramebufferBitmap  * cached  = nullptr;
    float                dpiX    = 96.0f;
    float                dpiY    = 96.0f;
    int                  drawnW  = 0;
    int                  drawnH  = 0;
    const uint32_t     * pixels  = srcBgraPixels;
    int                  width   = srcWidthPx;
    int                  height  = srcHeightPx;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_drawing);
    CBRA (srcBgraPixels);
    CBRA (srcWidthPx > 0 && srcHeightPx > 0);

    m_renderer.m_d2dContext->GetDpi (&dpiX, &dpiY);
    drawnW = std::max (1, (int) std::lround (destWidthDip  * dpiX / 96.0f));
    drawnH = std::max (1, (int) std::lround (destHeightDip * dpiY / 96.0f));

    if (drawnW < srcWidthPx || drawnH < srcHeightPx)
    {
        Shrink (srcBgraPixels, srcWidthPx, srcHeightPx, drawnW, drawnH, m_shrunk, width, height);
        pixels = m_shrunk.data();
    }

    if (m_framebufferBitmaps.size() >= s_kMaxFramebufferBitmaps && !m_framebufferBitmaps.contains (srcBgraPixels))
    {
        m_framebufferBitmaps.clear();
    }

    cached = &m_framebufferBitmaps[srcBgraPixels];

    if (cached->bitmap == nullptr ||
        cached->width  != width ||
        cached->height != height)
    {
        D2D1_BITMAP_PROPERTIES  props = {};

        props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
        props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
        props.dpiX                  = 96.0f;
        props.dpiY                  = 96.0f;

        cached->bitmap.Reset();
        hr = m_renderer.m_d2dContext->CreateBitmap (D2D1::SizeU ((UINT32) width, (UINT32) height),
                                         nullptr, 0, &props, &cached->bitmap);
        CHRA (hr);
        cached->width  = width;
        cached->height = height;
    }

    {
        D2D1_RECT_U  srcRect = D2D1::RectU (0, 0, (UINT32) width, (UINT32) height);

        hr = cached->bitmap->CopyFromMemory (&srcRect, pixels, (UINT32) (width * sizeof (uint32_t)));
        CHRA (hr);
    }

    dest.left   = destXDip;
    dest.top    = destYDip;
    dest.right  = destXDip + destWidthDip;
    dest.bottom = destYDip + destHeightDip;

    m_renderer.m_d2dContext->DrawBitmap (cached->bitmap.Get(),
                              &dest, m_renderer.m_globalAlpha,
                              sharp ? D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR : D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                              nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Shrink
//
//  A box filter, one axis at a time: each pixel is the mean of the source
//  pixels under it, a partly covered one weighted by the part covered.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiRenderIcons::Shrink (
    const uint32_t        * src,
    int                     srcWidth,
    int                     srcHeight,
    int                     width,
    int                     height,
    std::vector<uint32_t> & outPixels,
    int                   & outWidth,
    int                   & outHeight)
{
    std::vector<float>  across;
    double              scaleX = 0.0;
    double              scaleY = 0.0;



    outWidth  = std::clamp (width,  1, srcWidth);
    outHeight = std::clamp (height, 1, srcHeight);
    scaleX    = (double) srcWidth  / outWidth;
    scaleY    = (double) srcHeight / outHeight;

    across.assign ((size_t) outWidth * srcHeight * 3, 0.0f);

    for (int y = 0; y < srcHeight; y++)
    {
        for (int x = 0; x < outWidth; x++)
        {
            double   from = x * scaleX;
            double   to   = from + scaleX;
            float  * sum  = &across[((size_t) y * outWidth + x) * 3];

            for (int j = (int) from; j < srcWidth && j < to; j++)
            {
                uint32_t  pixel  = src[(size_t) y * srcWidth + j];
                float     weight = (float) ((std::min (to, (double) j + 1) - std::max (from, (double) j)) / scaleX);

                sum[0] += weight * (float) (pixel & 0xFF);
                sum[1] += weight * (float) ((pixel >> 8) & 0xFF);
                sum[2] += weight * (float) ((pixel >> 16) & 0xFF);
            }
        }
    }

    outPixels.assign ((size_t) outWidth * outHeight, 0xFF000000u);

    for (int y = 0; y < outHeight; y++)
    {
        double  from = y * scaleY;
        double  to   = from + scaleY;

        for (int x = 0; x < outWidth; x++)
        {
            float  sum[3] = {};

            for (int j = (int) from; j < srcHeight && j < to; j++)
            {
                const float  * row    = &across[((size_t) j * outWidth + x) * 3];
                float          weight = (float) ((std::min (to, (double) j + 1) - std::max (from, (double) j)) / scaleY);

                sum[0] += weight * row[0];
                sum[1] += weight * row[1];
                sum[2] += weight * row[2];
            }

            outPixels[(size_t) y * outWidth + x] = 0xFF000000u
                                                 | (uint32_t) std::lround (std::min (sum[2], 255.0f)) << 16
                                                 | (uint32_t) std::lround (std::min (sum[1], 255.0f)) << 8
                                                 | (uint32_t) std::lround (std::min (sum[0], 255.0f));
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawIconBitmap
//
//  Same upload-and-blit logic as DrawFramebuffer, over a cache of its own so
//  the title-bar app icon and the emulator framebuffer (560x384) don't
//  ping-pong recreating one cache slot every frame. The cache holds one
//  bitmap per distinct picture, because D2D batches a frame's draws: pixels
//  uploaded into a bitmap that has already been drawn change that earlier
//  draw too, which showed every picture of one size as the last one uploaded.
//  Source pixels are interpreted as premultiplied BGRA8 so the alpha channel
//  composites correctly.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::DrawIconBitmap (
    const uint32_t * srcBgraPremul,
    int              srcWidthPx,
    int              srcHeightPx,
    float            destXDip,
    float            destYDip,
    float            destWidthDip,
    float            destHeightDip)
{
    HRESULT        hr     = S_OK;
    uint64_t       key    = 0;
    ID2D1Bitmap *  bitmap = nullptr;
    D2D1_RECT_F    dest   = {};



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_drawing);
    CBRA (srcBgraPremul);
    CBRA (srcWidthPx > 0 && srcHeightPx > 0);

    key = HashPixels (srcBgraPremul, (size_t) srcWidthPx * (size_t) srcHeightPx);

    for (const IconBitmap & cached : m_iconBitmaps)
    {
        if (cached.key == key && cached.width == srcWidthPx && cached.height == srcHeightPx)
        {
            bitmap = cached.bitmap.Get();
            break;
        }
    }

    if (bitmap == nullptr)
    {
        hr = AddIconBitmap (srcBgraPremul, srcWidthPx, srcHeightPx, key, bitmap);
        CHR (hr);
    }

    dest.left   = destXDip;
    dest.top    = destYDip;
    dest.right  = destXDip + destWidthDip;
    dest.bottom = destYDip + destHeightDip;

    m_renderer.m_d2dContext->DrawBitmap (bitmap,
                              &dest, m_renderer.m_globalAlpha,
                              D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
                              nullptr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DrawSvgIcon
//
//  Direct2D lays the document out at a viewport size, so one is kept per
//  icon and size, the oldest going when the cache fills. The document is
//  drawn at the origin, so the transform moves it to its place for the call.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::DrawSvgIcon (const std::string & svg, float xDip, float yDip, float sizeDip)
{
    HRESULT                      hr       = S_OK;
    ComPtr<ID2D1DeviceContext5>  context;
    ComPtr<IStream>              stream;
    ID2D1SvgDocument           * document = nullptr;
    size_t                       key      = std::hash<std::string>{} (svg);
    HGLOBAL                      global   = nullptr;
    void                       * bytes    = nullptr;
    D2D1_MATRIX_3X2_F            saved    = {};
    bool                         layered  = false;
    bool                         drawable = !svg.empty() && sizeDip > 0.0f;



    DXUI_ASSERT_UI_THREAD();

    CBRA (m_renderer.m_d2dContext);
    CBRA (m_renderer.m_drawing);
    CBREx (drawable, E_INVALIDARG);

    hr = m_renderer.m_d2dContext.As (&context);
    BAIL_OUT_IF (FAILED (hr), E_NOTIMPL);

    for (const SvgIcon & cached : m_svgIcons)
    {
        if (cached.key == key && cached.size == sizeDip)
        {
            document = cached.document.Get();
            break;
        }
    }

    if (document == nullptr)
    {
        SvgIcon  added;

        global = GlobalAlloc (GMEM_MOVEABLE, svg.size());
        CWR (global);

        bytes = GlobalLock (global);
        CWR (bytes);

        memcpy (bytes, svg.data(), svg.size());
        GlobalUnlock (global);

        //  The stream owns the memory from here.
        hr = CreateStreamOnHGlobal (global, TRUE, &stream);
        CHR (hr);

        global = nullptr;

        hr = context->CreateSvgDocument (stream.Get(), D2D1::SizeF (sizeDip, sizeDip), &added.document);
        CHR (hr);

        added.key  = key;
        added.size = sizeDip;

        if (m_svgIcons.size() >= kMaxSvgIcons)
        {
            m_svgIcons.erase (m_svgIcons.begin());
        }

        m_svgIcons.push_back (std::move (added));
        document = m_svgIcons.back().document.Get();
    }

    if (m_renderer.m_globalAlpha < 1.0f)
    {
        context->PushLayer (D2D1::LayerParameters1 (D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                                    D2D1::IdentityMatrix(), m_renderer.m_globalAlpha),
                            nullptr);
        layered = true;
    }

    context->GetTransform (&saved);
    context->SetTransform (D2D1::Matrix3x2F::Translation (xDip, yDip) * saved);
    context->DrawSvgDocument (document);
    context->SetTransform (saved);

    if (layered)
    {
        context->PopLayer();
    }

Error:
    if (global != nullptr)
    {
        GlobalFree (global);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddIconBitmap
//
//  The oldest entry goes when the cache is full. D2D holds its own reference
//  to a bitmap it has been given to draw, so dropping ours mid-frame is safe;
//  what a batched frame cannot survive is a bitmap's pixels changing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DxuiRenderIcons::AddIconBitmap (
    const uint32_t  * srcBgraPremul,
    int               srcWidthPx,
    int               srcHeightPx,
    uint64_t          key,
    ID2D1Bitmap    * & outBitmap)
{
    HRESULT                 hr      = S_OK;
    IconBitmap              entry;
    D2D1_BITMAP_PROPERTIES  props   = {};
    D2D1_RECT_U             srcRect = D2D1::RectU (0, 0, (UINT32) srcWidthPx, (UINT32) srcHeightPx);



    props.pixelFormat.format    = DXGI_FORMAT_B8G8R8A8_UNORM;
    props.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    props.dpiX                  = 96.0f;
    props.dpiY                  = 96.0f;

    hr = m_renderer.m_d2dContext->CreateBitmap (D2D1::SizeU ((UINT32) srcWidthPx, (UINT32) srcHeightPx),
                                     nullptr, 0, &props, &entry.bitmap);
    CHRA (hr);

    hr = entry.bitmap->CopyFromMemory (&srcRect, srcBgraPremul,
                                       (UINT32) (srcWidthPx * sizeof (uint32_t)));
    CHRA (hr);

    entry.key    = key;
    entry.width  = srcWidthPx;
    entry.height = srcHeightPx;
    outBitmap    = entry.bitmap.Get();

    if (m_iconBitmaps.size() >= s_kMaxIconBitmaps)
    {
        m_iconBitmaps.erase (m_iconBitmaps.begin());
    }

    m_iconBitmaps.push_back (std::move (entry));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HashPixels
//
//  FNV-1a over the picture's pixels.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DxuiRenderIcons::HashPixels (const uint32_t * pixels, size_t count)
{
    constexpr uint64_t  kOffsetBasis = 14695981039346656037ULL;
    constexpr uint64_t  kPrime       = 1099511628211ULL;



    uint64_t  hash = kOffsetBasis;
    size_t    i    = 0;



    for (i = 0; i < count; i++)
    {
        hash = (hash ^ (uint64_t) pixels[i]) * kPrime;
    }

    return hash;
}
