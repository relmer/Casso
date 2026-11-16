#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CatalogModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StockDiskRow
//
//  A stock system disk the "Stock disks" node lists: its label, where its
//  image is or will be, and whether it has been downloaded.
//
////////////////////////////////////////////////////////////////////////////////

struct StockDiskRow
{
    std::wstring  label;
    std::wstring  path;
    bool          isDownloaded = false;

    //  The Apple II models the disk boots on, for its tooltip.
    std::wstring  note;

    //  A bundled disk's class, read from its file; unclassified for a
    //  stock disk, which is a 140K 5.25-inch disk.
    ImageClass    imageClass;
};





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisksListing
//
//  The "Stock disks" node's list: one row for each stock system disk, by
//  its label, with the path its image has once downloaded. A stock disk is
//  a 140K 5.25-inch disk, and its row has that class before its file is on
//  the disk. A bundled disk's row has the class read from its file.
//
////////////////////////////////////////////////////////////////////////////////

class StockDisksListing
{
public:
    static constexpr uint64_t  kDiskBytes  = 143360;
    static constexpr uint32_t  kDiskBlocks = 280;

    static void  Build (const std::vector<StockDiskRow> & disks, std::vector<CatalogRow> & outRows);
};
