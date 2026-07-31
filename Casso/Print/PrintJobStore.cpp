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
    std::ifstream   in (path, std::ios::binary);
    bool            isOpen   = false;
    bool            readWell = false;



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
    std::ifstream   in (path, std::ios::binary);
    bool            isOpen   = false;
    bool            readWell = false;



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
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);
    bool            isOpen    = false;
    bool            wroteWell = false;



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
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);
    bool            isOpen    = false;
    bool            wroteWell = false;



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
