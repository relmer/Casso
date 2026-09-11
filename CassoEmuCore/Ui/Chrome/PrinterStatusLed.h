#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterStatusModel.h"   // PrinterStatus
#include "Widgets/DxuiToolbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterStatusLed
//
//  The status light on the toolbar's printer entry: a small dot riding the
//  glyph's corner, unlit while idle (no light = no problem), bright green
//  while receiving a print, bright amber while a finished page waits, bright
//  red after a delivery error. It paints as a toolbar decoration, so it
//  knows where the glyph was drawn and nothing else about the entry.
//
//  Whether the machine has a printer card lives here too, since the same
//  entry's enabled state reads it.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterStatusLed
{
public:
    void           SetStatus  (PrinterStatus status)   { m_status = status; }
    PrinterStatus  GetStatus  () const                 { return m_status; }
    void           SetPresent (bool present)           { m_present = present; }
    bool           IsPresent  () const                 { return m_present; }

    // PrinterStatus -> LED core color, 0 while idle.
    static uint32_t  GetStatusCoreColor (PrinterStatus status);

    // A halo + core dot at (cx, cy) whose radius follows the DPI; core == 0
    // paints nothing at all.
    static void      Paint (IDxuiPainter & painter, float cx, float cy, UINT dpi, uint32_t core);

    // The decoration the toolbar calls after the printer glyph. Bound to
    // this object, so it must outlive the toolbar's entry table.
    DxuiToolbar::DecorationFn  MakeDecoration ();

private:
    PrinterStatus  m_status  = PrinterStatus::Idle;
    bool           m_present = false;
};
