#include "Pch.h"

#include "Devices/Printer/PrinterCard.h"
#include "Core/ComponentRegistry.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterCardTests
//
//  The guest-visible register contract: slot I/O window placement, data
//  latch -> ring ordering (FR-004), tolerant status reads, the high-water
//  ready/busy transition backing FR-002, and the first-touch reveal flag.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterCardTests
{
    TEST_CLASS (PrinterCardTests)
    {
    public:

        TEST_METHOD (Slot1ClaimsC090Window)
        {
            PrinterCard   card (1);

            Assert::AreEqual ((Word) 0xC090, card.GetStart());
            Assert::AreEqual ((Word) 0xC09F, card.GetEnd());
        }


        TEST_METHOD (Slot2ClaimsC0A0Window)
        {
            PrinterCard   card (2);

            Assert::AreEqual ((Word) 0xC0A0, card.GetStart());
            Assert::AreEqual ((Word) 0xC0AF, card.GetEnd());
        }


        TEST_METHOD (DataWritesReachRingInOrder)
        {
            PrinterCard   card (1);
            Word          dataAddr = (Word) (card.GetStart() + PrinterCard::kDataOffset);
            Byte          out      = 0;

            card.Write (dataAddr, 'A');
            card.Write (dataAddr, 'B');
            card.Write (dataAddr, 'C');

            Assert::AreEqual ((uint32_t) 3, card.ByteRing().ApproxSize());

            Assert::IsTrue (card.ByteRing().TryPop (out));
            Assert::AreEqual ((Byte) 'A', out);
            Assert::IsTrue (card.ByteRing().TryPop (out));
            Assert::AreEqual ((Byte) 'B', out);
            Assert::IsTrue (card.ByteRing().TryPop (out));
            Assert::AreEqual ((Byte) 'C', out);
        }


        TEST_METHOD (NonDataOffsetWritesIgnored)
        {
            PrinterCard   card (1);
            Word          i    = 0;

            // Every offset above the data latch is inert.
            for (i = 1; i < PrinterCard::kSlotIoSize; i++)
            {
                card.Write ((Word) (card.GetStart() + i), 0x55);
            }

            Assert::AreEqual ((uint32_t) 0, card.ByteRing().ApproxSize());
        }


        TEST_METHOD (AllOffsetsReadStatusReadyWhenIdle)
        {
            PrinterCard   card (1);
            Word          i    = 0;

            for (i = 0; i < PrinterCard::kSlotIoSize; i++)
            {
                Byte  status = card.Read ((Word) (card.GetStart() + i));
                Assert::AreEqual (PrinterCard::kStatusReady, status);
            }
        }


        TEST_METHOD (StatusGoesBusyAtHighWater)
        {
            PrinterCard   card (1);
            Word          dataAddr  = (Word) (card.GetStart() + PrinterCard::kDataOffset);
            Word          statusAddr = (Word) (card.GetStart() + PrinterCard::kStatusOffset);
            uint32_t      toFill    = PrinterByteRing::kByteRingCapacity - PrinterCard::kReadyHighWater;
            uint32_t      i         = 0;

            // Still ready one byte short of the guard.
            for (i = 0; i < toFill - 1; i++)
            {
                card.Write (dataAddr, (Byte) i);
            }

            Assert::AreEqual (PrinterCard::kStatusReady, card.Read (statusAddr));

            // Crossing into the high-water margin de-asserts ready.
            card.Write (dataAddr, 0x00);
            Assert::AreEqual (PrinterCard::kStatusBusy, card.Read (statusAddr));
        }


        TEST_METHOD (FirstTouchFlagArmsOnDataWriteAndResets)
        {
            PrinterCard   card (1);
            Word          dataAddr = (Word) (card.GetStart() + PrinterCard::kDataOffset);

            Assert::IsFalse (card.EverTouched());

            card.Write (dataAddr, 'X');
            Assert::IsTrue (card.EverTouched());

            card.Reset();
            Assert::IsFalse (card.EverTouched());
        }


        TEST_METHOD (RegisteredAsParallelPrinter)
        {
            ComponentRegistry   registry;

            ComponentRegistry::RegisterBuiltinDevices (registry);

            Assert::IsTrue (registry.IsRegistered ("parallel-printer"));
        }


        TEST_METHOD (FactoryBuildsCardForConfiguredSlot)
        {
            ComponentRegistry   registry;
            MemoryBus           bus;
            DeviceConfig        cfg;

            ComponentRegistry::RegisterBuiltinDevices (registry);

            cfg.type    = "parallel-printer";
            cfg.slot    = 2;
            cfg.hasSlot = true;

            unique_ptr<MemoryDevice>   device = registry.Create (cfg.type, cfg, bus);

            Assert::IsNotNull (device.get());
            Assert::AreEqual ((Word) 0xC0A0, device->GetStart());
            Assert::AreEqual ((Word) 0xC0AF, device->GetEnd());
        }
    };
}
