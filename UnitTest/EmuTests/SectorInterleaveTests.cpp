#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorInterleave.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SectorInterleaveTests
//
//  The DOS 3.3 and ProDOS skews, checked against tables written out here
//  rather than read from the code under test.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SectorInterleaveTests)
{
public:

    static constexpr int  kSectors = NibbleFormat::kSectorsPerTrack;



    TEST_METHOD (DosSkewIsTheStandardOrder)
    {
        constexpr int  kExpected[kSectors] = { 0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15 };



        int  physical = 0;



        for (physical = 0; physical < kSectors; physical++)
        {
            Assert::AreEqual (kExpected[physical], SectorInterleave::kDsk_PhysicalToFile[physical]);
            Assert::AreEqual (kExpected[physical], SectorInterleave::GetDosFileIndexForPhysicalSector (physical));
        }
    }



    TEST_METHOD (ProDosSkewIsTheStandardOrder)
    {
        constexpr int  kExpected[kSectors] = { 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15 };



        int  physical = 0;



        for (physical = 0; physical < kSectors; physical++)
        {
            Assert::AreEqual (kExpected[physical], SectorInterleave::kPo_PhysicalToFile[physical]);
        }
    }



    //  A DOS logical sector and its .po file index are under the same
    //  physical address field.
    TEST_METHOD (PoIndexForDosSectorMeetsAtTheSamePhysicalSector)
    {
        std::set<int>  seen;
        int            physical = 0;
        int            logical  = 0;
        int            poIndex  = 0;



        for (physical = 0; physical < kSectors; physical++)
        {
            logical = SectorInterleave::kDsk_PhysicalToFile[physical];
            poIndex = SectorInterleave::GetPoFileIndexForDosLogicalSector (logical);

            Assert::AreEqual (SectorInterleave::kPo_PhysicalToFile[physical], poIndex);
            seen.insert (poIndex);
        }

        Assert::AreEqual ((size_t) kSectors, seen.size(), L"Every .po index once");
    }
};
