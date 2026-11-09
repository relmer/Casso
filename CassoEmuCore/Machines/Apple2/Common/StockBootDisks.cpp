#include "Pch.h"

#include "Machines/Apple2/Common/StockBootDisks.h"

#include "Core/PathResolver.h"



//  The names live in the header, as public constants, because AssetBootstrap
//  builds its download table out of them. See StockBootDisks.h.





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * StockBootDisks::GetFileName (Which disk)
{
    switch (disk)
    {
        case Which::Dos33Master:      return kpszDos33MasterFile;
        case Which::ProDosUsersDisk:  return kpszProDosUsersDiskFile;
        case Which::PascalBootDisk:   return kpszPascalBootDiskFile;
        case Which::CpamBootDisk:     return kpszCpamBootDiskFile;
        case Which::SoftCardBootDisk: return kpszSoftCardBootDiskFile;
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * StockBootDisks::GetLabel (Which disk)
{
    switch (disk)
    {
        case Which::Dos33Master:      return L"DOS 3.3";
        case Which::ProDosUsersDisk:  return L"ProDOS";
        case Which::PascalBootDisk:   return L"Apple Pascal";
        case Which::CpamBootDisk:     return L"CP/AM 5.1";
        case Which::SoftCardBootDisk: return L"CP/M 2.23";
    }

    return L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::TryGetStockDisk
//
////////////////////////////////////////////////////////////////////////////////

bool StockBootDisks::TryGetStockDisk (const std::string & format, const std::string & source, Which & outDisk)
{
    bool  isCpm      = _stricmp (format.c_str(), "cpm") == 0 || _stricmp (format.c_str(), "cp/m") == 0;
    bool  isSoftCard = isCpm && _stricmp (source.c_str(), kpszSoftCardWord) == 0;



    if (!source.empty() && !isSoftCard)
    {
        return false;
    }

    if (isCpm)
    {
        outDisk = isSoftCard ? Which::SoftCardBootDisk : Which::CpamBootDisk;
    }
    else if (_stricmp (format.c_str(), "pascal") == 0)
    {
        outDisk = Which::PascalBootDisk;
    }
    else if (_stricmp (format.c_str(), "prodos") == 0)
    {
        outDisk = Which::ProDosUsersDisk;
    }
    else if (format.empty() || _strnicmp (format.c_str(), "dos", strlen ("dos")) == 0)
    {
        outDisk = Which::Dos33Master;
    }
    else
    {
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::CacheDirectory
//
//  <localAppData>\Casso\Disks, which is where the downloader puts them.
//
//  It does NOT create the directory, which is the one behavioral difference
//  from the version this was lifted out of. Asking where a file would be is
//  not the same as arranging for it to exist, and every `disk create` without
//  --bootable would otherwise leave an empty folder behind as a side effect of
//  a question it did not ask.
//
////////////////////////////////////////////////////////////////////////////////

std::string StockBootDisks::CacheDirectory()
{
    std::filesystem::path  base = PathResolver::GetLocalAppDataDir (L"Casso");



    return (base / L"Disks").string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::GetPath
//
////////////////////////////////////////////////////////////////////////////////

std::string StockBootDisks::GetPath (Which disk)
{
    std::filesystem::path  directory = CacheDirectory();



    return (directory / GetFileName (disk)).string();
}





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::IsCached
//
//  Whether the master has been downloaded yet.
//
//  A caller that gets false has to say so rather than carrying on: there is no
//  operating system to copy, and a disk built without one boots to nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool StockBootDisks::IsCached (Which disk)
{
    std::error_code  ec;



    return std::filesystem::exists (std::filesystem::path (GetPath (disk)), ec);
}
