#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintDelivery
//
//  Composes PaperRenderer + PngCodec into the one operation every image sink
//  needs: render a span of the strip and encode it to PNG bytes. Kept in core
//  so the composition is unit-tested; the shell's sinks add only the
//  destination-specific edge (file write, clipboard, dialog).
//
//  The caller must have COM initialised on the thread (PngCodec uses WIC).
//
////////////////////////////////////////////////////////////////////////////////

class PrintDelivery
{
public:
    static HRESULT RenderToPng (const PrintRaster & raster,
                                int                 firstRow,
                                int                 lastRow,
                                int                 dpi,
                                DotStyle            style,
                                vector<Byte> &      outPng);
};
