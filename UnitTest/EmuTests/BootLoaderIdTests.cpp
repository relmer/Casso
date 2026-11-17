#include "Pch.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BootLoaderId.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderIdTests
//
//  What a volume's boot blocks start: none, ProDOS 8 with its version, GS/OS,
//  a volume with no PRODOS file, SOS's loader with its version, Apple
//  Pascal's, Macintosh boot blocks on HFS, and unknown code.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootLoaderIdTests)
{
public:

    static constexpr size_t    kBlockBytes  = 512;
    static constexpr size_t    kVolumeBytes = 8 * kBlockBytes;
    static constexpr uint32_t  kProDosBlock = 4;
    static constexpr Byte      kHighBit     = 0x80;

    static void PutText (vector<Byte> & volume, size_t at, const char * text, Byte highBit = 0)
    {
        size_t  i = 0;



        for (i = 0; text[i] != '\0'; i++)
        {
            volume[at + i] = (Byte) text[i] | highBit;
        }
    }


    static vector<Byte> MakeVolume (std::initializer_list<Byte> start)
    {
        vector<Byte>  volume (kVolumeBytes, 0);



        std::copy (start.begin(), start.end(), volume.begin());

        return volume;
    }


    //  A volume with the ProDOS loader and a seedling PRODOS file in block 4
    //  whose bytes have TEXT.
    static vector<Byte> MakeProDosVolume (const char * text, Byte highBit, BlockMap & outMap)
    {
        vector<Byte>    volume = MakeVolume ({ 0x01, 0x38, 0xB0, 0x03, 0x4C, 0x32, 0xA1 });
        BlockFileEntry  entry;



        volume[kProDosBlock * kBlockBytes] = 0x4C;
        PutText (volume, kProDosBlock * kBlockBytes + 9, text, highBit);

        entry.name     = L"PRODOS";
        entry.storage  = 1;
        entry.keyBlock = kProDosBlock;
        entry.eof      = (uint32_t) kBlockBytes;

        outMap.fileSystem = BlockFileSystem::ProDos;
        outMap.files.push_back (L"PRODOS");
        outMap.entries.push_back (entry);

        return volume;
    }


    static std::wstring Identify (const vector<Byte> & volume, BlockFileSystem fileSystem = BlockFileSystem::ProDos)
    {
        BlockMap  map;



        map.fileSystem = fileSystem;

        return BootLoaderId::Identify (BlockImage (volume), map);
    }


    TEST_METHOD (ABlockOfZerosHasNoLoader)
    {
        Assert::AreEqual (std::wstring (L"None"), Identify (MakeVolume ({})));
    }


    TEST_METHOD (ProDos8IsFoundWithItsVersionInThePRODOSFile)
    {
        BlockMap      map;
        vector<Byte>  volume = MakeProDosVolume ("Apple IIProDOS 8 v2.4.2      18", kHighBit, map);



        Assert::AreEqual (std::wstring (L"ProDOS 8 v2.4.2"), BootLoaderId::Identify (BlockImage (volume), map));
    }


    TEST_METHOD (ProDos8WithoutAVersionIsProDos8)
    {
        BlockMap      map;
        vector<Byte>  volume = MakeProDosVolume ("PRODOS 1.1.1", 0, map);



        Assert::AreEqual (std::wstring (L"ProDOS 8"), BootLoaderId::Identify (BlockImage (volume), map));
    }


    TEST_METHOD (GsOsIsAPRODOSLoaderBesideSystemGsOs)
    {
        BlockMap      map;
        vector<Byte>  volume = MakeProDosVolume ("", 0, map);



        Assert::AreEqual (std::wstring (L"ProDOS loader"), BootLoaderId::Identify (BlockImage (volume), map), L"a PRODOS file with no text and no GS.OS");

        map.files.push_back (L"SYSTEM/GS.OS");

        Assert::AreEqual (std::wstring (L"GS/OS"), BootLoaderId::Identify (BlockImage (volume), map));
    }


    TEST_METHOD (TheProDosLoaderWithoutAPRODOSFileCantBoot)
    {
        Assert::AreEqual (std::wstring (L"No PRODOS file"), Identify (MakeVolume ({ 0x01, 0x38, 0xB0, 0x03, 0x4C, 0x1C, 0x09 })));
    }


    TEST_METHOD (TheSosLoaderHasItsVersion)
    {
        vector<Byte>  volume = MakeVolume ({ 0x4C, 0x6E, 0xA0 });
        vector<Byte>  bare   = MakeVolume ({ 0x4C, 0x6E, 0xA0 });



        PutText (volume, 3, "SOS BOOT  1.1 ");
        PutText (bare, 504, "SOS BOOT");

        Assert::AreEqual (std::wstring (L"SOS loader 1.1"), Identify (volume));
        Assert::AreEqual (std::wstring (L"SOS loader"),     Identify (bare), L"no version at the end of the block");
    }


    TEST_METHOD (ThePascalLoaderIsFoundByItsMessageInApple2Text)
    {
        vector<Byte>  volume = MakeVolume ({ 0x01, 0xE0, 0x60, 0xF0, 0x03 });



        PutText (volume, 0x180, "MUST BOOT FROM SLOT 6", kHighBit);

        Assert::AreEqual (std::wstring (L"Apple Pascal loader"), Identify (volume, BlockFileSystem::Pascal));
    }


    TEST_METHOD (MacintoshBootBlocksAreOnlyFoundOnHfs)
    {
        vector<Byte>  volume = MakeVolume ({ 0x4C, 0x4B, 0x60, 0x00 });



        Assert::AreEqual (std::wstring (L"Macintosh boot blocks"), Identify (volume, BlockFileSystem::Hfs));
        Assert::AreEqual (std::wstring (L"Unknown loader"),        Identify (volume, BlockFileSystem::ProDos), L"on ProDOS the same bytes are 6502 code");
    }


    TEST_METHOD (OtherCodeIsAnUnknownLoader)
    {
        Assert::AreEqual (std::wstring (L"Unknown loader"), Identify (MakeVolume ({ 0x01, 0x8A, 0x48, 0xD8 })));
    }


    TEST_METHOD (AVolumeShorterThanABlockHasNoAnswer)
    {
        vector<Byte>  volume (100, 0);



        Assert::AreEqual (std::wstring(), Identify (volume));
    }
};
