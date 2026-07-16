#include "Pch.h"

#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"




static constexpr int   s_kColumns        = PrinterGrid::kDotsPerRow;
static constexpr int   s_kNativeDpiH     = PrinterGrid::kDotsPerInchH;    // 160
static constexpr int   s_kNativeRowsIn   = PrinterGrid::kRowsPerInch;     // 144
static constexpr int   s_kHDenom         = s_kNativeDpiH * 2;             // 320: 1/(2*160)" grid
static constexpr int   s_kVDenom         = s_kNativeRowsIn * 2;           // 288
static constexpr int   s_kSuperSamples   = 8;                            // per-axis AA supersampling
static constexpr int   s_kWeaveSize      = 8;
static constexpr int   s_kWeaveAmp       = 14;                           // ~5% opacity modulation

// Paper white background.
static constexpr Byte  s_kPaperR = 0xFF;
static constexpr Byte  s_kPaperG = 0xFF;
static constexpr Byte  s_kPaperB = 0xFF;




////////////////////////////////////////////////////////////////////////////////
//
//  Ink palette
//
//  4-bit cell bitfield -> ribbon colour. Overprint composites (orange, green,
//  purple) and black-dominance (any black bit, or three primaries) are baked
//  in per R-004.
//
////////////////////////////////////////////////////////////////////////////////

static constexpr Byte  s_kPalette[16][3] =
{
    { 0xFF, 0xFF, 0xFF },   // 0  none (unused -- empty cells are skipped)
    { 0x20, 0x20, 0x20 },   // 1  black
    { 0xF0, 0xC8, 0x10 },   // 2  yellow
    { 0x20, 0x20, 0x20 },   // 3  yellow+black -> black
    { 0xC8, 0x30, 0x30 },   // 4  red
    { 0x20, 0x20, 0x20 },   // 5  red+black -> black
    { 0xE8, 0x78, 0x18 },   // 6  yellow+red -> orange
    { 0x20, 0x20, 0x20 },   // 7  +black
    { 0x28, 0x48, 0xA8 },   // 8  blue
    { 0x20, 0x20, 0x20 },   // 9  +black
    { 0x28, 0x88, 0x38 },   // 10 yellow+blue -> green
    { 0x20, 0x20, 0x20 },   // 11 +black
    { 0x78, 0x30, 0x88 },   // 12 red+blue -> purple
    { 0x20, 0x20, 0x20 },   // 13 +black
    { 0x20, 0x20, 0x20 },   // 14 yellow+red+blue -> black-dominant
    { 0x20, 0x20, 0x20 },   // 15 +black
};




////////////////////////////////////////////////////////////////////////////////
//
//  BuildWeaveTile
//
//  A small tileable diagonal modulation standing in for the ribbon weave. Few
//  percent amplitude, factor in 0..256 (256 == full ink).
//
////////////////////////////////////////////////////////////////////////////////

