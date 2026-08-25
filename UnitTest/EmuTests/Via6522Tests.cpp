#include "Pch.h"

#include "Devices/Mockingboard/Via6522.h"
#include "Core/InterruptController.h"
#include "ICpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Via6522TestNs
{
    ////////////////////////////////////////////////////////////////////////////
    //
    //  ViaTestCpu
    //
    //  Minimal ICpu double recording the maskable IRQ line state so a VIA
    //  wired through a real InterruptController can be observed end to end.
    //
    ////////////////////////////////////////////////////////////////////////////

    class ViaTestCpu : public ICpu
    {
    public:
        HRESULT     Reset         () override                     { return S_OK; }
        HRESULT     Step          (uint32_t & outCycles) override { outCycles = 0; return S_OK; }
        uint64_t    GetCycleCount() const override               { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
            }
        }

        bool        IrqAsserted() const { return m_irqAsserted; }

    private:
        bool    m_irqAsserted = false;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Via6522Tests
    //
    //  The VIA: port direction, the two timers, and the interrupt flag and
    //  enable registers.
    //
    //  The DATA DIRECTION registers decide whether a port bit is driven or
    //  read, so they are asserted before anything else -- a port left as an
    //  output after reset would drive whatever the output register held.
    //
    //  Timer 1's free-run versus one-shot modes are covered separately, since
    //  the Mockingboard drives its interrupt cadence from T1 and the two modes
    //  differ in whether the counter reloads.
    //
    //  The IFR/IER pair gets its own attention because setting a flag does not
    //  raise the line unless the matching enable is set -- and clearing works
    //  by WRITING the bit, which is the opposite of what the name suggests and
    //  the classic way an interrupt ends up stuck asserted.
    //
    //  Written from the datasheet, so these encode documented behavior rather
    //  than another implementation's choices.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Via6522Tests)
    {
    public:
        TEST_METHOD (ResetClearsRegistersAndInterrupts)
        {
            Via6522    via;



            Assert::AreEqual<Byte> (0, via.GetIfr());
            Assert::AreEqual<Byte> (Via6522::kIrqAny, via.GetIer());
            Assert::IsFalse (via.IsIrqAsserted());
            Assert::AreEqual<uint16_t> (0, via.GetTimer1());
            Assert::AreEqual<uint16_t> (0, via.GetTimer2());
        }


        TEST_METHOD (PortAOutputReadsBackThroughDdr)
        {
            Via6522    via;



            // All outputs: the output register drives every pin.
            via.WriteRegister (Via6522::kRegDdra, 0xFF);
            via.WriteRegister (Via6522::kRegOra, 0x5A);
            Assert::AreEqual<Byte> (0x5A, via.GetPortA());
            Assert::AreEqual<Byte> (0x5A, via.ReadRegister (Via6522::kRegOra));

            // All inputs: the external latch drives every pin.
            via.WriteRegister (Via6522::kRegDdra, 0x00);
            via.SetPortAInput (0x3C);
            Assert::AreEqual<Byte> (0x3C, via.GetPortA());
        }


        TEST_METHOD (PortBMixedDirectionMergesOutputAndInput)
        {
            Via6522    via;



            // Low nibble output, high nibble input.
            via.WriteRegister (Via6522::kRegDdrb, 0x0F);
            via.WriteRegister (Via6522::kRegOrb, 0xAA);   // outputs -> low nibble 0xA
            via.SetPortBInput (0x55);                     // inputs  -> high nibble 0x5

            Assert::AreEqual<Byte> (0x5A, via.GetPortB());
        }


        TEST_METHOD (Timer1OneShotFiresExactlyOnce)
        {
            Via6522    via;



            EnableTimer1Irq (via);

            // Latch = 100 -> underflow 101 cycles after the T1C-H load.
            LoadTimer1 (via, 100);

            via.Tick (100);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                             L"Must not fire one cycle early");

            via.Tick (1);
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                            L"Timer 1 must fire on underflow");
            Assert::IsTrue (via.IsIrqAsserted());

            // Clear the flag and confirm one-shot never re-fires.
            via.ReadRegister (Via6522::kRegT1CL);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer1) != 0);

            via.Tick (100000);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                             L"One-shot must not re-arm on its own");
        }


        TEST_METHOD (Timer1ContinuousReloadsAndRefires)
        {
            Via6522    via;



            EnableTimer1Irq (via);
            via.WriteRegister (Via6522::kRegAcr, Via6522::kAcrT1Continuous);
            LoadTimer1 (via, 100);

            via.Tick (101);
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                            L"First continuous underflow");
            Assert::AreEqual<uint16_t> (100, via.GetTimer1(),
                                        L"Continuous mode reloads from the latch");

            via.ReadRegister (Via6522::kRegT1CL);   // clears the flag
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer1) != 0);

            via.Tick (101);
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                            L"Continuous mode re-fires every latch+1 cycles");
        }


        TEST_METHOD (Timer1CounterCountsDownOnPartialTick)
        {
            Via6522    via;



            LoadTimer1 (via, 0x1000);
            via.Tick (0x100);

            Assert::AreEqual<uint16_t> (0x0F00, via.GetTimer1());
            Assert::AreEqual<Byte> (0x00, via.ReadRegister (Via6522::kRegT1CL));
            Assert::AreEqual<Byte> (0x0F, via.ReadRegister (Via6522::kRegT1CH));
        }


        TEST_METHOD (Timer1FlagClearedByWritingLatchHigh)
        {
            Via6522    via;



            EnableTimer1Irq (via);
            LoadTimer1 (via, 10);
            via.Tick (11);
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqTimer1) != 0);

            via.WriteRegister (Via6522::kRegT1LH, 0x00);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer1) != 0,
                             L"Writing T1L-H clears the Timer 1 flag");
        }


        TEST_METHOD (Timer2OneShotFiresOnce)
        {
            Via6522    via;



            via.WriteRegister (Via6522::kRegIer,
                               static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer2));

            // Latch low then load counter high: count = 0x0032 = 50.
            via.WriteRegister (Via6522::kRegT2CL, 0x32);
            via.WriteRegister (Via6522::kRegT2CH, 0x00);

            via.Tick (50);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer2) != 0);

            via.Tick (1);
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqTimer2) != 0,
                            L"Timer 2 fires on underflow");
            Assert::IsTrue (via.IsIrqAsserted());

            via.ReadRegister (Via6522::kRegT2CL);   // clears the flag
            via.Tick (100000);
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqTimer2) != 0,
                             L"Timer 2 is one-shot only");
        }


        TEST_METHOD (IfrSummaryBitTracksEnabledFlags)
        {
            Via6522    via;



            LoadTimer1 (via, 5);
            via.Tick (6);

            // Flag pending but not enabled: no summary bit, no line.
            Assert::IsFalse ((via.GetIfr() & Via6522::kIrqAny) != 0);
            Assert::IsFalse (via.IsIrqAsserted());

            // Enabling the source now surfaces the summary bit and the line.
            via.WriteRegister (Via6522::kRegIer,
                               static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
            Assert::IsTrue ((via.GetIfr() & Via6522::kIrqAny) != 0);
            Assert::IsTrue (via.IsIrqAsserted());
        }


        TEST_METHOD (IerSetAndClearControl)
        {
            Via6522    via;



            via.WriteRegister (Via6522::kRegIer,
                               static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1 | Via6522::kIrqTimer2));
            Assert::AreEqual<Byte> (static_cast<Byte> (Via6522::kIrqAny | Via6522::kIrqTimer1 | Via6522::kIrqTimer2),
                                    via.GetIer());

            // Clearing (bit 7 = 0) disables only the flagged sources.
            via.WriteRegister (Via6522::kRegIer, Via6522::kIrqTimer2);
            Assert::AreEqual<Byte> (static_cast<Byte> (Via6522::kIrqAny | Via6522::kIrqTimer1),
                                    via.GetIer());
        }


        TEST_METHOD (InterruptControllerSeesTimerIrq)
        {
            ViaTestCpu             cpu;
            Via6522                via;
            HRESULT                hr = S_OK;
            InterruptController    ic (&cpu);



            hr = via.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            EnableTimer1Irq (via);
            LoadTimer1 (via, 20);

            via.Tick (21);
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"Timer 1 underflow must drive the shared IRQ line");

            via.ReadRegister (Via6522::kRegT1CL);
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Clearing the flag must de-assert the line");
        }


        TEST_METHOD (Ca1FallingEdgeSetsFlag)
        {
            Via6522   via;



            via.WriteRegister (Via6522::kRegPcr, 0);          // CA1 falling edge

            via.SetCa1 (true);
            Assert::AreEqual (0, (int) (via.GetIfr() & Via6522::kIrqCa1),
                              L"Idle-high line must not raise a flag");

            via.SetCa1 (false);
            Assert::AreEqual ((int) Via6522::kIrqCa1, (int) (via.GetIfr() & Via6522::kIrqCa1),
                              L"Falling edge must latch the CA1 flag");
        }


        TEST_METHOD (Ca1EdgeSelectFollowsPcr)
        {
            Via6522   viaFall;
            Via6522   viaRise;



            viaFall.WriteRegister (Via6522::kRegPcr, 0);
            viaRise.WriteRegister (Via6522::kRegPcr, Via6522::kPcrCa1Rising);

            // Same stimulus, opposite configured edge: exactly one must latch.
            viaFall.SetCa1 (false);
            viaRise.SetCa1 (false);

            Assert::AreEqual ((int) Via6522::kIrqCa1, (int) (viaFall.GetIfr() & Via6522::kIrqCa1),
                              L"Falling-edge VIA must latch on a high-to-low transition");
            Assert::AreEqual (0, (int) (viaRise.GetIfr() & Via6522::kIrqCa1),
                              L"Rising-edge VIA must ignore a high-to-low transition");

            viaRise.SetCa1 (true);
            Assert::AreEqual ((int) Via6522::kIrqCa1, (int) (viaRise.GetIfr() & Via6522::kIrqCa1),
                              L"Rising-edge VIA must latch on a low-to-high transition");
        }


        TEST_METHOD (Cb1UsesItsOwnPcrBit)
        {
            Via6522   via;



            via.WriteRegister (Via6522::kRegPcr, Via6522::kPcrCb1Rising);

            via.SetCb1 (false);
            Assert::AreEqual (0, (int) (via.GetIfr() & Via6522::kIrqCb1),
                              L"CB1 configured rising must ignore a falling edge");

            via.SetCb1 (true);
            Assert::AreEqual ((int) Via6522::kIrqCb1, (int) (via.GetIfr() & Via6522::kIrqCb1),
                              L"CB1 must latch on its configured edge");
        }


        TEST_METHOD (Ca1FlagClearsOnPortAAccessButNotNoHandshakeAlias)
        {
            Via6522   via;



            via.WriteRegister (Via6522::kRegPcr, 0);

            via.SetCa1 (false);
            via.ReadRegister (Via6522::kRegOraNh);
            Assert::AreEqual ((int) Via6522::kIrqCa1, (int) (via.GetIfr() & Via6522::kIrqCa1),
                              L"$F is the no-handshake alias and must NOT clear CA1");

            via.ReadRegister (Via6522::kRegOra);
            Assert::AreEqual (0, (int) (via.GetIfr() & Via6522::kIrqCa1),
                              L"Reading ORA must clear the CA1 flag");
        }


        TEST_METHOD (Ca1InterruptVectorsAndAcknowledgeDoesNotRelatch)
        {
            ViaTestCpu            cpu;
            InterruptController   ic (&cpu);
            Via6522               via;



            via.AttachInterruptController (&ic);

            via.WriteRegister (Via6522::kRegPcr, 0);
            via.WriteRegister (Via6522::kRegIer,
                               static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqCa1));

            via.SetCa1 (false);
            Assert::IsTrue (cpu.IrqAsserted(),
                            L"An enabled CA1 edge must drive the shared IRQ line");

            via.ReadRegister (Via6522::kRegOra);
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"Acknowledging must de-assert the line");

            // The line is still low. A level that never transitions again must
            // not re-latch -- the flag is edge-triggered, not level-triggered.
            via.Tick (1000);
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"A held-low line must not spuriously re-latch");
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  UndrivenControlLinesChangeNothing
        //
        //  The compatibility hinge for the Mockingboard A/C split (spec 024
        //  R13). A card with no handshaking peripheral never calls SetCa1 or
        //  SetCb1, so the lines sit at their idle-high reset value, no edge is
        //  ever seen, and the VIA cannot acquire an interrupt source it did not
        //  have before control lines were modeled.
        //
        //  Enabling EVERY interrupt source and running the timers is the point:
        //  if anything about the control-line machinery could raise a flag on
        //  its own, this is where it would show.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UndrivenControlLinesChangeNothing)
        {
            ViaTestCpu            cpu;
            InterruptController   ic (&cpu);
            Via6522               via;
            Byte                  ifr = 0;



            via.AttachInterruptController (&ic);

            via.WriteRegister (Via6522::kRegIer, static_cast<Byte> (Via6522::kIerSetClear | 0x7F));

            via.Tick (100000);

            ifr = via.GetIfr();

            Assert::AreEqual (0, (int) (ifr & Via6522::kIrqCa1),
                              L"An undriven CA1 must never latch");
            Assert::AreEqual (0, (int) (ifr & Via6522::kIrqCb1),
                              L"An undriven CB1 must never latch");
            Assert::IsFalse (cpu.IrqAsserted(),
                             L"No timer armed and no line driven must leave the IRQ line idle");
        }


    private:
        static void EnableTimer1Irq (Via6522 & via)
        {
            via.WriteRegister (Via6522::kRegIer,
                               static_cast<Byte> (Via6522::kIerSetClear | Via6522::kIrqTimer1));
        }

        static void LoadTimer1 (Via6522 & via, uint16_t latch)
        {
            via.WriteRegister (Via6522::kRegT1CL, static_cast<Byte> (latch & 0xFF));
            via.WriteRegister (Via6522::kRegT1CH, static_cast<Byte> ((latch >> 8) & 0xFF));
        }
    };
}
