#pragma once

#include "Pch.h"

#include "Devices/Printer/ImageWriterInterpreter.h"
#include "Devices/Printer/PrintRaster.h"

class PrinterByteRing;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterJob
//
//  The consumer half of the printer pipeline: owns one strip's interpreter and
//  raster and drains raw bytes out of the card's SPSC ring into them. Pure and
//  system-free (FR-017) so the whole drain is unit-testable -- the Casso shell
//  supplies only the worker thread that calls Drain on a cadence and the
//  platform sinks that render/deliver the finished strip.
//
//  Drain is the single-consumer side of the ring contract: exactly one thread
//  (the printer worker) may call it. Interpreter state persists across calls,
//  so a command split across two drains completes correctly.
//
//  An optional byte observer sees every drained byte in order -- the seam the
//  shell uses for the temporary T011 capture tap and future FR-009 diagnostics,
//  without pulling file I/O into the core.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterJob
{
public:
    explicit PrinterJob (PrinterByteRing & ring) : m_ring (ring) {}

    // Power-on / discard: interpreter back to defaults, paper cleared.
    void    Reset ();

    // Pops every byte currently available and feeds it through the interpreter
    // into the raster, appending presentation events. Returns bytes drained.
    size_t  Drain (vector<PrinterEvent> & outEvents);

    bool                 HasContent () const { return m_raster.RowsUsed () > 0; }
    const PrintRaster &  Raster     () const { return m_raster; }
    PrintRaster &        Raster     ()       { return m_raster; }

    void    SetByteObserver (function<void (const Byte *, size_t)> observer) { m_observer = move (observer); }

private:
    PrinterByteRing &                        m_ring;
    ImageWriterInterpreter                   m_interpreter;
    PrintRaster                              m_raster;
    function<void (const Byte *, size_t)>    m_observer;
};
