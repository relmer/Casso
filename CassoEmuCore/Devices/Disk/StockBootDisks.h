#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks
//
//  Where the operating-system masters live once they have been downloaded.
//
//  MAKING A DISK BOOTABLE MEANS COPYING AN OPERATING SYSTEM ONTO IT, so
//  something has to specify which file that is. The emulator downloads each of
//  the two when the user picks it in a disk picker or clicks Download in
//  Create New Disk, and keeps them in a cache beside its other assets; this
//  is the half of that knowledge a tool needs in order to FIND one.
//
//  IT LIVES HERE BECAUSE BOTH EXECUTABLES NEED IT. All of it sat in
//  Casso.exe's AssetBootstrap, so the command line could not reach the cache
//  at all and `disk create --bootable` had to be told the path by hand. The
//  downloading stays there -- it starts from a click in a dialog, reports
//  failure through one, and needs a network stack, none of which belong in a
//  library -- and only the locating moved, which is the part with no platform
//  in it beyond a directory name.
//
//  NOTHING HERE CREATES ANYTHING. Asking where a file would be is not the same
//  as arranging for it to exist, and a locator that made a directory as a side
//  effect of being asked a question would leave empty folders behind every
//  time a disk was created without --bootable.
//
////////////////////////////////////////////////////////////////////////////////

class StockBootDisks
{
public:
    //  The two masters the emulator installs from, and the only ones a
    //  bootable disk can be built out of today.
    enum class Which
    {
        Dos33Master,
        ProDosUsersDisk,
    };

    //  The names the two masters are cached under, and the ONE place either
    //  is spelled.
    //
    //  They are the DOWNLOADED names rather than the names on the servers they
    //  come from: the emulator renames each one as it saves it, so this is
    //  what is actually on disk.
    //
    //  PUBLIC, AND constexpr, so the downloader can build its own table from
    //  them. AssetBootstrap held a second copy for a while; the two agreed by
    //  hand, and a rename in one would have left the command line looking for
    //  a file the emulator no longer writes -- with nothing to catch it,
    //  because each half was internally consistent.
    static constexpr const wchar_t *  kpszDos33MasterFile     = L"DOS 3.3 System Master.dsk";
    static constexpr const wchar_t *  kpszProDosUsersDiskFile = L"ProDOS Users Disk.dsk";

    //  Where that master sits in the cache, whether or not it is there yet.
    //
    //  NARROW, because that is what IDiskFileIo takes and what the command
    //  line hands it. std::filesystem::path converts both ways with the same
    //  encoding, so this is the exact inverse of the widening the file layer
    //  does on the way back in.
    static std::string  GetPath (Which disk);

    //  Whether it has actually been downloaded. A caller that gets false has
    //  to say so rather than proceeding: there is no operating system to copy.
    static bool  IsCached (Which disk);

    //  The directory the two of them share. Public because the message a
    //  caller gives when one is missing is more useful for naming it.
    static std::string  CacheDirectory();

    //  What the file is called, so a diagnostic can name the one that is
    //  missing rather than describing it.
    static const wchar_t *  GetFileName (Which disk);
};
