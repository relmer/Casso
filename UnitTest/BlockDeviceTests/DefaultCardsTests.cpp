#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "../EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DefaultCardsTests
//
//  The cards the shipped machines have: the //e and the enhanced //e have
//  the SCSI card in slot 7 with an HD20SC on the stock image, and the UniDisk
//  3.5 interface card in slot 5 with one empty UniDisk 3.5. The ][+ and the
//  //c have neither.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DefaultCardsTests)
{
public:

    TEST_METHOD (IIeHasTheScsiAndUniDiskCards)
    {
        for (const char * id : { "Apple2e", "Apple2eEnhanced" })
        {
            TestMachine   machine (id);
            std::wstring  name = TextEncoding::NarrowToWide (id);



            Assert::AreEqual ((size_t) 1, machine.GetRefs().scsiCards.size(), name.c_str());
            Assert::AreEqual ((size_t) 1, machine.GetRefs().lironCards.size(), name.c_str());

            Assert::AreEqual ((size_t) 1, machine.GetRefs().lironCards[0]->GetDriveCount(), L"one UniDisk 3.5");
            Assert::IsFalse (machine.GetRefs().lironCards[0]->GetDrive (0).GetMechanism().HasDisk(), L"with no disk");
        }
    }


    TEST_METHOD (OtherMachinesHaveNeither)
    {
        for (const char * id : { "Apple2Plus", "Apple2c" })
        {
            TestMachine   machine (id);
            std::wstring  name = TextEncoding::NarrowToWide (id);



            Assert::IsTrue (machine.GetRefs().scsiCards.empty(), name.c_str());
            Assert::IsTrue (machine.GetRefs().lironCards.empty(), name.c_str());
            Assert::IsNull (machine.GetRefs().apple2cSmartPort, name.c_str());
        }
    }
};
