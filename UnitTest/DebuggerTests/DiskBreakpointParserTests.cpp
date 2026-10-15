#include "Pch.h"

#include "Core/TextEncoding.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/DiskBreakSpecText.h"
#include "MockExpressionContext.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBreakpointParserTests
//
//  BPDISK's grammar, row by row from contracts/command-modes.md "Disk
//  breakpoints": each event with its arguments and options, the drive, a
//  star for any, the IF clause, and each error with its event's syntax line.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiskBreakpointParserTests)
    {
    public:

        static DiskBreakSpec ParseOk (const std::string & line)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse (line, context);
            std::wstring           where  = TextEncoding::NarrowToWide (line + ": " + result.error);



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status, where.c_str());
            Assert::IsTrue   (result.command.verb == DebugVerb::SetDiskBreakpoint, where.c_str());
            Assert::IsTrue   (result.command.disk.has_value(), where.c_str());
            return *result.command.disk;
        }

        static std::string ParseFails (const std::string & line)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse (line, context);



            Assert::AreEqual ((int) ParseStatus::Invalid, (int) result.status, TextEncoding::NarrowToWide (line).c_str());
            return result.error;
        }

        //  Each line read back to the same arguments it was given.
        static void AssertRoundTrip (const std::string & arguments)
        {
            Assert::AreEqual (arguments, DiskBreakSpecText::FormatArguments (ParseOk ("BPDISK " + arguments)));
        }

        TEST_METHOD (ReadTakesTrackAndSector_AndItsOptions)
        {
            DiskBreakSpec  spec = ParseOk ("BPDISK D1 READ 11 0 PASSED ADDR D5 AA ?? DATA D5 AA AD SECTORS 13");



            Assert::IsTrue   (spec.event == DiskEvent::SectorRead);
            Assert::IsTrue   (spec.drive == DiskDriveFilter::Drive1);
            Assert::AreEqual ((Byte) 0x11, spec.track.value_or (0xEE));
            Assert::AreEqual ((Byte) 0x00, spec.sector.value_or (0xEE));
            Assert::IsTrue   (spec.isPassed);
            Assert::IsTrue   (spec.addressMarks.has_value() && spec.addressMarks->IsAny (2));
            Assert::IsTrue   (spec.dataMarks.has_value());
            Assert::IsTrue   (spec.format == DiskFieldKind::Thirteen);
        }

        TEST_METHOD (AStarMatchesAny)
        {
            DiskBreakSpec  spec = ParseOk ("BPDISK READ * 5");



            Assert::IsFalse  (spec.track.has_value());
            Assert::AreEqual ((Byte) 5, spec.sector.value_or (0xEE));
        }

        TEST_METHOD (AddressTakesUpToThreeFields)
        {
            DiskBreakSpec  bare  = ParseOk ("BPDISK ADDRESS");
            DiskBreakSpec  full  = ParseOk ("BPDISK D2 ADDRESS 3 * FE BADSUM WRONGTRACK");



            Assert::IsFalse  (bare.track.has_value() || bare.sector.has_value() || bare.volume.has_value());
            Assert::AreEqual ((Byte) 3,    full.track.value_or (0));
            Assert::IsFalse  (full.sector.has_value());
            Assert::AreEqual ((Byte) 0xFE, full.volume.value_or (0));
            Assert::IsTrue   (full.isBadChecksum && full.isWrongTrack);
        }

        TEST_METHOD (HeadTakesQuarterTracksTracksOrHalf)
        {
            DiskBreakSpec  quarters = ParseOk ("BPDISK HEAD QT 0 BUMP");
            DiskBreakSpec  tracks   = ParseOk ("BPDISK HEAD T 11:12");
            DiskBreakSpec  half     = ParseOk ("BPDISK HEAD HALF");
            DiskBreakSpec  any      = ParseOk ("BPDISK HEAD BUMP");



            Assert::IsTrue   (quarters.head == DiskHeadFilter::Range && quarters.qtFirst == 0 && quarters.qtLast == 0 && quarters.isBump);
            Assert::AreEqual (0x44, tracks.qtFirst, L"T 11 is quarter track $44");
            Assert::AreEqual (0x48, tracks.qtLast);
            Assert::IsTrue   (tracks.isWholeTracks);
            Assert::IsTrue   (half.head == DiskHeadFilter::Half);
            Assert::IsTrue   (any.head == DiskHeadFilter::Any && any.isBump);
        }

        TEST_METHOD (WriteIsAModeAPrologueOrABlockedWrite)
        {
            Assert::IsTrue (ParseOk ("BPDISK WRITE").event                  == DiskEvent::WriteMode);
            Assert::IsTrue (ParseOk ("BPDISK WRITE QT 10:20").event         == DiskEvent::WriteMode);
            Assert::IsTrue (ParseOk ("BPDISK WRITE 11 0 DATA D5 AA AD").event == DiskEvent::WritePrologue);
            Assert::IsTrue (ParseOk ("BPDISK WRITE BLOCKED").event          == DiskEvent::WriteBlocked);
        }

        TEST_METHOD (DosBlockRwtsAndDriver)
        {
            DiskBreakSpec  dos    = ParseOk ("BPDISK DOS 11 F PASSED");
            DiskBreakSpec  block  = ParseOk ("BPDISK BLOCK 2");
            DiskBreakSpec  rwts   = ParseOk ("BPDISK RWTS 11 0 READ");
            DiskBreakSpec  driver = ParseOk ("BPDISK DRIVER BLOCK 2 FORMAT");
            DiskBreakSpec  any    = ParseOk ("BPDISK RWTS");



            Assert::IsTrue   (dos.event == DiskEvent::DosSector && dos.isPassed);
            Assert::AreEqual ((Word) 2, block.block.value_or (0));
            Assert::AreEqual ((Byte) 1, rwts.command.value_or (9), L"READ is RWTS's command 1");
            Assert::AreEqual ((Byte) 3, driver.command.value_or (9), L"FORMAT is the driver's command 3");
            Assert::AreEqual ((Word) 2, driver.block.value_or (0));
            Assert::IsFalse  (any.command.has_value() || any.track.has_value());
        }

        TEST_METHOD (MotorSelectInsertAndEject)
        {
            Assert::IsTrue (ParseOk ("BPDISK MOTOR STOPPED").motor == DiskMotorFilter::Stopped);
            Assert::IsTrue (ParseOk ("BPDISK MOTOR").motor         == DiskMotorFilter::Any);
            Assert::IsTrue (ParseOk ("BPDISK D2 SELECT EVERY").isEvery);
            Assert::IsTrue (ParseOk ("BPDISK INSERT").event        == DiskEvent::Inserted);
            Assert::IsTrue (ParseOk ("BPDISK EJECT").event         == DiskEvent::Ejected);
        }

        TEST_METHOD (TheIfClauseIsTheCondition)
        {
            MockExpressionContext  context;
            AppleWinParseResult    result = AppleWinParser::Parse ("BPDISK READ 11 0 IF A == 1", context);



            Assert::AreEqual ((int) ParseStatus::Ok, (int) result.status);
            Assert::IsFalse  (result.command.expression.postfix.empty(), L"the IF expression");
            Assert::IsTrue   (result.command.disk->event == DiskEvent::SectorRead);
        }

        TEST_METHOD (WhatBpsaveWritesReadsBackTheSame)
        {
            for (const char * arguments : { "D1 READ 11 0", "READ * F PASSED", "ADDRESS 3 BADSUM", "HEAD T 11:12 BUMP", "HEAD HALF",
                                            "WRITE QT 10:20", "WRITE 11 0 ADDR D4 AA 96", "WRITE BLOCKED", "DOS 11 F", "BLOCK 2 PASSED",
                                            "RWTS 11 0 READ", "DRIVER BLOCK 2 FORMAT", "MOTOR STOPPED", "D2 SELECT EVERY", "INSERT", "EJECT",
                                            "READ 0 0 ADDR D4 AA ?? SECTORS 13" })
            {
                AssertRoundTrip (arguments);
            }
        }

        TEST_METHOD (AWrongArgumentGivesTheEventsSyntaxLine)
        {
            std::string  read = "BPDISK [D1|D2] READ track sector [PASSED] [ADDR p p p] [DATA p p p] [SECTORS 13|16] [IF expr]";



            Assert::AreEqual (read, ParseFails ("BPDISK READ 11"),            L"a missing sector");
            Assert::AreEqual (read, ParseFails ("BPDISK READ 11 0 BUMP"),     L"an option READ does not take");
            Assert::AreEqual (read, ParseFails ("BPDISK READ 11 0 ADDR D5 AA"), L"two marks, not three");
            Assert::AreEqual (read, ParseFails ("BPDISK READ 11 0 SECTORS 14"));
            Assert::AreEqual (read, ParseFails ("BPDISK READ 100 0"),         L"a track past $FF");

            Assert::IsTrue (ParseFails ("BPDISK DOS 11 10").starts_with ("BPDISK [D1|D2] DOS"),   L"a DOS sector past 15");
            Assert::IsTrue (ParseFails ("BPDISK BLOCK 118").starts_with ("BPDISK [D1|D2] BLOCK"), L"a block past 279");
            Assert::IsTrue (ParseFails ("BPDISK HEAD QT 8C").starts_with ("BPDISK [D1|D2] HEAD"), L"a quarter track past $8B");
            Assert::IsTrue (ParseFails ("BPDISK HEAD T 12:11").starts_with ("BPDISK [D1|D2] HEAD"), L"a range that ends before it starts");
            Assert::IsTrue (ParseFails ("BPDISK MOTOR SIDEWAYS").starts_with ("BPDISK [D1|D2] MOTOR"));
        }

        TEST_METHOD (ASlotOrAnUnknownEventIsAnInvalidArgument)
        {
            Assert::IsTrue (ParseFails ("BPDISK S6 READ 0 0").starts_with ("BPDISK [D1|D2] READ|ADDRESS"), L"there is no slot");
            Assert::IsTrue (ParseFails ("BPDISK SPIN").starts_with ("BPDISK [D1|D2] READ|ADDRESS"));
            Assert::IsTrue (ParseFails ("BPDISK").starts_with ("BPDISK [D1|D2] READ|ADDRESS"));
        }
    };
}
