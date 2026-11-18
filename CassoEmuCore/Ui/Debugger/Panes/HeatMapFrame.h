#pragma once

class HeatMapBands;
class HeatMapCells;
class HeatMapGrid;





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapFrame
//
//  The picture of the heat map's cells in view, a pixel at a time, the size
//  of the map's area. A change to the cells marks it stale, and so does a new
//  revision of the grid; it is drawn again when it is next read, and only
//  while the pane is visible.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapFrame
{
public:
    HeatMapFrame (const HeatMapGrid & grid, const HeatMapBands & bands, const HeatMapCells & model) :
        m_grid  (grid),
        m_bands (bands),
        m_model (model)
    {
    }

    const std::vector<uint32_t> &  GetPixels (bool isVisible);
    void                           MarkStale () { m_isStale = true; }

private:
    void  Build (bool isVisible);

    const HeatMapGrid      & m_grid;
    const HeatMapBands     & m_bands;
    const HeatMapCells     & m_model;
    bool                     m_isStale       = false;
    uint64_t                 m_builtRevision = 0;   // the grid's revision the frame was last drawn at
    std::vector<uint32_t>    m_frame;
};
