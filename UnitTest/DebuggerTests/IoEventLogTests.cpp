#include "Pch.h"

#include "Debugger/IoEventLog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLogTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (IoEventLogTests)
    {
    public:

        static constexpr Word  kOrb  = 0xC400;
        static constexpr Word  kOra  = 0xC401;
        static constexpr Word  kOrb2 = 0xC480;
        static constexpr Word  kOra2 = 0xC481;



        TEST_METHOD (Off_RecordsNothing)
        {
            IoEventLog  log;



            log.RecordSpeaker      (10, 0xC030);
            log.RecordVideo        (10, 0, 0, 0, 0xC050, IoEventLog::kText, 0);
            log.RecordMockingboard (10, kOra, 0x07);

            Assert::AreEqual ((uint64_t) 0, log.GetTotal());
            Assert::IsTrue   (log.GetEntries().empty());
        }



        TEST_METHOD (GetVideoModeBits_TakesTheSwitchesThatAreOn)
        {
            std::vector<SoftSwitch>  switches = { { "TEXT", false }, { "MIXED", true }, { "PAGE2", true }, { "RAMRD", true }, { "80STORE", true } };



            Assert::AreEqual ((int) (IoEventLog::kMixed | IoEventLog::kPage2 | IoEventLog::k80Store), (int) IoEventLog::GetVideoModeBits (switches));
        }



        TEST_METHOD (IsVideoSwitch_DisplaySwitchesAnyAccess_IieSwitchesOnWrite)
        {
            Assert::IsTrue  (IoEventLog::IsVideoSwitch (0xC050, false));
            Assert::IsTrue  (IoEventLog::IsVideoSwitch (0xC05F, false));
            Assert::IsTrue  (IoEventLog::IsVideoSwitch (0xC00D, true));
            Assert::IsFalse (IoEventLog::IsVideoSwitch (0xC000, false), L"a keyboard read");
            Assert::IsFalse (IoEventLog::IsVideoSwitch (0xC030, true));
            Assert::IsFalse (IoEventLog::IsVideoSwitch (0xC060, false));
            Assert::IsTrue  (IoEventLog::IsSpeaker     (0xC030));
            Assert::IsTrue  (IoEventLog::IsSpeaker     (0xC03F));
            Assert::IsFalse (IoEventLog::IsSpeaker     (0xC040));
        }



        TEST_METHOD (RecordVideo_KeepsOnlyChanges_GroupsByFrame)
        {
            IoEventLog                log;
            std::vector<std::string>  lines;



            log.SetOn (true);
            log.RecordVideo (17030 + 650, 650, 10, 0, 0xC050, IoEventLog::kText, 0);
            log.RecordVideo (17030 + 700, 700, 10, 50, 0xC054, 0, 0);
            log.RecordVideo (17030 + 12480, 12480, 192, 0, 0xC057, 0, IoEventLog::kHires);
            log.RecordVideo (34060 + 5, 5, 0, 5, 0xC051, IoEventLog::kHires, IoEventLog::kText | IoEventLog::kMixed);

            Assert::AreEqual ((uint64_t) 3, log.GetTotal(), L"the access that changed nothing is not an entry");

            log.Format (10, lines);
            Assert::AreEqual ((size_t) 6, lines.size());
            Assert::AreEqual (std::string ("3 entries since the log was cleared; the last 3:"), lines[0]);
            Assert::AreEqual (std::string ("Frame from cycle 17030:"),                           lines[1]);
            Assert::AreEqual (std::string ("  cycle        17680  line  10  h  0  $C050  TEXT off"), lines[2]);
            Assert::AreEqual (std::string ("  cycle        29510  line 192  h  0  $C057  HIRES on"), lines[3]);
            Assert::AreEqual (std::string ("Frame from cycle 34060:"),                           lines[4]);
            Assert::AreEqual (std::string ("  cycle        34065  line   0  h  5  $C051  TEXT on, MIXED on, HIRES off"), lines[5]);
        }



        TEST_METHOD (RecordSpeaker_GivesTheCyclesSinceTheLastToggle)
        {
            IoEventLog                log;
            std::vector<std::string>  lines;



            log.SetOn (true);
            log.RecordSpeaker (100, 0xC030);
            log.RecordSpeaker (123, 0xC030);

            log.Format (1, lines);
            Assert::AreEqual (std::string ("2 entries since the log was cleared; the last 1:"), lines[0]);
            Assert::AreEqual (std::string ("cycle          123       +23  $C030  speaker"),    lines[1]);
            Assert::IsFalse  (log.GetEntries().front().hasSinceLast, L"the first toggle has nothing before it");
        }



        TEST_METHOD (RecordMockingboard_DecodesTheAyWriteFromThePorts)
        {
            IoEventLog                log;
            std::vector<std::string>  lines;



            log.SetOn (true);
            log.RecordMockingboard (10, kOra,  0x07);     // register 7 on port A
            log.RecordMockingboard (20, kOrb,  0x07);     // latch
            log.RecordMockingboard (30, kOrb,  0x04);     // inactive
            log.RecordMockingboard (40, kOra,  0x38);     // the value
            log.RecordMockingboard (50, kOrb,  0x06);     // write
            log.RecordMockingboard (60, kOra2, 0x01);
            log.RecordMockingboard (70, kOrb2, 0x06);     // chip 2 writes its latched register 0

            Assert::AreEqual ((uint64_t) 9, log.GetTotal());

            const IoEvent  & write = log.GetEntries()[5];

            Assert::IsTrue   (write.kind == IoEventKind::AyWrite);
            Assert::AreEqual ((int) 7,    (int) write.reg);
            Assert::AreEqual ((int) 0x38, (int) write.value);
            Assert::AreEqual ((int) 0,    (int) write.chip);

            log.Format (10, lines);
            Assert::AreEqual (std::string ("cycle           10            $C401  Mockingboard chip 1 ORA = $07"),       lines[1]);
            Assert::AreEqual (std::string ("cycle           50       +10  $C400  Mockingboard chip 1 ORB = $06"),       lines[5]);
            Assert::AreEqual (std::string ("cycle           50        +0         Mockingboard chip 1 AY R07 = $38"),    lines[6]);
            Assert::AreEqual (std::string ("cycle           70        +0         Mockingboard chip 2 AY R00 = $01"),    lines[9]);
        }



        TEST_METHOD (RecordMockingboard_ResetLowEndsTheDecode)
        {
            IoEventLog  log;



            log.SetOn (true);
            log.RecordMockingboard (10, kOra, 0x07);
            log.RecordMockingboard (20, kOrb, 0x02);      // write with reset low: the chip is held in reset

            Assert::AreEqual ((uint64_t) 2, log.GetTotal(), L"two port writes and no AY write");
        }



        TEST_METHOD (Capacity_DropsTheOldest_TotalCountsEverything)
        {
            IoEventLog  log;



            log.SetOn (true);

            for (uint64_t cycle = 0; cycle < IoEventLog::kCapacity + 5; ++cycle)
            {
                log.RecordSpeaker (cycle, 0xC030);
            }

            Assert::AreEqual (IoEventLog::kCapacity,                   log.GetEntries().size());
            Assert::AreEqual ((uint64_t) IoEventLog::kCapacity + 5,     log.GetTotal());
            Assert::AreEqual ((uint64_t) 5,                             log.GetEntries().front().cycle);

            log.Clear();
            Assert::AreEqual ((uint64_t) 0, log.GetTotal());
        }
    };
}
