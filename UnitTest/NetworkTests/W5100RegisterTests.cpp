#include "Pch.h"

#include "Devices/Network/W5100.h"

#include "W5100Driver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingBackend
//
//  Counts what the chip passes to its backend.
//
////////////////////////////////////////////////////////////////////////////////

namespace W5100RegisterTests
{
    class RecordingBackend : public IW5100Backend
    {
    public:

        void  OnCommand (int socket, Byte command) override { commands.push_back ({ socket, command }); }
        void  OnReset   ()                         override { resets++; }

        std::vector<std::pair<int, Byte>>  commands;
        int                                resets = 0;
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  W5100RegisterTests
    //
    //  Reset values and register access rules from the W5100 datasheet,
    //  through the indirect bus.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (W5100RegisterTests)
    {
    public:

        TEST_METHOD (CommonRegistersHaveDatasheetResetValues)
        {
            W5100        chip;
            W5100Driver  bus (chip);
            Word         address = 0;

            bus.Initialize();

            for (address = R::kGar; address < R::kIr; address++)
            {
                Assert::AreEqual ((Byte) 0, bus.Peek (address), std::format (L"${:04X}", address).c_str());
            }

            Assert::AreEqual ((Byte) 0,                  bus.Peek (R::kIr));
            Assert::AreEqual ((Byte) 0,                  bus.Peek (R::kImr));
            Assert::AreEqual (R::kRtrReset,              bus.PeekWord (R::kRtr));
            Assert::AreEqual (R::kRcrReset,              bus.Peek (R::kRcr));
            Assert::AreEqual (R::kMemorySizeReset,       bus.Peek (R::kRmsr));
            Assert::AreEqual (R::kMemorySizeReset,       bus.Peek (R::kTmsr));
            Assert::AreEqual ((Word) 0,                  bus.PeekWord (R::kPatr));
            Assert::AreEqual (R::kPtimerReset,           bus.Peek (R::kPtimer));
            Assert::AreEqual ((Byte) 0,                  bus.Peek (R::kPmagic));
            Assert::AreEqual ((Word) 0,                  bus.PeekWord (R::kUport));
        }


        TEST_METHOD (SocketRegistersHaveDatasheetResetValues)
        {
            constexpr Word  kTwoKilobytes = 0x0800;

            W5100        chip;
            W5100Driver  bus (chip);
            int          socket = 0;
            Word         i      = 0;

            bus.Initialize();

            for (socket = 0; socket < W5100::kSocketCount; socket++)
            {
                Assert::AreEqual ((Byte) 0, bus.Peek (W5100Driver::SocketReg (socket, R::kSnMr)));
                Assert::AreEqual ((Byte) 0, bus.Peek (W5100Driver::SocketReg (socket, R::kSnCr)));
                Assert::AreEqual ((Byte) 0, bus.Peek (W5100Driver::SocketReg (socket, R::kSnIr)));
                Assert::AreEqual ((Byte) 0, bus.Peek (W5100Driver::SocketReg (socket, R::kSnSr)));

                for (i = 0; i < R::kMacLength; i++)
                {
                    Assert::AreEqual (R::kSnDharReset, bus.Peek (W5100Driver::SocketReg (socket, (Word) (R::kSnDhar + i))));
                }

                Assert::AreEqual (R::kSnTtlReset,  bus.Peek     (W5100Driver::SocketReg (socket, R::kSnTtl)));
                Assert::AreEqual (kTwoKilobytes,   bus.PeekWord (W5100Driver::SocketReg (socket, R::kSnTxFsr)));
                Assert::AreEqual ((Word) 0,        bus.PeekWord (W5100Driver::SocketReg (socket, R::kSnRxRsr)));
                Assert::AreEqual ((Word) 0,        bus.PeekWord (W5100Driver::SocketReg (socket, R::kSnTxWr)));
                Assert::AreEqual ((Word) 0,        bus.PeekWord (W5100Driver::SocketReg (socket, R::kSnRxRd)));
            }
        }


        TEST_METHOD (WritableRegistersReadBack)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            bus.Poke (R::kShar, 0x02);
            bus.Poke (R::kSipr + 3, 0x0F);
            bus.PokeWord (R::kRtr, 0x0FA0);
            bus.Poke (R::kRcr, 0x03);
            bus.Poke (R::kImr, 0xEF);

            Assert::AreEqual ((Byte) 0x02,   bus.Peek (R::kShar));
            Assert::AreEqual ((Byte) 0x0F,   bus.Peek (R::kSipr + 3));
            Assert::AreEqual ((Word) 0x0FA0, bus.PeekWord (R::kRtr));
            Assert::AreEqual ((Byte) 0x03,   bus.Peek (R::kRcr));
            Assert::AreEqual ((Byte) 0xEF,   bus.Peek (R::kImr));
        }


