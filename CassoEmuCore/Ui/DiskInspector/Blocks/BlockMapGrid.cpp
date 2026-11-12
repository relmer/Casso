#include "Pch.h"

#include "Ui/DiskInspector/Blocks/BlockMapGrid.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::BlockMapGrid
//
//  The cells are as large as the area allows with one block a cell, down to
//  kMinCellPx; past that, a cell has as many blocks as it takes to fit.
//
////////////////////////////////////////////////////////////////////////////////

BlockMapGrid::BlockMapGrid (uint32_t blockCount, int widthPx, int heightPx) :
    m_blockCount (blockCount)
{
    int       pitch    = 0;
    uint64_t  capacity = 0;



    if (blockCount == 0 || widthPx < kMinCellPx || heightPx < kMinCellPx)
    {
        return;
    }

    for (m_cellPx = std::min (widthPx, heightPx); m_cellPx > kMinCellPx; m_cellPx--)
    {
        pitch    = m_cellPx + kGapPx;
        capacity = (uint64_t) ((widthPx + kGapPx) / pitch) * (uint64_t) ((heightPx + kGapPx) / pitch);

        if (capacity >= blockCount)
        {
            break;
        }
    }

    pitch     = m_cellPx + kGapPx;
    m_columns = std::max (1, (widthPx + kGapPx) / pitch);
    capacity  = (uint64_t) m_columns * (uint64_t) std::max (1, (heightPx + kGapPx) / pitch);

    m_blocksPerCell = (uint32_t) ((blockCount + capacity - 1) / capacity);
    m_cells         = (blockCount + m_blocksPerCell - 1) / m_blocksPerCell;
    m_rows          = (int) ((m_cells + m_columns - 1) / (uint32_t) m_columns);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetCellOrigin
//
////////////////////////////////////////////////////////////////////////////////

POINT BlockMapGrid::GetCellOrigin (uint32_t cell) const
{
    int  pitch = m_cellPx + kGapPx;



    if (m_columns == 0)
    {
        return {};
    }

    return { (LONG) ((cell % (uint32_t) m_columns) * pitch), (LONG) ((cell / (uint32_t) m_columns) * pitch) };
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::FindCellAt
//
////////////////////////////////////////////////////////////////////////////////

int64_t BlockMapGrid::FindCellAt (int xPx, int yPx) const
{
    int       pitch  = m_cellPx + kGapPx;
    int       column = 0;
    int       row    = 0;
    uint64_t  cell   = 0;



    if (m_columns == 0 || xPx < 0 || yPx < 0)
    {
        return -1;
    }

    column = xPx / pitch;
    row    = yPx / pitch;
    cell   = (uint64_t) row * (uint64_t) m_columns + (uint64_t) column;

    if (column >= m_columns || cell >= m_cells)
    {
        return -1;
    }

    return (int64_t) cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::GetPriority
//
////////////////////////////////////////////////////////////////////////////////

int BlockMapGrid::GetPriority (SectorRole role)
{
    static constexpr SectorRole  kOrder[] =
    {
        SectorRole::Free,
        SectorRole::FileData,
        SectorRole::UnusedCatalogSector,
        SectorRole::IndexBlock,
        SectorRole::Subdirectory,
        SectorRole::CatalogOrDirectory,
        SectorRole::VolumeBitmap,
        SectorRole::BootArea,
        SectorRole::VtocOrKeyBlock,
        SectorRole::BadBlocksFile,
        SectorRole::AllocatedUnowned,
        SectorRole::OwnedMarkedFree,
        SectorRole::CrossLinked,
    };



    const SectorRole  * found = std::find (std::begin (kOrder), std::end (kOrder), role);



    return found == std::end (kOrder) ? 0 : (int) (found - std::begin (kOrder));
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockMapGrid::PickRole
//
////////////////////////////////////////////////////////////////////////////////

SectorRole BlockMapGrid::PickRole (SectorRole a, SectorRole b)
{
    return GetPriority (b) > GetPriority (a) ? b : a;
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