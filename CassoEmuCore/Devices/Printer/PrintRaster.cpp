#include "Pch.h"

#include "Devices/Printer/PrintRaster.h"




static constexpr int   s_kColumns     = PrinterGrid::kDotsPerRow;
static constexpr int   s_kPageRows    = PrinterGrid::kPageRows;
static constexpr int   s_kMaxRows     = PrinterGrid::kMaxStripRows;




////////////////////////////////////////////////////////////////////////////////
//
//  EnsureRowAllocated
//
//  Grows the cell store so `row` is addressable. Growth is chunked to whole
//  form lengths to keep striking amortised O(1).
//
////////////////////////////////////////////////////////////////////////////////

void PrintRaster::EnsureRowAllocated (int row)
{
    int   neededRows = 0;
    int   chunkRows  = 0;

    if ((size_t) (row + 1) * s_kColumns <= m_cells.size ())
    {
        return;
    }

    neededRows = row + 1;
    chunkRows  = ((neededRows + s_kPageRows - 1) / s_kPageRows) * s_kPageRows;

    m_cells.resize ((size_t) chunkRows * s_kColumns, 0);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Strike
//
//  ORs an ink primary into a cell. Out-of-range columns are ignored; a row at
//  or beyond the 60-page cap is dropped and latches CapReached.
//
////////////////////////////////////////////////////////////////////////////////

void PrintRaster::Strike (int columnDot, int row, InkPrimary ink)
{
    if (columnDot < 0 || columnDot >= s_kColumns || row < 0)
    {
        return;
    }

    if (row >= s_kMaxRows)
    {
        m_capReached = true;
        return;
    }

    EnsureRowAllocated (row);

    m_cells[(size_t) row * s_kColumns + columnDot] |= (Byte) ink;

    if (row + 1 > m_rowsUsed)
    {
        m_rowsUsed = row + 1;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  AdvanceRows
//
//  Feeds the paper, extending the used extent even when the advanced-over rows
//  are never struck (blank line feeds must survive to the delivered page).
//
////////////////////////////////////////////////////////////////////////////////

void PrintRaster::AdvanceRows (int rows)
{
    if (rows <= 0)
    {
        return;
    }

    m_paperRow += rows;

    if (m_paperRow >= s_kMaxRows)
    {
        m_paperRow   = s_kMaxRows;
        m_capReached = true;
    }

    if (m_paperRow > m_rowsUsed)
    {
        m_rowsUsed = m_paperRow;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  MarkFormFeed
//
//  Advances the paper to the next form-length boundary and records it so the
//  Windows-printer path can paginate and the panel can draw perforations.
//
////////////////////////////////////////////////////////////////////////////////

void PrintRaster::MarkFormFeed ()
{
    int   nextTop = ((m_paperRow / s_kPageRows) + 1) * s_kPageRows;

    if (nextTop >= s_kMaxRows)
    {
        nextTop      = s_kMaxRows;
        m_capReached = true;
    }

    m_paperRow = nextTop;

    if (m_paperRow > m_rowsUsed)
    {
        m_rowsUsed = m_paperRow;
    }

    m_pageBoundaryRows.push_back (nextTop);
}




////////////////////////////////////////////////////////////////////////////////
//
//  Clear
//
//  Resets to an empty strip (eject / discard delivered the old one).
//
////////////////////////////////////////////////////////////////////////////////

void PrintRaster::Clear ()
{
    m_cells.clear ();
    m_pageBoundaryRows.clear ();
    m_paperRow   = 0;
    m_rowsUsed   = 0;
    m_capReached = false;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CellAt
//
////////////////////////////////////////////////////////////////////////////////

Byte PrintRaster::CellAt (int columnDot, int row) const
{
    size_t   index = 0;

    if (columnDot < 0 || columnDot >= s_kColumns || row < 0)
    {
        return 0;
    }

    index = (size_t) row * s_kColumns + columnDot;

    if (index >= m_cells.size ())
    {
        return 0;
    }

    return m_cells[index];
}
