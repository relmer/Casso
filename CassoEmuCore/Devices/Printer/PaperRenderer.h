#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"
#include "Devices/Printer/RgbaImage.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PaperRenderer
//
//  Deterministic CPU renderer turning the native 160x144-dpi dot grid into
//  true square-pixel ink imagery (FR-027/FR-028). Each struck cell stamps a
//  round pin impression (diameter 1/72", wider than the grid pitch so
//  adjacent dots merge as on paper); the cell's 4-bit ink field maps to one
//  of the seven ribbon colours via overprint composition; a fixed tileable
//  ribbon-weave modulates ink opacity. The Plain style swaps the disc for a
//  cell-sized square.
//
//  Determinism is a hard requirement (golden tests): antialiased disc
//  coverage is precomputed into fixed per-subpixel-phase tables, and every
//  per-pixel composite is integer arithmetic -- no floating-point ordering
//  hazards, bit-identical across runs and architectures.
//
////////////////////////////////////////////////////////////////////////////////

class PaperRenderer
{
public:
    struct Options
    {
        int       outputDpi = 576;              // 288 | 576 (FR-028)
        DotStyle  style     = DotStyle::Ink;    // Ink | Plain (FR-027)
    };

    HRESULT Render (const PrintRaster & raster,
                    int                 firstRow,
                    int                 lastRow,
                    const Options     & options,
                    RgbaImage         & outImage);

private:
    struct DiscKernel
    {
        int             offX = 0;      // top-left pixel offset from the dot's integer centre
        int             offY = 0;
        int             w    = 0;
        int             h    = 0;
        vector<Byte>    coverage;      // w*h coverage 0..255
    };

    void    BuildDiscKernels (int outputDpi);
    void    BuildWeaveTile   ();
    void    StampDisc        (RgbaImage & img, int xpx, int ypx, int phase,
                              Byte r, Byte g, Byte b);
    void    StampSquare      (RgbaImage & img, int xpx, int ypx, int wpx, int hpx,
                              Byte r, Byte g, Byte b);
    void    BlendPixel       (RgbaImage & img, int x, int y, int alpha,
                              Byte r, Byte g, Byte b);

    int                 m_kernelDpi = 0;        // dpi the current kernels were built for
    vector<DiscKernel>  m_kernels;              // one per horizontal subpixel phase
    vector<int>         m_weave;                // tile, factor 0..256
};
