#pragma once

#include "Pch.h"

class PrintRaster;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintPagination
//
//  Slices the continuous fanfold strip into physical printer pages for the
//  Windows-printer destination (FR-014: pagination applies ONLY to the Windows
//  printer; PNG / clipboard stay one continuous image). Pure and system-free so
//  the page arithmetic is unit-tested; the shell's GDI sink renders each range.
//
//  A page breaks at the greater of two rules: the guest's own form-feed page
//  boundaries (PrintRaster::PageBoundaryRows) are hard breaks, and no page runs
//  longer than one physical page (PrinterGrid::kPageRows). Rows are inclusive
//  absolute strip rows.
//
////////////////////////////////////////////////////////////////////////////////

class PrintPagination
{
public:
    struct PageRange
    {
        int  firstRow = 0;   // inclusive
        int  lastRow  = 0;   // inclusive
    };

    // Ordered top-to-bottom; empty when the strip has no used rows.
    static vector<PageRange>  Paginate (const PrintRaster & raster);
};