void PaperRenderer::BuildWeaveTile()
{
    int   x = 0;
    int   y = 0;

    if (!m_weave.empty())
    {
        return;
    }

    m_weave.resize ((size_t) s_kWeaveSize * s_kWeaveSize);

    for (y = 0; y < s_kWeaveSize; y++)
    {
        for (x = 0; x < s_kWeaveSize; x++)
        {
            int   diag = (x + y) & 3;
            m_weave[(size_t) y * s_kWeaveSize + x] = 256 - (s_kWeaveAmp * diag) / 3;
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  BuildDiscKernels
//
//  Precomputes an antialiased pin-dot coverage kernel for each horizontal
//  subpixel phase (rem of the 1/320" centre grid). Vertical centres land on
//  exact pixels for the supported 288/576 dpi, so no vertical phase is needed.
//  Coverage is supersampled once here (deterministic) into integer tables;
//  the per-pixel compositing that follows is pure integer.
//
////////////////////////////////////////////////////////////////////////////////

void PaperRenderer::BuildDiscKernels (int outputDpi)
{
    double   radius     = (double) outputDpi / (double) s_kVDenom * 2.0;   // outputDpi/144 px
    int      foot       = (int) radius + 2;
    int      side       = 2 * foot + 1;
    int      rem        = 0;

    if (m_kernelDpi == outputDpi && !m_kernels.empty())
    {
        return;
    }

    m_kernels.clear();
    m_kernels.resize (s_kHDenom);

    for (rem = 0; rem < s_kHDenom; rem++)
    {
        DiscKernel &   k  = m_kernels[(size_t) rem];
        double         fx = (double) rem / (double) s_kHDenom;
        int            px = 0;
        int            py = 0;

        k.offX = -foot;
        k.offY = -foot;
        k.w    = side;
        k.h    = side;
        k.coverage.assign ((size_t) side * side, 0);

        for (py = -foot; py <= foot; py++)
        {
            for (px = -foot; px <= foot; px++)
            {
                int   hits = 0;
                int   sx   = 0;
                int   sy   = 0;

                for (sy = 0; sy < s_kSuperSamples; sy++)
                {
                    for (sx = 0; sx < s_kSuperSamples; sx++)
                    {
                        double   ptX = (double) px - fx + ((double) sx + 0.5) / s_kSuperSamples;
                        double   ptY = (double) py       + ((double) sy + 0.5) / s_kSuperSamples;

                        if (ptX * ptX + ptY * ptY <= radius * radius)
                        {
                            hits++;
                        }
                    }
                }

                {
                    int   idx = (py + foot) * side + (px + foot);
                    k.coverage[(size_t) idx] = (Byte) (hits * 255 / (s_kSuperSamples * s_kSuperSamples));
                }
            }
        }
    }

    m_kernelDpi = outputDpi;
}




////////////////////////////////////////////////////////////////////////////////
//
//  BlendPixel
//
//  Alpha-composites an ink colour onto a paper pixel. Integer only; the
//  paper stays fully opaque.
//
////////////////////////////////////////////////////////////////////////////////

void PaperRenderer::BlendPixel (RgbaImage & img, int x, int y, int alpha,
                                Byte r, Byte g, Byte b)
{
    Byte *   p = nullptr;

    if (x < 0 || y < 0 || x >= img.width || y >= img.height || alpha <= 0)
    {
        return;
    }

    if (alpha > 255)
    {
        alpha = 255;
    }

    p = img.PixelAt (x, y);
    p[0] = (Byte) ((p[0] * (255 - alpha) + r * alpha) / 255);
    p[1] = (Byte) ((p[1] * (255 - alpha) + g * alpha) / 255);
    p[2] = (Byte) ((p[2] * (255 - alpha) + b * alpha) / 255);
}




////////////////////////////////////////////////////////////////////////////////
//
//  StampDisc
//
//  Stamps one antialiased pin dot centred on a cell, modulated by the weave.
//
////////////////////////////////////////////////////////////////////////////////

void PaperRenderer::StampDisc (RgbaImage & img, int xpx, int ypx, int phase,
                               Byte r, Byte g, Byte b)
{
    const DiscKernel &   k  = m_kernels[(size_t) phase];
    int                  ky = 0;
    int                  kx = 0;

    for (ky = 0; ky < k.h; ky++)
    {
        for (kx = 0; kx < k.w; kx++)
        {
            int   cov = k.coverage[(size_t) ky * k.w + kx];
            int   ox  = 0;
            int   oy  = 0;
            int   wf  = 0;
            int   a   = 0;

            if (cov == 0)
            {
                continue;
            }

            ox = xpx + k.offX + kx;
            oy = ypx + k.offY + ky;

            if (ox < 0 || oy < 0 || ox >= img.width || oy >= img.height)
            {
                continue;
            }

            wf = m_weave[(size_t) (oy % s_kWeaveSize) * s_kWeaveSize + (ox % s_kWeaveSize)];
            a  = cov * wf / 256;
            BlendPixel (img, ox, oy, a, r, g, b);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  StampSquare
//
//  Plain style: fills a cell-sized rectangle at full coverage, still weave-
//  modulated so the two styles share one pipeline.
//
////////////////////////////////////////////////////////////////////////////////

void PaperRenderer::StampSquare (RgbaImage & img, int x0, int y0, int x1, int y1,
                                 Byte r, Byte g, Byte b)
{
    int   x = 0;
    int   y = 0;

    for (y = y0; y < y1; y++)
    {
        for (x = x0; x < x1; x++)
        {
            int   wf = 0;
            int   a  = 0;

            if (x < 0 || y < 0 || x >= img.width || y >= img.height)
            {
                continue;
            }

            wf = m_weave[(size_t) (y % s_kWeaveSize) * s_kWeaveSize + (x % s_kWeaveSize)];
            a  = 255 * wf / 256;
            BlendPixel (img, x, y, a, r, g, b);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Render
//
//  Rasterises rows [firstRow, lastRow] of the native strip to a square-pixel
//  RGBA image at the requested dpi and dot style.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PaperRenderer::Render (
    const PrintRaster & raster,
    int                 firstRow,
    int                 lastRow,
    const Options     & options,
    RgbaImage         & outImage)
{
    HRESULT   hr      = S_OK;
    int       dpi     = options.outputDpi;
    int       numRows = lastRow - firstRow + 1;
    int       outW    = 0;
    int       outH    = 0;
    int       row     = 0;

    CBRAEx (dpi > 0, E_INVALIDARG);

    if (numRows <= 0)
    {
        outImage.Allocate (0, 0, s_kPaperR, s_kPaperG, s_kPaperB);
        goto Error;
    }

    BuildWeaveTile();
    BuildDiscKernels (dpi);

    outW = s_kColumns * dpi / s_kNativeDpiH;
    outH = numRows * dpi / s_kNativeRowsIn;
    outImage.Allocate (outW, outH, s_kPaperR, s_kPaperG, s_kPaperB);

    for (row = firstRow; row <= lastRow; row++)
    {
        int   rowLocal = row - firstRow;
        int   col      = 0;

        for (col = 0; col < s_kColumns; col++)
        {
            Byte   cell = raster.CellAt (col, row);

            if (cell == 0)
            {
                continue;
            }

            {
                const Byte *   rgb  = s_kPalette[cell & 0x0F];
                int            numX = (2 * col + 1) * dpi;
                int            numY = (2 * rowLocal + 1) * dpi;
                int            xpx  = numX / s_kHDenom;
                int            ypx  = numY / s_kVDenom;

                if (options.style == DotStyle::Ink)
                {
                    int   phase = numX % s_kHDenom;
                    StampDisc (outImage, xpx, ypx, phase, rgb[0], rgb[1], rgb[2]);
                }
                else
                {
                    int   sx0 = col * dpi / s_kNativeDpiH;
                    int   sx1 = (col + 1) * dpi / s_kNativeDpiH;
                    int   sy0 = rowLocal * dpi / s_kNativeRowsIn;
                    int   sy1 = (rowLocal + 1) * dpi / s_kNativeRowsIn;
                    StampSquare (outImage, sx0, sy0, sx1, sy1, rgb[0], rgb[1], rgb[2]);
                }
            }
        }
    }


Error:
    return hr;
}
