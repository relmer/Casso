#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/Disk/DiskFieldFormat.h"
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
//  Disk2AddressMarkWatcherTests
//
//  The address-mark watcher: recognizing sector fields in the nibble stream
//  as the guest reads them, decoding a data field's bytes and checking both
//  checksums, in either encoding.
//
//  It watches the READS the guest performs rather than decoding the track
//  itself, which is what lets the debug panel report which sector the boot ROM
//  is looking at. So the tests feed nibble sequences the way a read would
//  deliver them, one at a time, with the CPU's cycle where a gap matters.
//
//  Partial and interrupted fields are the substance: a field cut short must
//  be reported or dropped, never completed by the next stream's bytes.
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
                int                track;
                int                sector;
                int                volume;
                std::vector<Byte>  bytes;
            };

            std::vector<AddrEntry>  addrLog;
            std::vector<AddrEntry>  addrErrorLog;
            std::vector<DataEntry>  dataLog;
            std::vector<DataEntry>  dataErrorLog;
            std::vector<DataEntry>  orphanLog;
            std::vector<AddrEntry>  cutOffLog;
            std::vector<DataEntry>  writeLog;
            std::vector<int>        stallLog;

            void Clear()
            {
                *this = RecordingSink();
            }

            void OnMotorCommandOn() override                                  {}
            void OnMotorEngaged() override                                    {}
            void OnMotorCommandOff() override                                 {}
            void OnMotorDisengaged() override                                 {}
            void OnHeadStep (int, int) override                               {}
            void OnHeadBump (int) override                                    {}
            void OnAddressMark (int track, int sector, int volume) override   { addrLog.push_back ({ track, sector, volume }); }
            void OnDataMarkRead (int track, int sector, int volume, std::span<const Byte> bytes) override      { dataLog.push_back ({ track, sector, volume, { bytes.begin(), bytes.end() } }); }
            void OnDataMarkWrite (int track, int sector, int volume, std::span<const Byte> bytes) override     { writeLog.push_back ({ track, sector, volume, { bytes.begin(), bytes.end() } }); }
            void OnReadStall (int quarterTrack) override                      { stallLog.push_back (quarterTrack); }
            void OnAddressChecksumError (int track, int sector, int volume) override                          { addrErrorLog.push_back ({ track, sector, volume }); }
            void OnDataChecksumError (int track, int sector, int volume, std::span<const Byte> bytes) override { dataErrorLog.push_back ({ track, sector, volume, { bytes.begin(), bytes.end() } }); }
            void OnDataFieldCutOff (int track, int sector, int volume) override                               { cutOffLog.push_back ({ track, sector, volume }); }
            void OnDataWithoutAddress (std::span<const Byte> bytes) override                                  { orphanLog.push_back ({ -1, -1, -1, { bytes.begin(), bytes.end() } }); }
            void OnDriveSelect (int) override                                 {}
            void OnDiskInserted (int) override                                {}
            void OnDiskEjected (int) override                                 {}
        };



        ////////////////////////////////////////////////////////////////////////////
        //
        //  AppendAddressMark
        //
        //  Appends an address field (prologue, 4-and-4 fields, epilogue) to
        //  `out`, 16-sector or 13-sector. `chkOverride` lets a caller corrupt
        //  the checksum without disturbing the rest of the frame.
        //
        ////////////////////////////////////////////////////////////////////////////

        static void AppendAddressMark (std::vector<uint8_t> & out,
                                       uint8_t                vol,
                                       uint8_t                trk,
                                       uint8_t                sec,
                                       uint8_t                chkOverride,
                                       bool                   useOverride,
                                       DiskFieldKind          kind = DiskFieldKind::Sixteen)
        {
            uint8_t  chk  = useOverride ? chkOverride : (uint8_t) (vol ^ trk ^ sec);
            Byte     odd  = 0;
            Byte     even = 0;

            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue0);
            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue1);
            out.push_back ((kind == DiskFieldKind::Sixteen) ? Disk2AddressMarkWatcher::kAddrMarkPrologue2 : Disk2AddressMarkWatcher::kAddrMarkPrologue13);

            for (Byte value : { vol, trk, sec, chk })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                out.push_back (odd);
                out.push_back (even);
            }

            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue0);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue1);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue2);
        }



        ////////////////////////////////////////////////////////////////////////////
        //
        //  MakeSector
        //
        //  256 bytes that differ from one sector to the next.
        //
        ////////////////////////////////////////////////////////////////////////////

        static std::vector<Byte> MakeSector (Byte seed)
        {
            std::vector<Byte>  bytes (DiskFieldFormat::kSectorBytes);

            for (size_t i = 0; i < bytes.size(); i++)
            {
                bytes[i] = (Byte) (seed * 31 + i * 7);
            }

            return bytes;
        }



        ////////////////////////////////////////////////////////////////////////////
        //
        //  AppendDataMark
        //
        //  Appends a data field of `bytes`, in the kind's encoding: prologue,
        //  body, checksum nibble and epilogue.
        //
        ////////////////////////////////////////////////////////////////////////////

        static void AppendDataMark (std::vector<uint8_t> & out, const std::vector<Byte> & bytes, DiskFieldKind kind = DiskFieldKind::Sixteen)
        {
            std::vector<Byte>  body;
            Byte               checksum = 0;

            DiskFieldFormat::EncodeData (kind, bytes, body, checksum);

            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue0);
            out.push_back (Disk2AddressMarkWatcher::kAddrMarkPrologue1);
            out.push_back (Disk2AddressMarkWatcher::kDataMarkPrologue2);
            out.insert    (out.end(), body.begin(), body.end());
            out.push_back (checksum);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue0);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue1);
            out.push_back (Disk2AddressMarkWatcher::kSectorEpilogue2);
        }

        static void AppendDataMark (std::vector<uint8_t> & out)
        {
            AppendDataMark (out, MakeSector (1));
        }



        static void FeedAll (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles)
        {
            for (auto & nibble : nibbles)
            {
                w.ObserveNibble (nibble);
            }
        }

        //  Feeds each nibble a nibble time after the last, from `cycle`.
        static void FeedTimed (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles, uint64_t & cycle)
        {
            constexpr uint64_t  kNibbleCycles = 32;

            for (auto & nibble : nibbles)
            {
                cycle += kNibbleCycles;
                w.ObserveNibble (nibble, cycle);
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


        TEST_METHOD (CorruptChecksum_firesAnAddressChecksumError)
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
            Assert::AreEqual ((size_t) 1, sink.addrErrorLog.size());
            Assert::AreEqual (17, sink.addrErrorLog[0].track);
            Assert::AreEqual (5,  sink.addrErrorLog[0].sector);
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


        TEST_METHOD (DataMark_firesOnceWithItsBytes)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;
            std::vector<Byte>         bytes = MakeSector (7);

            watcher.SetEventSink (&sink);

            AppendAddressMark (stream, 254, 17, 7, 0, false);
            AppendDataMark    (stream, bytes);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (17,  sink.dataLog[0].track);
            Assert::AreEqual (7,   sink.dataLog[0].sector);
            Assert::AreEqual (254, sink.dataLog[0].volume);
            Assert::IsTrue   (sink.dataLog[0].bytes == bytes, L"the decoded bytes are the sector's");
        }


        //  A 13-sector field: D5 AA B5 address field and a 5-and-3 data field
        //  of 411 nibbles decode the same way.
        TEST_METHOD (ThirteenSectorFields_decode)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;
            std::vector<Byte>         bytes = MakeSector (3);

            watcher.SetEventSink (&sink);

            AppendAddressMark (stream, 254, 2, 12, 0, false, DiskFieldKind::Thirteen);
            AppendDataMark    (stream, bytes, DiskFieldKind::Thirteen);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.addrLog.size());
            Assert::AreEqual (12, sink.addrLog[0].sector);
            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (12, sink.dataLog[0].sector);
            Assert::IsTrue   (sink.dataLog[0].bytes == bytes);
        }


        //  A body nibble swapped for another valid one decodes with a bad
        //  checksum: the field is reported as a data checksum error, with
        //  the bytes as they decoded.
        TEST_METHOD (CorruptDataField_firesADataChecksumError)
        {
            constexpr size_t          kCorruptAt = 3 + 11 + 3 + 100;
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);

            AppendAddressMark (stream, 254, 17, 7, 0, false);
            AppendDataMark    (stream);

            stream[kCorruptAt] = (stream[kCorruptAt] == 0x96) ? 0x97 : 0x96;
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 0, sink.dataLog.size());
            Assert::AreEqual ((size_t) 1, sink.dataErrorLog.size());
            Assert::AreEqual (7,   sink.dataErrorLog[0].sector);
            Assert::AreEqual ((size_t) 256, sink.dataErrorLog[0].bytes.size());
        }


        TEST_METHOD (DataMarkAtTheStart_firesWithTheSectorUnknown)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;

            watcher.SetEventSink (&sink);
            AppendDataMark (stream);
            FeedAll (watcher, stream);

            // Nothing was read before it, so no address field is missing:
            // the read fires with track, sector and volume unknown, which
            // the UI formats as T? S? V?.
            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual (-1, sink.dataLog[0].track);
            Assert::AreEqual (-1, sink.dataLog[0].sector);
            Assert::AreEqual (-1, sink.dataLog[0].volume);
            Assert::AreEqual ((size_t) 0, sink.orphanLog.size());
        }


        //  Two data fields read in a row, with no address field between and
        //  no gap: the second has no address field before it.
        TEST_METHOD (SecondDataFieldInARow_firesWithoutAddress)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;
            std::vector<Byte>         bytes = MakeSector (9);

            watcher.SetEventSink (&sink);

            AppendAddressMark (stream, 254, 17, 1, 0, false);
            AppendDataMark    (stream);
            AppendDataMark    (stream, bytes);
            FeedAll (watcher, stream);

            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::AreEqual ((size_t) 1, sink.orphanLog.size());
            Assert::IsTrue   (sink.orphanLog[0].bytes == bytes);
        }


        //  A D5 inside the body, and an epilogue at a length neither encoding
        //  has, each cut the field off.
        TEST_METHOD (BrokenDataFields_fireCutOff)
        {
            constexpr size_t          kBodyAt = 3 + 11 + 3;
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      prologueInBody;
            std::vector<uint8_t>      shortBody;

            watcher.SetEventSink (&sink);

            AppendAddressMark (prologueInBody, 254, 17, 2, 0, false);
            AppendDataMark    (prologueInBody);
            prologueInBody[kBodyAt + 50] = Disk2AddressMarkWatcher::kAddrMarkPrologue0;

            AppendAddressMark (shortBody, 254, 17, 3, 0, false);
            AppendDataMark    (shortBody);
            shortBody[kBodyAt + 100] = Disk2AddressMarkWatcher::kSectorEpilogue0;
            shortBody[kBodyAt + 101] = Disk2AddressMarkWatcher::kSectorEpilogue1;

            FeedAll (watcher, prologueInBody);
            FeedAll (watcher, shortBody);

            Assert::AreEqual ((size_t) 2, sink.cutOffLog.size());
            Assert::AreEqual (2, sink.cutOffLog[0].sector);
            Assert::AreEqual (3, sink.cutOffLog[1].sector);
            Assert::AreEqual ((size_t) 0, sink.dataLog.size());
        }


        //  The CPU stopped reading partway through a field: the rest went by
        //  unseen, so the field is dropped without an event, and the next
        //  data field, with no address field read since, is not an orphan.
        TEST_METHOD (AGapDropsTheFieldWithoutAWarning)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      field;
            std::vector<uint8_t>      next;
            uint64_t                  cycle  = 1000;
            const size_t              kSplit = 200;

            watcher.SetEventSink (&sink);

            AppendAddressMark (field, 254, 17, 4, 0, false);
            AppendDataMark    (field);
            AppendDataMark    (next);

            FeedTimed (watcher, { field.begin(), field.begin() + kSplit }, cycle);
            cycle += 5000;
            FeedTimed (watcher, { field.begin() + kSplit, field.end() }, cycle);
            FeedTimed (watcher, next, cycle);

            Assert::AreEqual ((size_t) 0, sink.cutOffLog.size(), L"a gap is not a fault on the disk");
            Assert::AreEqual ((size_t) 0, sink.orphanLog.size());
            Assert::AreEqual ((size_t) 1, sink.dataLog.size(), L"only the field read whole");
            Assert::AreEqual (-1, sink.dataLog[0].sector, L"with its address field unknown");
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

            for (int i = 0; i < 3; i++)
            {
                Assert::AreEqual (i,   sink.addrLog[(size_t) i].sector);
                Assert::AreEqual (i,   sink.dataLog[(size_t) i].sector);
                Assert::AreEqual (17,  sink.dataLog[(size_t) i].track);
                Assert::AreEqual (254, sink.dataLog[(size_t) i].volume);
            }
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


        static void FeedFill (Disk2AddressMarkWatcher & w, uint32_t count)
        {
            uint32_t  i = 0;

            for (i = 0; i < count; i++)
            {
                w.ObserveNibble (0xFF);
            }
        }


        static void WriteAll (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles)
        {
            for (auto & nibble : nibbles)
            {
                w.ObserveWrittenNibble (nibble);
            }
        }


        //  RWTS reads the address field, then writes the data field: the
        //  write fires with that address and the bytes written. A format
        //  writes address fields, which fire nothing.
        TEST_METHOD (WrittenDataField_firesDataMarkWriteWithTheLastAddressRead)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      read;
            std::vector<uint8_t>      written;
            std::vector<uint8_t>      formatted;
            std::vector<Byte>         bytes = MakeSector (5);

            watcher.SetEventSink (&sink);

            AppendAddressMark (read, 254, 17, 7, 0, false);
            AppendDataMark    (written, bytes);
            AppendAddressMark (formatted, 254, 18, 0, 0, false);

            FeedAll  (watcher, read);
            WriteAll (watcher, written);
            WriteAll (watcher, formatted);

            Assert::AreEqual ((size_t) 1, sink.writeLog.size());
            Assert::AreEqual (17,  sink.writeLog[0].track);
            Assert::AreEqual (7,   sink.writeLog[0].sector);
            Assert::AreEqual (254, sink.writeLog[0].volume);
            Assert::IsTrue   (sink.writeLog[0].bytes == bytes);
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


        //  Saved partway through an address field, a data field being read,
        //  a data field being written and a stall count close to firing, a
        //  watcher loaded from the blob fires what the saved one fires on the
        //  same nibbles: the stall, the address mark, the read and the write.
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
            AppendDataMark    (data, MakeSector (4));
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
                FeedAll  (*watcher, data);
            }

            Assert::AreEqual ((size_t) 1, sourceSink.stallLog.size(), L"the stall, four nibbles in");
            Assert::AreEqual ((size_t) 1, sourceSink.addrLog.size(),  L"the split address field");
            Assert::AreEqual ((size_t) 1, sourceSink.writeLog.size(), L"the split written field");
            Assert::AreEqual ((size_t) 1, sourceSink.dataLog.size(),  L"the field read after it");
            Assert::AreEqual (sourceSink.addrLog.size(),  targetSink.addrLog.size());
            Assert::AreEqual (sourceSink.writeLog.size(), targetSink.writeLog.size());
            Assert::AreEqual (sourceSink.dataLog.size(),  targetSink.dataLog.size());
            Assert::AreEqual (sourceSink.stallLog.size(), targetSink.stallLog.size());
            Assert::AreEqual (sourceSink.addrLog[0].sector,  targetSink.addrLog[0].sector);
            Assert::IsTrue   (sourceSink.writeLog[0].bytes == targetSink.writeLog[0].bytes);
            Assert::AreEqual (sourceSink.dataLog[0].sector,  targetSink.dataLog[0].sector);
            Assert::AreEqual (18,                            targetSink.stallLog[0]);
            Assert::AreEqual (source.GetCachedTrack(),       target.GetCachedTrack());
        }


        //  The saved state is one size whatever each field's progress, so the
        //  keyframes of a turning disk are one size too and pack as
        //  differences, not as a whole snapshot each.
        TEST_METHOD (SavedState_isOneSizeWhateverTheFieldsProgress)
        {
            constexpr ptrdiff_t       kDataSplit = 100;
            Disk2AddressMarkWatcher   idle;
            Disk2AddressMarkWatcher   busy;
            std::vector<uint8_t>      addr;
            std::vector<uint8_t>      data;
            StateWriter               idleWriter;
            StateWriter               busyWriter;
            HRESULT                   hr         = S_OK;



            AppendAddressMark (addr, 254, 9, 4, 0, false);
            AppendDataMark    (data, MakeSector (4));

            FeedAll  (busy, addr);
            FeedAll  (busy, { data.begin(), data.begin() + kDataSplit });
            WriteAll (busy, { data.begin(), data.begin() + kDataSplit });

            hr = idle.SaveState (idleWriter);
            Assert::AreEqual (S_OK, hr);

            hr = busy.SaveState (busyWriter);
            Assert::AreEqual (S_OK, hr);

            Assert::AreEqual (idleWriter.GetBytes().size(), busyWriter.GetBytes().size(), L"a field half read and half written saves no more bytes than idle ones");
        }



        //  Feeds each nibble as read by the instruction at `instruction`.
        static void FeedBy (Disk2AddressMarkWatcher & w, const std::vector<uint8_t> & nibbles, Word instruction)
        {
            for (auto & nibble : nibbles)
            {
                w.ObserveNibble (nibble, 0, instruction);
            }
        }



        //  A sector a routine reads after the search found its address field
        //  is reported as its checksum nibble is read, so one read that stops
        //  there, as the boot ROM's and ProDOS's do, is reported too (GH #175).
        TEST_METHOD (ASectorReadByAnotherInstructionIsReportedAtItsChecksum)
        {
            constexpr Word            kSearch = 0xB94F;
            constexpr Word            kRead   = 0xB925;
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      address;
            std::vector<uint8_t>      data;



            watcher.SetEventSink (&sink);
            AppendAddressMark (address, 254, 17, 5, 0, false);
            AppendDataMark    (data, MakeSector (5));

            FeedBy (watcher, address, kSearch);
            FeedBy (watcher, { data.begin(), data.end() - 3 }, kRead);

            Assert::AreEqual ((size_t) 1, sink.dataLog.size(), L"reported at the checksum nibble, before any epilogue");
            Assert::AreEqual (5,          sink.dataLog[0].sector);
            Assert::IsTrue   (sink.dataLog[0].bytes == MakeSector (5));

            FeedBy (watcher, { data.end() - 3, data.end() }, kRead);
            Assert::AreEqual ((size_t) 1, sink.dataLog.size(), L"and not again at the epilogue");
        }



        //  An address search reads every nibble that passes the head with one
        //  instruction, the data fields of the sectors it passes over
        //  included; none of those is a sector read.
        TEST_METHOD (ASectorTheSearchPassesOverIsNotReported)
        {
            constexpr Word            kSearch = 0xB94F;
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      stream;



            watcher.SetEventSink (&sink);
            AppendAddressMark (stream, 254, 17, 5, 0, false);
            AppendDataMark    (stream, MakeSector (5));
            AppendAddressMark (stream, 254, 17, 6, 0, false);
            AppendDataMark    (stream, MakeSector (6));

            FeedBy (watcher, stream, kSearch);

            Assert::AreEqual ((size_t) 2, sink.addrLog.size(), L"both address fields");
            Assert::IsTrue   (sink.dataLog.empty(),           L"neither sector read");
            Assert::IsTrue   (sink.cutOffLog.empty() && sink.orphanLog.empty() && sink.dataErrorLog.empty());
        }



        //  A 13-sector address field sets the 5-and-3 length, so its sector
        //  is reported at the 411th nibble.
        TEST_METHOD (AThirteenSectorFieldIsReportedAtItsOwnChecksum)
        {
            Disk2AddressMarkWatcher   watcher;
            RecordingSink             sink;
            std::vector<uint8_t>      address;
            std::vector<uint8_t>      data;



            watcher.SetEventSink (&sink);
            AppendAddressMark (address, 254, 3, 9, 0, false, DiskFieldKind::Thirteen);
            AppendDataMark    (data, MakeSector (9), DiskFieldKind::Thirteen);

            FeedBy (watcher, address, 0x3D00);
            FeedBy (watcher, { data.begin(), data.end() - 4 }, 0x3E00);
            Assert::IsTrue   (sink.dataLog.empty(), L"not before the checksum nibble");

            FeedBy (watcher, { data.end() - 4, data.end() - 3 }, 0x3E00);
            Assert::AreEqual ((size_t) 1, sink.dataLog.size());
            Assert::IsTrue   (sink.dataLog[0].bytes == MakeSector (9));
        }
    };
}
