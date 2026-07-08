#pragma once

#include "Pch.h"

#include "Devices/Printer/RgbaImage.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PngCodec
//
//  In-memory PNG encode/decode over WIC, living in the core lib so both the
//  thin exe and the UnitTest project link it -- the delivered-image sink and
//  the pending-strip persistence both go through here, and the round trip is
//  unit-tested (research R-007, relocated from the shell into core).
//
//  All buffers are in memory: nothing here touches the filesystem, the
//  clipboard, or a print dialog -- those irreducible platform steps stay in
//  the shell. The caller must have initialised COM on the calling thread
//  (CoInitializeEx); the codec does not, so it composes with whatever
//  apartment the worker or test already established.
//
////////////////////////////////////////////////////////////////////////////////

class PngCodec
{
public:
    // 32bpp RGBA truecolour PNG, physical resolution stamped as pHYs (dpi).
    static HRESULT EncodeRgba (const RgbaImage & image, int dpi, vector<Byte> & outPng);

    // 8bpp palette-indexed PNG (lossless index preservation). `palette` holds
    // `paletteCount` (<=256) entries as 0xAARRGGBB; `indices` is width*height
    // bytes. Used for the persisted native-grid strip.
    static HRESULT EncodeIndexed (int                  width,
                                  int                  height,
                                  const vector<Byte> & indices,
                                  const uint32_t *     palette,
                                  int                  paletteCount,
                                  int                  dpi,
                                  vector<Byte> &       outPng);

    // Decode any PNG to 32bpp RGBA.
    static HRESULT DecodeRgba (const vector<Byte> & png, RgbaImage & outImage);

    // Recover the 8bpp index plane from a PNG encoded by EncodeIndexed.
    static HRESULT DecodeIndexed (const vector<Byte> & png,
                                  int &                outWidth,
                                  int &                outHeight,
                                  vector<Byte> &       outIndices);

    // Physical resolution (pHYs) in dpi; 0 if the image carries none.
    static HRESULT ReadDpi (const vector<Byte> & png, int & outDpi);

private:
    static HRESULT CreateFactory     (ComPtr<IWICImagingFactory> & outFactory);
    static HRESULT DecodeFirstFrame  (const vector<Byte> &            png,
                                      ComPtr<IWICImagingFactory> &    outFactory,
                                      ComPtr<IWICBitmapFrameDecode> & outFrame);
};
