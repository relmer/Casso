#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "ProDosVolumeFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosFileWriterTests
//
//  ProDosFileWriter: seedling and sapling files written onto a blank volume
//  and read back through ProDosReader.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (ProDosFileWriterTests)
{
public:

    static int CountFreeBlocks (const vector<Byte> & vol)
    {
        int  free  = 0;
        int  block = 0;

        for (block = 0; block < ProDosSkeleton::kTotalBlocks; block++)
        {
            Byte  b    = ProDosVolumeFixtures::At (vol, 6, (size_t) (block / 8));
            Byte  mask = (Byte) (0x80 >> (block % 8));

            if ((b & mask) != 0) { free++; }
        }

        return free;
    }


    TEST_METHOD (FileWriter_SeedlingRoundTripsThroughTheReader)
    {
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  payload  = ProDosVolumeFixtures::MakePattern (300);
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0x2000, payload));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", readBack, fileType, auxType));

        Assert::IsTrue (payload == readBack, L"seedling data must round-trip");
        Assert::AreEqual ((Byte) 0xFF,   fileType);
        Assert::AreEqual ((Word) 0x2000, auxType);
    }


    TEST_METHOD (FileWriter_SaplingRoundTripsThroughTheReader)
    {
        vector<Byte>  vol      = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  payload  = ProDosVolumeFixtures::MakePattern (10'873);   // 22 data blocks
        vector<Byte>  readBack;
        Byte          fileType = 0;
        Word          auxType  = 0;



        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BASIC.SYSTEM", 0xFF, 0x2000, payload));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "basic.system", readBack, fileType, auxType));

        Assert::IsTrue (payload == readBack, L"sapling data must round-trip, case-insensitive lookup");
    }


    TEST_METHOD (FileWriter_TwoFilesShareNoBlocks)
    {
        vector<Byte>  vol        = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  a          = ProDosVolumeFixtures::MakePattern (2'000);
        vector<Byte>  b          = ProDosVolumeFixtures::MakePattern (3'000);
        vector<Byte>  backA;
        vector<Byte>  backB;
        Byte          ft         = 0;
        Word          aux        = 0;
        int           freeBefore = CountFreeBlocks (vol);
        int           freeAfter  = 0;



        // Distinct content so cross-contamination cannot cancel out.
        for (size_t i = 0; i < b.size(); i++) { b[i] = (Byte) ~b[i]; }

        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, a));
        AssertSucceeded (ProDosFileWriter::WriteFile (vol, "BASIC.SYSTEM", 0xFF, 0, b));

        AssertSucceeded (ProDosReader::ExtractFile (vol, "PRODOS", backA, ft, aux));
        AssertSucceeded (ProDosReader::ExtractFile (vol, "BASIC.SYSTEM", backB, ft, aux));

        Assert::IsTrue (a == backA, L"first file intact after the second write");
        Assert::IsTrue (b == backB, L"second file intact");

        // Bitmap accounting: 4+1 blocks for a (sapling), 6+1 for b.
        freeAfter = CountFreeBlocks (vol);
        Assert::AreEqual (freeBefore - 12, freeAfter, L"exactly the allocated blocks left the bitmap");

        // The header counts both files.
        Assert::AreEqual ((Word) 2, ProDosVolumeFixtures::WordAt (vol, 2, ProDosVolumeFixtures::kKeyBlockEntry + 0x21));
    }


    TEST_METHOD (FileWriter_RejectsBadArguments)
    {
        UnitTestHelpers::ExpectedEhmAssert  expect;

        vector<Byte>  vol       = ProDosVolumeFixtures::MakeVolume();
        vector<Byte>  undersized (100, 0);
        vector<Byte>  payload    = ProDosVolumeFixtures::MakePattern (10);
        vector<Byte>  oversized  = ProDosVolumeFixtures::MakePattern (257 * 512);



        AssertFailed (ProDosFileWriter::WriteFile (undersized, "PRODOS", 0xFF, 0, payload));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "", 0xFF, 0, payload));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, {}));
        AssertFailed (ProDosFileWriter::WriteFile (vol, "PRODOS", 0xFF, 0, oversized));

        expect.RequireCount (4);
    }
};
