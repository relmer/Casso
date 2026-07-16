#include "Pch.h"

#include "Devices/Printer/PrintPagination.h"

#include "Devices/Printer/PrinterTypes.h"
#include "Devices/Printer/PrintRaster.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrintPagination::Paginate
//
////////////////////////////////////////////////////////////////////////////////

vector<PrintPagination::PageRange> PrintPagination::Paginate (const PrintRaster & raster)
{
    vector<PageRange>   pages;
    vector<int>         breaks;
    int                 rowsUsed = raster.RowsUsed ();

    if (rowsUsed <= 0)
    {
        return pages;
    }

    // Guest form feeds are hard page breaks; bracket them with the strip ends.
    breaks.push_back (0);

    for (int b : raster.PageBoundaryRows ())
    {
        if (b > 0 && b < rowsUsed)
        {
            breaks.push_back (b);
        }
    }

    breaks.push_back (rowsUsed);

    std::sort (breaks.begin (), breaks.end ());
    breaks.erase (std::unique (breaks.begin (), breaks.end ()), breaks.end ());

    // Within each form-feed segment, cap every page at one physical page height.
    for (size_t i = 0; i + 1 < breaks.size (); i++)
    {
        int   segStart = breaks[i];
        int   segEnd   = breaks[i + 1];   // exclusive

        for (int r = segStart; r < segEnd; r += PrinterGrid::kPageRows)
        {
            int   last = std::min (r + PrinterGrid::kPageRows, segEnd) - 1;

            pages.push_back (PageRange { r, last });
        }
    }

    return pages;
}
