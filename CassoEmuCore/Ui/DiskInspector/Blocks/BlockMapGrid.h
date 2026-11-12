#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid
//
//  Where block mode's map draws each block: cells in rows from the top left,
//  as many columns as fit the width at kMinCellPx, and as many blocks to a
//  cell as it takes for every block to fit the height. A cell of several
//  blocks takes the color of its most notable role: damage first, then the
//  volume's structures, then files' data, then free space. Pure.
//
////////////////////////////////////////////////////////////////////////////////

class BlockMapGrid
{
public:
    static constexpr int  kMinCellPx = 6;
    static constexpr int  kGapPx     = 1;

    BlockMapGrid () = default;
    BlockMapGrid (uint32_t blockCount, int widthPx, int heightPx);

    int       GetColumns       () const { return m_columns; }
    int       GetRows          () const { return m_rows; }
    int       GetCellPx        () const { return m_cellPx; }
    uint32_t  GetBlocksPerCell () const { return m_blocksPerCell; }
    uint32_t  GetCellCount     () const { return m_cells; }

    uint32_t  GetCellOf     (uint32_t block) const { return m_blocksPerCell == 0 ? 0 : block / m_blocksPerCell; }
    uint32_t  GetFirstBlock (uint32_t cell)  const { return cell * m_blocksPerCell; }
    uint32_t  GetLastBlock  (uint32_t cell)  const { return std::min (GetFirstBlock (cell) + m_blocksPerCell, m_blockCount) - 1; }

    //  A cell's top left, from the grid's.
    POINT     GetCellOrigin (uint32_t cell) const;

    //  The cell at a point from the grid's top left, or -1. A point in the
    //  gap right of or below a cell is in that cell.
    int64_t   FindCellAt (int xPx, int yPx) const;

    //  A role's name in the map's pointer text and status line.
    static LPCWSTR     GetRoleName (SectorRole role);

private:
    uint32_t  m_blockCount    = 0;
    int       m_columns       = 0;
    int       m_rows          = 0;
    int       m_cellPx        = 0;
    uint32_t  m_blocksPerCell = 1;
    uint32_t  m_cells         = 0;
};
