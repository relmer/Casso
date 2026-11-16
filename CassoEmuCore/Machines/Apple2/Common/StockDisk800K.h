#pragma once

#include "Pch.h"

class ProDosVolume;





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisk800K
//
//  The 800K stock disk for a 3.5-inch drive, built from the 140K ProDOS
//  2.4.3 disk rather than downloaded: an 800K ProDOS volume in block order
//  with the 140K disk's volume name, its boot blocks and every file and
//  folder on it.
//
////////////////////////////////////////////////////////////////////////////////

class StockDisk800K
{
public:
    static constexpr size_t           kBytes            = 819200;
    static constexpr uint32_t         kBlocks           = 1600;
    static constexpr const wchar_t *  kpszFileName      = L"ProDOS 2.4.3 800K.po";
    static constexpr const wchar_t *  kpszDisplayName   = L"ProDOS 2.4.3 (800K)";
    static constexpr const wchar_t *  kpszCompatibility = L"Boots from a 3.5-inch drive on every Apple II with 64K.";

    //  `source` is a 140K ProDOS disk in ProDOS order, as a .po file has it.
    static HRESULT  Build (const vector<Byte> & source, vector<Byte> & outVolume);

    //  The same disk in DOS logical sector order, as ProDosSkeleton reads a
    //  140K disk.
    static void     ToDosOrder (const vector<Byte> & proDosOrder, vector<Byte> & outDosOrder);

private:
    static HRESULT  CopyDirectory (const ProDosVolume & from, const std::string & directory, vector<Byte> & ioVolume);
};
