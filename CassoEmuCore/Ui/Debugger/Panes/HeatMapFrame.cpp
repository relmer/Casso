#include "Pch.h"

#include "Ui/Debugger/Panes/HeatMapFrame.h"
#include "Ui/Debugger/Panes/HeatMapBands.h"
#include "Ui/Debugger/Panes/HeatMapCells.h"
#include "Ui/Debugger/Panes/HeatMapGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapFrame::GetPixels
//
//  A change marks the frame stale rather than drawing it again: a wheel spun
//  or a snapshot that came in redrew every pixel of the map each time, and
//  input waited behind it. The frame is drawn once, when it is painted or
//  read.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<uint32_t> & HeatMapFrame::GetPixels (bool isVisible)
{
    if (m_isStale || m_builtRevision != m_grid.GetRevision())
    {
        Build (isVisible);
    }

    return m_frame;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapFrame::Build
//
//  The map's area a pixel at a time, as scrolled: each cell its color, and
//  the street after it, a range's header and the part of its last row past
//  its last address the page.
//
////////////////////////////////////////////////////////////////////////////////

void HeatMapFrame::Build (bool isVisible)
{
    RECT                                       map        = m_grid.GetMapRect();
    long                                       width      = map.right  - map.left;
    long                                       height     = map.bottom - map.top;
    long                                       pitch      = m_grid.GetPitch();
    int                                        cellPx     = m_grid.GetCellPx();
    long                                       rowPx      = m_grid.GetRowPx();
    long                                       scrollTop  = m_grid.GetScrollTop();
    const std::vector<HeatMapBands::Band>    & all        = m_bands.GetAll();
    const std::vector<HeatMapBands::Placed>  & placed     = m_bands.GetPlaced();
    uint32_t                                   background = m_model.GetPalette().background | 0xFF000000u;
    std::vector<std::vector<int>>    columns (all.size());
    std::vector<long>                bands;
    std::vector<int>                 rows;



    //  A pane behind another tab is drawn when it next paints, not on every
    //  snapshot the machine sends meanwhile.
    m_isStale = !isVisible;

    if (m_isStale)
    {
        return;
    }

    m_builtRevision = m_grid.GetRevision();

    if (width <= 0 || height <= 0 || cellPx <= 0)
    {
        m_frame.clear();
        return;
    }

    //  The range and the row each pixel row falls in, or -1 for a street or
    //  a header.
    bands.resize ((size_t) height);
    rows.resize  ((size_t) height);

    for (long y = 0; y < height; y++)
    {
        long  at   = y + scrollTop;
        long  band = m_bands.GetBandAt (at, pitch);
        long  into = (band < 0) ? 0 : at - placed[(size_t) band].rowsTop;



        bands[(size_t) y] = band;
        rows[(size_t) y]  = (band >= 0 && into % pitch < cellPx) ? (int) (into / pitch) : -1;
    }

    m_frame.assign ((size_t) (width * height), background);

    for (long y = 0; y < height; y++)
    {
        uint32_t          * line   = m_frame.data() + (size_t) (y * width);
        int                 row    = rows[(size_t) y];
        long                band   = bands[(size_t) y];
        std::vector<int>  * across = (row < 0) ? nullptr : &columns[(size_t) band];



        if (across == nullptr)
        {
            continue;
        }

        //  The cell each pixel column falls in, or -1 for a street, for each
        //  range as its first row comes into view.
        if (across->empty())
        {
            const HeatMapBands::Placed  & where = placed[(size_t) band];



            across->resize ((size_t) width);

            for (long x = 0; x < width; x++)
            {
                long  column = m_grid.GetColumnAt (where, x);



                (*across)[(size_t) x] = (x < rowPx && x < m_grid.GetColumnLeft (where, column + 1) - HeatMapSections::kStreetPx) ? (int) column : -1;
            }
        }

        for (long x = 0; x < width; x++)
        {
            int   column = (*across)[(size_t) x];
            long  offset = (long) row * placed[(size_t) band].columns + column;



            if (column >= 0 && offset < all[(size_t) band].count)
            {
                line[x] = m_model.GetCellColor ((Word) (all[(size_t) band].first + offset));
            }
        }
    }
}
