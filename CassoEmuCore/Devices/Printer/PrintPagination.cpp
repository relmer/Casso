#include "Pch.h"

#include "Devices/Printer/PrintPagination.h"

#include "Devices/Printer/PrinterTypes.h"
#include "Devices/Printer/PrintRaster.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PrintPagination::Paginate
//
//  Splits a continuous fanfold strip into printable pages.
//
//  TWO independent things cause a page break, and both must be honored. A
//  guest form feed is a HARD break the program asked for; a segment longer
//  than a physical sheet also has to spill onto further sheets. Handling only
//  form feeds would print a ten-foot banner onto one page; handling only the
//  page height would ignore the program's own layout.
//
//  So form-feed rows become segment boundaries, and each segment is then cut
//  at the page height. A segment shorter than a sheet yields one short page,
//  which is correct -- that is what a form feed means.
//
//  The break list is bracketed with 0 and the row count so every segment has
//  both a start and an end, then sorted and deduplicated so a form feed
//  landing exactly on a strip end cannot produce an empty page.
//
//  An EMPTY strip paginates to no pages at all, not one blank one -- printing
//  nothing should send nothing to the printer.
//
//  Ranges are inclusive of the last row, matching how the raster is indexed.
//
////////////////////////////////////////////////////////////////////////////////

vector<PrintPagination::PageRange> PrintPagination::Paginate (const PrintRaster & raster)
{
    vector<PageRange>   pages;
    vector<int>         breaks;
    int                 rowsUsed = raster.GetRowsUsed();



    // An empty strip paginates to no pages at all, not one blank one.
    if (rowsUsed > 0)
    {
        // Guest form feeds are hard page breaks; bracket them with the strip
        // ends so every segment has both a start and an end.
        breaks.push_back (0);

        for (int b : raster.GetPageBoundaryRows())
        {
            if (b > 0 && b < rowsUsed)
            {
                breaks.push_back (b);
            }
        }

        breaks.push_back (rowsUsed);

        std::sort (breaks.begin(), breaks.end());
        breaks.erase (std::unique (breaks.begin(), breaks.end()), breaks.end());

        // Within each form-feed segment, cap every page at one physical page
        // height -- a segment longer than a sheet spills onto further sheets.
        for (size_t i = 0; i + 1 < breaks.size(); i++)
        {
            int   segStart = breaks[i];
            int   segEnd   = breaks[i + 1];   // exclusive

            for (int r = segStart; r < segEnd; r += PrinterGrid::kPageRows)
            {
                int   last = std::min (r + PrinterGrid::kPageRows, segEnd) - 1;

                pages.push_back (PageRange { r, last });
            }
        }
    }

    return pages;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrintPagination::FitFullPageToBox
//
//  Fit a FULL page to the box height, capped by the box width, at one uniform
//  scale. The ImageWriter's 8"-wide content is a narrower aspect than Letter, so
//  fitting to width alone scales an 11" page to ~11.7" and spills its bottom onto
//  a second sheet; fitting the full page height prints it at true size, and the
//  same scale on every page keeps the fanfold columns aligned page-to-page.
//
////////////////////////////////////////////////////////////////////////////////

PrintPagination::PageFit PrintPagination::FitFullPageToBox (double contentW, double contentH,
                                                            double availW,   double availH,
                                                            double boxUnitsPerInch)
{
    PageFit   fit;
    double    fullPageH = (double) PrinterGrid::kPageRows / (double) PrinterGrid::kRowsPerInch * boxUnitsPerInch;
    double    scaleW    = (contentW  > 0.0) ? (availW / contentW)  : 1.0;
    double    scaleH    = (fullPageH > 0.0) ? (availH / fullPageH) : scaleW;



    fit.scale = (std::min) (scaleW, scaleH);

    if (fit.scale <= 0.0)
    {
        fit.scale = 1.0;
    }

    fit.destW = contentW * fit.scale;
    fit.destH = contentH * fit.scale;

    return fit;
}
