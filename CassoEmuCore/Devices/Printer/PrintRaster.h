#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrintRaster
//
//  The native-grid fanfold strip: one byte per dot cell (kDotsPerRow cells
//  per row), the low four bits an InkPrimary bitfield. Overprinting simply
//  ORs the struck primary into the cell, so composite colours (orange, green,
//  purple) and black-dominance fall out at render time exactly as on paper --
//  strike history is never lost.
//
//  Positioning: the raster owns the paper feed position. Strikes address an
//  absolute strip row (the interpreter strikes at PaperRow() plus the dot's
//  vertical offset); AdvanceRows feeds the paper, extending the used extent
//  even across blank line feeds; MarkFormFeed advances to the next form-length
//  boundary and records it for pagination. Growth is chunked by page length.
//
//  Pure and system-free (FR-017): the renderer and serializer read cells back
//  out; the shell owns all delivery.
//
////////////////////////////////////////////////////////////////////////////////

class PrintRaster
{
public:
    void    Strike            (int columnDot, int row, InkPrimary ink);
    void    AdvanceRows       (int rows);
    void    MarkFormFeed      ();
    void    Clear             ();

    // Persistence restore: repopulate the whole strip from a native-grid index
    // plane (kDotsPerRow cells per row, one 4-bit ink value each) and saved
    // feed state. Used by PrintJobSerializer when reloading a pending strip.
    void    RestoreFromIndexed (int rows, const vector<Byte> & cells,
                                int paperRow, const vector<int> & boundaries,
                                bool capReached);

    // Copy rows [firstRow, lastRow] into `out`, rebased so out's row 0 is this
    // strip's firstRow; page boundaries falling inside the span carry over
    // (rebased). Bounded by the span, never the whole strip -- this is the
    // live preview's per-refresh snapshot (FR-033), so a 60-page banner costs
    // the same to snapshot as a receipt. Out-of-range spans yield an empty out.
    void    CopyRowSpan       (int firstRow, int lastRow, PrintRaster & out) const;

    int     PaperRow          () const { return m_paperRow; }
    int     RowsUsed          () const { return m_rowsUsed; }
    bool    CapReached        () const { return m_capReached; }

    const vector<int> & PageBoundaryRows () const { return m_pageBoundaryRows; }

    // Ink bitfield at a cell; 0 (paper white) for any out-of-range or
    // never-allocated position.
    Byte    CellAt            (int columnDot, int row) const;

private:
    void    EnsureRowAllocated (int row);

    vector<Byte>   m_cells;                 // row-major, kDotsPerRow per row
    int            m_paperRow         = 0;   // current feed position
    int            m_rowsUsed         = 0;   // high-water extent
    bool           m_capReached       = false;
    vector<int>    m_pageBoundaryRows;       // form-feed boundary rows
};
