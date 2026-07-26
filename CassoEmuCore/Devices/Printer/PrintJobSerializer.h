#pragma once

#include "Pch.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  StripMeta
//
//  The sidecar state that travels with a persisted pending strip alongside its
//  native-grid pixel plane (contracts/printing-settings.md).
//
////////////////////////////////////////////////////////////////////////////////

struct StripMeta
{
    int          formatVersion   = 1;
    int          rowsUsed        = 0;
    int          paperRow        = 0;
    vector<int>  pageBoundaryRows;
    bool         capReached      = false;
};




////////////////////////////////////////////////////////////////////////////////
//
//  PrintJobSerializer
//
//  Pure (system-free) conversion between a PrintRaster and the two persistence
//  buffers: a native-grid indexed pixel plane (one byte per cell, ink bitfield
//  0-15) and a JSON metadata string. The Casso shell's PrintJobStore wraps the
//  pixel plane in a lossless indexed PNG via WIC and does the file I/O; keeping
//  the codec out of here leaves the transform unit-testable (research R-010).
//
////////////////////////////////////////////////////////////////////////////////

class PrintJobSerializer
{
public:
    static constexpr int   kFormatVersion = 1;

    // Metadata <-> JSON text.
    static string    WriteMetaJson (const PrintRaster & raster);
    static HRESULT   ReadMetaJson  (const string & json, StripMeta & outMeta);

    // Raster -> native-grid index plane (width == PrinterGrid::kDotsPerRow,
    // height == rowsUsed, one 4-bit ink value per byte).
    static void      ExtractIndexPlane (const PrintRaster & raster,
                                        int & outWidth, int & outHeight,
                                        vector<Byte> & outPixels);

    // Index plane + metadata -> reconstructed raster.
    static HRESULT   RebuildRaster (int width, int height,
                                    const vector<Byte> & pixels,
                                    const StripMeta & meta,
                                    PrintRaster & outRaster);
};
