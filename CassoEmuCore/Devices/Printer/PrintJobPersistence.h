#pragma once

#include "Pch.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintJobPersistence
//
//  Composes PrintJobSerializer and PngCodec into the two persistence buffers a
//  pending strip round-trips through: the native-grid indexed PNG and the JSON
//  sidecar (contracts/printing-settings.md). Pure over in-memory buffers, so
//  the shell's PrintJobStore reduces to writing/reading the two files -- the
//  whole serialize->encode->decode->rebuild path is unit-tested here.
//
//  The caller must have COM initialised on the thread (PngCodec uses WIC).
//
////////////////////////////////////////////////////////////////////////////////

class PrintJobPersistence
{
public:
    // strip -> (strip.png bytes, strip.json text).
    static HRESULT Save (const PrintRaster & raster, vector<Byte> & outPng, string & outJson);

    // (strip.png bytes, strip.json text) -> strip. Any decode/parse/shape
    // failure is reported so the store can fall back to empty paper (FR-026).
    static HRESULT Load (const vector<Byte> & png, const string & json, PrintRaster & outRaster);
};
