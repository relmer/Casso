#include "Pch.h"

#include "CassoExplorer/Picker/PickerRequest.h"
#include "CassoExplorer/Window/Panes/ExplorerTreePane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTreePaneTests
//
//  The Casso node's tip in the file manager and in each kind of picker.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ExplorerTreePaneTests)
{
public:

    TEST_METHOD (CassoNodeTip_IsDiskAndTapeImages_InTheFileManager)
    {
        PickerRequest  request;



        Assert::AreEqual (ExplorerTreePane::kCassoNodeTip, ExplorerTreePane::GetCassoNodeTip (nullptr));
        Assert::AreEqual (ExplorerTreePane::kCassoNodeTip, ExplorerTreePane::GetCassoNodeTip (&request));
    }


    TEST_METHOD (CassoNodeTip_IsDiskImages_InADiskDrivesPicker)
    {
        PickerRequest  request;



        for (ReceivingDriveKind kind : { ReceivingDriveKind::Floppy525, ReceivingDriveKind::Apple35, ReceivingDriveKind::UniDisk35, ReceivingDriveKind::HardDisk })
        {
            request.receivingDrive = ReceivingDrive { kind, 0 };

            Assert::AreEqual (ExplorerTreePane::kCassoNodeDiskTip, ExplorerTreePane::GetCassoNodeTip (&request));
        }
    }


    TEST_METHOD (CassoNodeTip_IsTapeImages_InTheRecordersPicker)
    {
        PickerRequest  request;



        request.receivingDrive = ReceivingDrive { ReceivingDriveKind::Recorder, 0 };

        Assert::AreEqual (ExplorerTreePane::kCassoNodeTapeTip, ExplorerTreePane::GetCassoNodeTip (&request));
    }
};
