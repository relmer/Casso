#include "Pch.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/Inspector/Blocks/BootLoaderId.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BootLoaderIdTests
//
//  The loader in a volume's first block: none, ProDOS's, SOS's with its
//  version, Apple Pascal's, Macintosh boot blocks on HFS, and unknown code.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BootLoaderIdTests)
{
public:

    static constexpr size_t  kVolumeBytes = 1024;

    static vector<Byte> MakeVolume (std::initializer_list<Byte> start, size_t textAt = 0, const char * text = nullptr, Byte highBit = 0)
    {
        vector<Byte>  volume (kVolumeBytes, 0);
        size_t        i      = 0;



        std::copy (start.begin(), start.end(), volume.begin());

        for (i = 0; text != nullptr && text[i] != '\0'; i++)
        {
            volume[textAt + i] = (Byte) text[i] | highBit;
        }

        return volume;
    }


    static std::wstring Identify (const vector<Byte> & volume, BlockFileSystem fileSystem = BlockFileSystem::ProDos)
    {
        return BootLoaderId::Identify (BlockImage (volume), fileSystem);
    }


    TEST_METHOD (ABlockOfZerosHasNoLoader)
    {
        Assert::AreEqual (std::wstring (L"None"), Identify (MakeVolume ({})));
    }


    TEST_METHOD (TheProDosLoaderIsFoundByItsFirstInstructions)
    {
        Assert::AreEqual (std::wstring (L"ProDOS loader"), Identify (MakeVolume ({ 0x01, 0x38, 0xB0, 0x03, 0x4C, 0x32, 0xA1 })));
        Assert::AreEqual (std::wstring (L"ProDOS loader"), Identify (MakeVolume ({ 0x01, 0x38, 0xB0, 0x03, 0x4C, 0x1C, 0x09 })), L"either version of the loader");
    }


    TEST_METHOD (TheSosLoaderHasItsVersion)
    {
        Assert::AreEqual (std::wstring (L"SOS loader 1.1"), Identify (MakeVolume ({ 0x4C, 0x6E, 0xA0 }, 3, "SOS BOOT  1.1 ")));
        Assert::AreEqual (std::wstring (L"SOS loader"),     Identify (MakeVolume ({ 0x4C, 0x6E, 0xA0 }, 504, "SOS BOOT")), L"no version at the end of the block");
    }


    TEST_METHOD (ThePascalLoaderIsFoundByItsMessageInApple2Text)
    {
        vector<Byte>  volume = MakeVolume ({ 0x01, 0xE0, 0x60, 0xF0, 0x03 }, 0x180, "MUST BOOT FROM SLOT 6", 0x80);



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
