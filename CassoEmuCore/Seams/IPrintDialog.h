#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"   // DotStyle

class PrintRaster;





////////////////////////////////////////////////////////////////////////////////
//
//  IPrintDialog
//
//  The operating system's print experience, as one call that either puts it
//  up or does not.
//
//  S_OK means the experience is on screen and now owns the outcome: it posts
//  its own sent-or-failed result when the user is done with it. A failure
//  means it never came up, and the caller falls back to the classic dialog.
//  A test stands in a dialog that answers either way and never shows a thing.
//
////////////////////////////////////////////////////////////////////////////////

class IPrintDialog
{
public:

    virtual ~IPrintDialog () = default;

    virtual HRESULT  ShowAsync (HWND hwnd, const PrintRaster & raster, int outputDpi, DotStyle style) = 0;
};
