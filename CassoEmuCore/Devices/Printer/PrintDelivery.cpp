#include "Pch.h"

#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintRaster.h"




////////////////////////////////////////////////////////////////////////////////
//
//  RenderToPng
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintDelivery::RenderToPng (
    const PrintRaster & raster,
    int                 firstRow,
    int                 lastRow,
    int                 dpi,
    DotStyle            style,
    vector<Byte> &      outPng)
{
    HRESULT                  hr = S_OK;
    PaperRenderer            renderer;
    PaperRenderer::Options   options;
    RgbaImage                image;

    options.outputDpi = dpi;
    options.style     = style;

    hr = renderer.Render (raster, firstRow, lastRow, options, image);
    CHR (hr);

    hr = PngCodec::EncodeRgba (image, dpi, outPng);
    CHR (hr);

Error:
    return hr;
}
