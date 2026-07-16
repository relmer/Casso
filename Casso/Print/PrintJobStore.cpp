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
    HRESULT         hr = S_OK;
    std::ifstream   in (path, std::ios::binary);

    CBR (in.is_open());
    out.assign ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char> ());
    CBR (in.good() || in.eof());

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
    HRESULT         hr = S_OK;
    std::ifstream   in (path, std::ios::binary);

    CBR (in.is_open());
    out.assign ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char> ());
    CBR (in.good() || in.eof());

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
    HRESULT         hr = S_OK;
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);

    CBR (out.is_open());
    out.write ((const char *) bytes.data(), (std::streamsize) bytes.size());
    CBR (out.good());

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
    HRESULT         hr = S_OK;
    std::ofstream   out (path, std::ios::binary | std::ios::trunc);

    CBR (out.is_open());
    out.write (text.data(), (std::streamsize) text.size());
    CBR (out.good());

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

    have = fs::exists (png, ec) && fs::exists (json, ec);
    CBRF (have, hr = S_FALSE);   // no pending strip -- clean first-run open

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
