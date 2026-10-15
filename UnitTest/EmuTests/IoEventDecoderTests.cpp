#include "Pch.h"

#include "Core/IIoAccessSink.h"
#include "Debugger/Events/IoEventDecoder.h"
#include "EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace IoEventDecoderTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DecodingSink
    //
    //  The bus's I/O accesses on the History slot, decoded as they come, and
    //  each access counted.
    //
    ////////////////////////////////////////////////////////////////////////////////

    class DecodingSink : public IIoAccessSink
    {
    public:
        explicit DecodingSink (MachineHost & machine) : m_machine (machine) {}

        IoEventDecoder        decoder;
        std::vector<IoEvent>  events;
        std::vector<Word>     accesses;

        void  OnIoAccess (Word address, Byte value, bool isWrite) override
        {
            IoEventDecoder::Events  decoded = {};
            size_t                  count   = decoder.Decode (m_machine, address, value, isWrite, decoded);



            accesses.push_back (address);
            events.insert (events.end(), decoded.begin(), decoded.begin() + (ptrdiff_t) count);
        }

    private:
        MachineHost  & m_machine;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  IoEventDecoderTests
    //
    //  The bus reports what a device at $C000-$CFFF decoded, and the decoder
    //  makes the video and sound logs' entries of it: a video switch only
    //  when the mode changes, the speaker, and the Mockingboard's writes with
    //  the AY write its port B control lines make.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (IoEventDecoderTests)
    {
    public:

        static constexpr Word  kOrb  = 0xC400;
        static constexpr Word  kOra  = 0xC401;
        static constexpr Word  kDdrb = 0xC402;
        static constexpr Word  kDdra = 0xC403;


        //  Port A all output and port B's control lines output, as a player
        //  sets them up: two 6522 writes.
        static void  SetUpPorts (MachineHost & machine)
        {
            machine.GetMemoryBus().WriteByte (kDdra, 0xFF);
            machine.GetMemoryBus().WriteByte (kDdrb, 0x07);
        }


        struct Rig
        {
            TestMachine    machine { "Apple2e" };
            DecodingSink   sink    { machine };

            IoAccessFilter  filter;

            //  The whole I/O space, or the decoder's own filter.
            explicit Rig (bool isWhole = true)
            {
                machine.PowerCycle();
                sink.decoder.Seed (machine);

                if (isWhole)
                {
                    filter.AddReads  (0xC000, 0xCFFF);
                    filter.AddWrites (0xC000, 0xCFFF);
                }
                else
                {
                    sink.decoder.GetFilter (filter);
                }

                machine.GetMemoryBus().SetIoAccessSink (ObserverSlot::History, &sink, filter);
            }

            ~Rig()
            {
                machine.GetMemoryBus().SetIoAccessSink (ObserverSlot::History, nullptr, IoAccessFilter());
            }
        };


        TEST_METHOD (TheBusReportsOnlyWhatADeviceInTheIoSpaceDecoded)
        {
            Rig  rig;



            (void) rig.machine.GetMemoryBus().ReadByte (0xC030);
            (void) rig.machine.GetMemoryBus().ReadByte (0xE000);
            rig.machine.GetMemoryBus().WriteByte (0x0300, 0x12);

            Assert::AreEqual ((size_t) 1, rig.sink.accesses.size(), L"the speaker; not ROM, not RAM");
            Assert::AreEqual ((Word) 0xC030, rig.sink.accesses[0]);
        }


        //  The decoder's filter: the bus does not call it for a keyboard poll,
        //  and does for a write to the card's page.
        TEST_METHOD (TheDecodersFilterPassesOnlyWhatTheLogsTake)
        {
            Rig  rig (false);



            (void) rig.machine.GetMemoryBus().ReadByte (0xC000);
            (void) rig.machine.GetMemoryBus().ReadByte (0xC00C);
            (void) rig.machine.GetMemoryBus().ReadByte (0xC030);
            rig.machine.GetMemoryBus().WriteByte (0xC00D, 0);
            rig.machine.GetMemoryBus().WriteByte (kOra, 0);

            Assert::AreEqual ((size_t) 3, rig.sink.accesses.size(), L"the speaker, the 80COL write and the card");
            Assert::AreEqual ((Word) 0xC030, rig.sink.accesses[0]);
            Assert::AreEqual ((Word) 0xC00D, rig.sink.accesses[1]);
            Assert::AreEqual (kOra,          rig.sink.accesses[2]);
        }


        TEST_METHOD (AVideoSwitchMakesAnEntryOnlyWhenTheModeChanges)
        {
            Rig  rig;



            rig.machine.GetMemoryBus().WriteByte (0xC050, 0);     // graphics
            rig.machine.GetMemoryBus().WriteByte (0xC054, 0);     // page 1, already on
            rig.machine.GetMemoryBus().WriteByte (0xC057, 0);     // hires

            Assert::AreEqual ((size_t) 2, rig.sink.events.size());
            Assert::IsTrue   (rig.sink.events[0].kind == IoEventKind::VideoMode);
            Assert::AreEqual ((int) IoEventLog::kText,  (int) rig.sink.events[0].turnedOff);
            Assert::AreEqual ((int) IoEventLog::kHires, (int) rig.sink.events[1].turnedOn);
        }


        //  A machine whose mode was set before the decoder was seeded: an access
        //  that does not change it makes no entry.
        TEST_METHOD (TheSeedTakesTheModeTheMachineIsIn)
        {
            Rig  rig;



            rig.machine.GetMemoryBus().SetIoAccessSink (ObserverSlot::History, nullptr, IoAccessFilter());
            rig.machine.GetMemoryBus().WriteByte (0xC050, 0);

            rig.sink.decoder.Seed (rig.machine);
            rig.machine.GetMemoryBus().SetIoAccessSink (ObserverSlot::History, &rig.sink, rig.filter);
            rig.machine.GetMemoryBus().WriteByte (0xC050, 0);

            Assert::IsTrue (rig.sink.events.empty());
        }


        //  ORA = 7, latch, inactive, ORA = $38, write: the AY's register 7
        //  takes $38.
        TEST_METHOD (APortBWriteToWriteMakesTheAyWriteOfPortA)
        {
            Rig  rig;



            SetUpPorts (rig.machine);
            rig.machine.GetMemoryBus().WriteByte (kOra, 0x07);
            rig.machine.GetMemoryBus().WriteByte (kOrb, 0x07);
            rig.machine.GetMemoryBus().WriteByte (kOrb, 0x04);
            rig.machine.GetMemoryBus().WriteByte (kOra, 0x38);
            rig.machine.GetMemoryBus().WriteByte (kOrb, 0x06);

            Assert::AreEqual ((size_t) 8, rig.sink.events.size(), L"seven 6522 writes and one AY write");
            Assert::IsTrue   (rig.sink.events[7].kind == IoEventKind::AyWrite);
            Assert::AreEqual ((int) 7,    (int) rig.sink.events[7].reg);
            Assert::AreEqual ((int) 0x38, (int) rig.sink.events[7].value);
            Assert::AreEqual ((int) 0,    (int) rig.sink.events[7].chip);
        }


        TEST_METHOD (ResetLowMakesNoAyWrite)
        {
            Rig  rig;



            SetUpPorts (rig.machine);
            rig.machine.GetMemoryBus().WriteByte (kOra, 0x07);
            rig.machine.GetMemoryBus().WriteByte (kOrb, 0x02);    // write with reset low: no AY write

            Assert::AreEqual ((size_t) 4, rig.sink.events.size(), L"four 6522 writes and no AY write");
        }
    };
}
