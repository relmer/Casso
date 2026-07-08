#include "Pch.h"

#include "Devices/Printer/PrintJobPersistence.h"
#include "Devices/Printer/PrintJobSerializer.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintRaster.h"




// Index-plane palette (index == ink bitfield). Mirrors the delivery renderer's
// composite mapping so the persisted strip.png is human-inspectable; only the
// indices are load-bearing on reload.
static constexpr uint32_t   s_kInkPalette[16] =
{
    0xFFFFFFFF,   // 0  paper
    0xFF202020,   // 1  black
    0xFFF0C810,   // 2  yellow
    0xFF202020,   // 3  +black
    0xFFC83030,   // 4  red
    0xFF202020,   // 5  +black
    0xFFE87818,   // 6  orange
    0xFF202020,   // 7  +black
    0xFF2848A8,   // 8  blue
    0xFF202020,   // 9  +black
    0xFF288838,   // 10 green
    0xFF202020,   // 11 +black
    0xFF783088,   // 12 purple
    0xFF202020,   // 13 +black
    0xFF202020,   // 14 3-primary
    0xFF202020,   // 15 +black
};

static constexpr int   s_kNominalDpi = 96;   // pHYs is meaningless for the non-square native grid




////////////////////////////////////////////////////////////////////////////////
//
//  Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobPersistence::Save (const PrintRaster & raster, vector<Byte> & outPng, string & outJson)
{
    HRESULT        hr     = S_OK;
    vector<Byte>   plane;
    int            width  = 0;
    int            height = 0;

    PrintJobSerializer::ExtractIndexPlane (raster, width, height, plane);

    // An empty strip still persists as a valid 1-row image so reload is uniform.
    if (height == 0)
    {
        height = 1;
        plane.assign ((size_t) width, 0);
    }

    hr = PngCodec::EncodeIndexed (width, height, plane, s_kInkPalette, 16, s_kNominalDpi, outPng);
    CHR (hr);
    outJson = PrintJobSerializer::WriteMetaJson (raster);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobPersistence::Load (const vector<Byte> & png, const string & json, PrintRaster & outRaster)
{
    HRESULT        hr     = S_OK;
    vector<Byte>   plane;
    StripMeta      meta;
    int            width  = 0;
    int            height = 0;

    hr = PngCodec::DecodeIndexed (png, width, height, plane);
    CHR (hr);

    hr = PrintJobSerializer::ReadMetaJson (json, meta);
    CHR (hr);

    // The plane defines the real cell extent; a strip persisted as one blank
    // row (empty save) rebuilds to an empty raster via rowsUsed.
    if (meta.rowsUsed < height)
    {
        height = meta.rowsUsed;
    }

    hr = PrintJobSerializer::RebuildRaster (width, height, plane, meta, outRaster);
    CHR (hr);

Error:
    return hr;
}
