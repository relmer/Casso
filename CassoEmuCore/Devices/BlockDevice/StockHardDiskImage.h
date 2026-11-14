#pragma once

#include "Pch.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  StockHardDiskImage
//
//  The image an HD20SC whose machine file lists "stock" uses: an empty 32 MB
//  ProDOS volume with a blank boot block, made in the user's Casso data
//  folder the first time a machine lists it, and never written over after
//  that, so what the user stores on it stays.
//
////////////////////////////////////////////////////////////////////////////////

class StockHardDiskImage
{
public:
    static constexpr const char *  kpszFileName   = "Stock hard disk.hdv";
    static constexpr const char *  kpszVolumeName = "HARD.DISK";
    static constexpr uint32_t      kBlocks        = 65535;

    //  The image's path in `folder`, made there first when it is not there.
    static HRESULT  Ensure (IDiskFileIo & fileIo, const std::string & folder, std::string & outPath);
};
