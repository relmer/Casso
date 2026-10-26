#include "Pch.h"

#include "Machines/Apple2/Common/PrinterCard.h"
#include "Core/ComponentRegistry.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrinterCardTests
//
//  The guest-visible register contract: slot I/O window placement, data
//  latch -> ring ordering (FR-004), tolerant status reads, the high-water
//  ready/busy transition backing FR-002, and the first-touch reveal flag.
//
//  This file has the case that writes the text copy to a real file. The
//  register cases are in the unit suite's PrinterCardTests.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterCardTests
{
    TEST_CLASS (PrinterCardTests)
    {
    public:

        //  A path in CASSO_PRINTER_TEXT makes the card keep a text copy of what
    //  the guest printed, which is how a program's printed output is read
    //  back: the high bit goes, a carriage return ends the line, and the line
    //  feed a driver adds after it is left out.
    TEST_METHOD (TheTextCopyHoldsWhatTheGuestPrinted)
    {
        std::filesystem::path  path = std::filesystem::temp_directory_path() / "casso-printer-text-test.txt";
        std::string            text;
        std::ifstream          in;



        std::filesystem::remove (path);
        _putenv_s (PrinterCard::kTextPathVariable, path.string().c_str());

        {
            auto  card = std::make_unique<PrinterCard> (1);   // heap: embeds the 64KB ring (C6262)

            for (Byte value : { (Byte) 0xC8, (Byte) 0xE9, (Byte) 0x8D, (Byte) 0x0A, (Byte) 0xA1 })
            {
                card->Write (PrinterCard::kSlotIoBase + 0x10, value);
            }
        }

        _putenv_s (PrinterCard::kTextPathVariable, "");

        in.open (path, std::ios::binary);
        std::getline (in, text);

        Assert::AreEqual (std::string ("Hi"), text, L"the high bit is dropped");

        std::getline (in, text);

        Assert::AreEqual (std::string ("!"), text, L"the carriage return ended the line, and the line feed added none");

        in.close();
        std::filesystem::remove (path);
    }

    };
}
