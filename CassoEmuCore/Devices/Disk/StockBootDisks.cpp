#include "Pch.h"

#include "StockBootDisks.h"

#include "Core/PathResolver.h"



//  The names the two masters are cached under.
//
//  They are the DOWNLOADED names rather than the names on the servers they
//  come from: AssetBootstrap renames each one as it saves it, so this is what
//  is actually on disk. Kept beside the locator that returns them, and
//  AssetBootstrap now reads them from here rather than holding a second copy
//  that could drift.
static constexpr const wchar_t *  s_kpszDos33Master     = L"DOS 3.3 System Master.dsk";
static constexpr const wchar_t *  s_kpszProDosUsersDisk = L"ProDOS Users Disk.dsk";





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks::GetFileName
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * StockBootDisks::GetFileName (Which disk)
{
    return (disk == Which::Dos33Master) ? s_kpszDos33Master : s_kpszProDosUsersDisk;
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
