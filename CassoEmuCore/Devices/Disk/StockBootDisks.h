#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  StockBootDisks
//
//  Where the operating-system masters live once they have been downloaded.
//
//  MAKING A DISK BOOTABLE MEANS COPYING AN OPERATING SYSTEM ONTO IT, so
//  something has to say which file that is. The emulator downloads two on
//  first run and keeps them in a cache beside its other assets; this is the
//  half of that knowledge a tool needs in order to FIND one.
//
//  IT LIVES HERE BECAUSE BOTH EXECUTABLES NEED IT. All of it sat in
//  Casso.exe's AssetBootstrap, so the command line could not reach the cache
//  at all and `disk create --bootable` had to be told the path by hand. The
//  downloading stays there -- it needs consent, a progress report and a
//  network stack, none of which belong in a library -- and only the locating
//  moved, which is the part with no platform in it beyond a directory name.
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

    //  Where that master sits in the cache, whether or not it is there yet.
    //
    //  NARROW, because that is what IDiskFileIo takes and what the command
    //  line hands it. std::filesystem::path converts both ways with the same
    //  encoding, so this is the exact inverse of the widening the file layer
    //  does on the way back in.
    static std::string  PathFor (Which disk);

    //  Whether it has actually been downloaded. A caller that gets false has
    //  to say so rather than proceeding: there is no operating system to copy.
    static bool  IsCached (Which disk);

    //  The directory the two of them share. Public because the message a
    //  caller gives when one is missing is more useful for naming it.
    static std::string  CacheDirectory();

    //  What the file is called, so a diagnostic can name the one that is
    //  missing rather than describing it.
    static const wchar_t *  FileNameFor (Which disk);
};
