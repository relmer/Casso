#include "Pch.h"

#include "Print/PrintJobStore.h"

#include "Devices/Printer/PrintJobPersistence.h"
#include "Devices/Printer/PrintRaster.h"




static const wchar_t   s_kszStripPng[]  = L"strip.png";
static const wchar_t   s_kszStripJson[] = L"strip.json";





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ReadAllBytes (const fs::path & path, vector<Byte> & out)
{
    HRESULT         hr       = S_OK;
    bool            isOpen   = false;
    bool            readWell = false;
    std::ifstream   in (path, std::ios::binary);



    isOpen = in.is_open();
    CBR (isOpen);

    out.assign ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char> ());

    readWell = in.good() || in.eof();
    CBR (readWell);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadAllText
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ReadAllText (const fs::path & path, string & out)
{
    HRESULT         hr       = S_OK;
    bool            isOpen   = false;
    bool            readWell = false;
    std::ifstream   in (path, std::ios::binary);



    isOpen = in.is_open();
    CBR (isOpen);

    out.assign ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char> ());

    readWell = in.good() || in.eof();
    CBR (readWell);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteAllBytes
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteAllBytes (const fs::path & path, const vector<Byte> & bytes)
{
    HRESULT         hr        = S_OK;
    bool            isOpen    = false;
    bool            wroteWell = false;
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);



    isOpen = out.is_open();
    CBR (isOpen);

    out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());

    wroteWell = out.good();
    CBR (wroteWell);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteAllText
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT WriteAllText (const fs::path & path, const string & text)
{
    HRESULT         hr        = S_OK;
    bool            isOpen    = false;
    bool            wroteWell = false;
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);



    isOpen = out.is_open();
    CBR (isOpen);

    out.write (text.data(), (std::streamsize) text.size());

    wroteWell = out.good();
    CBR (wroteWell);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Load
//
//  Reads a pending print strip's two sidecar files from a directory and
//  rebuilds the raster.
//
//  BOTH files must exist. A strip is only meaningful with its metadata, so
//  finding one without the other is treated as nothing pending rather than
//  attempting a partial rebuild with invented page boundaries.
//
//  That case returns ERROR_FILE_NOT_FOUND, which the caller reads as a clean
//  first run rather than a failure -- it is the normal state for a machine
//  that has never printed. It is still not a raster the caller can use, hence
//  a failing code rather than S_OK.
//
//  This layer is only file plumbing; the decode and the rebuild live in
//  PrintJobPersistence, so the format is testable from bytes with no
//  filesystem at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobStore::Load (const fs::path & dir, PrintRaster & outRaster)
{
    HRESULT           hr    = S_OK;
    fs::path          png   = dir / s_kszStripPng;
    fs::path          json  = dir / s_kszStripJson;
    std::error_code   ec;
    bool              have  = false;
    vector<Byte>      pngBytes;
    string            jsonText;



    // No pending strip -- a clean first-run open, not a load failure, but
    // it still is not a raster the caller can use.
    have = fs::exists (png, ec) && fs::exists (json, ec);
    BAIL_OUT_IF (!have, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = ReadAllBytes (png, pngBytes);
    CHR (hr);

    hr = ReadAllText (json, jsonText);
    CHR (hr);

    hr = PrintJobPersistence::Load (pngBytes, jsonText, outRaster);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PrintJobStore::Save (const fs::path & dir, const PrintRaster & raster)
{
    HRESULT           hr   = S_OK;
    std::error_code   ec;
    vector<Byte>      png;
    string            json;



    hr = PrintJobPersistence::Save (raster, png, json);
    CHR (hr);

    fs::create_directories (dir, ec);

    hr = WriteAllBytes (dir / s_kszStripPng, png);
    CHR (hr);

    hr = WriteAllText (dir / s_kszStripJson, json);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
////////////////////////////////////////////////////////////////////////////////

void PrintJobStore::Clear (const fs::path & dir)
{
    std::error_code   ec;



    fs::remove (dir / s_kszStripPng, ec);
    fs::remove (dir / s_kszStripJson, ec);
}
