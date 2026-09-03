#include "Pch.h"

#include "StockBootDisks.h"

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
    return (disk == Which::Dos33Master) ? kpszDos33MasterFile : kpszProDosUsersDiskFile;
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
