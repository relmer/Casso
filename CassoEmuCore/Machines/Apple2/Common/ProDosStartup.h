#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/VolumeTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosStartup
//
//  Which computers a ProDOS volume starts, and SOS installed on one so it
//  starts an Apple ///. SOS and ProDOS share one file system and differ in
//  what is on the disk: an Apple II starts from the Apple II loader in block
//  0 and the PRODOS file, and an Apple /// from the SOS loader and the
//  SOS.KERNEL, SOS.DRIVER and SOS.INTERP files. A disk SOS formats has the
//  SOS loader in block 0; a disk ProDOS formats has the Apple II loader in
//  block 0 and the SOS loader in block 1, so it can start either.
//
//  The loaders and system files are always copied from a disk the user
//  supplies, never written from bytes in Casso. The buffers are the
//  volume layer's: a 140K volume in DOS logical order, a larger one in block
//  order.
//
////////////////////////////////////////////////////////////////////////////////

class ProDosStartup
{
public:
    static constexpr size_t  kBlockBytes = 512;

    static constexpr const char *  kpszSosFiles[] = { "SOS.KERNEL", "SOS.DRIVER", "SOS.INTERP" };

    //  From a volume's blocks 0 and 1 and the entries of its root directory.
    static StartupSystems  Detect (std::span<const Byte> block0, std::span<const Byte> block1, const vector<FileEntry> & root);

    //  The SOS loader's code begins with a jump and the text "SOS BOOT".
    static bool  IsSosLoader     (std::span<const Byte> block);

    //  ProDOS's Apple II loader begins with the count of sectors the Disk II
    //  ROM reads, 1.
    static bool  IsAppleIILoader (std::span<const Byte> block);

    //  The note a listing has for a ProDOS volume, or empty for one that
    //  starts only an Apple II.
    static std::string  DescribeListingNote (const StartupSystems & startup, bool hasProDosFile);

    //  "Apple II", "Apple ///", "Apple II and Apple ///" or "None".
    static std::wstring  GetLabel (const StartupSystems & startup);

    //  Copies the SOS loader and system files from the user's SOS disk. The
    //  loader goes in block 1 when block 0 has the Apple II loader, and in
    //  block 0 otherwise. Fails, changing nothing, when the user's disk
    //  doesn't start an Apple ///.
    static HRESULT  InstallSos (vector<Byte> & ioSectors, const vector<Byte> & sosDisk, std::string & outError);

    static void  ReadBlock (const vector<Byte> & sectors, int block, std::array<Byte, kBlockBytes> & outBlock);

private:
    static void  WriteBlock (vector<Byte> & ioSectors, int block, const std::array<Byte, kBlockBytes> & bytes);
    static bool  HasFile    (const vector<FileEntry> & root, const char * name);
};