        TEST_METHOD (ReservedLocationsReadZeroAndIgnoreWrites)
        {
            constexpr Word  kCommonReserved = 0x0013;
            constexpr Word  kAboveCommon    = 0x0100;
            constexpr Word  kSocketReserved = 0x0418;
            constexpr Word  kGapBelowTx     = 0x2000;

            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            for (Word address : { kCommonReserved, kAboveCommon, kSocketReserved, kGapBelowTx })
            {
                bus.Poke (address, 0x5A);
                Assert::AreEqual ((Byte) 0, bus.Peek (address), std::format (L"${:04X}", address).c_str());
            }
        }


        TEST_METHOD (ReadOnlyRegistersIgnoreWrites)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            bus.Poke (R::kUipr, 0x12);
            bus.PokeWord (R::kUport, 0x3456);
            bus.Poke (W5100Driver::SocketReg (0, R::kSnSr), R::kSockEstablished);
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnTxFsr), 0x0001);
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnRxRsr), 0x0001);
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnTxRd), 0x0001);

            Assert::AreEqual ((Byte) 0,      bus.Peek (R::kUipr));
            Assert::AreEqual ((Word) 0,      bus.PeekWord (R::kUport));
            Assert::AreEqual (R::kSockClosed, bus.Status (0));
            Assert::AreEqual ((Word) 0x0800, bus.PeekWord (W5100Driver::SocketReg (0, R::kSnTxFsr)));
            Assert::AreEqual ((Word) 0,      bus.PeekWord (W5100Driver::SocketReg (0, R::kSnRxRsr)));
            Assert::AreEqual ((Word) 0,      bus.PeekWord (W5100Driver::SocketReg (0, R::kSnTxRd)));
        }


        TEST_METHOD (ManualProbeFindsTheChip)
        {
            W5100  chip;

            chip.WriteIndirect (R::kIndirectMode, R::kMrReset);
            Assert::AreEqual ((Byte) 0, chip.ReadIndirect (R::kIndirectMode), L"reset reads back $00");

            chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect | R::kMrAutoIncrement);
            Assert::AreEqual ((Byte) 0x03, chip.ReadIndirect (R::kIndirectMode), L"$03 reads back $03");
        }


        TEST_METHOD (SoftwareResetRestoresRegistersButNotThePointer)
        {
            W5100             chip;
            W5100Driver       bus (chip);
            RecordingBackend  backend;

            chip.SetBackend (&backend);
            bus.Initialize();

            bus.Poke (R::kShar, 0x02);
            bus.Poke (R::kRmsr, 0x0A);
            bus.SetPointer (0x1234);

            chip.WriteIndirect (R::kIndirectMode, R::kMrReset);

            Assert::AreEqual ((Word) 0x1234, chip.GetAddressPointer(), L"software reset keeps the pointer");
            Assert::AreEqual ((Byte) 0,      chip.ReadIndirect (R::kIndirectMode));
            Assert::AreEqual (2,             backend.resets, L"one reset from Initialize, one here");

            bus.Initialize();
            Assert::AreEqual ((Byte) 0,             bus.Peek (R::kShar));
            Assert::AreEqual (R::kMemorySizeReset,  bus.Peek (R::kRmsr));
        }


        TEST_METHOD (HardwareResetClearsThePointer)
        {
            W5100  chip;

            chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect);
            chip.WriteIndirect (R::kIndirectAddressHigh, 0x45);
            chip.WriteIndirect (R::kIndirectAddressLow, 0x67);

            chip.HardwareReset();

            Assert::AreEqual ((Word) 0, chip.GetAddressPointer());
            Assert::AreEqual ((Byte) 0, chip.ReadIndirect (R::kIndirectMode));
        }


        TEST_METHOD (DirectModeReachesTheFirstFourAddresses)
        {
            W5100  chip;

            chip.HardwareReset();

            // IND clear: offsets 1-3 are GAR0-GAR2, not the pointer and data port.
            chip.WriteIndirect (R::kIndirectAddressHigh, 0xC0);
            chip.WriteIndirect (R::kIndirectAddressLow,  0xA8);
            chip.WriteIndirect (R::kIndirectData,        0x01);

            Assert::AreEqual ((Word) 0, chip.GetAddressPointer());

            chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect);
            chip.WriteIndirect (R::kIndirectAddressHigh, 0x00);
            chip.WriteIndirect (R::kIndirectAddressLow,  (Byte) R::kGar);

            Assert::AreEqual ((Byte) 0xC0, chip.ReadIndirect (R::kIndirectData));
            chip.WriteIndirect (R::kIndirectAddressLow, (Byte) (R::kGar + 1));
            Assert::AreEqual ((Byte) 0xA8, chip.ReadIndirect (R::kIndirectData));
            chip.WriteIndirect (R::kIndirectAddressLow, (Byte) (R::kGar + 2));
            Assert::AreEqual ((Byte) 0x01, chip.ReadIndirect (R::kIndirectData));
        }


        TEST_METHOD (CommandRegisterReadsZeroAndReachesTheBackend)
        {
            W5100             chip;
            W5100Driver       bus (chip);
            RecordingBackend  backend;

            chip.SetBackend (&backend);
            bus.Initialize();

            bus.Poke (W5100Driver::SocketReg (2, R::kSnMr), R::kProtocolTcp);
            bus.Command (2, R::kCmdOpen);

            Assert::AreEqual ((Byte) 0,     bus.Peek (W5100Driver::SocketReg (2, R::kSnCr)));
            Assert::AreEqual (R::kSockInit, bus.Status (2));
            Assert::AreEqual ((size_t) 1,   backend.commands.size());
            Assert::AreEqual (2,            backend.commands[0].first);
            Assert::AreEqual (R::kCmdOpen,  backend.commands[0].second);
        }


        TEST_METHOD (OpenSetsStatusFromProtocol)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            bus.Poke (W5100Driver::SocketReg (1, R::kSnMr), R::kProtocolUdp);
            bus.Command (1, R::kCmdOpen);
            Assert::AreEqual (R::kSockUdp, bus.Status (1));

            bus.Poke (W5100Driver::SocketReg (0, R::kSnMr), R::kProtocolMacRaw | R::kSnMrMacFilter);
            bus.Command (0, R::kCmdOpen);
            Assert::AreEqual (R::kSockMacRaw, bus.Status (0));

            bus.Poke (W5100Driver::SocketReg (3, R::kSnMr), R::kProtocolMacRaw);
            bus.Command (3, R::kCmdOpen);
            Assert::AreEqual (R::kSockClosed, bus.Status (3), L"MACRAW exists only on socket 0");

            bus.Poke (W5100Driver::SocketReg (2, R::kSnMr), R::kProtocolIpRaw);
            bus.Command (2, R::kCmdOpen);
            Assert::AreEqual (R::kSockClosed, bus.Status (2), L"IPRAW is not emulated");

            bus.Poke (W5100Driver::SocketReg (0, R::kSnMr), R::kProtocolPppoe);
            bus.Command (0, R::kCmdOpen);
            Assert::AreEqual (R::kSockClosed, bus.Status (0), L"PPPoE is not emulated");
        }


        TEST_METHOD (SocketInterruptBitsClearOnWriteOne)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            chip.RaiseSocketInterrupt (1, R::kSnIrConnect | R::kSnIrReceive);
            Assert::AreEqual ((Byte) 0x02, bus.Peek (R::kIr), L"IR bit 1 follows S1_IR");

            bus.ClearInterrupts (1, R::kSnIrConnect);
            Assert::AreEqual (R::kSnIrReceive, bus.Interrupts (1));
            Assert::AreEqual ((Byte) 0x02,     bus.Peek (R::kIr));

            bus.ClearInterrupts (1, R::kSnIrReceive);
            Assert::AreEqual ((Byte) 0, bus.Interrupts (1));
            Assert::AreEqual ((Byte) 0, bus.Peek (R::kIr), L"IR bit 1 clears with S1_IR");
        }


        TEST_METHOD (UnreachableSetsIrAndClearsOnWriteOne)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            chip.SetUnreachable (0xC0A8000B, 5000);

            Assert::AreEqual (R::kIrUnreachable, bus.Peek (R::kIr));
            Assert::AreEqual ((Byte) 0xC0,       bus.Peek (R::kUipr));
            Assert::AreEqual ((Byte) 0x0B,       bus.Peek (R::kUipr + 3));
            Assert::AreEqual ((Word) 5000,       bus.PeekWord (R::kUport));

            bus.Poke (R::kIr, R::kIrUnreachable);
            Assert::AreEqual ((Byte) 0, bus.Peek (R::kIr));
        }


        TEST_METHOD (InterruptPendingFollowsTheMask)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            chip.RaiseSocketInterrupt (0, R::kSnIrReceive);
            Assert::IsFalse (chip.IsInterruptPending(), L"masked by IMR = 0");

            bus.Poke (R::kImr, 0x01);
            Assert::IsTrue (chip.IsInterruptPending());

            bus.ClearInterrupts (0, R::kSnIrReceive);
            Assert::IsFalse (chip.IsInterruptPending());
        }
    };
}
