#include "Pch.h"

#include "Devices/Acia6551.h"
#include "Devices/AciaEndpoints.h"
#include "Devices/IAciaEndpoint.h"
#include "Core/InterruptController.h"
#include "Core/ComponentRegistry.h"
#include "Core/MemoryBus.h"
#include "Core/MachineConfig.h"
#include "ICpu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




namespace Acia6551TestNs
{
    // Slot-1 register addresses ($C088 + 1*16 = $C098).
    static constexpr Word    kBase       = 0xC098;
    static constexpr Word    kAddrData   = 0xC098;
    static constexpr Word    kAddrStatus = 0xC099;
    static constexpr Word    kAddrCmd    = 0xC09A;
    static constexpr Word    kAddrCtrl   = 0xC09B;




    ////////////////////////////////////////////////////////////////////////////
    //
    //  AciaTestCpu
    //
    //  Minimal ICpu double recording the maskable IRQ line state.
    //
    ////////////////////////////////////////////////////////////////////////////

    class AciaTestCpu : public ICpu
    {
    public:
        HRESULT     Reset         () override                     { return S_OK; }
        HRESULT     Step          (uint32_t & outCycles) override { outCycles = 0; return S_OK; }
        uint64_t    GetCycleCount () const override               { return 0; }

        void        SetInterruptLine (CpuInterruptKind kind, bool asserted) override
        {
            if (kind == CpuInterruptKind::kMaskable)
            {
                m_irqAsserted = asserted;
            }
        }

        bool        IrqAsserted () const { return m_irqAsserted; }

    private:
        bool    m_irqAsserted = false;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  RecordingEndpoint
    //
    //  Captures every transmitted byte in memory (no real I/O).
    //
    ////////////////////////////////////////////////////////////////////////////

    class RecordingEndpoint : public IAciaEndpoint
    {
    public:
        void    OnByteTransmitted (Byte value) override { m_bytes.push_back (value); }

        const vector<Byte> &    Bytes () const { return m_bytes; }

    private:
        vector<Byte>    m_bytes;
    };




