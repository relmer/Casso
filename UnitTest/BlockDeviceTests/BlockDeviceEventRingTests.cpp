#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceEventRing.h"
#include "Devices/Floppy35/Floppy35EventRing.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceEventRingTests
//
//  The block device and 3.5-inch event rings: order, overflow counting, and
//  delivery through the sink interface the devices call.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BlockDeviceEventRingTests)
{
public:

    static BlockDeviceEvent MakeEvent (uint32_t block)
    {
        BlockDeviceEvent  event;



        event.op         = BlockDeviceOp::Read;
        event.firstBlock = block;
        event.blockCount = 1;

        return event;
    }


    //  EVENTS COME OUT IN THE ORDER THEY WENT IN, through the sink interface.
    TEST_METHOD (EventsPopInPushOrder)
    {
        BlockDeviceEventRing     ring;
        IBlockDeviceEventSink  & sink  = ring;
        BlockDeviceEvent         event;
        uint32_t                 count = 0;



        for (uint32_t block = 0; block < 10; block++)
        {
            sink.OnBlockDeviceEvent (MakeEvent (block));
        }

        while (ring.TryPop (event))
        {
            Assert::AreEqual (count, event.firstBlock);
            count++;
        }

        Assert::AreEqual (10u, count);
    }


    //  A FULL RING DROPS THE EVENT AND COUNTS IT, and the count resets when
    //  read.
    TEST_METHOD (FullRingCountsDrops)
    {
        auto                     ring  = std::make_unique<BlockDeviceEventRing>();
        constexpr uint32_t       kOver = 5;



        for (uint32_t block = 0; block < BlockDeviceEventRing::kCapacity + kOver; block++)
        {
            ring->OnBlockDeviceEvent (MakeEvent (block));
        }

        Assert::AreEqual (BlockDeviceEventRing::kCapacity, ring->GetApproxSize());
        Assert::AreEqual ((uint64_t) kOver, ring->TakeDroppedCount());
        Assert::AreEqual ((uint64_t) 0,     ring->TakeDroppedCount());
    }


    //  THE 3.5-INCH RING KEEPS EACH FIELD OF ITS EVENTS.
    TEST_METHOD (FloppyRingKeepsFields)
    {
        Floppy35EventRing     ring;
        IFloppy35EventSink  & sink = ring;
        Floppy35Event         in;
        Floppy35Event         out;



        in.cycle  = 12345;
        in.drive  = 1;
        in.op     = Floppy35Op::Step;
        in.track  = 79;
        in.side   = 1;
        in.sector = 11;

        sink.OnFloppy35Event (in);

        Assert::IsTrue  (ring.TryPop (out));
        Assert::AreEqual ((uint64_t) 12345, out.cycle);
        Assert::AreEqual ((int) Floppy35Op::Step, (int) out.op);
        Assert::AreEqual ((Byte) 79, out.track);
        Assert::AreEqual ((Byte) 11, out.sector);
        Assert::IsFalse (ring.TryPop (out));
    }
};
