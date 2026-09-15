#include "Pch.h"

#include "Core/IWatchSink.h"
#include "Core/MemoryBus.h"
#include "Core/MemoryDevice.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBusWatchMaskTests
//
//  The bus's per-page watch mask. A watched page is published as null so the
//  CPU's inline read falls through to the bus, the MMU's pointer is kept in a
//  shadow table, and every access to the page reaches the watch sink.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MemoryBusWatchMaskTests)
    {
    public:

        struct Access
        {
            Word                 address;
            Byte                 value;
            BusAccess            access;
            std::optional<Byte>  previous;
        };

        class RecordingSink : public IWatchSink
        {
        public:
            std::vector<Access> accesses;

            void OnWatchedAccess (Word address, Byte value, BusAccess access, std::optional<Byte> previous) override
            {
                accesses.push_back ({ address, value, access, previous });
            }
        };

        class CountingDevice : public MemoryDevice
        {
        public:
            int  reads  = 0;
            int  writes = 0;

            Byte Read  (Word) override        { ++reads; return 0x5A; }
            void Write (Word, Byte) override  { ++writes; }
            Word GetStart() const override    { return 0xC000; }
            Word GetEnd() const override      { return 0xC0FF; }
            void Reset() override             {}
        };

        static constexpr int  kPageSize = 0x100;



        TEST_METHOD (EmptyMask_PublishesPointers)
        {
            MemoryBus          bus;
            std::vector<Byte>  page (kPageSize, 0);



            bus.SetReadPage  (0x03, page.data());
            bus.SetWritePage (0x03, page.data());

            Assert::IsTrue (bus.GetReadPageTable()[0x03] == page.data());
            Assert::IsTrue (bus.GetReadPage  (0x0300)    == page.data());
            Assert::IsTrue (bus.GetWritePage (0x0300)    == page.data());
        }



        TEST_METHOD (WatchedPage_PublishesNull_KeepsShadow)
        {
            MemoryBus          bus;
            std::vector<Byte>  first  (kPageSize, 0);
            std::vector<Byte>  second (kPageSize, 0);



            bus.SetReadPage    (0x03, first.data());
            bus.SetWritePage   (0x03, first.data());
            bus.SetWatchedPage (0x03, true);

            Assert::IsTrue (bus.GetReadPageTable()[0x03]   == nullptr);
            Assert::IsTrue (bus.GetWritePage (0x0300)      == nullptr);
            Assert::IsTrue (bus.GetShadowReadPage (0x0300) == first.data());

            bus.SetReadPage  (0x03, second.data());
            bus.SetWritePage (0x03, second.data());

            Assert::IsTrue (bus.GetReadPageTable()[0x03]    == nullptr);
            Assert::IsTrue (bus.GetShadowReadPage  (0x0300) == second.data());
            Assert::IsTrue (bus.GetShadowWritePage (0x0300) == second.data());

            bus.SetWatchedPage (0x03, false);

            Assert::IsTrue (bus.GetReadPageTable()[0x03] == second.data());
            Assert::IsTrue (bus.GetWritePage (0x0300)    == second.data());
        }



        TEST_METHOD (WatchedPage_ReadsAndWritesThroughSlowPath)
        {
            MemoryBus          bus;
            RecordingSink      sink;
            std::vector<Byte>  page (kPageSize, 0);



            page[0x10] = 0x77;
            bus.SetReadPage    (0x03, page.data());
            bus.SetWritePage   (0x03, page.data());
            bus.SetWatchSink   (&sink);
            bus.SetWatchedPage (0x03, true);

            Assert::AreEqual ((Byte) 0x77, bus.ReadByte (0x0310));
            bus.WriteByte (0x0311, 0x99);
            Assert::AreEqual ((Byte) 0x99, page[0x11]);

            Assert::AreEqual ((size_t) 2,        sink.accesses.size());
            Assert::AreEqual ((Word) 0x0310,     sink.accesses[0].address);
            Assert::AreEqual ((Byte) 0x77,       sink.accesses[0].value);
            Assert::IsTrue   (sink.accesses[0].access == BusAccess::Read);
            Assert::AreEqual ((Word) 0x0311,     sink.accesses[1].address);
            Assert::AreEqual ((Byte) 0x99,       sink.accesses[1].value);
            Assert::IsTrue   (sink.accesses[1].access == BusAccess::Write);
        }



        TEST_METHOD (WatchedWrite_ReportsReplacedByte_MemoryOnly)
        {
            MemoryBus          bus;
            RecordingSink      sink;
            CountingDevice     device;
            std::vector<Byte>  page (kPageSize, 0);



            page[0x20] = 0xA0;
            bus.SetReadPage    (0x03, page.data());
            bus.SetWritePage   (0x03, page.data());
            bus.AddDevice      (&device);
            bus.SetWatchSink   (&sink);
            bus.SetWatchedPage (0x03, true);
            bus.SetWatchedPage (0xC0, true);

            bus.WriteByte (0x0320, 0x41);
            bus.ReadByte  (0x0320);
            bus.WriteByte (0xC010, 0x01);

            Assert::AreEqual ((size_t) 3, sink.accesses.size());
            Assert::IsTrue   (sink.accesses[0].previous.has_value());
            Assert::AreEqual ((Byte) 0xA0, *sink.accesses[0].previous);
            Assert::AreEqual ((Byte) 0x41, sink.accesses[0].value);
            Assert::IsFalse  (sink.accesses[1].previous.has_value());
            Assert::IsFalse  (sink.accesses[2].previous.has_value());
        }



        TEST_METHOD (WatchedVideoPage_RaisesVideoDirty)
        {
            MemoryBus          bus;
            std::vector<Byte>  page (kPageSize, 0);



            bus.SetReadPage       (0x04, page.data());
            bus.SetWritePage      (0x04, page.data());
            bus.SetVideoWatchPage (0x04, true);
            bus.SetWatchedPage    (0x04, true);

            bus.ClearVideoDirty();
            bus.WriteByte (0x0400, 0xC1);
            Assert::IsTrue (bus.IsVideoDirty());

            bus.ClearVideoDirty();
            bus.WriteByte (0x0478, 0xC1);
            Assert::IsFalse (bus.IsVideoDirty());

            bus.WriteByte (0x0400, 0xC1);
            Assert::IsFalse (bus.IsVideoDirty());
        }



        TEST_METHOD (WatchedIoPage_CallsDeviceOnce)
        {
            MemoryBus       bus;
            RecordingSink   sink;
            CountingDevice  device;



            bus.AddDevice      (&device);
            bus.SetWatchSink   (&sink);
            bus.SetWatchedPage (0xC0, true);

            Assert::AreEqual ((Byte) 0x5A, bus.ReadByte (0xC000));
            bus.WriteByte (0xC010, 0x01);

            Assert::AreEqual (1,          device.reads);
            Assert::AreEqual (1,          device.writes);
            Assert::AreEqual ((size_t) 2, sink.accesses.size());
        }



        TEST_METHOD (UnwatchedPage_ReportsNothing)
        {
            MemoryBus          bus;
            RecordingSink      sink;
            std::vector<Byte>  page (kPageSize, 0);



            bus.SetReadPage    (0x05, page.data());
            bus.SetWritePage   (0x05, page.data());
            bus.SetWatchSink   (&sink);
            bus.SetWatchedPage (0x03, true);

            bus.ReadByte  (0x0500);
            bus.WriteByte (0x0501, 0x01);

            Assert::AreEqual ((size_t) 0, sink.accesses.size());
        }
    };
}
