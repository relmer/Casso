#include "Pch.h"

#include "Debugger/IDiagnosticsProvider.h"
#include "EmuTests/TestMachine.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"
#include "Machines/Apple2/Common/MockingboardCard.h"
#include "Machines/Apple2/Common/PrinterCard.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiagnosticsProviderTests
//
//  What each device publishes for its debugger panel: named groups of rows,
//  bit decodes, and the payloads the panel graphics draw. Every provider is
//  read on a real machine where it can be, so a row reports the state the
//  machine is really in.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DiagnosticsProviderTests)
    {
    public:

        static inline const DiagnosticsRow  kMissing;


        static DiagnosticsSnapshot Snapshot (const IDiagnosticsProvider & provider)
        {
            DiagnosticsSnapshot  snapshot;



            provider.GetDiagnostics (snapshot);
            return snapshot;
        }


        //  The first row with this label in any group; a test that asks for a
        //  row the provider does not publish fails here rather than reading an
        //  empty one.
        static const DiagnosticsRow & FindRow (const DiagnosticsSnapshot & snapshot, const std::string & label)
        {
            for (const DiagnosticsGroup & group : snapshot.groups)
            {
                for (const DiagnosticsRow & row : group.rows)
                {
                    if (row.label == label)
                    {
                        return row;
                    }
                }
            }

            Assert::Fail ((L"no row " + std::wstring (label.begin(), label.end())).c_str());
            return kMissing;
        }


        static bool IsBitSet (const DiagnosticsRow & row, const std::string & name)
        {
            for (const DiagnosticsBit & bit : row.bits)
            {
                if (bit.name == name)
                {
                    return bit.set;
                }
            }

            Assert::Fail ((L"no bit " + std::wstring (name.begin(), name.end())).c_str());
            return false;
        }


        static std::vector<std::string> GetIds (const MachineHost & machine)
        {
            std::vector<std::string>  ids;



            for (const IDiagnosticsProvider * provider : machine.GetDiagnosticsProviders())
            {
                ids.push_back (provider->GetDiagnosticsId());
            }

            return ids;
        }


        static bool Contains (const std::vector<std::string> & ids, const std::string & id)
        {
            return std::find (ids.begin(), ids.end(), id) != ids.end();
        }



        TEST_METHOD (AByteRowDecodesEachNamedBitHighestFirst)
        {
            DiagnosticsRow  row = IDiagnosticsProvider::MakeByteRow ("R", 0xA1, { "B7", "B6", "", "B4", "", "", "", "B0" });



            Assert::AreEqual (std::string ("$A1"), row.value);
            Assert::AreEqual ((size_t) 4, row.bits.size(), L"an empty name leaves its bit out");
            Assert::AreEqual (std::string ("B7"), row.bits[0].name);
            Assert::IsTrue   (row.bits[0].set);
            Assert::IsFalse  (row.bits[1].set, L"B6");
            Assert::IsFalse  (row.bits[2].set, L"B4");
            Assert::IsTrue   (row.bits[3].set, L"B0");
        }


        TEST_METHOD (TheDiskPanelFollowsASeek)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::DiskOnly);
            Disk2Controller            * disk   = machine.GetRefs().diskController;
            DiagnosticsSnapshot          before;
            DiagnosticsSnapshot          after;
            const DiagnosticsDiskHead  * head   = nullptr;



            Assert::IsNotNull (disk);
            before = Snapshot (*disk);

            Assert::AreEqual (std::string ("off"), FindRow (before, "Motor").value);
            Assert::AreEqual (std::string ("0"),   FindRow (before, "Quarter track").value);

            //  Motor on, then phase 1 on: the head steps toward it.
            (void) machine.GetMemoryBus().ReadByte (0xC0E9);
            (void) machine.GetMemoryBus().ReadByte (0xC0E3);
            after = Snapshot (*disk);

            Assert::AreEqual (std::string ("on"), FindRow (after, "Motor").value);
            Assert::AreNotEqual (FindRow (before, "Quarter track").value, FindRow (after, "Quarter track").value, L"the head moved");
            Assert::AreEqual (std::format ("{}", disk->GetQuarterTrack()), FindRow (after, "Quarter track").value);
            Assert::IsTrue   (IsBitSet (FindRow (after, "Phases"), "PH1"));
            Assert::IsFalse  (IsBitSet (FindRow (after, "Phases"), "PH0"));
            Assert::AreEqual (std::string ("read"), FindRow (after, "Mode").value);
            Assert::AreNotEqual (FindRow (before, "Spin-up left").value, FindRow (after, "Spin-up left").value, L"spin-up starts with the motor");

            head = std::get_if<DiagnosticsDiskHead> (&after.visual);
            Assert::IsNotNull (head, L"the head graphic's payload");
            Assert::AreEqual  (disk->GetQuarterTrack(), head->quarterTrack);
            Assert::IsTrue    (head->motorOn);
            Assert::AreEqual  ((int) Disk2Controller::kMaxQuarterTrack, head->maxQuarterTrack);
            Assert::AreEqual  ((Byte) 0x02, head->phases);
        }


        TEST_METHOD (TheMmuPanelListsEverySwitchAndMapsEveryPage)
        {
            TestMachine                  machine  ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot          snapshot = Snapshot (*machine.GetMmu());
            const DiagnosticsMemoryMap * map      = std::get_if<DiagnosticsMemoryMap> (&snapshot.visual);
            const DiagnosticsRow       & packed   = FindRow (snapshot, "MMU");



            Assert::AreEqual ((size_t) 7, packed.bits.size(), L"RAMRD to INTC8ROM");

            for (const char * name : { "RAMRD", "RAMWRT", "ALTZP", "80STORE", "INTCXROM", "SLOTC3ROM", "INTC8ROM" })
            {
                (void) FindRow (snapshot, name);
            }

            Assert::IsNotNull (map);
            Assert::AreEqual  (DiagnosticsMemoryMap::kPageCount, map->pages.size());
            Assert::IsTrue    (map->pages[0x00].read  == MemorySource::Main);
            Assert::IsTrue    (map->pages[0x20].write == MemorySource::Main);
            Assert::IsTrue    (map->pages[0xC0].read  == MemorySource::Io);
            Assert::IsTrue    (map->pages[0xE0].read  == MemorySource::Rom || map->pages[0xE0].read == MemorySource::LcBank2);
        }


        //  ALTZP moves the zero page and the stack to aux, and back.
        TEST_METHOD (TheMemoryMapChangesOnAltZp)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  on;
            DiagnosticsSnapshot  off;



            machine.GetMemoryBus().WriteByte (0xC009, 0);
            on = Snapshot (*machine.GetMmu());

            Assert::AreEqual (std::string ("on"), FindRow (on, "ALTZP").value);
            Assert::IsTrue   (IsBitSet (FindRow (on, "MMU"), "ALTZP"));
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x00].read  == MemorySource::Aux);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x01].write == MemorySource::Aux);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (on.visual).pages[0x02].read  == MemorySource::Main, L"only pages 0 and 1");

            machine.GetMemoryBus().WriteByte (0xC008, 0);
            off = Snapshot (*machine.GetMmu());

            Assert::AreEqual (std::string ("off"), FindRow (off, "ALTZP").value);
            Assert::IsTrue   (std::get<DiagnosticsMemoryMap> (off.visual).pages[0x00].read == MemorySource::Main);
        }


        TEST_METHOD (TheVideoPanelNamesTheMode)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            AppleSoftSwitchBank  & video    = *machine.GetRefs().softSwitches;
            DiagnosticsSnapshot    snapshot;



            machine.GetMemoryBus().WriteByte (0xC051, 0);   // TEXT
            snapshot = Snapshot (video);
            Assert::AreEqual (std::string ("text"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),   FindRow (snapshot, "TEXT").value);

            machine.GetMemoryBus().WriteByte (0xC050, 0);   // graphics
            machine.GetMemoryBus().WriteByte (0xC057, 0);   // hi-res
            machine.GetMemoryBus().WriteByte (0xC053, 0);   // mixed
            machine.GetMemoryBus().WriteByte (0xC055, 0);   // page 2
            snapshot = Snapshot (video);

            Assert::AreEqual (std::string ("hi-res, mixed"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),            FindRow (snapshot, "PAGE2").value);
            Assert::AreEqual (std::string ("off"),           FindRow (snapshot, "80COL").value, L"the //e's switches follow");

            machine.GetMemoryBus().WriteByte (0xC00D, 0);   // 80COL
            machine.GetMemoryBus().WriteByte (0xC05E, 0);   // DHIRES
            machine.GetMemoryBus().WriteByte (0xC052, 0);   // full screen
            snapshot = Snapshot (video);

            Assert::AreEqual (std::string ("double hi-res"), FindRow (snapshot, "Mode").value);
            Assert::AreEqual (std::string ("on"),            FindRow (snapshot, "DHIRES").value);
        }


        TEST_METHOD (TheIIPlusVideoPanelHasNoIIeSwitches)
        {
            TestMachine          machine  ("Apple2Plus", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  snapshot = Snapshot (*machine.GetRefs().softSwitches);



            Assert::AreEqual ((size_t) 1, snapshot.groups.size());
            Assert::AreEqual (std::string ("Display"), snapshot.groups[0].title);
        }


        TEST_METHOD (TheKeyboardPanelShowsTheLatchWithoutClearingIt)
        {
            TestMachine          machine  ("Apple2e", TestMachine::Slots::Empty);
            AppleKeyboard      & keyboard = *machine.GetRefs().keyboard;
            DiagnosticsSnapshot  snapshot;



            keyboard.PressKey ('A');
            snapshot = Snapshot (keyboard);

            Assert::AreEqual (std::string ("$C1"), FindRow (snapshot, "Latch ($C000)").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Latch ($C000)"), "STROBE"));
            Assert::AreEqual (std::string ("'A'"), FindRow (snapshot, "Key").value);
            Assert::AreEqual (std::string ("on"),  FindRow (snapshot, "Key pending").value);
            Assert::IsFalse  (keyboard.IsStrobeClear(), L"reading the panel left the strobe set");
        }


        //  Each 6522 and each AY as a group, and meters for the six channels and
        //  the four timers.
        TEST_METHOD (TheMockingboardPanelCoversBothHalves)
        {
            MockingboardCard     card (4);
            DiagnosticsSnapshot       snapshot;
            std::vector<std::string>  titles;



            snapshot = Snapshot (card);

            for (const DiagnosticsGroup & group : snapshot.groups)
            {
                titles.push_back (group.title);
            }

            Assert::IsTrue   (Contains (titles, "6522 #1") && Contains (titles, "6522 #2"));
            Assert::IsTrue   (Contains (titles, "AY #1")   && Contains (titles, "AY #2"));
            Assert::IsTrue   (std::holds_alternative<DiagnosticsMeters> (snapshot.visual));
            Assert::AreEqual ((size_t) 10, std::get<DiagnosticsMeters> (snapshot.visual).levels.size());
            (void) FindRow (snapshot, "IFR");
            (void) FindRow (snapshot, "T1 count");
            (void) FindRow (snapshot, "Mixer");
        }


        TEST_METHOD (TheViaPublishesItsTimersAndInterruptRegisters)
        {
            Via6522              via;
            DiagnosticsSnapshot  snapshot;
            DiagnosticsMeters    meters;



            via.WriteRegister (Via6522::kRegT1LL, 0x00);
            via.WriteRegister (Via6522::kRegT1CH, 0x10);     // T1 = $1000, and started
            via.WriteRegister (Via6522::kRegIer,  0xC0);     // enable T1
            via.AppendDiagnostics ("6522", snapshot);
            via.AppendTimerLevels ("6522", meters);

            Assert::AreEqual (std::string ("$1000"), FindRow (snapshot, "T1 count").value);
            Assert::AreEqual (std::string ("$1000"), FindRow (snapshot, "T1 latch").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "IER"), "T1"));
            Assert::IsFalse  (IsBitSet (FindRow (snapshot, "IER"), "T2"));
            Assert::AreEqual ((size_t) 2, meters.levels.size());
            Assert::AreEqual (1.0f, meters.levels[0].level, 0.001f, L"a full count against its latch");

            via.Tick (0x0800);
            meters.levels.clear();
            via.AppendTimerLevels ("6522", meters);
            Assert::IsTrue (meters.levels[0].level > 0.4f && meters.levels[0].level < 0.6f, L"half counted down");
        }


        TEST_METHOD (TheAyPublishesItsRegistersAndChannelLevels)
        {
            Ay8910               ay;
            DiagnosticsSnapshot  snapshot;
            DiagnosticsMeters    meters;



            ay.WriteRegister (Ay8910::kRegMixer, 0x3E);      // tone A only
            ay.WriteRegister (Ay8910::kRegAmpA,  0x0F);
            ay.WriteRegister (Ay8910::kRegAmpB,  0x08);
            ay.AppendDiagnostics ("AY", snapshot);
            ay.AppendChannelLevels ("AY", meters);

            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Mixer"), "TB"), L"a set bit disables tone B");
            Assert::IsFalse  (IsBitSet (FindRow (snapshot, "Mixer"), "TA"));
            Assert::AreEqual (std::string ("$0F"), FindRow (snapshot, "Amplitude A").value);
            Assert::AreEqual ((size_t) 3, meters.levels.size());
            Assert::AreEqual (1.0f, meters.levels[0].level, 0.001f, L"A at full amplitude");
            Assert::AreEqual (0.0f, meters.levels[1].level, 0.001f, L"B is silenced by the mixer");
        }


        TEST_METHOD (ThePrinterPanelShowsTheStatusTheGuestReads)
        {
            //  The card holds its print buffer, too large for the stack.
            std::unique_ptr<PrinterCard>  card     = std::make_unique<PrinterCard> (1);
            DiagnosticsSnapshot           snapshot = Snapshot (*card);



            Assert::AreEqual (std::format ("${:02X}", PrinterCard::kStatusReady), FindRow (snapshot, "Status").value);
            Assert::IsTrue   (IsBitSet (FindRow (snapshot, "Status"), "READY"));
            Assert::AreEqual (std::string ("off"), FindRow (snapshot, "Used").value);
            Assert::AreEqual (std::string ("1"),   FindRow (snapshot, "Slot").value);
        }


        TEST_METHOD (TheClockPanelShowsCyclesAndSpeed)
        {
            TestMachine          machine ("Apple2e", TestMachine::Slots::Empty);
            DiagnosticsSnapshot  before;
            DiagnosticsSnapshot  after;



            before = Snapshot (machine);
            (void) machine.RunCycles (1000);
            machine.SetSpeedMode (SpeedMode::Double);
            after  = Snapshot (machine);

            Assert::AreEqual    (std::string ("authentic"), FindRow (before, "Speed").value);
            Assert::AreEqual    (std::string ("double"),    FindRow (after,  "Speed").value);
            Assert::AreNotEqual (FindRow (before, "Cycles").value, FindRow (after, "Cycles").value);
            (void) FindRow (after, "Scanline");
        }


        //  Only the current machine's devices are listed, the clock first.
        TEST_METHOD (TheProvidersAreTheMachinesOwn)
        {
            TestMachine               iie   ("Apple2e",    TestMachine::Slots::AsShipped);
            TestMachine               plus  ("Apple2Plus", TestMachine::Slots::DiskOnly);
            std::vector<std::string>  iieIds  = GetIds (iie);
            std::vector<std::string>  plusIds = GetIds (plus);



            Assert::AreEqual (std::string ("clock"), iieIds.front());

            for (const char * id : { "keyboard", "video", "mmu", "disk", "mockingboard" })
            {
                Assert::IsTrue (Contains (iieIds, id), std::wstring (id, id + strlen (id)).c_str());
            }

            for (const char * id : { "clock", "keyboard", "video", "disk" })
            {
                Assert::IsTrue (Contains (plusIds, id), std::wstring (id, id + strlen (id)).c_str());
            }

            Assert::IsFalse (Contains (plusIds, "mmu"),          L"a ][+ has no MMU");
            Assert::IsFalse (Contains (plusIds, "mockingboard"), L"nor a card in an empty slot");
        }
    };
}
