#include "Pch.h"

#include "PrinterPanel.h"

#include "CassoTheme.h"
#include "Printer3DScene.h"
#include "../resource.h"
#include "Render/IDxuiPainter.h"
#include "Render/IDxuiTextRenderer.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/RgbaImage.h"
#include "Print/PrinterWorker.h"




namespace
{
    constexpr wchar_t   s_kpszTitle     [] = L"Casso Printer";
    constexpr wchar_t   s_kpszClassName [] = L"CassoPrinterPanel";

    constexpr int       s_kPreferredWidthDip  = 560;
    constexpr int       s_kPreferredHeightDip = 680;

    // Viewport render: 144 dpi maps native rows 1:1 to pixels, so the visible
    // ~1-page span is a fixed 1152x1584 image regardless of strip length --
    // bounded memory, stable scale-to-fit, and delta-friendly row alignment.
    constexpr int       s_kPreviewDpi = PrinterGrid::kRowsPerInch;

    // Minimum interval between live re-renders while bytes stream (~60 Hz).
    // Viewport motion (scroll / snap) bypasses it so input feels immediate.
    constexpr int64_t   s_kMinRenderIntervalMs = 16;

    // Scroll step sizes in native rows (144 rows/inch).
    constexpr int       s_kWheelRowsPerNotch = 96;    // 2/3" per wheel notch
    constexpr int       s_kArrowScrollRows   = 48;    // 1/3" per key press

    // Fanfold paper furniture (FR-032; panel-only per FR-027), all in px at the
    // fixed 144 dpi preview scale. Real continuous-form stock: 9.5" wide with
    // 0.5" tractor strips both sides (tear width 8.5"), 5/32" sprocket holes on
    // a 1/2" pitch, and the 8" printable area centered between the strips.
    constexpr int       s_kStockWidthPx   = (19 * PrinterGrid::kRowsPerInch) / 2;   // 9.5" = 1368
    constexpr int       s_kStripWidthPx   = PrinterGrid::kRowsPerInch / 2;          // 0.5" =   72
    constexpr int       s_kContentXPx     = s_kStripWidthPx + PrinterGrid::kRowsPerInch / 4;   // 0.75" = 108
    constexpr int       s_kHoleRadiusPx   = 11;                                     // ~5/32" dia
    constexpr int       s_kHolePitchPx    = PrinterGrid::kRowsPerInch / 2;          // 0.5" =   72
    constexpr uint32_t  s_kArgbHoleRim    = 0xFFB8B8B8;   // sprocket hole edge

    // The live pin band (FR-034): a head pass strikes 8 pins spaced 1/72",
    // i.e. 2 native rows each -- 16 rows below the paper row. The reveal mask
    // clips this band at the paced head column; rows above it are complete.
    constexpr int       s_kPinBandRows = 8 * (PrinterGrid::kRowsPerInch / 72);


    // Floor modulus: hole / perforation phase stays continuous for rows above
    // the top of the strip (the leading fanfold paper), where absRow < 0.
    constexpr int FloorMod (int a, int m)
    {
        return ((a % m) + m) % m;
    }


    // Perforation dash: a slight darkening of whatever it crosses -- light gray
    // on paper white, a shade darker on ink -- like a real perf cut, instead of
    // stamping gray over (and visually erasing) printed content.
    inline void DarkenPerf (uint32_t & px)
    {
        uint32_t  a = px & 0xFF000000u;
        uint32_t  r = ((px >> 16) & 0xFF) * 210 / 255;
        uint32_t  g = ((px >>  8) & 0xFF) * 210 / 255;
        uint32_t  b = ( px        & 0xFF) * 210 / 255;

        px = a | (r << 16) | (g << 8) | b;
    }

    constexpr wchar_t   s_kpszScrollHint [] =
        L"Scroll wheel or Up/Down to review \u2022 scroll past the end to lift the last page \u2022 rejoins a live print when idle";


