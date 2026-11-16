#pragma once

#include "Pch.h"

#include "Devices/Disk/ImageClassifier.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageHeadBuilder
//
//  The first bytes of disk images for tests, built by hand from each
//  container's published layout: a WOZ head with INFO first (the WOZ 2
//  reference), a 2IMG header (the 2IMG specification), and a Disk Copy 4.2
//  header (Apple's technical note). Each head is ImageClassifier::kHeadBytes
//  long, and Classify* passes it with the file length a test gives.
//
////////////////////////////////////////////////////////////////////////////////

class ImageHeadBuilder
{
public:
    static constexpr uint64_t  kBlock     = 512;
    static constexpr uint64_t  k140K      = 143360;
    static constexpr uint64_t  k400K      = 409600;
    static constexpr uint64_t  k800K      = 819200;
    static constexpr uint64_t  k32M       = 65535 * kBlock;
    static constexpr size_t    kWozInfoAt = 20;



    static std::vector<Byte> MakeWozHead (const char * signature, Byte diskType, Byte sides)
    {
        constexpr Byte    kSigTail[]   = { 0xFF, 0x0A, 0x0D, 0x0A };
        constexpr size_t  kSigBytes    = 4;
        constexpr size_t  kSigTailAt   = 4;
        constexpr size_t  kInfoIdAt    = 12;
        constexpr size_t  kInfoSizeAt  = 16;
        constexpr Byte    kInfoSize    = 60;
        constexpr Byte    kInfoVersion = 2;

        std::vector<Byte>  head (ImageClassifier::kHeadBytes, 0);



        std::memcpy (head.data(), signature, kSigBytes);
        std::memcpy (head.data() + kSigTailAt, kSigTail, sizeof (kSigTail));
        std::memcpy (head.data() + kInfoIdAt, "INFO", kSigBytes);

        head[kInfoSizeAt]                                  = kInfoSize;
        head[kWozInfoAt + WozLoader::kInfoOffsetVersion]   = kInfoVersion;
        head[kWozInfoAt + WozLoader::kInfoOffsetDiskType]  = diskType;
        head[kWozInfoAt + WozLoader::kInfoOffsetDiskSides] = sides;

        return head;
    }



    static void PutLittleLong (std::vector<Byte> & bytes, size_t at, uint32_t value)
    {
        for (size_t i = 0; i < 4; i++)
        {
            bytes[at + i] = (Byte) (value >> (8 * i));
        }
    }



    static void PutBigLong (std::vector<Byte> & bytes, size_t at, uint32_t value)
    {
        for (size_t i = 0; i < 4; i++)
        {
            bytes[at + i] = (Byte) (value >> (8 * (3 - i)));
        }
    }



    static std::vector<Byte> MakeTwoImgHead (uint32_t format, uint32_t dataBytes)
    {
        std::vector<Byte>  head (ImageClassifier::kHeadBytes, 0);



        std::memcpy (head.data(), "2IMG", 4);

        PutLittleLong (head, BlockImageFile::kTwoImgFormat,     format);
        PutLittleLong (head, BlockImageFile::kTwoImgBlocks,     (uint32_t) (dataBytes / kBlock));
        PutLittleLong (head, BlockImageFile::kTwoImgDataOffset, (uint32_t) BlockImageFile::kTwoImgHeader);
        PutLittleLong (head, BlockImageFile::kTwoImgDataLength, dataBytes);

        return head;
    }



    static std::vector<Byte> MakeDiskCopyHead (uint32_t dataBytes)
    {
        constexpr size_t  kPrivateAt = 82;
        constexpr Byte    kNameBytes = 8;

        std::vector<Byte>  head (ImageClassifier::kHeadBytes, 0);



        head[0] = kNameBytes;

        PutBigLong (head, BlockImageFile::kDcDataSize, dataBytes);
        PutBigLong (head, BlockImageFile::kDcTagSize,  0);

        head[kPrivateAt]     = 0x01;
        head[kPrivateAt + 1] = 0x00;

        return head;
    }



    static ImageClass ClassifyRaw (const wchar_t * extension, uint64_t bytes)
    {
        std::vector<Byte>  head ((size_t) std::min<uint64_t> (bytes, ImageClassifier::kHeadBytes), 0);



        return ImageClassifier::ClassifyHeader (extension, bytes, head);
    }



    static ImageClass ClassifyTwoImg (uint32_t format, uint64_t dataBytes)
    {
        return ImageClassifier::ClassifyHeader (L".2mg", BlockImageFile::kTwoImgHeader + dataBytes, MakeTwoImgHead (format, (uint32_t) dataBytes));
    }



    static ImageClass ClassifyDiskCopy (const wchar_t * extension, uint64_t dataBytes)
    {
        return ImageClassifier::ClassifyHeader (extension, BlockImageFile::kDcHeader + dataBytes, MakeDiskCopyHead ((uint32_t) dataBytes));
    }



    static ImageClass ClassifyWoz (const wchar_t * extension, Byte diskType, Byte sides)
    {
        constexpr uint64_t  kWozFileBytes = 234496;



        return ImageClassifier::ClassifyHeader (extension, kWozFileBytes, MakeWozHead ("WOZ2", diskType, sides));
    }
};
