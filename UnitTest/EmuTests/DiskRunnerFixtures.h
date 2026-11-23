#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "FakeDiskFileIo.h"

struct DiskCommandResult;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRunnerFixtures
//
//  Images, host files and option sets for the disk command tests: a blank
//  DOS 3.3 volume, payloads, the options each command takes, and checks on
//  what a command committed.
//
////////////////////////////////////////////////////////////////////////////////

class DiskRunnerFixtures
{
public:

    static constexpr const char *  kImage = "C:\\disks\\merlin.dsk";

    //  Short names for the numbering a maker is called with, so a call site
    //  reads as the command line would.
    static constexpr CommandLineOptions::DiskOptions::Numbering  kLogical =
        CommandLineOptions::DiskOptions::Numbering::Logical;

    static constexpr const char *  kBlankImage = "C:\\disks\\blank.dsk";
    static constexpr const char *  kHostFile   = "C:\\build\\prog.bin";
    static constexpr const char *  kProImage   = "C:\\disks\\merlin.po.dsk";

    static constexpr Byte      kBlankVolumeNumber = 254;
    static constexpr size_t    kPayloadBytes      = 512;
    static constexpr Word      kLoadAddress       = 0x6000;

    static constexpr const char *  kProDosVolumeName = "MERLIN";

    //  Catalog geometry, restated here only for the two tests that reach into
    //  a catalog entry directly -- to lock a file, and to break its chain.
    static constexpr int       kVtocTrack          = 17;
    static constexpr int       kCatalogFirstSector = 15;

    static constexpr size_t    kEntryBase         = 0x0B;
    static constexpr size_t    kEntOffType        = 0x02;
    static constexpr Byte      kLockedBit         = 0x80;

    static CommandLineOptions  MakeOptions                      (CommandLineOptions::DiskOptions::Command command, const char * image = kImage);
    static void                AssertEveryLineFitsEightyColumns (const std::string & text);
    static void                SeedProDosDisk                   (FakeDiskFileIo & io, const char * path = kImage);
    static void                SeedDos33Disk                    (FakeDiskFileIo & io, const char * path = kImage);
    static void                SeedBlankImage                   (FakeDiskFileIo & io, const char * path);
    static CommandLineOptions  MakeSectorRead                   (const char * image, int track, int sector, int count, CommandLineOptions::DiskOptions::Numbering numbering);
    static CommandLineOptions  MakeDirectBoot                   (const char * image, const char * payload);
    static CommandLineOptions  MakeCreate                       (const char * path);
    static vector<Byte>        MakeBlankDos33Image              ();
    static vector<Byte>        MakeBlankProDosDsk               ();
    static void                SeedFile                         (FakeDiskFileIo & io, const char * path, const vector<Byte> & bytes);
    static vector<Byte>        MakePayload                      (size_t count = kPayloadBytes);
    static CommandLineOptions  MakePutOptions                   (const char * image, const char * hostFile, const char * asName);
    static DiskCommandResult   GetFromCommittedImage            (FakeDiskFileIo & io, const char * image, const char * path, CommandLineOptions::DiskOptions::Encoding encoding = CommandLineOptions::DiskOptions::Encoding::Verbatim);
    static std::string         ListCommittedImage               (FakeDiskFileIo & io, const char * image);
    static void                AssertImageMatches               (FakeDiskFileIo & io, const char * path, const vector<Byte> & expected);
    static void                AssertNamesNoPlatformCode        (const std::string & diagnostics);
    static void                LockFirstCatalogEntry            (vector<Byte> & buffer);
    static CommandLineOptions  MakeBinaryPutOptions             (const char * image);
};
