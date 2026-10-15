#include "Pch.h"

#include "Debugger/DiskBreakSpecText.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakSpecTextTests
//
//  A disk breakpoint as BPDISK's arguments and as the replies' description,
//  with the contract's examples (contracts/command-modes.md, "Disk
//  breakpoints").
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskBreakSpecTextTests)
    {
    public:

        static DiskBreakSpec MakeRead (std::optional<Byte> track, std::optional<Byte> sector)
        {
            DiskBreakSpec  spec;



            spec.event  = DiskEvent::SectorRead;
            spec.track  = track;
            spec.sector = sector;
            return spec;
        }

        TEST_METHOD (ASectorReadOnDriveOne)
        {
            DiskBreakSpec  spec = MakeRead (0x11, 0x00);



            spec.drive = DiskDriveFilter::Drive1;

            Assert::AreEqual (std::string ("D1 READ 11 0"), DiskBreakSpecText::FormatArguments (spec));
            Assert::AreEqual (std::string ("sector read, track $11 sector $00, drive 1"), DiskBreakSpecText::GetDescription (spec));
            Assert::AreEqual ("Sector read", DiskBreakSpecText::GetTriggerText (spec.event));
        }

        TEST_METHOD (AStarStandsForAnyTrackOrSector)
        {
            DiskBreakSpec  spec = MakeRead (std::nullopt, 0x0F);



            spec.isPassed = true;

            Assert::AreEqual (std::string ("READ * F PASSED"), DiskBreakSpecText::FormatArguments (spec));
            Assert::AreEqual (std::string ("sector read, any track sector $0F, fields passed over too"), DiskBreakSpecText::GetDescription (spec));
        }

        TEST_METHOD (AnRwtsCallWithItsRequest)
        {
            DiskBreakSpec  spec;



            spec.event   = DiskEvent::RwtsCall;
            spec.track   = 0x11;
            spec.sector  = 0x00;
            spec.command = 1;

            Assert::AreEqual (std::string ("RWTS 11 0 READ"), DiskBreakSpecText::FormatArguments (spec));
            Assert::AreEqual (std::string ("RWTS call, track $11 sector $00, read"), DiskBreakSpecText::GetDescription (spec));
        }

        TEST_METHOD (AnAddressFieldLeavesOutTheFieldsNotGiven)
        {
            DiskBreakSpec  spec;



            spec.event         = DiskEvent::AddressField;
            spec.track         = 0x03;
            spec.isBadChecksum = true;

            Assert::AreEqual (std::string ("ADDRESS 3 BADSUM"), DiskBreakSpecText::FormatArguments (spec));
            Assert::AreEqual (std::string ("address field, track $03, bad checksum"), DiskBreakSpecText::GetDescription (spec));
        }

        TEST_METHOD (AHeadRangeInWholeTracksAndABump)
        {
            DiskBreakSpec  spec;



            spec.event         = DiskEvent::Head;
            spec.head          = DiskHeadFilter::Range;
            spec.qtFirst       = 0x44;
            spec.qtLast        = 0x48;
            spec.isWholeTracks = true;
            spec.isBump        = true;

            Assert::AreEqual (std::string ("HEAD T 11:12 BUMP"), DiskBreakSpecText::FormatArguments (spec));
            Assert::AreEqual (std::string ("head, tracks $11-$12, bump"), DiskBreakSpecText::GetDescription (spec));
        }

        TEST_METHOD (CustomMarksAndThirteenSectors)
        {
            DiskBreakSpec    spec = MakeRead (0x00, 0x00);
            DiskMarkPattern  marks;
            std::string      error;



            Assert::IsTrue (DiskMarkPattern::TryParse ("D4 AA ??", marks, error));

            spec.addressMarks = marks;
            spec.format       = DiskFieldKind::Thirteen;

            Assert::AreEqual (std::string ("READ 0 0 ADDR D4 AA ?? SECTORS 13"), DiskBreakSpecText::FormatArguments (spec));
        }

        TEST_METHOD (TheDriverAndTheOtherEvents)
        {
            DiskBreakSpec  driver;
            DiskBreakSpec  motor;
            DiskBreakSpec  select;
            DiskBreakSpec  blocked;



            driver.event   = DiskEvent::DriverCall;
            driver.block   = 0x0002;
            driver.command = 3;

            motor.event  = DiskEvent::Motor;
            motor.motor  = DiskMotorFilter::Stopped;

            select.event   = DiskEvent::DriveSelect;
            select.drive   = DiskDriveFilter::Drive2;
            select.isEvery = true;

            blocked.event = DiskEvent::WriteBlocked;

            Assert::AreEqual (std::string ("DRIVER BLOCK 2 FORMAT"), DiskBreakSpecText::FormatArguments (driver));
            Assert::AreEqual (std::string ("MOTOR STOPPED"),         DiskBreakSpecText::FormatArguments (motor));
            Assert::AreEqual (std::string ("D2 SELECT EVERY"),       DiskBreakSpecText::FormatArguments (select));
            Assert::AreEqual (std::string ("WRITE BLOCKED"),         DiskBreakSpecText::FormatArguments (blocked));
            Assert::AreEqual ("Write", DiskBreakSpecText::GetTriggerText (blocked.event));
        }
    };
}
