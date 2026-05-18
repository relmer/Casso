#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>
#include <random>

#include "Devices/DiskIIAddressMarkWatcher.h"
#include "Devices/Disk/DiskIINibbleEngine.h"
#include "Devices/Disk/DiskImage.h"


// Some helpers below allocate large stack buffers (random-nibble
// torture stream). Suppress C6262 for this file only.
#pragma warning (disable: 6262)


using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  RecordingSink
//
//  Captures an ordered log of address-mark and data-mark events.
//  Other IDiskIIEventSink methods are no-ops; the watcher only fires
//  OnAddressMark / OnDataMarkRead per FR-008.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    class RecordingSink : public IDiskIIEventSink
    {
    public:
        struct AddrEntry
        {
            int    track;
            int    sector;
            int    volume;
        };

        struct DataEntry
        {
            int    sector;
            int    byteCount;
        };

        std::vector<AddrEntry>  addrLog;
        std::vector<DataEntry>  dataLog;

        void OnMotorCommandOn  () override                                  {}
        void OnMotorEngaged    () override                                  {}
        void OnMotorCommandOff() override                                  {}
        void OnMotorDisengaged() override                                  {}
        void OnHeadStep        (int, int) override                          {}
        void OnHeadBump        (int) override                               {}
        void OnAddressMark     (int track, int sector, int volume) override { addrLog.push_back ({ track, sector, volume }); }
        void OnDataMarkRead    (int sector, int byteCount) override         { dataLog.push_back ({ sector, byteCount }); }
        void OnDataMarkWrite   (int, int) override                          {}
        void OnDriveSelect     (int) override                               {}
        void OnDiskInserted    (int) override                               {}
        void OnDiskEjected     (int) override                               {}
    };



    ////////////////////////////////////////////////////////////////////////////
    //
    //  Encode4and4
    //
    //  Encodes one source byte into a (hi, lo) pair so that
    //  ((hi << 1) | 1) & lo == value. Standard Apple II RWTS encoder.
    //
    ////////////////////////////////////////////////////////////////////////////

    void Encode4and4 (uint8_t value, uint8_t & hi, uint8_t & lo)
    {
        hi = (uint8_t) (((value >> 1) & 0x55) | 0xAA);
        lo = (uint8_t) ((value         & 0x55) | 0xAA);
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  AppendAddressMark
    //
    //  Appends a stock DOS 3.3 address mark (prologue + 4-and-4 fields +
    //  epilogue) to `out`. `chkOverride` lets a caller corrupt the
    //  checksum without disturbing the rest of the frame.
    //
    ////////////////////////////////////////////////////////////////////////////

    void AppendAddressMark (std::vector<uint8_t> & out,
                            uint8_t                vol,
                            uint8_t                trk,
                            uint8_t                sec,
                            uint8_t                chkOverride,
                            bool                   useOverride)
    {
        uint8_t  chk   = useOverride ? chkOverride : (uint8_t) (vol ^ trk ^ sec);
        uint8_t  volHi = 0;
        uint8_t  volLo = 0;
        uint8_t  trkHi = 0;
        uint8_t  trkLo = 0;
        uint8_t  secHi = 0;
        uint8_t  secLo = 0;
        uint8_t  chkHi = 0;
        uint8_t  chkLo = 0;

        Encode4and4 (vol, volHi, volLo);
        Encode4and4 (trk, trkHi, trkLo);
        Encode4and4 (sec, secHi, secLo);
        Encode4and4 (chk, chkHi, chkLo);

        out.push_back (DiskIIAddressMarkWatcher::kAddrMarkPrologue0);
        out.push_back (DiskIIAddressMarkWatcher::kAddrMarkPrologue1);
        out.push_back (DiskIIAddressMarkWatcher::kAddrMarkPrologue2);
        out.push_back (volHi);
        out.push_back (volLo);
        out.push_back (trkHi);
        out.push_back (trkLo);
        out.push_back (secHi);
        out.push_back (secLo);
        out.push_back (chkHi);
        out.push_back (chkLo);
        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue0);
        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue1);
        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue2);
    }



    ////////////////////////////////////////////////////////////////////////////
    //
    //  AppendDataMark
    //
    //  Appends a 6-and-2 data mark: prologue + 342 body nibbles + 1
    //  checksum nibble + epilogue. Body nibbles can be anything that
    //  doesn't accidentally spell DE AA EB; the watcher counts but does
    //  not decode them.
    //
    ////////////////////////////////////////////////////////////////////////////

    void AppendDataMark (std::vector<uint8_t> & out)
    {
        uint32_t  i = 0;

        out.push_back (DiskIIAddressMarkWatcher::kAddrMarkPrologue0);
        out.push_back (DiskIIAddressMarkWatcher::kAddrMarkPrologue1);
        out.push_back (DiskIIAddressMarkWatcher::kDataMarkPrologue2);

        for (i = 0; i < DiskIIAddressMarkWatcher::kDataNibbleCount + 1; i++)
        {
            out.push_back (0x96);
        }

        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue0);
        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue1);
        out.push_back (DiskIIAddressMarkWatcher::kSectorEpilogue2);
    }



    void FeedAll (DiskIIAddressMarkWatcher & w, const std::vector<uint8_t> & nibbles)
    {
        size_t   i = 0;

        for (i = 0; i < nibbles.size(); i++)
        {
            w.ObserveNibble (nibbles[i]);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  DiskIIAddressMarkWatcherTests
//
////////////////////////////////////////////////////////////////////////////////

namespace DiskIIAddressMarkWatcherTests
{
    TEST_CLASS (DiskIIAddressMarkWatcherTests)
    {
    public:

        TEST_METHOD (GoodAddressMark_firesOnceWithDecodedFields)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            AppendAddressMark (stream, 254, 17, 5, 0, false);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.addrLog.size());
            Assert::AreEqual (17, sink.addrLog[0].track);
            Assert::AreEqual (5,  sink.addrLog[0].sector);
            Assert::AreEqual (254, sink.addrLog[0].volume);
            Assert::AreEqual (5,  watcher.GetCachedSector());
        }


        TEST_METHOD (CorruptChecksum_firesZeroAddressMarks)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            // Deliberately wrong checksum: true XOR is 254 ^ 17 ^ 5 = 0xEA;
            // override with 0x00 to guarantee mismatch.
            AppendAddressMark (stream, 254, 17, 5, 0x00, true);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 0, sink.addrLog.size());
            Assert::AreEqual (-1, watcher.GetCachedSector());
        }


        TEST_METHOD (RandomNibbleStream_firesZeroAddressMarks)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::mt19937              rng (0xC0FFEEu);
            std::uniform_int_distribution<int>  dist (0, 255);
            uint32_t                  i = 0;
            constexpr uint32_t        kStreamBytes = 1u << 20;   // 1 MiB

            watcher.SetEventSink (&sink);

            for (i = 0; i < kStreamBytes; i++)
            {
                watcher.ObserveNibble ((uint8_t) dist (rng));
            }

            // False-positive guard. With uniform random bytes the
            // expected number of spurious D5 AA 96 triplets in 1 MiB
            // is ~1MiB / 256^3 ~= 0.06, and each survives the
            // checksum gate with probability 1/256 -- so any non-zero
            // count flags a real regression. Fixed seed keeps the
            // assertion deterministic.
            Assert::AreEqual ((size_t) 0, sink.addrLog.size());
        }


        TEST_METHOD (DataMark_firesOnceOnEpilogue)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);

            // Establish a known cached sector first.
            AppendAddressMark (stream, 254, 17, 7, 0, false);
            AppendDataMark    (stream);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (7,   sink.dataLog[0].sector);
            Assert::AreEqual (256, sink.dataLog[0].byteCount);
        }


        TEST_METHOD (DataMarkWithoutAddressMark_firesWithCachedSectorMinusOne)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            AppendDataMark (stream);
            FeedAll (watcher, stream);

            // Per FR-008 / T021: data mark seen without preceding
            // address mark still fires; sector falls back to the
            // cached value (-1 == "unknown", formatted as S? at the
            // UI layer).
            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (-1,  sink.dataLog[0].sector);
            Assert::AreEqual (256, sink.dataLog[0].byteCount);
        }


        TEST_METHOD (InterleavedSectorCadence_firesInOrder)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);

            // Three sectors in normal address-then-data cadence.
            AppendAddressMark (stream, 254, 17, 0, 0, false);
            AppendDataMark    (stream);
            AppendAddressMark (stream, 254, 17, 1, 0, false);
            AppendDataMark    (stream);
            AppendAddressMark (stream, 254, 17, 2, 0, false);
            AppendDataMark    (stream);

            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 3, sink.addrLog.size());
            Assert::AreEqual ((size_t) 3, sink.dataLog.size());

            Assert::AreEqual (0, sink.addrLog[0].sector);
            Assert::AreEqual (1, sink.addrLog[1].sector);
            Assert::AreEqual (2, sink.addrLog[2].sector);

            // Each data read picks up the most-recently-decoded
            // sector number cached by the preceding address mark.
            Assert::AreEqual (0, sink.dataLog[0].sector);
            Assert::AreEqual (1, sink.dataLog[1].sector);
            Assert::AreEqual (2, sink.dataLog[2].sector);
        }


        ////////////////////////////////////////////////////////////////////////
        //
        //  EnginePollingLoop_firesExactlyOneAddressAndDataMark
        //
        //  Spec-006 regression: the production bug shipped a watcher
        //  that received every CPU poll of $C0EC -- including the
        //  many MSB-clear and repeated samples the 6502's BPL spin
        //  loop generates between assembled nibbles -- so its
        //  state machine never matched a real D5 AA 96 prologue
        //  and zero events fired on real DOS 3.3 boot reads. This
        //  test lays one well-formed sector on a synthetic track,
        //  pumps the engine bit-by-bit, simulates a tight CPU poll
        //  loop (many ReadLatch calls per bit), and feeds the
        //  watcher through the same ConsumeFreshNibble side
        //  channel the controller uses. The watcher MUST fire
        //  exactly one OnAddressMark and exactly one
        //  OnDataMarkRead.
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EnginePollingLoop_firesExactlyOneAddressAndDataMark)
        {
            DiskIIAddressMarkWatcher  watcher;
            RecordingSink             sink;
            DiskImage                 img;
            DiskIINibbleEngine        eng;
            std::vector<uint8_t>      stream;
            size_t                    bitOffset    = 0;
            size_t                    i            = 0;
            int                       bit          = 0;
            int                       pollIndex    = 0;
            uint8_t                   fresh        = 0;
            uint8_t                   sample       = 0;

            watcher.SetEventSink (&sink);

            // Sync gap + one full sector frame. The leading FF
            // gives the LSS time to align before any prologue
            // nibble matters.
            for (i = 0; i < 8; i++)
            {
                stream.push_back (0xFF);
            }
            AppendAddressMark (stream, 254, 17, 5, 0, false);
            for (i = 0; i < 8; i++)
            {
                stream.push_back (0xFF);
            }
            AppendDataMark (stream);
            for (i = 0; i < 8; i++)
            {
                stream.push_back (0xFF);
            }

            img.ResizeTrack (0, stream.size() * 8 + 64);

            for (i = 0; i < stream.size(); i++)
            {
                int   b = 0;

                for (b = 0; b < 8; b++)
                {
                    img.WriteBit (0, bitOffset + (size_t) b,
                                  (uint8_t) ((stream[i] >> (7 - b)) & 1));
                }
                bitOffset += 8;
            }

            eng.SetDiskImage (&img);
            eng.SetMotorOn   (true);

            // Pump the engine one bit at a time. After each bit
            // simulate the 6502's "LDA $C0EC / BPL" spin loop with
            // several ReadLatch calls, then route the watcher
            // through ConsumeFreshNibble exactly as
            // DiskIIController::HandleReadDispatch does.
            for (bit = 0; bit < (int) (stream.size() * 8); bit++)
            {
                eng.Tick ((uint32_t) DiskIINibbleEngine::kCyclesPerBit);

                for (pollIndex = 0; pollIndex < 8; pollIndex++)
                {
                    sample = eng.ReadLatch ();
                    (void) sample;

                    if (eng.ConsumeFreshNibble (fresh))
                    {
                        watcher.ObserveNibble (fresh);
                    }
                }
            }

            Assert::AreEqual ((size_t) 1, sink.addrLog.size(),
                L"Polling loop must produce exactly one address-mark event");
            Assert::AreEqual ((size_t) 1, sink.dataLog.size(),
                L"Polling loop must produce exactly one data-mark event");
            Assert::AreEqual (17, sink.addrLog[0].track);
            Assert::AreEqual (5,  sink.addrLog[0].sector);
            Assert::AreEqual (5,  sink.dataLog[0].sector);
        }
    };
}
