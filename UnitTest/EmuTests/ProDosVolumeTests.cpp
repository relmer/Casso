#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeTests
//
//  ProDOS skeleton structural invariants, file writer / reader round-trips
//  and bitmap-directory coherence, and boot install placement over synthetic
//  payload bytes. No host fixture files — synthetic volumes throughout.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosVolumeTests)
{
public:

    static constexpr size_t  kKeyBlockEntry = 0x04;

    static vector<Byte> MakeVolume (const std::string & name = "NEWDISK")
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0xEE);

        AssertSucceeded (ProDosSkeleton::Write (buffer, name));

        return buffer;
    }

    static Byte At (const vector<Byte> & buffer, int block, size_t offset)
    {
        return buffer[ProDosSkeleton::BlockByteOffset (block, offset)];
    }

    static Word WordAt (const vector<Byte> & buffer, int block, size_t offset)
    {
        return (Word) (At (buffer, block, offset) | (At (buffer, block, offset + 1) << 8));
    }


    TEST_METHOD (SkeletonConstants_MatchGeometry)
    {
        // 280 blocks x 512 bytes == the 143,360-byte 5.25" sector image.
        Assert::AreEqual (NibblizationLayer::kImageByteSize,
                          ProDosSkeleton::kTotalBlocks * 512);
    }


    TEST_METHOD (BlockByteOffset_CoversTheWholeImageExactlyOnce)
    {
        // Every (block, offset) must land on a distinct buffer byte and the
        // union must tile the full image — the interleave map is a bijection.
        std::vector<char>  seen (NibblizationLayer::kImageByteSize, 0);
        int                block  = 0;
        size_t             offset = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            for (offset = 0; offset < 512; offset++)
            {
                size_t  at = ProDosSkeleton::BlockByteOffset (block, offset);

                Assert::IsTrue (at < (size_t) NibblizationLayer::kImageByteSize);
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

        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);



        AssertFailed (ProDosSkeleton::Write (buffer, ""));
        AssertFailed (ProDosSkeleton::Write (buffer, "1DISK"));
        AssertFailed (ProDosSkeleton::Write (buffer, "BAD NAME"));
        AssertFailed (ProDosSkeleton::Write (buffer, "WAYTOOLONGDISKNAME"));

        expect.RequireCount (4);
    }


    TEST_METHOD (KeyBlock_CarriesTheVolumeHeader)
    {
        vector<Byte>  vol = MakeVolume();

        // Storage type 0xF, name length 7, name NEWDISK uppercased.
        Assert::AreEqual ((Byte) 0xF7, At (vol, 2, kKeyBlockEntry + 0x00));

        Assert::AreEqual ((Byte) 'N', At (vol, 2, kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'E', At (vol, 2, kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) 'W', At (vol, 2, kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D', At (vol, 2, kKeyBlockEntry + 0x04));
        Assert::AreEqual ((Byte) 'I', At (vol, 2, kKeyBlockEntry + 0x05));
        Assert::AreEqual ((Byte) 'S', At (vol, 2, kKeyBlockEntry + 0x06));
        Assert::AreEqual ((Byte) 'K', At (vol, 2, kKeyBlockEntry + 0x07));

        Assert::AreEqual ((Byte) 0xC3, At (vol, 2, kKeyBlockEntry + 0x1E));  // access
        Assert::AreEqual ((Byte) 0x27, At (vol, 2, kKeyBlockEntry + 0x1F));  // entry length
        Assert::AreEqual ((Byte) 0x0D, At (vol, 2, kKeyBlockEntry + 0x20));  // entries/block

        Assert::AreEqual ((Word) 0,   WordAt (vol, 2, kKeyBlockEntry + 0x21));  // file count
        Assert::AreEqual ((Word) 6,   WordAt (vol, 2, kKeyBlockEntry + 0x23));  // bitmap ptr
        Assert::AreEqual ((Word) 280, WordAt (vol, 2, kKeyBlockEntry + 0x25));  // total blocks
    }


    TEST_METHOD (Write_UppercasesTheVolumeName)
    {
        vector<Byte>  vol = MakeVolume ("My.Disk");

        Assert::AreEqual ((Byte) 0xF7, At (vol, 2, kKeyBlockEntry + 0x00));
        Assert::AreEqual ((Byte) 'M',  At (vol, 2, kKeyBlockEntry + 0x01));
        Assert::AreEqual ((Byte) 'Y',  At (vol, 2, kKeyBlockEntry + 0x02));
        Assert::AreEqual ((Byte) '.',  At (vol, 2, kKeyBlockEntry + 0x03));
        Assert::AreEqual ((Byte) 'D',  At (vol, 2, kKeyBlockEntry + 0x04));
    }


    TEST_METHOD (DirectoryChain_LinksBlocks2Through5)
    {
        vector<Byte>  vol = MakeVolume();

        Assert::AreEqual ((Word) 0, WordAt (vol, 2, 0x00));  // key has no prev
        Assert::AreEqual ((Word) 3, WordAt (vol, 2, 0x02));

        Assert::AreEqual ((Word) 2, WordAt (vol, 3, 0x00));
        Assert::AreEqual ((Word) 4, WordAt (vol, 3, 0x02));

        Assert::AreEqual ((Word) 3, WordAt (vol, 4, 0x00));
        Assert::AreEqual ((Word) 5, WordAt (vol, 4, 0x02));

        Assert::AreEqual ((Word) 4, WordAt (vol, 5, 0x00));
        Assert::AreEqual ((Word) 0, WordAt (vol, 5, 0x02));  // chain ends
    }


    TEST_METHOD (Bitmap_MarksBlocks0Through6UsedAndTheRestFree)
    {
        vector<Byte>  vol = MakeVolume();
        size_t        i   = 0;

        // Byte 0: blocks 0-6 used (clear), block 7 free (set).
        Assert::AreEqual ((Byte) 0x01, At (vol, 6, 0));

        // Bytes 1-34: blocks 8-279 all free. 280 blocks is exactly 35 bytes.
        for (i = 1; i < 35; i++)
        {
            Assert::AreEqual ((Byte) 0xFF, At (vol, 6, i));
        }

        // The rest of the bitmap block is dead space and stays zero.
        for (i = 35; i < 512; i++)
        {
            Assert::AreEqual ((Byte) 0x00, At (vol, 6, i));
        }
    }


    TEST_METHOD (EverythingOutsideDirAndBitmapIsZero)
    {
        vector<Byte>  vol    = MakeVolume();
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
                if (At (vol, block, offset) != 0)
                {
                    wchar_t  msg[96] = {};

                    swprintf_s (msg, L"block %d offset %zu must be zero", block, offset);
                    Assert::Fail (msg);
                }
            }
        }
    }
};
