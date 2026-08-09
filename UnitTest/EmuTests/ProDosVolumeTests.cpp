#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ProDosVolumeTests
//
//  Spec 017: ProDOS skeleton structural invariants (R-005), file writer /
//  reader round-trips and bitmap-directory coherence (T016/T017), and boot
//  install placement over synthetic payload bytes (T018). No host fixture
//  files — synthetic volumes throughout.
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
