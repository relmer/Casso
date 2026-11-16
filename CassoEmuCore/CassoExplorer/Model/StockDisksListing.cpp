#include "Pch.h"

#include "CassoExplorer/Model/StockDisksListing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisksListing::Build
//
////////////////////////////////////////////////////////////////////////////////

void StockDisksListing::Build (const std::vector<StockDiskRow> & disks, std::vector<CatalogRow> & outRows)
{
    outRows.clear();

    for (const StockDiskRow & disk : disks)
    {
        CatalogRow  row;

        row.name                      = disk.label;
        row.hostPath                  = disk.path;
        row.isDiskImage               = true;
        row.isStockDisk               = true;
        row.stockDownloaded           = disk.isDownloaded;
        row.stockNote                 = disk.note;
        row.sizeBytes                 = disk.isDownloaded ? kDiskBytes : 0;
        row.imageClass                = disk.imageClass;

        if (!row.imageClass.IsClassified())
        {
            row.imageClass.status         = ImageStatus::Classified;
            row.imageClass.formFactor     = ImageFormFactor::Floppy525;
            row.imageClass.layout         = ImageLayout::Sector;
            row.imageClass.formattedBytes = kDiskBytes;
            row.imageClass.blockCount     = kDiskBlocks;
        }

        outRows.push_back (std::move (row));
    }
}
