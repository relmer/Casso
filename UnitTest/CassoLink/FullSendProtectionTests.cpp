#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoLink/FullSend/FullSendProtection.h"
#include "Machines/Apple2/Common/Dos33Skeleton.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FullSendProtectionTests
//
//  The signs of copy protection in a sector image: an unrecognized file
//  system, disk code outside the operating system, and a boot sector of no
//  known loader.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FullSendProtectionTests)
{
public:

    using Image = std::vector<Byte>;

    //  A DOS 3.3 disk with one binary file of the given bytes.
    static Image  DosWithFile (const char * name, const std::vector<Byte> & bytes)
    {
        Image        buffer (NibblizationLayer::kImageByteSize, 0);
        Image        next;
        FilePayload  payload;



        payload.type           = Dos33Volume::kTypeBinary;
        payload.loadAddress    = 0x8A00;
        payload.hasLoadAddress = true;
        payload.bytes          = bytes;

        AssertSucceeded (Dos33Skeleton::Write (buffer, 254));
        AssertSucceeded (Dos33Volume (buffer).Write (FilePath::Parse (name), payload, next));

        return next;
    }

    //  Code that reads the data register `reads` times, as LDA $C08C,X with
    //  a BPL back to it, among NOPs.
    static std::vector<Byte>  DiskCode (int reads)
    {
        std::vector<Byte>  code (40, 0xEA);



        for (int i = 0; i < reads; i++)
        {
            code.insert (code.end(), { 0xBD, 0x8C, 0xC0, 0x10, 0xFB, 0xEA, 0xEA });
        }

        return code;
    }

    //  Bytes that differ from one another, with no file system.
    static Image  Noise()
    {
        Image     image (NibblizationLayer::kImageByteSize, 0);
        uint32_t  state = 12345;



        for (Byte & value : image)
        {
            state = state * 1103515245 + 12345;
            value = (Byte) (state >> 16);
        }

        return image;
    }

    static void  Put (Image & image, size_t at, std::initializer_list<Byte> bytes)
    {
        std::copy (bytes.begin(), bytes.end(), image.begin() + (ptrdiff_t) at);
    }

    TEST_METHOD (BootFamily_IsKnownByItsLoadersOpening_OrPascalsMessage)
    {
        Image              image (NibblizationLayer::kImageByteSize, 0);
        std::vector<Byte>  message;



        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::None, L"Zeros");

        Put (image, 0, { 0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x18, 0xA5, 0x2B });
        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::Dos33, L"DOS 3.3");

        Put (image, 0, { 0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x0F, 0x8A });
        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::Cpm, L"CP/M");

        Put (image, 0, { 0x01, 0x38, 0xB0, 0x03, 0x4C, 0x00, 0x00, 0x00, 0x00 });
        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::ProDos, L"ProDOS");

        Put (image, 0, { 0x01, 0xBD, 0x88, 0xC0, 0x20, 0x58, 0xFC, 0x00, 0x00 });
        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::ProDos, L"A ProDOS data disk");

        //  Apple Pascal's runtime loader, with its message in the boot block's
        //  second half, logical sector 14.
        Put (image, 0, { 0x01, 0xE0, 0x60, 0xF0, 0x03, 0x4C, 0xCB, 0x08, 0xAD });
        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::Other, L"A loader of no known family");

        for (char ch : std::string_view ("NO FILE RTSTRP.APPLE"))
        {
            message.push_back ((Byte) (ch | 0x80));
        }

        std::copy (message.begin(), message.end(), image.begin() + (ptrdiff_t) (14 * NibblizationLayer::kSectorByteSize + 0x20));

        Assert::IsTrue (FullSendProtection::GetBootFamily (image) == FullSendProtection::BootFamily::Pascal, L"Apple Pascal's runtime loader");
    }

    TEST_METHOD (DiskReads_AreIndexedReadsOfC08C_AndReadsOfSlot6sC0EC)
    {
        std::vector<Byte>  code = DiskCode (3);



        Assert::AreEqual (3, FullSendProtection::CountDiskReads (code));

        code.insert (code.end(), { 0xAD, 0xEC, 0xC0, 0xBD, 0x8D, 0xC0, 0xBD, 0x8C, 0xC1 });
        Assert::AreEqual (4, FullSendProtection::CountDiskReads (code), L"LDA $C0EC counts; $C08D,X and $C18C,X don't");
    }

    //  Wizardry I's boot disk has its track check in SYSTEM.STARTUP and the
    //  BIOS's disk code in RTSTRP.APPLE. The first is a sign; the second, a
    //  system file, isn't, nor is a file with fewer reads than the minimum.
    TEST_METHOD (DiskCode_InAProgram_IsASign_UnlessASystemFileOrTooFewReads)
    {
        ProtectionSigns  game    = FullSendProtection::Find (DosWithFile ("GAME", DiskCode (FullSendProtection::kDiskCodeReads)));
        ProtectionSigns  muffin  = FullSendProtection::Find (DosWithFile ("MUFFIN", DiskCode (FullSendProtection::kDiskCodeReads)));
        ProtectionSigns  few     = FullSendProtection::Find (DosWithFile ("GAME", DiskCode (FullSendProtection::kDiskCodeReads - 1)));



        Assert::IsTrue  (game.hasDiskCode, L"A program with disk code");
        Assert::AreEqual (std::string ("GAME"), game.diskCodeFile);
        Assert::IsFalse (game.isUnrecognized, L"A DOS 3.3 catalog");
        Assert::IsFalse (muffin.hasDiskCode, L"MUFFIN, on DOS 3.3's System Master");
        Assert::IsFalse (few.hasDiskCode, L"Fewer reads than the minimum");
        Assert::IsTrue  (FullSendProtection::IsSystemFile (VolumeKind::Pascal, "rtstrp.apple"), L"A system file in lowercase");
        Assert::IsTrue  (FullSendProtection::IsSystemFile (VolumeKind::Pascal, "128K.APPLE"), L"Apple Pascal's 128K BIOS");
        Assert::IsFalse (FullSendProtection::IsSystemFile (VolumeKind::Dos33, "128K.APPLE"), L"A .APPLE file on DOS 3.3");
        Assert::IsTrue  (FullSendProtection::IsSystemFile (VolumeKind::ProDos, "FILER"), L"ProDOS's Filer");
    }

    //  A disk with no file system is a sign, unless every byte is the same,
    //  as a disk formatted with nothing on it, or it has CP/M's loader.
    TEST_METHOD (Unrecognized_IsASign_UnlessUniformOrCpm)
    {
        Image  noise = Noise();
        Image  cpm   = Noise();
        Image  zeros (NibblizationLayer::kImageByteSize, 0);



        Put (cpm, 0, { 0x01, 0xA5, 0x27, 0xC9, 0x09, 0xD0, 0x0F, 0x8A });

        Assert::IsTrue  (FullSendProtection::Find (noise).isUnrecognized, L"No file system");
        Assert::IsTrue  (FullSendProtection::Find (noise).hasOtherBoot, L"No known loader");
        Assert::IsFalse (FullSendProtection::Find (zeros).isUnrecognized, L"Every byte the same");
        Assert::IsFalse (FullSendProtection::Find (zeros).hasOtherBoot, L"A boot sector of zeros");
        Assert::IsFalse (FullSendProtection::Find (cpm).isUnrecognized, L"CP/M's loader");
    }
};
