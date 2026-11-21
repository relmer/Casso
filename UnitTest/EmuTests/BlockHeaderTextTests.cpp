#include "Pch.h"
#include "Devices/Disk/Inspector/Blocks/BlockFieldList.h"
#include "Ui/DiskInspector/Blocks/BlockHeaderText.h"
#include "Ui/DiskInspector/Blocks/BlockHexSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockHeaderTextTests
//
//  A byte's tip in the hex view, and a block as text for Copy block.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockHeaderTextTests)
{
public:

    TEST_METHOD (ByteTip_WithNoMark_HasNoNote)
    {
        int  noteLine = 0;



        Assert::AreEqual (std::wstring (L"Byte $01F"), BlockHeaderText::BuildByteTip (0x1F, BlockHexSource::kMarkNone, nullptr, L"", noteLine));
        Assert::AreEqual (-1, noteLine);
    }


    TEST_METHOD (ByteTip_ForALink_ListsTheFieldAndTheBlock)
    {
        BlockField  field;
        int         noteLine = 0;



        field.label = L"Key block";
        field.value = L"2";
        field.link  = 2;

        Assert::AreEqual (std::wstring (L"Byte $004\nKey block: 2\nLinks to block 2. Click to open it."),
                          BlockHeaderText::BuildByteTip (4, BlockHexSource::kMarkLink, &field, L"", noteLine));
        Assert::AreEqual (2, noteLine);
    }


    TEST_METHOD (ByteTip_ForADifference_ListsTheOtherImage)
    {
        int  noteLine = 0;



        Assert::AreEqual (std::wstring (L"Byte $000\nDiffers from other.po"), BlockHeaderText::BuildByteTip (0, BlockHexSource::kMarkDiffers, nullptr, L"other.po", noteLine));
        Assert::AreEqual (1, noteLine);
    }


    TEST_METHOD (BlockText_HasTheOffsetBytesAndText)
    {
        std::array<Byte, 4>  bytes = { 0xC1, 0x42, 0x00, 0x7F };



        Assert::AreEqual (std::wstring (L"0000  C1 42  AB\r\n0002  00 7F  ..\r\n"), BlockHeaderText::BuildBlockText (bytes, 2));
    }
};