    // Read an embedded RCDATA resource (the user's ImageWriter CAD model)
    // into a string. Returns empty on any failure -- the scene then keeps
    // its procedural body, so a missing model never blanks the panel.
    std::string LoadTextResource (int resourceId)
    {
        HINSTANCE   hInstance = GetModuleHandleW (nullptr);
        HRSRC       hRes      = nullptr;
        HGLOBAL     hMem      = nullptr;
        DWORD       cbData    = 0;
        void      * pData     = nullptr;

        hRes = FindResourceW (hInstance, MAKEINTRESOURCEW (resourceId), RT_RCDATA);
        if (hRes == nullptr) { return {}; }

        cbData = SizeofResource (hInstance, hRes);
        hMem   = LoadResource (hInstance, hRes);
        if (cbData == 0 || hMem == nullptr) { return {}; }

        pData = LockResource (hMem);
        if (pData == nullptr) { return {}; }

        return std::string ((const char *) pData, cbData);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel ctor / dtor
//
//  Defined here (not defaulted in the header) so unique_ptr<Printer3DScene>
//  destroys against the complete type.
//
////////////////////////////////////////////////////////////////////////////////

PrinterPanel::PrinterPanel () = default;

PrinterPanel::~PrinterPanel () = default;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Create
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterPanel::Create (
    HINSTANCE              hInstance,
    HWND                   hwndOwner,
    ID3D11Device         * device,
    ID3D11DeviceContext  * context,
    const CassoTheme     * theme)
{
    HRESULT                    hr = S_OK;
    DxuiWindow::CreateParams   params;

    UNREFERENCED_PARAMETER (device);
    UNREFERENCED_PARAMETER (context);

    if (IsCreated ())
    {
        return S_OK;
    }

    m_theme = theme;

    params.title             = s_kpszTitle;
    params.hInstance         = hInstance;
    params.ownerHwnd         = hwndOwner;
    params.initialSizeDip    = { s_kPreferredWidthDip, s_kPreferredHeightDip };
    params.minSizeDip        = { 360, 420 };
    params.resizable         = true;
    params.captionStyle      = DxuiCaptionStyle::CloseOnly;
    params.classNameOverride = s_kpszClassName;

    hr = DxuiWindow::Create (params);
    CHR (hr);

    SetTheme (m_theme);

    // 3D presentation (FR-032): build the scene on THIS window's own device
    // (its swap chain does not live on the emulator renderer's device) and
    // draw it from the before-present hook -- under the panel chrome, which
    // deliberately leaves the paper rect unfilled. Failure falls back to the
    // flat PrinterPaperView silently.
    {
        std::unique_ptr<Printer3DScene>   scene = std::make_unique<Printer3DScene> ();

        if (PopupHost () != nullptr &&
            SUCCEEDED (scene->Initialize (PopupHost ()->GetDevice (), PopupHost ()->GetContext ())))
        {
            m_scene = std::move (scene);

            // The user's ImageWriter CAD model, embedded as OBJ+MTL. Failure
            // silently keeps the procedural body.
            {
                std::string   obj = LoadTextResource (IDR_MODEL_IMAGEWRITER_OBJ);
                std::string   mtl = LoadTextResource (IDR_MODEL_IMAGEWRITER_MTL);

                if (!obj.empty ())
                {
                    IGNORE_RETURN_VALUE (hr, m_scene->SetModel (obj, mtl));
                }
            }

            PopupHost ()->SetBeforePresentHook ([this] ()
            {
                if (m_scene != nullptr && m_paperRectPx.right > m_paperRectPx.left)
                {
                    m_scene->Render (m_paperRectPx);
                }
            });

            if (m_paper != nullptr)
            {
                m_paper->SetVisible (false);   // the scene presents the content now
            }
        }
    }

    Show ();

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnCreate
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::OnCreate ()
{
    m_paper   = CreateChild<PrinterPaperView> ();
    m_finish  = CreateChild<DxuiButton> (L"Finish");
    m_copy    = CreateChild<DxuiButton> (L"Copy");
    m_discard = CreateChild<DxuiButton> (L"Discard");
    m_refresh = CreateChild<DxuiButton> (L"Refresh");

    m_finish->SetOnClick  ([this] () { if (m_onFinish)  { m_onFinish  (); } });
    m_copy->SetOnClick    ([this] () { if (m_onCopy)    { m_onCopy    (); } });
    m_discard->SetOnClick ([this] () { if (m_onDiscard) { m_onDiscard (); } });
    m_refresh->SetOnClick ([this] () { if (m_onRefresh) { m_onRefresh (); } });
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SetTheme
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SetTheme (const CassoTheme * theme)
{
    m_theme = theme;
    DxuiWindow::SetTheme (theme);   // implicit upcast CassoTheme -> DxuiTheme -> IDxuiTheme
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RenderFrame
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrinterPanel::RenderFrame ()
{
    if (!IsCreated ())
    {
        return S_OK;
    }

    Invalidate ();
    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::NowMs
//
////////////////////////////////////////////////////////////////////////////////

int64_t PrinterPanel::NowMs ()
{
    return (int64_t) std::chrono::duration_cast<std::chrono::milliseconds> (
               std::chrono::steady_clock::now ().time_since_epoch ()).count ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RefreshLive
//
//  The per-frame heartbeat: sync the viewport to the worker's newest row and
//  re-render the visible span only when something changed -- new bytes landed,
//  the viewport moved (scroll or snap-back), or the caller forced it. The
//  span snapshot and render are both bounded by the viewport (~1 page), so a
//  60-page banner costs the same per frame as a receipt (SC-010).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RefreshLive (PrinterWorker & worker, int64_t nowMs, bool force)
{
    int                     rows        = worker.RowsUsed ();
    uint64_t                activity    = worker.ActivityCount ();
    double                  nowSec      = (double) nowMs / 1000.0;
    int                     headRow     = 0;
    int                     headCol     = 0;
    int                     revealRow   = 0;
    int                     revealCol   = 0;
    int                     bandBottom  = 0;
    bool                    moved       = false;
    bool                    revealMoved = false;
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    worker.HeadPosition (headRow, headCol);

    // A shrunk strip means eject/discard tore the paper off: rewind the view
    // and the reveal to the fresh sheet instead of staring past its end.
    if (rows - 1 < m_viewport.LiveRow ())
    {
        m_viewport.Reset ();
        m_pacing.Reset (nowSec, 0);
        m_spanImgValid = false;
    }

    // First refresh starts caught up at the head, so opening the panel over a
    // restored pending strip never replays its history (only NEW ink animates).
    if (!m_pacingPrimed)
    {
        m_pacing.Reset (nowSec, headRow);
        m_pacingPrimed = true;
    }

    // The FR-034 reveal: pacing chases the head at ImageWriter speed. At
    // authentic guest speed it stays caught up (the sweep IS the guest's own
    // timing); at max speed it animates behind, jump-cutting past big backlogs.
    m_pacing.SetTargetPosition (headRow, headCol);
    m_pacing.Advance (nowSec);

    revealRow  = m_pacing.RevealedRows ();
    revealCol  = m_pacing.RevealedColDots ();
    bandBottom = (std::min) (revealRow + s_kPinBandRows - 1, rows - 1);

    if (m_scene != nullptr)
    {
        m_scene->SetHeadColumn01 ((float) revealCol / (float) PrinterGrid::kDotsPerRow);
    }

    // The viewport follows the REVEALED edge, not the raster's -- so a paced
    // reveal happens on-screen instead of scrolling past unseen.
    if (rows > 0)
    {
        m_viewport.Advance ((std::max) (bandBottom, 0));
    }
    m_viewport.Tick (nowMs);

    span        = m_viewport.VisibleSpan ();
    moved       = (span.firstRow != m_renderedSpan.firstRow || span.lastRow != m_renderedSpan.lastRow);
    revealMoved = (revealRow != m_renderedRevealRow || revealCol != m_renderedRevealCol);

    if (!force && !moved && !revealMoved && m_hasRendered && activity == m_renderedActivity)
    {
        return;   // nothing changed: zero work this frame
    }

    if (!force && !moved && nowMs - m_lastRenderMs < s_kMinRenderIntervalMs)
    {
        return;   // pace streaming re-renders; the change lands next frame
    }

    if (rows <= 0)
    {
        ShowBlankSheet ();
    }
    else if (worker.SnapshotStripSpan (span.firstRow, span.lastRow, spanRaster))
    {
        bool   contentDirty = (activity != m_renderedActivity) || !m_hasRendered;

        RenderSpan (spanRaster, span.firstRow, span.lastRow, contentDirty, revealRow, revealCol);
    }
    else
    {
        ShowBlankSheet ();   // no active job: fresh paper in the platen
    }

    m_renderedSpan      = span;
    m_renderedActivity  = activity;
    m_renderedRevealRow = revealRow;
    m_renderedRevealCol = revealCol;
    m_lastRenderMs      = nowMs;
    m_hasRendered       = true;
    Invalidate ();
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::SetStrip
//
//  Direct push for worker-less paths (no printer card / tests): same viewport
//  and span render, sourced from the supplied raster instead of the worker.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::SetStrip (const PrintRaster & raster)
{
    int                     rows = raster.RowsUsed ();
    PrinterViewport::Span   span;
    PrintRaster             spanRaster;

    if (m_paper == nullptr)
    {
        return;
    }

    if (rows - 1 < m_viewport.LiveRow ())
    {
        m_viewport.Reset ();
    }

    if (rows <= 0)
    {
        ShowBlankSheet ();
        m_hasRendered = true;
        return;
    }

    m_viewport.Advance (rows - 1);

    span = m_viewport.VisibleSpan ();
    raster.CopyRowSpan (span.firstRow, span.lastRow, spanRaster);
    RenderSpan (spanRaster, span.firstRow, span.lastRow, true, -1, 0);   // no live head: show everything

    m_renderedSpan = span;
    m_hasRendered  = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::ShowBlankSheet
//
//  A blank sheet rather than an empty window, so the preview always reads as
//  "paper loaded, nothing printed yet" -- same fanfold canvas, no content.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::ShowBlankSheet ()
{
    ComposeCanvas (nullptr, 0, 0, -1, 0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::RenderSpan
//
//  Render the (rebased) span raster, then compose it onto the fanfold canvas.
//  `firstAbsRow`/`lastAbsRow` are the span's bounds in strip-absolute terms:
//  the canvas bottom is the span's LAST row (the live row at the platen), and
//  the sprocket-hole / perforation phase keys off the same frame so the
//  furniture scrolls WITH the paper instead of sitting still while content
//  slides past it. The reveal pair (band top + head column, FR-034) clips
//  the live line; -1 disables.
//
//  The ink render is the expensive step, so it is cached by absolute row:
//  scrolling shifts the window over UNCHANGED content, and only the newly
//  exposed rows are rendered (the rest memmove within the cache). New bytes
//  (`contentDirty`) or a span that stops lining up rebuild the whole image.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::RenderSpan (const PrintRaster & spanRaster, int firstAbsRow, int lastAbsRow,
                               bool contentDirty, int revealBandTopAbs, int revealColDots)
{
    HRESULT                  hr       = S_OK;
    int                      spanRows = lastAbsRow - firstAbsRow + 1;
    PaperRenderer            renderer;
    PaperRenderer::Options   opt;

    if (spanRows <= 0)
    {
        // Degenerate span: blank paper, furniture still tracking the scroll.
        m_spanImgValid = false;
        ComposeCanvas (nullptr, 0, lastAbsRow, -1, 0);
        return;
    }

    opt.outputDpi = s_kPreviewDpi;
    opt.style     = DotStyle::Ink;

    bool   haveImg = false;

    if (!contentDirty && m_spanImgValid && m_spanImg.height == spanRows)
    {
        int   delta = firstAbsRow - m_spanImgFirstAbsRow;   // + = scrolled toward live

        if (delta == 0)
        {
            haveImg = true;   // reveal-only update: the ink image is current
        }
        else if (std::abs (delta) < spanRows)
        {
            // Pure scroll: shift the overlap, render only the exposed edge.
            int         keepRows = spanRows - std::abs (delta);
            size_t      rowBytes = (size_t) m_spanImg.width * 4;
            int         newFirst = (delta > 0) ? spanRows - delta : 0;   // exposed block, span-relative
            int         newCount = std::abs (delta);
            RgbaImage   edge;

            if (delta > 0)
            {
                memmove (m_spanImg.PixelAt (0, 0), m_spanImg.PixelAt (0, delta), rowBytes * keepRows);
            }
            else
            {
                memmove (m_spanImg.PixelAt (0, -delta), m_spanImg.PixelAt (0, 0), rowBytes * keepRows);
            }

            hr = renderer.Render (spanRaster, newFirst, newFirst + newCount - 1, opt, edge);

            if (SUCCEEDED (hr) && edge.width == m_spanImg.width && edge.height == newCount)
            {
                memcpy (m_spanImg.PixelAt (0, newFirst), edge.PixelAt (0, 0), rowBytes * newCount);
                m_spanImgFirstAbsRow = firstAbsRow;
                haveImg              = true;
            }
        }
    }

    if (!haveImg)
    {
        hr = renderer.Render (spanRaster, 0, spanRows - 1, opt, m_spanImg);

        if (FAILED (hr) || m_spanImg.width <= 0 || m_spanImg.height != spanRows)
        {
            m_spanImgValid = false;
            return;   // keep the previous frame rather than flash a bad one
        }

        m_spanImgFirstAbsRow = firstAbsRow;
        m_spanImgValid       = true;
        m_spanImgGen++;      // content pixels changed: the canvas cache must rebuild
    }

    if (m_spanImg.height > m_viewport.ViewportRows ())
    {
        return;   // span larger than the canvas: keep the previous frame
    }

    ComposeCanvas (&m_spanImg, firstAbsRow, lastAbsRow, revealBandTopAbs, revealColDots);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::ComposeCanvas
//
//  Builds the fanfold-paper view (FR-032): a fixed full-viewport canvas at
//  9.5" stock width -- tractor strips with sprocket holes down both edges,
//  light vertical perforations where the strips tear off, cross perforations
//  at every 11" page boundary. The canvas bottom is `bottomAbsRow` (the
//  span's live row, where the platen will be) and content rows land at their
//  true strip positions above it -- ink that ends before the live row keeps
//  its trailing gap instead of being dragged down to the platen. Hole and
//  perforation phase is strip-absolute, so the furniture feeds upward with
//  the paper. Constant canvas size = stable texture and scale-to-fit (no
//  zoom jumps). Holes are punched transparent so the dark mat shows through
//  them.
//
//  The FR-034 reveal mask clips the live pin band: rows at or below
//  `revealBandTopAbs` show ink only left of the paced head column, so the
//  head visibly lays ink as it sweeps. -1 disables the mask.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::ComposeCanvas (const RgbaImage * content, int contentFirstAbsRow, int bottomAbsRow,
                                  int revealBandTopAbs, int revealColDots)
{
    HRESULT   hr        = S_OK;
    int       canvasW   = s_kStockWidthPx;
    int       canvasH   = m_viewport.ViewportRows ();   // px == rows at 144 dpi
    int       topAbsRow = bottomAbsRow - canvasH + 1;   // canvas bottom = span's live row
    int       holeR     = s_kHoleRadiusPx;

    if (m_canvas.size () != (size_t) canvasW * canvasH)
    {
        m_canvas.assign ((size_t) canvasW * canvasH, 0xFFFFFFFFu);
        m_canvasValid = false;
    }

    // Rebuild canvas rows [rowFirst..rowLast] (canvas-relative, inclusive)
    // from scratch: paper white, content blit with the FR-034 reveal clip,
    // perforations, then the sprocket holes whose circles reach the range.
    // Everything is a function of absolute row phase, so a range rebuild is
    // bit-identical to the same rows of a full rebuild.
    auto RebuildRows = [&] (int rowFirst, int rowLast)
    {
        rowFirst = (std::max) (rowFirst, 0);
        rowLast  = (std::min) (rowLast, canvasH - 1);

        if (rowLast < rowFirst)
        {
            return;
        }

        std::fill (m_canvas.begin () + (size_t) rowFirst * canvasW,
                   m_canvas.begin () + ((size_t) rowLast + 1) * canvasW, 0xFFFFFFFFu);

        // Content, bottom-anchored in the printable area, premultiplied for
        // the GPU blit (paper is opaque, but anti-aliased dot edges carry
        // alpha). Rows in the live pin band blit only up to the paced head
        // column; ink to its right stays paper white until the sweep gets
        // there (FR-034).
        if (content != nullptr)
        {
            int   yTop = (contentFirstAbsRow - topAbsRow);

            for (int y = 0; y < content->height; y++)
            {
                if (yTop + y < rowFirst || yTop + y > rowLast)
                {
                    continue;
                }

                uint32_t *     dst  = &m_canvas[(size_t) (yTop + y) * canvasW + s_kContentXPx];
                const Byte *   src  = content->PixelAt (0, y);
                int            xEnd = content->width;

                if (revealBandTopAbs >= 0 && contentFirstAbsRow + y >= revealBandTopAbs)
                {
                    xEnd = std::clamp (revealColDots * content->width / PrinterGrid::kDotsPerRow,
                                       0, content->width);
                }

                for (int x = 0; x < xEnd; x++)
                {
                    uint32_t  r = src[x * 4 + 0];
                    uint32_t  g = src[x * 4 + 1];
                    uint32_t  b = src[x * 4 + 2];
                    uint32_t  a = src[x * 4 + 3];

                    dst[x] = (a << 24) | ((r * a / 255) << 16) | ((g * a / 255) << 8) | (b * a / 255);
                }
            }
        }

        // Vertical tear-off perforations where the tractor strips meet the
        // sheet: dotted 1-px columns, phase locked to the paper.
        for (int y = rowFirst; y <= rowLast; y++)
        {
            if (FloorMod (topAbsRow + y, 8) < 4)
            {
                DarkenPerf (m_canvas[(size_t) y * canvasW + s_kStripWidthPx - 1]);
                DarkenPerf (m_canvas[(size_t) y * canvasW + canvasW - s_kStripWidthPx]);
            }
        }

        // Cross perforations at every page boundary (11" pitch).
        for (int y = rowFirst; y <= rowLast; y++)
        {
            if (FloorMod (topAbsRow + y, PrinterGrid::kPageRows) == 0)
            {
                uint32_t *   row = &m_canvas[(size_t) y * canvasW];

                for (int x = 0; x < canvasW; x++)
                {
                    if (x % 8 < 4)
                    {
                        DarkenPerf (row[x]);
                    }
                }
            }
        }

        // Sprocket holes: punched transparent (alpha 0 -- the mat shows
        // through) with a soft rim, centered in each strip on the 1/2"
        // pitch. Center rows just outside the range still reach into it, so
        // scan wider and clip the writes to the range.
        {
            int   xL = s_kStripWidthPx / 2;
            int   xR = canvasW - s_kStripWidthPx / 2;

            for (int y = rowFirst - holeR - 1; y <= rowLast + holeR + 1; y++)
            {
                if (FloorMod (topAbsRow + y, s_kHolePitchPx) != s_kHolePitchPx / 2)
                {
                    continue;   // y is not a hole-center row
                }

                for (int dy = -holeR - 1; dy <= holeR + 1; dy++)
                {
                    int   py = y + dy;

                    if (py < rowFirst || py > rowLast)
                    {
                        continue;
                    }

                    for (int dx = -holeR - 1; dx <= holeR + 1; dx++)
                    {
                        int   d2 = dx * dx + dy * dy;

                        for (int cx : { xL, xR })
                        {
                            uint32_t &   px = m_canvas[(size_t) py * canvasW + cx + dx];

                            if      (d2 <= holeR * holeR)             { px = 0x00000000u;   }
                            else if (d2 <= (holeR + 1) * (holeR + 1)) { px = s_kArgbHoleRim; }
                        }
                    }
                }
            }
        }
    };

    // Fast paths apply only while the SAME full-height content sits aligned
    // under the canvas (no strip-start clamp, no new ink since the cache was
    // composed): a scroll step memmoves and rebuilds the exposed edge; a
    // reveal-sweep frame rebuilds just the pin-band rows.
    bool   aligned = m_canvasValid
                     && content != nullptr && m_canvasHasContent
                     && m_canvasSpanGen == m_spanImgGen
                     && content->height == canvasH
                     && contentFirstAbsRow == topAbsRow;
    bool   revealSame = (revealBandTopAbs == m_canvasRevealTop && revealColDots == m_canvasRevealCol);
    int    delta      = topAbsRow - m_canvasTopAbs;

    if (aligned && delta == 0 && revealSame)
    {
        // Bit-identical frame; the caller's change detection normally
        // prevents this, so just re-upload.
    }
    else if (aligned && delta == 0)
    {
        // Reveal sweep only: rebuild from the older of the two band tops
        // down to the canvas bottom (the bands live in the last rows).
        int   oldTop = (m_canvasRevealTop >= 0) ? m_canvasRevealTop - topAbsRow : canvasH;
        int   newTop = (revealBandTopAbs  >= 0) ? revealBandTopAbs  - topAbsRow : canvasH;

        RebuildRows ((std::min) (oldTop, newTop), canvasH - 1);
    }
    else if (aligned && revealSame && std::abs (delta) < canvasH)
    {
        // Scroll step: shift the finished canvas and rebuild the exposed
        // edge (padded by the hole radius so straddling holes stay round).
        int      keepRows = canvasH - std::abs (delta);
        size_t   rowBytes = (size_t) canvasW * 4;

        if (delta > 0)
        {
            memmove (m_canvas.data (), m_canvas.data () + (size_t) delta * canvasW, rowBytes * keepRows);
            RebuildRows (canvasH - delta - holeR - 1, canvasH - 1);
        }
        else
        {
            memmove (m_canvas.data () + (size_t) (-delta) * canvasW, m_canvas.data (), rowBytes * keepRows);
            RebuildRows (0, -delta + holeR);
        }
    }
    else
    {
        RebuildRows (0, canvasH - 1);
    }

    m_canvasTopAbs     = topAbsRow;
    m_canvasRevealTop  = revealBandTopAbs;
    m_canvasRevealCol  = revealColDots;
    m_canvasSpanGen    = m_spanImgGen;
    m_canvasHasContent = (content != nullptr);
    m_canvasValid      = true;

    if (m_scene != nullptr)
    {
        IGNORE_RETURN_VALUE (hr, m_scene->SetContent (m_canvas.data (), canvasW, canvasH));
    }
    else
    {
        std::vector<uint32_t>   copy = m_canvas;

        m_paper->SetImage (std::move (copy), canvasW, canvasH);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnMouse
//
//  Vertical wheel scrolls the viewport (wheel up = back toward earlier pages,
//  like every scrolling surface); everything else -- including the toolbar
//  buttons' clicks -- flows through the base dispatch untouched.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnMouse (const DxuiMouseEvent & ev)
{
    if (ev.kind == DxuiMouseEventKind::Wheel && !ev.wheelHorizontal && ev.wheelDelta != 0.0f)
    {
        m_viewport.Scroll ((int) (-ev.wheelDelta * s_kWheelRowsPerNotch), NowMs ());
        return true;   // next RefreshLive renders the moved span immediately
    }

    return DxuiWindow::OnMouse (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::OnKey
//
//  Up/Down and PageUp/PageDown scroll the viewport (FR-033); Escape hides the
//  preview (its close-box does the same). Everything else falls through to
//  the base, which fans the key to the child controls.
//
////////////////////////////////////////////////////////////////////////////////

bool PrinterPanel::OnKey (const DxuiKeyEvent & ev)
{
    if (ev.kind == DxuiKeyEventKind::Down)
    {
        switch (ev.vk)
        {
            case VK_ESCAPE:
                Hide ();
                return true;

            case VK_UP:
                m_viewport.Scroll (-s_kArrowScrollRows, NowMs ());
                return true;

            case VK_DOWN:
                m_viewport.Scroll (+s_kArrowScrollRows, NowMs ());
                return true;

            case VK_PRIOR:
                m_viewport.Scroll (-PrinterGrid::kPageRows, NowMs ());
                return true;

            case VK_NEXT:
                m_viewport.Scroll (+PrinterGrid::kPageRows, NowMs ());
                return true;
        }
    }

    return DxuiWindow::OnKey (ev);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Layout
//
//  Paper view fills the client above a hint strip and bottom toolbar; Finish /
//  Copy / Discard run left-to-right and Refresh anchors to the right.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Layout (const RECT & boundsDip, const DxuiDpiScaler & scaler)
{
    int   pad      = scaler.Px (10);
    int   captionH = CaptionHeightPx ();
    int   toolbarH = scaler.Px (46);
    int   hintH    = scaler.Px (20);
    int   btnH     = scaler.Px (30);
    int   btnW     = scaler.Px (92);
    int   by       = boundsDip.bottom - toolbarH + (toolbarH - btnH) / 2;
    int   bx       = boundsDip.left + pad;

    // Our override replaces DxuiPanel::Layout, which would otherwise record the
    // panel's own bounds; set them so Paint's backdrop fills the whole client.
    SetBounds (boundsDip);

    m_hintFontPx = scaler.Pxf (11.0f);
    m_hintRect   = { boundsDip.left + pad,
                     boundsDip.bottom - toolbarH - hintH,
                     boundsDip.right - pad,
                     boundsDip.bottom - toolbarH };

    {
        // Reserve the caption band at the top (the content root spans the full
        // client, so without this the paper would draw up over the title bar).
        RECT  paperR = { boundsDip.left + pad, boundsDip.top + captionH + pad,
                         boundsDip.right - pad, boundsDip.bottom - toolbarH - hintH };

        m_paperRectPx = paperR;   // the 3D scene's viewport (before-present hook)

        if (m_paper != nullptr)
        {
            m_paper->Layout (paperR, scaler);
        }
    }

    for (DxuiButton * btn : { m_finish, m_copy, m_discard })
    {
        if (btn != nullptr)
        {
            RECT  r = { bx, by, bx + btnW, by + btnH };
            btn->Layout (r, scaler);
            bx += btnW + pad;
        }
    }

    if (m_refresh != nullptr)
    {
        RECT  r = { boundsDip.right - pad - btnW, by, boundsDip.right - pad, by + btnH };
        m_refresh->Layout (r, scaler);
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPanel::Paint
//
//  Fill the client with the device-bezel backdrop, let the base pump paint the
//  paper view and toolbar buttons, then draw the scroll hint in its strip
//  between them (FR-033's discoverability line).
//
////////////////////////////////////////////////////////////////////////////////

void PrinterPanel::Paint (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme)
{
    HRESULT  hr = S_OK;
    RECT     b  = Bounds ();

    // Matches the 3D scene's mat color (Printer3DScene s_kArgbMat) so the
    // frame and the scene backdrop read as one surface.
    constexpr uint32_t   kArgbFrame = 0xFF4A505A;

    if (m_scene != nullptr && m_paperRectPx.right > m_paperRectPx.left)
    {
        // The 3D scene owns the paper rect (drawn from the before-present
        // hook, UNDER this painter flush) -- fill only the frame around it,
        // or the backdrop would cover the scene.
        RECT   p = m_paperRectPx;

        painter.FillRect ((float) b.left, (float) b.top,
                          (float) (b.right - b.left), (float) (p.top - b.top), kArgbFrame);
        painter.FillRect ((float) b.left, (float) p.bottom,
                          (float) (b.right - b.left), (float) (b.bottom - p.bottom), kArgbFrame);
        painter.FillRect ((float) b.left, (float) p.top,
                          (float) (p.left - b.left), (float) (p.bottom - p.top), kArgbFrame);
        painter.FillRect ((float) p.right, (float) p.top,
                          (float) (b.right - p.right), (float) (p.bottom - p.top), kArgbFrame);
    }
    else
    {
        painter.FillRect ((float) b.left,
                          (float) b.top,
                          (float) (b.right  - b.left),
                          (float) (b.bottom - b.top),
                          kArgbFrame);
    }

    DxuiPanel::Paint (painter, text, theme);

    {
        DxuiFontHandle  bf = theme.BodyFont ();

        IGNORE_RETURN_VALUE (hr, text.DrawString (
            s_kpszScrollHint,
            (float) m_hintRect.left,
            (float) m_hintRect.top,
            (float) (m_hintRect.right  - m_hintRect.left),
            (float) (m_hintRect.bottom - m_hintRect.top),
            0xFF8A8F98,
            m_hintFontPx,
            bf.face,
            DxuiTextHAlign::Center,
            DxuiTextVAlign::Center,
            DxuiFontWeight::Normal,
            false));
    }
}
