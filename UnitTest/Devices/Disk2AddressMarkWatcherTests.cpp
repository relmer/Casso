#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Machines/Apple2/Common/Disk2AddressMarkWatcher.h"
#include "Machines/Apple2/Common/Disk2NibbleEngine.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/IDisk2EventSink.h"


// Some helpers below allocate large stack buffers (random-nibble
// torture stream). Suppress C6262 for this file only.
#pragma warning (disable: 6262)


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RecordingSink
//
//  Captures an ordered log of address-mark and data-mark events.
//  Other IDisk2EventSink methods are no-ops; the watcher only fires
//  OnAddressMark / OnDataMarkRead per FR-008.
//
////////////////////////////////////////////////////////////////////////////////





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2AddressMarkWatcherTests
//
//  The address-mark watcher: recognizing sector headers in the nibble stream
//  as the guest reads them.
//
//  It watches the READS the guest performs rather than decoding the track
//  itself, which is what lets the debug panel report which sector the boot ROM
//  is looking at without interpreting the disk format. So the tests feed nibble
//  sequences the way a read would deliver them, one at a time.
//
//  Partial and interrupted marks are the substance: a prologue that starts and
//  is cut short by a seek must reset the watcher rather than leaving it
//  half-matched, or the next stream's bytes complete a mark that never
//  occurred.
//
//  Marks are matched against the actual $D5 $AA $96 and $D5 $AA $AD prologues,
//  since the difference between an address field and a data field is what makes
//  the report useful.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2AddressMarkWatcherTests
{
    TEST_CLASS (Disk2AddressMarkWatcherTests)
    {
    public:

        class RecordingSink : public IDisk2EventSink
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
                int    track;
                int    sector;
                int    volume;
                int    byteCount;
            };

            std::vector<AddrEntry>  addrLog;
            std::vector<DataEntry>  dataLog;
            std::vector<DataEntry>  writeLog;
            std::vector<int>        stallLog;

            void Clear()
            {
                addrLog.clear();
                dataLog.clear();
                writeLog.clear();
                stallLog.clear();
            }

            void OnMotorCommandOn() override                                  {}
            void OnMotorEngaged() override                                    {}
            void OnMotorCommandOff() override                                 {}
            void OnMotorDisengaged() override                                 {}
            void OnHeadStep (int, int) override                               {}
            void OnHeadBump (int) override                                    {}
            void OnAddressMark (int track, int sector, int volume) override   { addrLog.push_back ({ track, sector, volume }); }
            void OnDataMarkRead (int track, int sector, int volume, int byteCount) override { dataLog.push_back ({ track, sector, volume, byteCount }); }
            void OnDataMarkWrite (int track, int sector, int volume, int byteCount) override { writeLog.push_back ({ track, sector, volume, byteCount }); }
            void OnReadStall (int quarterTrack) override                      { stallLog.push_back (quarterTrack); }
            void OnDriveSelect (int) override                                 {}
            void OnDiskInserted (int) override                                {}
            void OnDiskEjected (int) override                                 {}
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

            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue0);
            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue1);
            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue2);
            out.push_back (volHi);
            out.push_back (volLo);
            out.push_back (trkHi);
            out.push_back (trkLo);
            out.push_back (secHi);
            out.push_back (secLo);
            out.push_back (chkHi);
            out.push_back (chkLo);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue0);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue1);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue2);
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

            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue0);
            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue1);
            out.push_back (Disk2AddressMarkWatcher::kDataMarkPrologue2);

            for (i = 0; i < Disk2AddressMarkWatcher::kDataNibbleCount + 1; i++)
            {
                out.push_back (0x96);
            }

            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue0);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue1);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue2);
        }



        void FeedAll (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles)
        {
            size_t   i = 0;

            for (auto & nibble : nibbles)
            {
                w.ObserveNibble (nibble);
            }
        }

        TEST_METHOD (GoodAddressMark_firesOnceWithDecodedFields)
        {
            Disk2AddressMarkWatcher   watcher;
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
            Disk2AddressMarkWatcher   watcher;
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
            Disk2AddressMarkWatcher  watcher;
            RecordingSink            sink;
            uint32_t                 i            = 0;
            constexpr uint32_t       kStreamBytes = 1u << 20;   // 1 MiB
            std::mt19937              rng (0xC0FFEEu);
            std::uniform_int_distribution<int>  dist (0, 255);

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
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);

            // Establish a known cached sector first.
            AppendAddressMark (stream, 254, 17, 7, 0, false);
            AppendDataMark    (stream);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (17,  sink.dataLog[0].track);
            Assert::AreEqual (7,   sink.dataLog[0].sector);
            Assert::AreEqual (254, sink.dataLog[0].volume);
            Assert::AreEqual (256, sink.dataLog[0].byteCount);
        }


        TEST_METHOD (DataMarkWithoutAddressMark_firesWithCachedSectorMinusOne)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            AppendDataMark (stream);
            FeedAll (watcher, stream);

            // Per FR-008 / T021: data mark seen without preceding
            // address mark still fires; track / sector / volume all
            // fall back to the cached value (-1 == "unknown",
            // formatted as T? / S? / V? at the UI layer).
            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (-1,  sink.dataLog[0].track);
            Assert::AreEqual (-1,  sink.dataLog[0].sector);
            Assert::AreEqual (-1,  sink.dataLog[0].volume);
            Assert::AreEqual (256, sink.dataLog[0].byteCount);
        }


        TEST_METHOD (InterleavedSectorCadence_firesInOrder)
        {
            Disk2AddressMarkWatcher   watcher;
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
            // sector / track / volume cached by the preceding
            // address mark.
            Assert::AreEqual (0, sink.dataLog[0].sector);
            Assert::AreEqual (1, sink.dataLog[1].sector);
            Assert::AreEqual (2, sink.dataLog[2].sector);

            Assert::AreEqual (17, sink.dataLog[0].track);
            Assert::AreEqual (17, sink.dataLog[1].track);
            Assert::AreEqual (17, sink.dataLog[2].track);

            Assert::AreEqual (254, sink.dataLog[0].volume);
            Assert::AreEqual (254, sink.dataLog[1].volume);
            Assert::AreEqual (254, sink.dataLog[2].volume);
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
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            DiskImage                 img;
            Disk2NibbleEngine         eng;
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

            for (Byte nibble : stream)
            {
                int   b = 0;

                for (b = 0; b < 8; b++)
                {
                    img.WriteBit (0, bitOffset + (size_t) b,
                                  (uint8_t) ((nibble >> (7 - b)) & 1));
                }

                bitOffset += 8;
            }

            eng.SetDiskImage (&img);
            eng.SetMotorOn   (true);

            // Pump the engine one bit at a time. After each bit
            // simulate the 6502's "LDA $C0EC / BPL" spin loop with
            // several ReadLatch calls, then route the watcher
            // through ConsumeFreshNibble exactly as
            // Disk2Controller::HandleReadDispatch does.
            for (bit = 0; bit < (int) (stream.size() * 8); bit++)
            {
                eng.Tick ((uint32_t) Disk2NibbleEngine::kCyclesPerBit);

                for (pollIndex = 0; pollIndex < 8; pollIndex++)
                {
                    sample = eng.ReadLatch();
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
            Assert::AreEqual (17, sink.dataLog[0].track);
            Assert::AreEqual (5,  sink.dataLog[0].sector);
            Assert::AreEqual (254, sink.dataLog[0].volume);
        }


        void FeedFill (Disk2AddressMarkWatcher & w, uint32_t count)
        {
            uint32_t  i = 0;

            for (i = 0; i < count; i++)
            {
                w.ObserveNibble (0xFF);
            }
        }


        void WriteAll (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles)
        {
            for (auto & nibble : nibbles)
            {
                w.ObserveWrittenNibble (nibble);
            }
        }


        //  RWTS reads the address field, then writes the data field: the
        //  write fires with that address. A format writes address fields,
        //  which fire nothing.
        TEST_METHOD (WrittenDataField_firesDataMarkWriteWithTheLastAddressRead)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      read;
            std::vector<uint8_t>      written;
            std::vector<uint8_t>      formatted;

            watcher.SetEventSink (&sink);

            AppendAddressMark (read, 254, 17, 7, 0, false);
            AppendDataMark    (written);
            AppendAddressMark (formatted, 254, 18, 0, 0, false);

            FeedAll  (watcher, read);
            WriteAll (watcher, written);
            WriteAll (watcher, formatted);

            Assert::AreEqual ((size_t) 1, sink.writeLog.size());
            Assert::AreEqual (17,  sink.writeLog[0].track);
            Assert::AreEqual (7,   sink.writeLog[0].sector);
            Assert::AreEqual (254, sink.writeLog[0].volume);
            Assert::AreEqual (256, sink.writeLog[0].byteCount);
            Assert::AreEqual ((size_t) 0, sink.dataLog.size(), L"a written field is not a read");
            Assert::AreEqual ((size_t) 1, sink.addrLog.size(), L"nor is a written address field");
        }


        //  Two revolutions of nibbles with no address mark fire one stall at
        //  the head's quarter track; it fires again only after an address
        //  mark or a step starts the count over.
        TEST_METHOD (ReadStall_firesOncePerStall)
        {
            constexpr uint32_t        kStall  = Disk2AddressMarkWatcher::kStallNibbles;
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            watcher.NoteHeadMoved (12);

            FeedFill (watcher, kStall - 1);
            Assert::AreEqual ((size_t) 0, sink.stallLog.size(), L"one nibble short");

            FeedFill (watcher, 1);
            Assert::AreEqual ((size_t) 1, sink.stallLog.size());
            Assert::AreEqual (12, sink.stallLog[0]);

            FeedFill (watcher, kStall * 2);
            Assert::AreEqual ((size_t) 1, sink.stallLog.size(), L"one stall, however long");

            AppendAddressMark (stream, 254, 3, 0, 0, false);
            FeedAll  (watcher, stream);
            FeedFill (watcher, kStall);
            Assert::AreEqual ((size_t) 2, sink.stallLog.size(), L"an address mark starts the count over");

            watcher.NoteHeadMoved (14);
            FeedFill (watcher, kStall);
            Assert::AreEqual ((size_t) 3, sink.stallLog.size(), L"so does a step");
            Assert::AreEqual (14, sink.stallLog[2]);
        }


        //  Saved partway through an address field, a data field being
        //  written and a stall count four nibbles from firing, a watcher
        //  loaded from the blob fires what the saved one fires on the same
        //  nibbles: the stall, then the address mark, then the write.
        TEST_METHOD (SavedState_resumesEveryMachine)
        {
            constexpr ptrdiff_t       kAddrSplit  = 6;
            constexpr ptrdiff_t       kDataSplit  = 100;
            Disk2AddressMarkWatcher   source;
            Disk2AddressMarkWatcher   target;
            RecordingSink             sourceSink;
            RecordingSink             targetSink;
            std::vector<uint8_t>      addr;
            std::vector<uint8_t>      data;
            std::vector<uint8_t>      other;
            StateWriter               writer;
            HRESULT                   hr          = S_OK;

            source.SetEventSink (&sourceSink);
            target.SetEventSink (&targetSink);

            AppendAddressMark (addr, 254, 9, 4, 0, false);
            AppendDataMark    (data);
            AppendAddressMark (other, 1, 30, 15, 0, false);

            // The source: an address mark read, a data field half written,
            // a step, the stall count ten short, then the first six nibbles
            // of the next address field.
            FeedAll  (source, addr);
            WriteAll (source, { data.begin(), data.begin() + kDataSplit });
            source.NoteHeadMoved (18);
            FeedFill (source, Disk2AddressMarkWatcher::kStallNibbles - 10);
            FeedAll  (source, { addr.begin(), addr.begin() + kAddrSplit });

            // The target: somewhere else entirely.
            FeedAll (target, other);

            hr = source.SaveState (writer);
            Assert::AreEqual (S_OK, hr);

            {
                StateReader  reader (writer.GetBytes());

                hr = target.LoadState (reader);
                Assert::AreEqual (S_OK, hr);
                Assert::IsTrue   (reader.IsAtEnd());
            }

            sourceSink.Clear();
            targetSink.Clear();

            for (auto * watcher : { &source, &target })
            {
                FeedAll  (*watcher, { addr.begin() + kAddrSplit, addr.end() });
                WriteAll (*watcher, { data.begin() + kDataSplit, data.end() });
            }

            Assert::AreEqual ((size_t) 1, sourceSink.stallLog.size(), L"the stall, four nibbles in");
            Assert::AreEqual ((size_t) 1, sourceSink.addrLog.size(),  L"the split address field");
            Assert::AreEqual ((size_t) 1, sourceSink.writeLog.size(), L"the split written field");
            Assert::AreEqual (sourceSink.addrLog.size(),  targetSink.addrLog.size());
            Assert::AreEqual (sourceSink.writeLog.size(), targetSink.writeLog.size());
            Assert::AreEqual (sourceSink.stallLog.size(), targetSink.stallLog.size());
            Assert::AreEqual (sourceSink.addrLog[0].sector,  targetSink.addrLog[0].sector);
            Assert::AreEqual (sourceSink.writeLog[0].sector, targetSink.writeLog[0].sector);
            Assert::AreEqual (18,                            targetSink.stallLog[0]);
            Assert::AreEqual (source.GetCachedTrack(),       target.GetCachedTrack());
        }
    };
}

