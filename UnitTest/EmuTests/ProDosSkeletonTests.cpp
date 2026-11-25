#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosSkeletonTests
//
//  ProDosSkeleton: the blank volume layout, the volume header, directory
//  chain and bitmap, and the boot install.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosSkeletonTests)
{
public:

    TEST_METHOD (SkeletonConstants_MatchGeometry)
    {
        // 280 blocks x 512 bytes == the 143,360-byte 5.25" sector image.
        Assert::AreEqual (NibbleFormat::kImageByteSize,
                          ProDosSkeleton::kTotalBlocks * 512);
    }


    TEST_METHOD (BlockByteOffset_CoversTheWholeImageExactlyOnce)
    {
        // Every (block, offset) must land on a distinct buffer byte and the
        // union must tile the full image — the interleave map is a bijection.
        std::vector<char>  seen (NibbleFormat::kImageByteSize, 0);
        int                block  = 0;
        size_t             offset = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            for (offset = 0; offset < 512; offset++)
            {
                size_t  at = ProDosSkeleton::GetBlockByteOffset (block, offset);

                Assert::IsTrue (at < (size_t) NibbleFormat::kImageByteSize);
                Assert::AreEqual ((char) 0, seen[at]);
                seen[at] = 1;
            }
        }
    }


    TEST_METHOD (Write_RejectsWrongBufferSize)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  undersized (512, 0);



        AssertFailed (ProDosSkeleton::Write (undersized, "NEWDISK"));
        expect.RequireCount (1);
    }


    TEST_METHOD (Write_RejectsInvalidVolumeNames)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  buffer (NibbleFormat::kImageByteSize, 0);



        AssertFailed (ProDosSkeleton::Write (buffer, ""));
        AssertFailed (ProDosSkeleton::Write (buffer, "1DISK"));
        AssertFailed (ProDosSkeleton::Write (buffer, "BAD NAME"));
        AssertFailed (ProDosSkeleton::Write (buffer, "WAYTOOLONGDISKNAME"));

        expect.RequireCount (4);
    }


    TEST_METHOD (KeyBlock_CarriesTheVolumeHeader)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();

        // Storage type 0xF, name length 7, name NEWDISK uppercased.
        Assert::AreEqual ((Byte) 0xF7, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x00));

        Assert::AreEqual ((Byte) 'N', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'E', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) 'W', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x04));
        Assert::AreEqual ((Byte) 'I', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x05));
        Assert::AreEqual ((Byte) 'S', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x06));
        Assert::AreEqual ((Byte) 'K', ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x07));

        Assert::AreEqual ((Byte) 0xC3, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x1E));  // access
        Assert::AreEqual ((Byte) 0x27, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x1F));  // entry length
        Assert::AreEqual ((Byte) 0x0D, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x20));  // entries/block

        Assert::AreEqual ((Word) 0,   ProDosVolumeFixtures::WordAt (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x21));  // file count
        Assert::AreEqual ((Word) 6,   ProDosVolumeFixtures::WordAt (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x23));  // bitmap ptr
        Assert::AreEqual ((Word) 280, ProDosVolumeFixtures::WordAt (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x25));  // total blocks
    }


    TEST_METHOD (Write_UppercasesTheVolumeName)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume ("My.Disk");

        Assert::AreEqual ((Byte) 0xF7, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x00));
        Assert::AreEqual ((Byte) 'M',  ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'Y',  ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) '.',  ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D',  ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x04));
    }


    TEST_METHOD (DirectoryChain_LinksBlocks2Through5)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();

        Assert::AreEqual ((Word) 0, ProDosVolumeFixtures::WordAt (vol, 2, 0x00));  // key has no prev
        Assert::AreEqual ((Word) 3, ProDosVolumeFixtures::WordAt (vol, 2, 0x02));

        Assert::AreEqual ((Word) 2, ProDosVolumeFixtures::WordAt (vol, 3, 0x00));
        Assert::AreEqual ((Word) 4, ProDosVolumeFixtures::WordAt (vol, 3, 0x02));

        Assert::AreEqual ((Word) 3, ProDosVolumeFixtures::WordAt (vol, 4, 0x00));
        Assert::AreEqual ((Word) 5, ProDosVolumeFixtures::WordAt (vol, 4, 0x02));

        Assert::AreEqual ((Word) 4, ProDosVolumeFixtures::WordAt (vol, 5, 0x00));
        Assert::AreEqual ((Word) 0, ProDosVolumeFixtures::WordAt (vol, 5, 0x02));  // chain ends
    }


    TEST_METHOD (Bitmap_MarksBlocks0Through6UsedAndTheRestFree)
    {
        vector<Byte>  vol = ProDosVolumeFixtures::MakeVolume();
        size_t        i   = 0;

        // Byte 0: blocks 0-6 used (clear), block 7 free (set).
        Assert::AreEqual ((Byte) 0x01, ProDosVolumeFixtures::At (vol, 6, 0));

        // Bytes 1-34: blocks 8-279 all free. 280 blocks is exactly 35 bytes.
        for (i = 1; i < 35; i++)
        {
            Assert::AreEqual ((Byte) 0xFF, ProDosVolumeFixtures::At (vol, 6, i));
        }

        // The rest of the bitmap block is dead space and stays zero.
        for (i = 35; i < 512; i++)
        {
            Assert::AreEqual ((Byte) 0x00, ProDosVolumeFixtures::At (vol, 6, i));
        }
    }


    static vector<Byte> MakeSyntheticUsersDisk (vector<Byte> & outProdos, vector<Byte> & outBasic)
    {
        // A fabricated Users Disk: a real skeleton volume carrying a boot
        // pattern in blocks 0-1 and synthetic PRODOS / BASIC.SYSTEM files.
        vector<Byte>  users = ProDosVolumeFixtures::MakeVolume ("USERS.DISK");
        size_t        i     = 0;

        for (i = 0; i < 1024; i++)
        {
            users[ProDosSkeleton::GetBlockByteOffset ((int) (i / 512), i % 512)] =
                (Byte) ((i * 13 + 1) & 0xFF);
        }

        outProdos = ProDosVolumeFixtures::MakePattern (15'000);
        outBasic  = ProDosVolumeFixtures::MakePattern (10'000);

        for (i = 0; i < outBasic.size(); i++)
        {
            outBasic[i] = (Byte) ~outBasic[i];
        }

        AssertSucceeded (ProDosFileWriter::WriteFile (users, "PRODOS", 0xFF, 0x2000, outProdos));
        AssertSucceeded (ProDosFileWriter::WriteFile (users, "BASIC.SYSTEM", 0xFF, 0x2000, outBasic));

        return users;
    }


    TEST_METHOD (InstallBoot_CopiesBootBlocksAndBothSystemFiles)
    {
        vector<Byte>  prodosBytes;
        vector<Byte>  basicBytes;
        vector<Byte>  users    = MakeSyntheticUsersDisk (prodosBytes, basicBytes);
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;
        size_t        i        = 0;



        AssertSucceeded (ProDosSkeleton::InstallBoot (vol, users));

        // Boot blocks 0-1 copied verbatim.
        for (i = 0; i < 1024; i++)
        {
            size_t  at = ProDosSkeleton::GetBlockByteOffset ((int) (i / 512), i % 512);

            if (vol[at] != users[at])
            {
                Assert::Fail (L"boot blocks must copy verbatim");
            }
        }

        // Both system files landed as real, readable files.
        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", readBack, fileType, auxType));
        Assert::IsTrue (prodosBytes == readBack, L"PRODOS must round-trip through the install");
        Assert::AreEqual ((Word) 0x2000, auxType);

        AssertSucceeded (ProDosReader::ExtractFile (vol, "BASIC.SYSTEM", readBack, fileType, auxType));
        Assert::IsTrue (basicBytes == readBack, L"BASIC.SYSTEM must round-trip through the install");

        // The volume name stayed the skeleton's own.
        Assert::AreEqual ((Byte) 0xF7, ProDosVolumeFixtures::At (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x00));
    }


    TEST_METHOD (EverythingOutsideDirAndBitmapIsZero)
    {
        vector<Byte>  vol    = ProDosVolumeFixtures::MakeVolume();
        int           block  = 0;
        size_t        offset = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            bool  structural = (block >= 2 && block <= 6);

            if (structural)
            {
                continue;
            }

            for (offset = 0; offset < 512; offset++)
            {
                if (ProDosVolumeFixtures::At (vol, block, offset) != 0)
                {
                    wchar_t  msg[96] = {};

                    swprintf_s (msg, L"block %d offset %zu must be zero", block, offset);
                    Assert::Fail (msg);
                }
            }
        }
    }
};
