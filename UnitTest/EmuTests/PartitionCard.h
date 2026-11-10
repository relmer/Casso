#pragma once

#include "Pch.h"
#include "../EhmTestHelper.h"
#include "../UiTests/InMemoryFileSystem.h"
#include "Devices/Disk/Partitions/PartitionLayout.h"
#include "Machines/Apple2/Common/Dos33Geometry.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionCard
//
//  A MicroDrive card image and a UniDOS disk image built in memory, for the
//  tests of partitions.
//
////////////////////////////////////////////////////////////////////////////////

struct PartitionCard
{
    using vector = std::vector<Byte>;
    static constexpr size_t           kBlock        = IBlockStore::kBlockBytes;
    static constexpr size_t           kFloppyBytes  = 143360;
    static constexpr uint32_t         kCardBlocks   = 2000;
    static constexpr uint32_t         kFirstStart   = 8;
    static constexpr uint32_t         kSecondStart  = 300;
    static constexpr uint32_t         kSecondBlocks = 800;
    static constexpr size_t           kSecondBytes  = (size_t) kSecondBlocks * kBlock;
    static constexpr const char *     kCardName     = "C:\\Disks\\card.po";
    static constexpr const wchar_t *  kCardPath     = L"C:\\Disks\\card.po";
    static constexpr const wchar_t *  kGzipPath     = L"C:\\Disks\\card.po.gz";

    static void WriteLong (std::vector<Byte> & bytes, size_t offset, uint32_t value)
    {
        bytes[offset]     = (Byte) (value & 0xFF);
        bytes[offset + 1] = (Byte) ((value >> 8) & 0xFF);
        bytes[offset + 2] = (Byte) ((value >> 16) & 0xFF);
        bytes[offset + 3] = (Byte) ((value >> 24) & 0xFF);
    }

    static void Place (std::vector<Byte> & bytes, uint32_t block, const std::vector<Byte> & source)
    {
        std::copy (source.begin(), source.end(), bytes.begin() + (ptrdiff_t) ((size_t) block * kBlock));
    }

    static std::vector<Byte> MakeProDos (uint32_t blocks, const char * volumeName)
    {
        std::vector<Byte>  buffer ((size_t) blocks * kBlock, 0);

        AssertSucceeded (ProDosSkeleton::Write (buffer, volumeName));

        return buffer;
    }

    //  A 5.25" ProDOS volume as the bytes of a `.po` file.
    static std::vector<Byte> MakeFloppyFile (const char * volumeName)
    {
        std::vector<Byte>  logical = MakeProDos (PartitionLayout::kFloppyBlocks, volumeName);
        std::vector<Byte>  file;

        VolumeImage::DosLogicalToProDosFile (logical, file);

        return file;
    }

    static std::vector<Byte> MakeDos (int tracks, int sectorsPerTrack)
    {
        Dos33Geometry  geometry = Dos33Geometry::Make (tracks, sectorsPerTrack);
        std::vector<Byte>   buffer (geometry.GetImageByteSize(), 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254, geometry));

        return buffer;
    }

    static FilePayload MakePayload (size_t bytes, Byte fill)
    {
        FilePayload  payload;

        payload.bytes.assign (bytes, fill);
        payload.type           = ProDosVolume::kTypeBinary;
        payload.loadAddress    = 0x2000;
        payload.hasLoadAddress = true;

        return payload;
    }

    //  The second partition's volume, with a file HELLO.
    static std::vector<Byte> MakeSecondVolume()
    {
        std::vector<Byte>  volume = MakeProDos (kSecondBlocks, "SECOND");
        std::vector<Byte>  withHello;

        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("HELLO"), MakePayload (3000, 0x11), withHello));

        return withHello;
    }

    //  A map of three partitions: a 5.25" volume, an 800K volume, and one that
    //  ends past the end of the card. Bytes after the last whole block follow.
    static std::vector<Byte> MakeCard()
    {
        std::vector<Byte>  bytes ((size_t) kCardBlocks * kBlock, 0);

        bytes[0]    = 0xCA;
        bytes[1]    = 0xCC;
        bytes[0x0C] = 2;
        bytes[0x0D] = 1;

        WriteLong (bytes, 0x20, kFirstStart);
        WriteLong (bytes, 0x40, PartitionLayout::kFloppyBlocks);
        WriteLong (bytes, 0x24, kSecondStart);
        WriteLong (bytes, 0x44, kSecondBlocks);
        WriteLong (bytes, 0x80, 1900);
        WriteLong (bytes, 0xA0, 500);

        Place (bytes, kFirstStart, MakeFloppyFile ("FLOPPY"));
        Place (bytes, kSecondStart, MakeSecondVolume());

        bytes.insert (bytes.end(), 100, 0x5A);

        return bytes;
    }

    //  Two volumes of 50 tracks and 32 sectors, one after the other.
    static std::vector<Byte> MakeUniDosDisk()
    {
        std::vector<Byte>  volume = MakeDos (50, 32);
        std::vector<Byte>  bytes ((size_t) 1600 * kBlock, 0);

        Place (bytes, 0, volume);
        Place (bytes, 800, volume);

        return bytes;
    }

    static std::vector<Byte> GetSecond (const std::vector<Byte> & card)
    {
        ptrdiff_t  start = (ptrdiff_t) ((size_t) kSecondStart * kBlock);

        return std::vector<Byte> (card.begin() + start, card.begin() + start + (ptrdiff_t) kSecondBytes);
    }

    static std::vector<Byte> ReadHostFile (InMemoryFileSystem & fs, const wchar_t * path)
    {
        std::string  content;

        AssertSucceeded (fs.ReadAllText (path, content));

        return std::vector<Byte> (content.begin(), content.end());
    }
};
