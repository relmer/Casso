#include "Pch.h"

#include "Devices/Network/W5100.h"

#include "W5100Driver.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

using R = W5100Registers;





////////////////////////////////////////////////////////////////////////////////
//
//  W5100BufferTests
//
//  The address pointer, its auto-increment and the wrap the Uthernet II
//  manual documents, and the TX/RX buffer arithmetic of the datasheet.
//
////////////////////////////////////////////////////////////////////////////////

namespace W5100BufferTests
{
    TEST_CLASS (W5100BufferTests)
    {
    public:

        TEST_METHOD (PointerReadsBack)
        {
            W5100  chip;

            chip.HardwareReset();
            chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect);
            chip.WriteIndirect (R::kIndirectAddressHigh, 0x04);
            chip.WriteIndirect (R::kIndirectAddressLow, 0x01);

            Assert::AreEqual ((Byte) 0x04, chip.ReadIndirect (R::kIndirectAddressHigh));
            Assert::AreEqual ((Byte) 0x01, chip.ReadIndirect (R::kIndirectAddressLow));
        }


        TEST_METHOD (AutoIncrementAdvancesOnEachDataAccess)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();
            bus.SetPointer (R::kShar);

            chip.WriteIndirect (R::kIndirectData, 0x00);
            chip.WriteIndirect (R::kIndirectData, 0x08);
            chip.WriteIndirect (R::kIndirectData, 0xDC);