    ////////////////////////////////////////////////////////////////////////////
    //
    //  Acia6551Tests
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (Acia6551Tests)
    {
    public:
        TEST_METHOD (ResetLeavesTransmitterEmpty)
        {
            Acia6551    acia (kBase);



            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusTxEmpty) != 0,
                            L"TDRE must be set after reset");
            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusRxFull) == 0,
                            L"RDRF must be clear after reset");
            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusIrq) == 0,
                            L"IRQ latch must be clear after reset");
            Assert::AreEqual<Byte> (0, acia.GetCommand ());
            Assert::AreEqual<Byte> (0, acia.GetControl ());
        }


        TEST_METHOD (AddressWindowIsFourRegisters)
        {
            Acia6551    acia (kBase);



            Assert::AreEqual<Word> (kBase, acia.GetStart ());
            Assert::AreEqual<Word> (kAddrCtrl, acia.GetEnd ());
        }


        TEST_METHOD (TransmitByteReachesEndpoint)
        {
            Acia6551            acia (kBase);
            RecordingEndpoint   endpoint;



            acia.SetEndpoint (&endpoint);
            acia.Write (kAddrData, 0x41);

            Assert::AreEqual<size_t> (1, endpoint.Bytes ().size ());
            Assert::AreEqual<Byte> (0x41, endpoint.Bytes ().front ());
        }


        TEST_METHOD (LoopbackDeliversByteToReceiver)
        {
            Acia6551                acia (kBase);
            AciaLoopbackEndpoint    loopback (&acia);
            Byte                    received = 0;



            acia.SetEndpoint (&loopback);
            acia.Write (kAddrData, 0x37);

            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusRxFull) != 0,
                            L"Loopback must fill the receiver");

            received = acia.Read (kAddrData);
            Assert::AreEqual<Byte> (0x37, received);
            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusRxFull) == 0,
                            L"Reading data must clear RDRF");
        }


        TEST_METHOD (ReceiverIrqAssertsAndStatusReadClearsIt)
        {
            AciaTestCpu             cpu;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);
            AciaLoopbackEndpoint    loopback (&acia);
            HRESULT                 hr = S_OK;
            Byte                    status = 0;



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.SetEndpoint (&loopback);

            // DTR set, receiver IRQ enabled (bit 1 clear).
            acia.Write (kAddrCmd, Acia6551::kCommandDtr);
            acia.Write (kAddrData, 0x5A);

            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusIrq) != 0,
                            L"Receiver-full must latch the IRQ bit");
            Assert::IsTrue (cpu.IrqAsserted (), L"IRQ line must be asserted");

            status = acia.Read (kAddrStatus);
            Assert::IsTrue ((status & Acia6551::kStatusIrq) != 0,
                            L"Status snapshot still shows the IRQ that fired");
            Assert::IsFalse (cpu.IrqAsserted (),
                             L"Reading status must de-assert the IRQ line");
            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusIrq) == 0,
                            L"IRQ latch cleared after status read");
        }


        TEST_METHOD (ReceiverIrqSuppressedWhenDisabled)
        {
            AciaTestCpu             cpu;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);
            AciaLoopbackEndpoint    loopback (&acia);
            HRESULT                 hr = S_OK;



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.SetEndpoint (&loopback);

            // DTR set but receiver IRQ disabled (bit 1 set).
            acia.Write (kAddrCmd, Acia6551::kCommandDtr | Acia6551::kCommandRxIrqDisable);
            acia.Write (kAddrData, 0x5A);

            Assert::IsFalse (cpu.IrqAsserted (),
                             L"Disabled receiver IRQ must not drive the line");
        }


        TEST_METHOD (TransmitIrqAssertsWhenEnabled)
        {
            AciaTestCpu             cpu;
            InterruptController     ic (&cpu);
            Acia6551                acia (kBase);
            RecordingEndpoint       endpoint;
            HRESULT                 hr = S_OK;



            hr = acia.AttachInterruptController (&ic);
            Assert::AreEqual (S_OK, hr);

            acia.SetEndpoint (&endpoint);

            // DTR set, transmitter control = 01 (IRQ enabled, RTS low).
            acia.Write (kAddrCmd, Acia6551::kCommandDtr | Acia6551::kCommandTicTxIrqOn);
            acia.Write (kAddrData, 0x21);

            Assert::IsTrue (cpu.IrqAsserted (),
                            L"Transmitter-empty must assert the IRQ when enabled");
        }


        TEST_METHOD (OverrunSetWhenUnreadByteOverwritten)
        {
            Acia6551                acia (kBase);
            AciaLoopbackEndpoint    loopback (&acia);



            acia.SetEndpoint (&loopback);
            acia.Write (kAddrData, 0x11);
            acia.Write (kAddrData, 0x22);

            Assert::IsTrue ((acia.GetStatus () & Acia6551::kStatusOverrun) != 0,
                            L"Second unread byte must set overrun");
        }


        TEST_METHOD (ProgrammedResetClearsCommandButKeepsParityAndControl)
        {
            Acia6551    acia (kBase);
            Byte        parityBits = Acia6551::kCommandParityMask;



            acia.Write (kAddrCmd, static_cast<Byte> (parityBits | Acia6551::kCommandDtr));
            acia.Write (kAddrCtrl, 0x1E);

            // A write to the status address is a programmed reset.
            acia.Write (kAddrStatus, 0x00);

            Assert::AreEqual<Byte> (parityBits, acia.GetCommand (),
                                    L"Programmed reset keeps parity bits, clears the rest");
            Assert::AreEqual<Byte> (0x1E, acia.GetControl (),
                                    L"Programmed reset leaves control untouched");
        }


        TEST_METHOD (ControlRegisterSelectsWordLength)
        {
            Acia6551    acia (kBase);



            // Bits 6-5 = 00 -> 8-bit words.
            acia.Write (kAddrCtrl, 0x00);
            Assert::AreEqual (8, acia.GetWordLengthBits ());

            // Bits 6-5 = 01 -> 7-bit words.
            acia.Write (kAddrCtrl, 0x20);
            Assert::AreEqual (7, acia.GetWordLengthBits ());

            // Bits 6-5 = 11 -> 5-bit words.
            acia.Write (kAddrCtrl, 0x60);
            Assert::AreEqual (5, acia.GetWordLengthBits ());
        }


        TEST_METHOD (FactoryPlacesAciaInSlotIoSpace)
        {
            ComponentRegistry           registry;
            MemoryBus                   bus;
            DeviceConfig                config;
            unique_ptr<MemoryDevice>    device = nullptr;



            ComponentRegistry::RegisterBuiltinDevices (registry);
            Assert::IsTrue (registry.IsRegistered ("acia-6551"),
                            L"acia-6551 must be a registered device type");

            config.type    = "acia-6551";
            config.slot    = 1;
            config.hasSlot = true;

            device = registry.Create ("acia-6551", config, bus);
            Assert::IsNotNull (device.get ());
            Assert::AreEqual<Word> (0xC098, device->GetStart ());
            Assert::AreEqual<Word> (0xC09B, device->GetEnd ());
        }
    };
}
