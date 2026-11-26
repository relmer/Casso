#include "Pch.h"
#include "CassoExplorer/Model/LinkImageDropRule.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkImageDropRuleTests
//
//  One image dropped on a linked drive, by the image's class and the drive's
//  size and state.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkImageDropRuleTests)
{
public:

    static constexpr uint32_t  kSliceBlocks = 65535;



    static ImageClass Classify (const wchar_t * extension, uint64_t bytes)
    {
        std::vector<Byte>  head (ImageClassifier::kHeadBytes, 0);

        return ImageClassifier::ClassifyHeader (extension, bytes, head);
    }


    static LinkTreeEntry::Drive Floppy (bool empty)
    {
        LinkTreeEntry::Drive  drive;

        drive.empty = empty;
        return drive;
    }


    static LinkTreeEntry::Drive BlockDevice (uint32_t blocks, bool empty)
    {
        LinkTreeEntry::Drive  drive;

        drive.isBlockDevice = true;
        drive.drive         = 3;
        drive.blocks        = blocks;
        drive.empty         = empty;
        return drive;
    }



    TEST_METHOD (AFloppyImage_IsWrittenToAFloppyDrive)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".dsk", ImageClassifier::k525Bytes), Floppy (false)) == LinkImageDrop::Write);
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".do",  ImageClassifier::k525Bytes), Floppy (true))  == LinkImageDrop::Write);
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".nib", ImageClassifier::kNibBytes), Floppy (true))  == LinkImageDrop::Write);
    }


    TEST_METHOD (ABiggerImage_DoesNothingOnAFloppyDrive)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".po", 819200), Floppy (false)) == LinkImageDrop::None);
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".po", 819200), Floppy (true))  == LinkImageDrop::None);
    }


    TEST_METHOD (AnImageTheDevicesSize_IsWritten)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".hdv", kSliceBlocks * 512ull), BlockDevice (kSliceBlocks, false)) == LinkImageDrop::Write);
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".po",  819200),                BlockDevice (1600, false))         == LinkImageDrop::Write);
    }


    TEST_METHOD (ASmallerImage_OnAFormattedDevice_OffersBoth)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".po", 819200), BlockDevice (kSliceBlocks, false)) == LinkImageDrop::Choose);
    }


    TEST_METHOD (ASmallerImage_OnABlankDevice_IsWritten)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".po", 819200), BlockDevice (kSliceBlocks, true)) == LinkImageDrop::Write);
    }


    TEST_METHOD (ABiggerImage_DoesNothingOnADevice)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".hdv", kSliceBlocks * 512ull), BlockDevice (1600, false)) == LinkImageDrop::None);
    }


    TEST_METHOD (AFloppyImage_IsCopiedOntoADevicesVolume)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".dsk", ImageClassifier::k525Bytes), BlockDevice (kSliceBlocks, false)) == LinkImageDrop::Files);
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".dsk", ImageClassifier::k525Bytes), BlockDevice (kSliceBlocks, true))  == LinkImageDrop::None);
    }


    TEST_METHOD (AnUnclassifiedImage_IsCopiedAsAFile)
    {
        Assert::IsTrue (LinkImageDropRule::GetDrop (Classify (L".dsk", 1000), Floppy (false)) == LinkImageDrop::Files);
    }
};
