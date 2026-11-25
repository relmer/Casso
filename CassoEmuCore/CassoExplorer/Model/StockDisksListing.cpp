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

        //  A disk on this computer is listed by its file name, as in its folder.
        row.name                      = (disk.isDownloaded && !disk.path.empty()) ? std::filesystem::path (disk.path).filename().wstring() : disk.label;
        row.hostPath                  = disk.path;
        row.isDiskImage               = true;
        row.isStockDisk               = true;
        row.stockDownloaded           = disk.isDownloaded;
        row.stockNote                 = disk.note;
        row.imageClass                = disk.imageClass;

        if (!row.imageClass.IsClassified())
        {
            row.imageClass.status         = ImageStatus::Classified;
            row.imageClass.formFactor     = ImageFormFactor::Floppy525;
            row.imageClass.layout         = ImageLayout::Sector;
            row.imageClass.formattedBytes = kDiskBytes;
            row.imageClass.blockCount     = kDiskBlocks;
        }

        row.sizeBytes = disk.isDownloaded ? row.imageClass.formattedBytes : 0;

        outRows.push_back (std::move (row));
    }
}
