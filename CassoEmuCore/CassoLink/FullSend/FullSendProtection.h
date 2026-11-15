#pragma once

#include "Pch.h"

enum class VolumeKind;





////////////////////////////////////////////////////////////////////////////////
//
//  ProtectionSigns
//
//  The signs of copy protection in a sector image that read without errors:
//  no file system Casso recognizes, code outside the operating system that
//  reads the Disk II's data register, and a boot sector of no known loader.
//
////////////////////////////////////////////////////////////////////////////////

struct ProtectionSigns
{
    bool         isUnrecognized = false;
    bool         hasDiskCode    = false;
    bool         hasOtherBoot   = false;
    std::string  diskCodeFile;              // the first file with disk code
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtectionChoices
//
//  The Full Send options' Copy-protected disks section: the conditions that
//  mark a disk as likely copy-protected, and whether such a disk is captured
//  raw or saved as its sector copy with that status.
//
////////////////////////////////////////////////////////////////////////////////

struct FullSendProtectionChoices
{
    bool  readErrors   = true;
    bool  unrecognized = true;
    bool  diskCode     = true;
    bool  otherBoot    = true;
    bool  captureRaw   = true;

    bool  IsMet (const ProtectionSigns & signs) const
    {
        return (unrecognized && signs.isUnrecognized) || (diskCode && signs.hasDiskCode) || (otherBoot && signs.hasOtherBoot);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtection
//
//  ProtectionSigns from a disk's sectors, in DOS logical order in an
//  image-sized buffer. Pure: no I/O, no Win32.
//
//  Disk code is an instruction that reads the data register, $C08C,X or
//  slot 6's $C0EC, at least kDiskCodeReads times in a file. The disk's
//  operating system has its disk code on the boot tracks, in PRODOS, or in
//  Apple Pascal's SYSTEM.APPLE; those files, and Apple's disk utilities on
//  its system disks, aren't counted. Text and BASIC files aren't read.
//
//  A boot sector is a known loader's when it opens as DOS 3.3's, CP/M's or
//  ProDOS's does, or has Apple Pascal's message for a missing SYSTEM.APPLE
//  or RTSTRP.APPLE, or SOS's SOS BOOT. A boot sector of zeros is no loader,
//  and not counted. CP/M's programs are Z80 code, so a CP/M disk's files
//  aren't read.
//
////////////////////////////////////////////////////////////////////////////////

class FullSendProtection
{
public:
    enum class BootFamily
    {
        None,
        Dos33,
        Cpm,
        ProDos,
        Pascal,
        Sos,
        Other,
    };

    static ProtectionSigns  Find (const std::vector<Byte> & image);

    static BootFamily  GetBootFamily  (const std::vector<Byte> & image);
    static int         CountDiskReads (std::span<const Byte> bytes);
    static bool        IsSystemFile   (VolumeKind kind, const std::string & name);
    static bool        IsUniform      (const std::vector<Byte> & image);

    static constexpr int  kDiskCodeReads = 4;

private:
    static bool  FindDiskCode   (VolumeKind kind, const std::vector<Byte> & image, std::string & outFile);
    static bool  HasPrefix      (std::span<const Byte> sector, std::span<const Byte> prefix);
    static bool  HasText        (std::span<const Byte> block, std::string_view text);
    static bool  IsReadableType (VolumeKind kind, Byte type);

    static constexpr size_t  s_kBootSectorBytes = 256;

    static constexpr Byte  s_kDos33Boot[]  = { 0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x18, 0xA5, 0x2B };
    static constexpr Byte  s_kCpmBoot[]    = { 0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x0F, 0x8A };
    static constexpr Byte  s_kProDosBoot[] = { 0x01, 0x38, 0xB0, 0x03, 0x4C };

    //  A later ProDOS formatter's boot block for a data disk, which prints
    //  that it isn't a startup disk.
    static constexpr Byte  s_kDataDiskBoot[] = { 0x01, 0xBD, 0x88, 0xC0, 0x20, 0x58, 0xFC };

    static constexpr std::string_view  s_kPascalLoaders[] = { "SYSTEM.APPLE", "RTSTRP.APPLE" };
    static constexpr std::string_view  s_kSosLoader       = "SOS BOOT";

    //  The operating systems' disk code and Apple's disk utilities. Any
    //  Apple Pascal file whose name ends in .APPLE is a BIOS, as SYSTEM.APPLE,
    //  RTSTRP.APPLE and 128K.APPLE are.
    static constexpr std::string_view  s_kSystemFiles[] = { "PRODOS", "FILER", "SOS.KERNEL", "SOS.DRIVER", "FORMATTER.DATA", "FORMATTER.CODE", "MUFFIN", "BOOT13" };
    static constexpr std::string_view  s_kPascalBios    = ".APPLE";
};
