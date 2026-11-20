#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/BlankDiskSpecResolver.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/IDiskCommandContext.h"

enum class DiskFormat;
enum class ProDosBootSystem;
struct FileStamp;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCreateCommand
//
//  The disk tool's create and init commands, which write a new image or
//  reformat one.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCreateCommand
{
public:
    explicit DiskCreateCommand (IDiskCommandContext & context) : m_resolver (context), m_context (context) {}

    //  Puts a computed image over the target, or refuses and leaves the target
    //  byte-for-byte as it was.
    //
    //  Makes a new image file, or reformats one that is already there.
    //
    //  TWO COMMANDS BECAUSE THEY ANSWER TO DIFFERENT THINGS. `create` writes a
    //  file that did not exist and decides its container; `init` finds the
    //  container already decided by the file it was handed and only rewrites
    //  what is inside it. So `create` takes --type and `init` does not, and
    //  create refuses to write over something rather than quietly replacing a
    //  disk somebody still wanted.
    void  RunCreate (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunInit   (const CommandLineOptions & options, DiskCommandResult & result);

private:
    //  A disk that starts a binary with no operating system on it. Its own
    //  path rather than a flag on the formatted one: there is no filesystem
    //  here to put the binary into.
    void  BuildDirectBoot (const CommandLineOptions & options,
                           DiskFormat                 format,
                           size_t                     nibbleTrackSize,
                           DiskCommandResult        & result);

    static void  AppendTypeExtension (std::string & imagePath, const std::string & containerType);

    static bool                IsThirteenSectorImage  (const CommandLineOptions & options);
    void                       BuildThirteenSector    (const CommandLineOptions & options, bool overExisting, DiskCommandResult & result);
    static bool                IsLargeCreate          (const CommandLineOptions & options);
    static bool                IsHfsFormat            (const CommandLineOptions & options);
    static const char        * GetVolumeNameRule      (bool isHfs);
    static bool                IsBlockOnlyExtension   (const std::string & imagePath);
    static bool                IsProDosOrderExtension (const std::string & imagePath);
    static bool                TryGetLargeBlockCount  (const std::string & sizeName, uint32_t & outBlocks);
    void                       BuildLargeProDos       (const CommandLineOptions & options, DiskCommandResult & result);
    void                       ReformatLargeProDos    (const CommandLineOptions & options, const FileStamp & stamp, DiskCommandResult & result);

    //  --bootable on a volume larger than 140K, as on a 140K one: PRODOS and
    //  BASIC.SYSTEM from the master, and SOS for a format that starts an
    //  Apple ///. `outBootWords` is what the summary line adds, or empty.
    HRESULT                    InstallLargeBoot       (const CommandLineOptions & options, bool isHfs, vector<Byte> & ioVolume, std::string & outBootWords, DiskCommandResult & result);

    //  Everything create and init share: build the bytes, then put them there.
    void  BuildAndWrite (const CommandLineOptions & options,
                         DiskFormat                 format,
                         size_t                     nibbleTrackSize,
                         bool                       overExisting,
                         DiskCommandResult        & result);

    BlankDiskSpecResolver   m_resolver;
    IDiskCommandContext   & m_context;
};
