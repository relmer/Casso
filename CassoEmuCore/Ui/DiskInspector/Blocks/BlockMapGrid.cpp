#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::BlockMapGrid
//
//  The whole volume fitted to an area: the blocks in rows as wide for their
//  height as the area is, each block as large as the area allows, and
//  squares of blocks joined into a cell where a block alone would be less
//  than a pixel.
//
////////////////////////////////////////////////////////////////////////////////

BlockMapGrid::BlockMapGrid (uint32_t blockCount, int widthPx, int heightPx) :
    m_blockCount (blockCount)
{
    double    blockPx = 0.0;
    uint32_t  group   = 1;



    if (blockCount == 0 || widthPx < kMinCellPx || heightPx < kMinCellPx)
    {
        return;
    }

    m_blockColumns = (int) std::clamp<double> (std::ceil (std::sqrt ((double) blockCount * widthPx / heightPx)), 1.0, (double) blockCount);
    m_blockRows    = (int) ((blockCount + (uint32_t) m_blockColumns - 1) / (uint32_t) m_blockColumns);

    //  The block's size with the squares rounded up, so the last row and
    //  column of cells fit the area too.
    for (group = 1; group <= blockCount; group++)
    {
        blockPx = std::min ((double) widthPx  / ((m_blockColumns + group - 1) / group * group),
                            (double) heightPx / ((m_blockRows    + group - 1) / group * group));

        if (group * blockPx >= 1.0)
        {
            break;
        }
    }

    Arrange (blockPx, group);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::BlockMapGrid
//
//  The same layout of blocks at another size, as a zoom draws it: a block
//  a cell, or where a block is less than a pixel, the least square of
//  blocks that is a pixel.
//
////////////////////////////////////////////////////////////////////////////////

BlockMapGrid::BlockMapGrid (uint32_t blockCount, int blockColumns, double blockPx) :
    m_blockCount   (blockCount),
    m_blockColumns (std::max (1, blockColumns))
{
    if (blockCount == 0 || blockPx <= 0.0)
    {
        return;
    }

    m_blockRows = (int) ((blockCount + (uint32_t) m_blockColumns - 1) / (uint32_t) m_blockColumns);

    Arrange (blockPx, (uint32_t) std::max (1.0, std::ceil (1.0 / blockPx)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::Arrange
//
//  Cells of `group` by `group` blocks, `group` blocks across apart.
//
////////////////////////////////////////////////////////////////////////////////

void BlockMapGrid::Arrange (double blockPx, uint32_t group)
{
    m_blockPx = blockPx;
    m_group   = std::max<uint32_t> (1, group);
    m_pitch   = m_group * blockPx;
    m_columns = (int) ((m_blockColumns + m_group - 1) / m_group);
    m_rows    = (int) ((m_blockRows + m_group - 1) / m_group);
    m_cells   = (uint32_t) m_columns * (uint32_t) m_rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetCellOf
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapGrid::GetCellOf (uint32_t block) const
{
    uint32_t  row    = block / (uint32_t) m_blockColumns;
    uint32_t  column = block % (uint32_t) m_blockColumns;



    return m_columns == 0 ? 0 : (row / m_group) * (uint32_t) m_columns + column / m_group;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetFirstBlock
//
//  The cell's top left block, which for a cell past the volume's end is
//  past it too.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapGrid::GetFirstBlock (uint32_t cell) const
{
    uint32_t  row    = m_columns == 0 ? 0 : cell / (uint32_t) m_columns * m_group;
    uint32_t  column = m_columns == 0 ? 0 : cell % (uint32_t) m_columns * m_group;



    return row * (uint32_t) m_blockColumns + column;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetLastBlock
//
//  The cell's bottom right block within the volume.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapGrid::GetLastBlock (uint32_t cell) const
{
    uint32_t  first  = GetFirstBlock (cell);
    uint32_t  row    = first / (uint32_t) m_blockColumns;
    uint32_t  column = first % (uint32_t) m_blockColumns;
    uint32_t  last   = 0;



    if (m_blockCount == 0)
    {
        return 0;
    }

    last = (std::min<uint32_t> (row + m_group, (uint32_t) m_blockRows) - 1) * (uint32_t) m_blockColumns + std::min<uint32_t> (column + m_group, (uint32_t) m_blockColumns) - 1;

    return std::min (last, m_blockCount - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::CountBlocks
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockMapGrid::CountBlocks (uint32_t cell) const
{
    uint32_t  first  = GetFirstBlock (cell);
    uint32_t  row    = first / (uint32_t) m_blockColumns;
    uint32_t  column = first % (uint32_t) m_blockColumns;
    uint32_t  count  = 0;
    uint32_t  r      = 0;
    uint32_t  block  = 0;



    for (r = row; r < row + m_group && r < (uint32_t) m_blockRows; r++)
    {
        block  = r * (uint32_t) m_blockColumns + column;
        count += block >= m_blockCount ? 0 : std::min<uint32_t> ({ m_group, (uint32_t) m_blockColumns - column, m_blockCount - block });
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetCellOrigin
//
////////////////////////////////////////////////////////////////////////////////

POINT BlockMapGrid::GetCellOrigin (uint32_t cell) const
{
    if (m_columns == 0)
    {
        return {};
    }

    return { (LONG) std::floor ((cell % (uint32_t) m_columns) * m_pitch), (LONG) std::floor ((cell / (uint32_t) m_columns) * m_pitch) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetCellRect
//
////////////////////////////////////////////////////////////////////////////////

RECT BlockMapGrid::GetCellRect (uint32_t cell) const
{
    POINT  at     = GetCellOrigin (cell);
    LONG   right  = 0;
    LONG   bottom = 0;



    if (m_columns == 0)
    {
        return {};
    }

    right  = (LONG) std::floor ((cell % (uint32_t) m_columns + 1) * m_pitch) - GetGapPx();
    bottom = (LONG) std::floor ((cell / (uint32_t) m_columns + 1) * m_pitch) - GetGapPx();

    return { at.x, at.y, std::max (at.x + 1, right), std::max (at.y + 1, bottom) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::FindCellAt
//
////////////////////////////////////////////////////////////////////////////////

int64_t BlockMapGrid::FindCellAt (int xPx, int yPx) const
{
    int       column = 0;
    int       row    = 0;
    uint64_t  cell   = 0;



    if (m_columns == 0 || xPx < 0 || yPx < 0)
    {
        return -1;
    }

    //  Each cell starts on the whole pixel at or left of its place, so a
    //  point left of the next cell's place may already be in it.
    column = (int) (xPx / m_pitch);
    row    = (int) (yPx / m_pitch);
    column = std::floor ((column + 1) * m_pitch) <= xPx ? column + 1 : column;
    row    = std::floor ((row + 1) * m_pitch) <= yPx ? row + 1 : row;
    cell   = (uint64_t) row * (uint64_t) m_columns + (uint64_t) column;

    if (column >= m_columns || cell >= m_cells || !HasBlocks ((uint32_t) cell))
    {
        return -1;
    }

    return (int64_t) cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetRoleName
//
////////////////////////////////////////////////////////////////////////////////

LPCWSTR BlockMapGrid::GetRoleName (SectorRole role)
{
    switch (role)
    {
        case SectorRole::BootArea:            return L"Boot blocks";
        case SectorRole::VtocOrKeyBlock:      return L"Volume header";
        case SectorRole::CatalogOrDirectory:  return L"Directory";
        case SectorRole::UnusedCatalogSector: return L"Unused directory";
        case SectorRole::VolumeBitmap:        return L"Volume bitmap";
        case SectorRole::Subdirectory:        return L"Subdirectory";
        case SectorRole::IndexBlock:          return L"Index block";
        case SectorRole::FileData:            return L"File data";
        case SectorRole::BadBlocksFile:       return L"Bad blocks";
        case SectorRole::AllocatedUnowned:    return L"Marked used, in no file";
        case SectorRole::OwnedMarkedFree:     return L"In a file, marked free";
        case SectorRole::CrossLinked:         return L"In two or more files";
        case SectorRole::Free:                return L"Free";
        case SectorRole::Count:               break;
    }

    return L"Free";
}
