#include "Pch.h"

#include "Debugger/IoEventLog.h"
#include "Debugger/Reply.h"
#include "Machines/Apple2/Common/VideoTiming.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  IoEventLogTests
//
//  Which accesses the video and sound logs take, the video mode as bits,
//  and the lines VIDEOLOG and SOUNDLOG list.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (IoEventLogTests)
    {
    public:

        static DebugEventEntry<IoEvent>  MakeVideo (uint64_t frame, uint32_t scanline, uint32_t cycleInLine, Word address, uint16_t turnedOn, uint16_t turnedOff)
        {
            DebugEventEntry<IoEvent>  entry;



            entry.event.kind         = IoEventKind::VideoMode;
            entry.event.cycleInFrame = scanline * VideoTiming::kCyclesPerScanline + cycleInLine;
            entry.event.cycle        = frame + entry.event.cycleInFrame;
            entry.event.scanline     = scanline;
            entry.event.cycleInLine  = cycleInLine;
            entry.event.address      = address;
            entry.event.turnedOn     = turnedOn;
            entry.event.turnedOff    = turnedOff;
            entry.cycle              = entry.event.cycle;
            return entry;
        }


        static DebugEventEntry<IoEvent>  MakeSound (IoEventKind kind, uint64_t cycle, Word address, Byte chip = 0, Byte reg = 0, Byte value = 0)
        {
            DebugEventEntry<IoEvent>  entry;



            entry.event.kind    = kind;
            entry.event.cycle   = cycle;
            entry.event.address = address;
            entry.event.chip    = chip;
            entry.event.reg     = reg;
            entry.event.value   = value;
            entry.cycle         = cycle;
            return entry;
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



        TEST_METHOD (FormatEntries_GroupsVideoEntriesByFrame)
        {
            std::vector<DebugEventEntry<IoEvent>>  entries;
            std::vector<std::string>               lines;



            entries.push_back (MakeVideo (17030, 10, 0,  0xC050, 0, IoEventLog::kText));
            entries.push_back (MakeVideo (17030, 192, 0, 0xC057, IoEventLog::kHires, 0));
            entries.push_back (MakeVideo (34060, 0, 5,   0xC051, IoEventLog::kText | IoEventLog::kMixed, IoEventLog::kHires));

            IoEventLog::FormatEntries (entries, lines);

            Assert::AreEqual ((size_t) 5, lines.size());
            Assert::AreEqual (std::string ("Frame from cycle 17030:"),                                               lines[0]);
            Assert::AreEqual (std::string ("  cycle        17680  line  10  h  0  $C050  TEXT off"),                 lines[1]);
            Assert::AreEqual (std::string ("  cycle        29510  line 192  h  0  $C057  HIRES on"),                 lines[2]);
            Assert::AreEqual (std::string ("Frame from cycle 34060:"),                                               lines[3]);
            Assert::AreEqual (std::string ("  cycle        34065  line   0  h  5  $C051  TEXT on, MIXED on, HIRES off"), lines[4]);
        }



        //  The speaker and the card count their own cycles since the last; a
        //  gap in recording starts both again.
        TEST_METHOD (FormatEntries_GivesTheCyclesSinceTheLastOfItsKind)
        {
            std::vector<DebugEventEntry<IoEvent>>  entries;
            std::vector<std::string>               lines;
            DebugEventEntry<IoEvent>               gap;



            gap.kind = DebugEventEntryKind::Gap;

            entries.push_back (MakeSound (IoEventKind::Speaker,  100, 0xC030));
            entries.push_back (MakeSound (IoEventKind::ViaWrite, 110, 0xC401, 0, 1, 0x07));
            entries.push_back (MakeSound (IoEventKind::Speaker,  123, 0xC030));
            entries.push_back (MakeSound (IoEventKind::AyWrite,  150, 0,      0, 7, 0x38));
            entries.push_back (gap);
            entries.push_back (MakeSound (IoEventKind::Speaker,  200, 0xC030));

            IoEventLog::FormatEntries (entries, lines);

            Assert::AreEqual ((size_t) 6, lines.size());
            Assert::AreEqual (std::string ("cycle          100            $C030  speaker"),                       lines[0], L"nothing before it");
            Assert::AreEqual (std::string ("cycle          110            $C401  Mockingboard chip 1 ORA = $07"), lines[1]);
            Assert::AreEqual (std::string ("cycle          123       +23  $C030  speaker"),                       lines[2]);
            Assert::AreEqual (std::string ("cycle          150       +40         Mockingboard chip 1 AY R07 = $38"), lines[3]);
            Assert::AreEqual (std::string ("History recording paused here."),                                    lines[4]);
            Assert::AreEqual (std::string ("cycle          200            $C030  speaker"),                       lines[5], L"the gap starts the count again");
        }
    };
}
