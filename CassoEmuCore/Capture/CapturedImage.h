#pragma once

#include "Pch.h"

#include "Devices/Printer/RgbaImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedImage
//
//  The pixels of one capture, in flight between the source and the two sinks.
//
//  BGRA, NOT RGBA, AND DELIBERATELY SO. Every producer hands over that order
//  already -- the scene render target is DXGI_FORMAT_B8G8R8A8_UNORM, the back
//  buffer matches it, and the emulator framebuffer's 0xAARRGGBB words are
//  B,G,R,A in memory -- and so does the one sink that matters for speed, since
//  a 32bpp BI_RGB clipboard DIB is BGRA too. Storing RGBA here would mean
//  converting on the way in and converting straight back for the clipboard.
//  The PNG encoder is the only consumer that wants the other order, so it is
//  the only place a conversion happens.
//
//  Rows are TOP-DOWN and tightly packed. A staging texture's row pitch is not
//  width * 4, and a DIB is bottom-up; both are dealt with at their own edges
//  rather than carried around in this struct as another thing to remember.
//
////////////////////////////////////////////////////////////////////////////////

struct CapturedImage
{
    int             widthPx  = 0;
    int             heightPx = 0;
    vector<Byte>    bgra;                 // widthPx * heightPx * 4, order B,G,R,A

    static constexpr int  kBytesPerPixel = 4;

    bool  IsValid() const;

    // BGRA to the RGBA the PNG encoder takes. A separate function with its own
    // test because a channel swap is invisible until someone looks at a
    // screenshot and cannot say why the sky is orange.
    static HRESULT  ToRgbaImage (const CapturedImage & source, RgbaImage & outImage);
};