            Assert::AreEqual ((Word) (R::kShar + 3), chip.GetAddressPointer());
            Assert::AreEqual ((Byte) 0xDC, bus.Peek (R::kShar + 2));
        }


        TEST_METHOD (NoAdvanceWithoutAutoIncrement)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            chip.HardwareReset();
            chip.WriteIndirect (R::kIndirectMode, R::kMrIndirect);
            bus.SetPointer (R::kShar);

            chip.WriteIndirect (R::kIndirectData, 0x11);
            chip.WriteIndirect (R::kIndirectData, 0x22);

            Assert::AreEqual (R::kShar, chip.GetAddressPointer());
            Assert::AreEqual ((Byte) 0x22, chip.ReadIndirect (R::kIndirectData));
        }


        TEST_METHOD (PointerWrapsAtTheEndOfEachBuffer)
        {
            struct Case { Word from; Word to; };

            static constexpr Case  s_kCases[] =
            {
                { 0x5FFF, 0x4000 },
                { 0x7FFF, 0x6000 },
                { 0xFFFF, 0xE000 },
                { 0x3FFF, 0x4000 },
                { 0x8000, 0x8001 },
            };

            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            for (const Case & c : s_kCases)
            {
                bus.SetPointer (c.from);
                chip.ReadIndirect (R::kIndirectData);

                Assert::AreEqual (c.to, chip.GetAddressPointer(), std::format (L"from ${:04X}", c.from).c_str());
            }
        }


        TEST_METHOD (AddressesAbove7FFFReachTheLocation32KbLower)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();

            Assert::AreEqual (R::kMemorySizeReset, bus.Peek (0x801A), L"$801A reads RMSR");

            bus.Poke (0xC000, 0x77);
            Assert::AreEqual ((Byte) 0x77, bus.Peek (0x4000));
        }


        TEST_METHOD (DefaultSplitIsTwoKilobytesEach)
        {
            W5100  chip;
            int    socket = 0;

            chip.HardwareReset();

            for (socket = 0; socket < W5100::kSocketCount; socket++)
            {
                Assert::AreEqual ((size_t) 0x800, chip.GetTxBufferSize (socket));
                Assert::AreEqual ((size_t) 0x800, chip.GetRxBufferSize (socket));
            }
        }


        TEST_METHOD (MemorySizeRegistersAllocateFromSocketZero)
        {
            struct Case { Byte value; size_t sizes[W5100::kSocketCount]; };

            static constexpr Case  s_kCases[] =
            {
                { 0x06, { 0x1000, 0x0800, 0x0400, 0x0400 } },
                { 0x0A, { 0x1000, 0x1000, 0x0000, 0x0000 } },
                { 0xAA, { 0x1000, 0x1000, 0x0000, 0x0000 } },
                { 0xFF, { 0x2000, 0x0000, 0x0000, 0x0000 } },
                { 0x00, { 0x0400, 0x0400, 0x0400, 0x0400 } },
            };

            W5100        chip;
            W5100Driver  bus (chip);
            int          socket = 0;

            bus.Initialize();

            for (const Case & c : s_kCases)
            {
                bus.Poke (R::kRmsr, c.value);
                bus.Poke (R::kTmsr, c.value);

                for (socket = 0; socket < W5100::kSocketCount; socket++)
                {
                    std::wstring  what = std::format (L"${:02X} socket {}", c.value, socket);

                    Assert::AreEqual (c.sizes[socket], chip.GetRxBufferSize (socket), what.c_str());
                    Assert::AreEqual (c.sizes[socket], chip.GetTxBufferSize (socket), what.c_str());
                }
            }
        }


        TEST_METHOD (FreeSizeTracksTheWritePointer)
        {
            W5100        chip;
            W5100Driver  bus (chip);

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (1, R::kSnMr), R::kProtocolTcp);
            bus.Command (1, R::kCmdOpen);

            bus.PokeWord (W5100Driver::SocketReg (1, R::kSnTxWr), 0x0300);

            Assert::AreEqual ((size_t) 0x300, chip.GetTxPending (1));
            Assert::AreEqual ((Word) 0x500,   bus.PeekWord (W5100Driver::SocketReg (1, R::kSnTxFsr)));

            chip.CommitTx (1, 0x100);
            Assert::AreEqual ((Word) 0x100,   bus.PeekWord (W5100Driver::SocketReg (1, R::kSnTxRd)));
            Assert::AreEqual ((Word) 0x600,   bus.PeekWord (W5100Driver::SocketReg (1, R::kSnTxFsr)));
        }


        TEST_METHOD (TxCopyWrapsAtTheSocketBufferEnd)
        {
            W5100              chip;
            W5100Driver        bus (chip);
            std::vector<Byte>  payload (0x200);
            std::vector<Byte>  copied;
            size_t             i       = 0;

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (1, R::kSnMr), R::kProtocolTcp);
            bus.Command (1, R::kCmdOpen);

            for (i = 0; i < payload.size(); i++)
            {
                payload[i] = (Byte) i;
            }

            // Start 0x100 short of socket 1's 2 KB end so the copy wraps.
            bus.PokeWord (W5100Driver::SocketReg (1, R::kSnTxWr), 0x0700);
            chip.CommitTx (1, 0x700);
            bus.Send (1, payload);

            Assert::AreEqual (payload.size(), chip.GetTxPending (1));
            chip.CopyTx (1, 0, payload.size(), copied);

            Assert::IsTrue (copied == payload, L"bytes come back in order across the wrap");
        }


        TEST_METHOD (RxAppendWrapsAndSetsReceivedSize)
        {
            W5100              chip;
            W5100Driver        bus (chip);
            std::vector<Byte>  first (0x700, 0xAA);
            std::vector<Byte>  second (0x200);
            std::vector<Byte>  got;
            size_t             i       = 0;

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (2, R::kSnMr), R::kProtocolTcp);
            bus.Command (2, R::kCmdOpen);

            for (i = 0; i < second.size(); i++)
            {
                second[i] = (Byte) (i * 3);
            }

            Assert::IsTrue (chip.TryAppendRx (2, first));
            got = bus.ReceiveAll (2);
            Assert::AreEqual (first.size(), got.size());

            Assert::IsTrue (chip.TryAppendRx (2, second));
            Assert::AreEqual ((Word) 0x200, bus.PeekWord (W5100Driver::SocketReg (2, R::kSnRxRsr)));

            got = bus.ReceiveAll (2);
            Assert::IsTrue (got == second, L"bytes come back in order across the wrap");
            Assert::AreEqual ((Word) 0, bus.PeekWord (W5100Driver::SocketReg (2, R::kSnRxRsr)));
        }


        TEST_METHOD (RxAppendRefusesMoreThanTheFreeSpace)
        {
            W5100              chip;
            W5100Driver        bus (chip);
            std::vector<Byte>  big (0x801, 0x01);
            std::vector<Byte>  fits (0x800, 0x02);

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (3, R::kSnMr), R::kProtocolTcp);
            bus.Command (3, R::kCmdOpen);

            Assert::IsFalse (chip.TryAppendRx (3, big));
            Assert::AreEqual ((size_t) 0x800, chip.GetRxFree (3));

            Assert::IsTrue (chip.TryAppendRx (3, fits));
            Assert::AreEqual ((size_t) 0, chip.GetRxFree (3));
        }


        TEST_METHOD (RecvWithDataRemainingRaisesReceiveAgain)
        {
            W5100              chip;
            W5100Driver        bus (chip);
            std::vector<Byte>  data (0x10, 0x5A);

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (0, R::kSnMr), R::kProtocolTcp);
            bus.Command (0, R::kCmdOpen);

            Assert::IsTrue (chip.TryAppendRx (0, data));
            bus.ClearInterrupts (0, 0xFF);

            // Consume half, then RECV.
            bus.PokeWord (W5100Driver::SocketReg (0, R::kSnRxRd), 0x0008);
            bus.Command (0, R::kCmdRecv);

            Assert::AreEqual (R::kSnIrReceive, bus.Interrupts (0));
            Assert::AreEqual ((Word) 8, bus.PeekWord (W5100Driver::SocketReg (0, R::kSnRxRsr)));
        }


        TEST_METHOD (HighByteReadLatchesTheWord)
        {
            W5100        chip;
            W5100Driver  bus (chip);
            std::vector<Byte>  data (0x0FF, 0x01);
            std::vector<Byte>  more (0x002, 0x02);
            Byte         high = 0;
            Byte         low  = 0;

            bus.Initialize();
            bus.Poke (W5100Driver::SocketReg (0, R::kSnMr), R::kProtocolTcp);
            bus.Command (0, R::kCmdOpen);
            Assert::IsTrue (chip.TryAppendRx (0, data));

            high = bus.Peek (W5100Driver::SocketReg (0, R::kSnRxRsr));
            Assert::IsTrue (chip.TryAppendRx (0, more));
            low  = bus.Peek (W5100Driver::SocketReg (0, R::kSnRxRsr + 1));

            Assert::AreEqual ((Byte) 0x00, high);
            Assert::AreEqual ((Byte) 0xFF, low, L"low byte comes from the value latched with the high byte");
        }
    };
}
