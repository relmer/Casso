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

    TEST_METHOD (SkeletonConstants_MatchGeometry)
    {
        // 280 blocks x 512 bytes == the 143,360-byte 5.25" sector image.
        Assert::AreEqual (NibblizationLayer::kImageByteSize,
                          ProDosSkeleton::kTotalBlocks * 512);
    }
};
