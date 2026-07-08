#include "Pch.h"

#include "Devices/Printer/PrintJobSerializer.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PrinterTypes.h"
#include "Core/JsonValue.h"
#include "Core/JsonWriter.h"
#include "Core/JsonParser.h"




static const char   s_kszFormatVersion[] = "formatVersion";
static const char   s_kszRowsUsed[]      = "rowsUsed";
static const char   s_kszPaperRow[]      = "paperRow";
static const char   s_kszBoundaries[]    = "pageBoundaryRows";
static const char   s_kszCapReached[]    = "capReached";




////////////////////////////////////////////////////////////////////////////////
//
//  WriteMetaJson
//
////////////////////////////////////////////////////////////////////////////////

string PrintJobSerializer::WriteMetaJson (const PrintRaster & raster)
{
    vector<JsonValue>                 boundaries;
    vector<pair<string, JsonValue>>   root;
    size_t                            i = 0;

    for (i = 0; i < raster.PageBoundaryRows ().size (); i++)
    {
        boundaries.push_back (JsonValue ((double) raster.PageBoundaryRows ()[i]));
    }

    root.push_back ({ s_kszFormatVersion, JsonValue ((double) kFormatVersion)        });
    root.push_back ({ s_kszRowsUsed,      JsonValue ((double) raster.RowsUsed ())     });
    root.push_back ({ s_kszPaperRow,      JsonValue ((double) raster.PaperRow ())     });
    root.push_back ({ s_kszBoundaries,    JsonValue (move (boundaries))              });
    root.push_back ({ s_kszCapReached,    JsonValue (raster.CapReached ())            });

    return JsonWriter::Write (JsonValue (move (root)));
}




////////////////////////////////////////////////////////////////////////////////
//
//  ReadMetaJson
//
//  Parses and validates the sidecar. A parse failure, wrong root type, or an
//  unrecognised format version is reported as an error so the store falls back
//  to empty paper silently (FR-026). Absent pageBoundaryRows is tolerated.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobSerializer::ReadMetaJson (const string & json, StripMeta & outMeta)
{
    HRESULT             hr     = S_OK;
    HRESULT             hrArr  = S_OK;
    JsonValue           root;
    JsonParseError      err;
    const JsonValue *   arr    = nullptr;
    StripMeta           meta;

    hr = JsonParser::Parse (json, root, err);
    CHR (hr);
    CBREx (root.GetType () == JsonType::Object, E_FAIL);

    hr = root.GetInt (s_kszFormatVersion, meta.formatVersion);
    CHR (hr);
    CBREx (meta.formatVersion == kFormatVersion, E_FAIL);

    hr = root.GetInt (s_kszRowsUsed, meta.rowsUsed);
    CHR (hr);

    hr = root.GetInt (s_kszPaperRow, meta.paperRow);
    CHR (hr);

    hr = root.GetBool (s_kszCapReached, meta.capReached);
    CHR (hr);

    hrArr = root.GetArray (s_kszBoundaries, arr);
    if (SUCCEEDED (hrArr))
    {
        size_t   i = 0;

        for (i = 0; i < arr->ArraySize (); i++)
        {
            meta.pageBoundaryRows.push_back (arr->ArrayAt (i).GetInt ());
        }
    }

    outMeta = move (meta);

Error:
    return hr;
}




////////////////////////////////////////////////////////////////////////////////
//
//  ExtractIndexPlane
//
////////////////////////////////////////////////////////////////////////////////

void PrintJobSerializer::ExtractIndexPlane (
    const PrintRaster & raster,
    int &               outWidth,
    int &               outHeight,
    vector<Byte> &      outPixels)
{
    int   width  = PrinterGrid::kDotsPerRow;
    int   height = raster.RowsUsed ();
    int   row    = 0;
    int   col    = 0;

    outWidth  = width;
    outHeight = height;
    outPixels.assign ((size_t) width * height, 0);

    for (row = 0; row < height; row++)
    {
        for (col = 0; col < width; col++)
        {
            outPixels[(size_t) row * width + col] = (Byte) (raster.CellAt (col, row) & 0x0F);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  RebuildRaster
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobSerializer::RebuildRaster (
    int                  width,
    int                  height,
    const vector<Byte> & pixels,
    const StripMeta &    meta,
    PrintRaster &        outRaster)
{
    HRESULT   hr = S_OK;

    // Graceful rejection of a malformed/corrupt plane -- not a programming
    // error, so no assert (the store falls back to empty paper, FR-026).
    CBREx (width == PrinterGrid::kDotsPerRow, E_INVALIDARG);
    CBREx (height >= 0,                       E_INVALIDARG);
    CBREx (pixels.size () >= (size_t) width * height, E_INVALIDARG);

    outRaster.RestoreFromIndexed (height, pixels, meta.paperRow,
                                  meta.pageBoundaryRows, meta.capReached);

Error:
    return hr;
}
