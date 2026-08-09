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

    struct PageFit
    {
        double  scale = 1.0;   // uniform source->destination scale
        double  destW = 0.0;   // scaled content width  (source units * scale)
        double  destH = 0.0;   // scaled content height (source units * scale)
    };

    // The uniform scale + destination size to draw one strip page of
    // `contentW x contentH` into an `availW x availH` box so a FULL 11" page fits
    // the box height, capped by the box width. Every page of a fanfold job shares
    // this scale: a short last page is NOT enlarged to fill, so all pages keep the
    // same horizontal scale and left edge and line up top-to-bottom. `boxUnitsPerInch`
    // is the box's vertical unit (96 for DIPs, the output dpi for device pixels)
    // so the full-page height lands in the box's own units.
    static PageFit  FitFullPageToBox (double contentW, double contentH,
                                      double availW,   double availH,
                                      double boxUnitsPerInch);
};
