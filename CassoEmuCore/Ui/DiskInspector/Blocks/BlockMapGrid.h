#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid
//
//  Where block mode's map draws each block: the blocks in rows from the top
//  left, in a fixed number of columns, and a zoom draws the same rows
//  larger, so the blocks under the pointer stay under it, and every block
//  larger than the last zoom drew it. A block is a cell; only where a block
//  would be smaller than a pixel is a square of blocks one cell. Cells are
//  drawn apart by kGapPx once they are kGapFromPx across, and touch below
//  that, so a step of the zoom never draws a cell smaller.
//  A cell of several blocks takes the color of its most notable role:
//  damage first, then the volume's structures, then files' data, then free
//  space. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockMapGrid
{
public:
    static constexpr int  kMinCellPx = 6;
    static constexpr int  kGapPx     = 1;
    static constexpr int  kGapFromPx = 6;

    BlockMapGrid () = default;

    //  The whole volume fitted to an area, and the same rows of blocks with
    //  each block `blockPx` across, as a zoom draws them.
    BlockMapGrid (uint32_t blockCount, int widthPx, int heightPx);
    BlockMapGrid (uint32_t blockCount, int blockColumns, double blockPx);

    int       GetColumns       () const { return m_columns; }
    int       GetRows          () const { return m_rows; }
    int       GetCellPx        () const { return std::max (1, (int) std::floor (m_pitch) - GetGapPx()); }
    int       GetGapPx         () const { return m_pitch >= kGapFromPx ? kGapPx : 0; }
    double    GetPitch         () const { return m_pitch; }
    uint32_t  GetGroup         () const { return m_group; }
    uint32_t  GetBlocksPerCell () const { return m_group * m_group; }
    uint32_t  GetCellCount     () const { return m_cells; }
    int       GetBlockColumns  () const { return m_blockColumns; }
    double    GetBlockPx       () const { return m_blockPx; }

    uint32_t  GetCellOf     (uint32_t block) const;
    uint32_t  GetFirstBlock (uint32_t cell) const;
    uint32_t  GetLastBlock  (uint32_t cell) const;
    uint32_t  CountBlocks   (uint32_t cell) const;
    bool      HasBlocks     (uint32_t cell) const { return GetFirstBlock (cell) < m_blockCount; }

    //  A cell's top left, and its rectangle, from the grid's. Each edge is
    //  on a whole pixel, so every gap between cells is kGapPx and the cells
    //  differ in size by at most a pixel.
    POINT     GetCellOrigin (uint32_t cell) const;
    RECT      GetCellRect   (uint32_t cell) const;

    //  The cell at a point from the grid's top left, or -1. A point in the
    //  gap right of or below a cell is in that cell.
    int64_t   FindCellAt (int xPx, int yPx) const;

    //  A role's name in the map's pointer text and status line.
    static LPCWSTR     GetRoleName (SectorRole role);

private:
    void  Arrange (double blockPx, uint32_t group);

    uint32_t  m_blockCount   = 0;
    int       m_blockColumns = 1;
    int       m_blockRows    = 0;
    double    m_blockPx      = 0.0;
    uint32_t  m_group        = 1;      // the blocks across a cell, and down it
    double    m_pitch        = kMinCellPx + kGapPx;   // a cell and its gap, across
    int       m_columns      = 0;
    int       m_rows         = 0;
    uint32_t  m_cells        = 0;
};
