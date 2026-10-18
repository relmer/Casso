#include "Pch.h"

#include "Debugger/DiskFieldTracker.h"
#include "Devices/Disk/DiskFieldFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFieldTrackerTests
//
//  The disk breakpoints' tracker, fed entries built here: 16-
//  and 13-sector fields, failed checksums, the read test in DOS 3.3's and
//  the boot ROM's patterns, custom and wildcard marks, a disk change and the
//  spin-up cutting a field, a missed checksum nibble, a written prologue,
//  and priming partway through a field.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    //  The instructions of DOS 3.3's RWTS and the Disk II boot ROM that read
    //  the latch (research R-041).
    static constexpr Word  kRdadr16       = 0xB94F;    // RDADR16's search
    static constexpr Word  kRead16Mark    = 0xB8E1;    // READ16's data prologue search
    static constexpr Word  kRead16Body    = 0xB8F0;
    static constexpr Word  kRead16Sum     = 0xB925;
    static constexpr Word  kRomSearch     = 0xC65E;
    static constexpr Word  kRomBody       = 0xC6AA;
    static constexpr Word  kRomSum        = 0xC6CB;
    static constexpr Word  kWriter        = 0xB82A;
    static constexpr Byte  kVolume        = 0xFE;
    static constexpr int   kSyncNibbles   = 6;



    //  Who reads each part of a sector.
    struct Readers
    {
        Word  address;      // the address field, its prologue included
        Word  dataMark;     // the data prologue
        Word  body;
        Word  checksum;
    };

    static constexpr Readers  s_kSearch = { kRdadr16,   kRdadr16,    kRdadr16,    kRdadr16    };   // a sector passed over
    static constexpr Readers  s_kRead   = { kRdadr16,   kRead16Mark, kRead16Body, kRead16Sum  };   // the sector RWTS was called for
    static constexpr Readers  s_kRom    = { kRomSearch, kRomSearch,  kRomBody,    kRomSum     };   // the boot ROM's read



    //  Entries collected in order, which a test may change before they are
    //  added to a record.
    class RecordBuilder
    {
    public:
        std::vector<Disk2NibbleRecord::Entry>  entries;

        void Read (Byte nibble, Word pc, Byte flags = 0)            { entries.push_back ({ nibble, flags, pc }); }
        void Write (Byte nibble, Word pc)                           { entries.push_back ({ nibble, Disk2NibbleRecord::kWritten, pc }); }

        void Read (std::initializer_list<Byte> nibbles, Word pc)
        {
            for (Byte nibble : nibbles)
            {
                Read (nibble, pc);
            }
        }

        static std::array<Byte, 256> MakeBytes (Byte seed)
        {
            std::array<Byte, 256>  bytes = {};



            for (size_t i = 0; i < bytes.size(); i++)
            {
                bytes[i] = static_cast<Byte> (seed + i * 7);
            }

            return bytes;
        }

        //  The address field: prologue, 4-and-4 volume, track, sector and
        //  checksum, epilogue.
        void AddressField (DiskFieldKind kind, Byte track, Byte sector, Word pc, std::optional<Byte> checksum = std::nullopt)
        {
            DiskMarkPattern  prologue = DiskFieldFormat::GetAddressPrologue (kind);
            Byte             odd      = 0;
            Byte             even     = 0;



            for (int i = 0; i < prologue.GetLength(); i++)
            {
                Read (prologue.GetValue (i), pc);
            }

            for (Byte value : { kVolume, track, sector, checksum.value_or (DiskFieldFormat::ComputeAddressChecksum (kVolume, track, sector)) })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                Read ({ odd, even }, pc);
            }

            Read ({ 0xDE, 0xAA, 0xEB }, pc);
        }

        //  A whole sector, sync before each field, with each part read by
        //  its reader. A body nibble may be replaced, to break its checksum.
        void Sector (DiskFieldKind kind, Byte track, Byte sector, const Readers & readers, std::optional<size_t> spoilAt = std::nullopt)
        {
            std::array<Byte, 256>  bytes    = MakeBytes (static_cast<Byte> (track * 16 + sector));
            std::vector<Byte>      body;
            Byte                   checksum = 0;



            DiskFieldFormat::EncodeData (kind, bytes, body, checksum);

            if (spoilAt.has_value())
            {
                body[*spoilAt] = (body[*spoilAt] == 0x96) ? 0x97 : 0x96;
            }

            for (int i = 0; i < kSyncNibbles; i++)
            {
                Read (0xFF, readers.address);
            }

            AddressField (kind, track, sector, readers.address);

            for (int i = 0; i < kSyncNibbles; i++)
            {
                Read (0xFF, readers.dataMark);
            }

            Read ({ 0xD5, 0xAA, 0xAD }, readers.dataMark);

            for (Byte nibble : body)
            {
                Read (nibble, readers.body);
            }

            Read (checksum, readers.checksum);
            Read ({ 0xDE, 0xAA, 0xEB }, readers.checksum);
        }
    };



    TEST_CLASS (DiskFieldTrackerTests)
    {
    public:

        //  The builder's entries added one at a time, the tracker fed after
        //  each, as the controller and the session do.
        static std::vector<DiskFieldEvent> FeedAll (DiskFieldTracker & tracker, const RecordBuilder & builder)
        {
            Disk2NibbleRecord            record;
            std::vector<DiskFieldEvent>  events;



            for (const Disk2NibbleRecord::Entry & entry : builder.entries)
            {
                record.Add (0, entry.nibble, entry.flags, entry.address);
                tracker.Feed (record, events);
            }

            return events;
        }

        static size_t CountOf (const std::vector<DiskFieldEvent> & events, DiskFieldEventKind kind)
        {
            return (size_t) std::count_if (events.begin(), events.end(), [kind] (const DiskFieldEvent & e) { return e.kind == kind; });
        }

        static const DiskFieldEvent & Last (const std::vector<DiskFieldEvent> & events, DiskFieldEventKind kind)
        {
            auto  found = std::find_if (events.rbegin(), events.rend(), [kind] (const DiskFieldEvent & e) { return e.kind == kind; });



            Assert::IsTrue (found != events.rend(), L"an event of the kind");
            return *found;
        }

        TEST_METHOD (ASixteenSectorSectorReadByRwts)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.Sector (DiskFieldKind::Sixteen, 3, 5, s_kRead);

            auto          events  = FeedAll (tracker, b);
            const auto  & address = Last (events, DiskFieldEventKind::AddressField);
            const auto  & data    = Last (events, DiskFieldEventKind::DataChecksum);

            Assert::AreEqual ((Byte) 3,       address.track);
            Assert::AreEqual ((Byte) 5,       address.sector);
            Assert::AreEqual (kVolume,        address.volume);
            Assert::AreEqual (address.expectedChecksum, address.storedChecksum);
            Assert::AreEqual (kRdadr16,       address.addressInstruction);
            Assert::IsTrue   (DiskFieldKind::Sixteen == data.format);
            Assert::IsTrue   (data.isDataChecksumGood, L"a good data checksum");
            Assert::AreEqual (kRead16Mark,    data.dataInstruction);
            Assert::AreEqual (kRead16Sum,     data.lastInstruction);
            Assert::IsFalse  (data.isPassed,  L"read: the checksum nibble's reader is not the address prologue's");
        }

        TEST_METHOD (AThirteenSectorSectorReadBySeparateRoutines)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.Sector (DiskFieldKind::Thirteen, 7, 12, s_kRead);

            auto          events = FeedAll (tracker, b);
            const auto  & data   = Last (events, DiskFieldEventKind::DataChecksum);

            Assert::IsTrue   (DiskFieldKind::Thirteen == data.format);
            Assert::AreEqual ((Byte) 7,  data.track);
            Assert::AreEqual ((Byte) 12, data.sector);
            Assert::IsTrue   (data.isDataChecksumGood);
            Assert::IsFalse  (data.isPassed);
        }

        TEST_METHOD (FailedAddressAndDataChecksumsWithTheirValues)
        {
            RecordBuilder     bad;
            RecordBuilder     spoiled;
            DiskFieldTracker  first;
            DiskFieldTracker  second;



            bad.AddressField (DiskFieldKind::Sixteen, 3, 5, kRdadr16, (Byte) 0x00);
            spoiled.Sector (DiskFieldKind::Sixteen, 3, 5, s_kRead, (size_t) 100);

            auto  address = Last (FeedAll (first,  bad),     DiskFieldEventKind::AddressField);
            auto  data    = Last (FeedAll (second, spoiled), DiskFieldEventKind::DataChecksum);

            Assert::AreEqual ((Byte) 0x00, address.storedChecksum);
            Assert::AreEqual (DiskFieldFormat::ComputeAddressChecksum (kVolume, 3, 5), address.expectedChecksum);
            Assert::IsFalse  (data.isDataChecksumGood, L"the data checksum fails");
            Assert::AreNotEqual (data.storedDataChecksum, data.expectedDataChecksum);
        }

        //  DOS 3.3: RDADR16 reads every sector that passes, data fields and
        //  all, until it finds the one RWTS was called for, which READ16 reads.
        TEST_METHOD (DosPassesOverSectorsAndReadsTheOneItWants)
        {
            RecordBuilder                b;
            DiskFieldTracker             tracker;
            std::vector<DiskFieldEvent>  data;



            b.Sector (DiskFieldKind::Sixteen, 3, 1, s_kSearch);
            b.Sector (DiskFieldKind::Sixteen, 3, 2, s_kSearch);
            b.Sector (DiskFieldKind::Sixteen, 3, 3, s_kRead);

            for (const DiskFieldEvent & event : FeedAll (tracker, b))
            {
                if (event.kind == DiskFieldEventKind::DataChecksum)
                {
                    data.push_back (event);
                }
            }

            Assert::AreEqual ((size_t) 3, data.size());
            Assert::IsTrue   (data[0].isPassed && data[1].isPassed, L"the sectors passed over");
            Assert::IsFalse  (data[2].isPassed,                     L"the sector read");
            Assert::AreEqual ((Byte) 3, data[2].sector);
        }

        //  The boot ROM reads both prologues' first nibble with one
        //  instruction and the body and checksum with others: a read.
        TEST_METHOD (TheBootRomsReadCountsAsARead)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.Sector (DiskFieldKind::Sixteen, 0, 0, s_kRom);

            auto  data = Last (FeedAll (tracker, b), DiskFieldEventKind::DataChecksum);

            Assert::AreEqual (kRomSearch, data.addressInstruction);
            Assert::AreEqual (kRomSearch, data.dataInstruction);
            Assert::AreEqual (kRomSum,    data.lastInstruction);
            Assert::IsFalse  (data.isPassed);
        }

        //  The ROM checks no address checksum and can miss its first nibble:
        //  the field is reported with the checksum unread.
        //  One instruction reading the whole data field, prologue to
        //  checksum, and another the address field: a read, since the
        //  test compares the checksum nibble's reader with the address
        //  prologue's.
        TEST_METHOD (ADataFieldReadByOneInstructionIsARead)
        {
            constexpr Readers  kOneLoop = { 0x0A10, 0x0A40, 0x0A40, 0x0A40 };
            RecordBuilder      b;
            DiskFieldTracker   tracker;



            b.Sector (DiskFieldKind::Sixteen, 1, 8, kOneLoop);

            Assert::IsFalse (Last (FeedAll (tracker, b), DiskFieldEventKind::DataChecksum).isPassed);
        }

        TEST_METHOD (AMissedChecksumNibbleLeavesTheChecksumUnread)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;
            Byte              odd  = 0;
            Byte              even = 0;



            b.Read ({ 0xD5, 0xAA, 0x96 }, kRomSearch);

            for (Byte value : { kVolume, (Byte) 0, (Byte) 4 })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                b.Read ({ odd, even }, kRomSearch);
            }

            DiskFieldFormat::Encode44 (DiskFieldFormat::ComputeAddressChecksum (kVolume, 0, 4), odd, even);
            b.Read (even, kRomSearch, Disk2NibbleRecord::kAfterGap);
            b.Read ({ 0xDE, 0xAA }, kRomSearch);

            auto  address = Last (FeedAll (tracker, b), DiskFieldEventKind::AddressField);

            Assert::AreEqual ((Byte) 4, address.sector);
            Assert::IsFalse  (address.isChecksumRead);
            Assert::AreEqual (address.expectedChecksum, address.storedChecksum, L"an unread checksum is taken as good");
        }

        TEST_METHOD (CustomMarksReplaceTheStandardOnes)
        {
            RecordBuilder             b;
            DiskFieldTracker          standard;
            DiskFieldTracker::Marks   marks;
            std::string               error;
            Byte                      odd  = 0;
            Byte                      even = 0;



            Assert::IsTrue (DiskMarkPattern::TryParse ("D4 AA 96", *(marks.address = DiskMarkPattern()), error));

            DiskFieldTracker  custom (marks);

            b.Read ({ 0xD4, 0xAA, 0x96 }, kRdadr16);

            for (Byte value : { kVolume, (Byte) 1, (Byte) 2, DiskFieldFormat::ComputeAddressChecksum (kVolume, 1, 2) })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                b.Read ({ odd, even }, kRdadr16);
            }

            Assert::AreEqual ((size_t) 0, CountOf (FeedAll (standard, b), DiskFieldEventKind::AddressField), L"not under the standard marks");
            Assert::AreEqual ((size_t) 1, CountOf (FeedAll (custom,   b), DiskFieldEventKind::AddressField), L"under D4 AA 96");
        }

        TEST_METHOD (WildcardMarks)
        {
            RecordBuilder             b;
            DiskFieldTracker::Marks   marks;
            std::string               error;
            Byte                      odd  = 0;
            Byte                      even = 0;



            Assert::IsTrue (DiskMarkPattern::TryParse ("D5 AA ??", *(marks.address = DiskMarkPattern()), error));

            DiskFieldTracker  custom (marks);

            b.Read ({ 0xD5, 0xAA, 0x97 }, kRdadr16);

            for (Byte value : { kVolume, (Byte) 1, (Byte) 2, DiskFieldFormat::ComputeAddressChecksum (kVolume, 1, 2) })
            {
                DiskFieldFormat::Encode44 (value, odd, even);
                b.Read ({ odd, even }, kRdadr16);
            }

            Assert::AreEqual ((size_t) 1, CountOf (FeedAll (custom, b), DiskFieldEventKind::AddressField));
        }

        TEST_METHOD (ADiskChangeCutsAField)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.Sector (DiskFieldKind::Sixteen, 3, 5, s_kRead);
            b.entries[b.entries.size() - 100].flags |= Disk2NibbleRecord::kNewDisk;

            auto  events = FeedAll (tracker, b);

            Assert::AreEqual ((size_t) 1, CountOf (events, DiskFieldEventKind::AddressField));
            Assert::AreEqual ((size_t) 0, CountOf (events, DiskFieldEventKind::DataChecksum), L"no data field across the change");
        }

        TEST_METHOD (ANibbleDuringTheSpinUpCutsItsField)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.Sector (DiskFieldKind::Sixteen, 3, 5, s_kRead);
            b.entries[b.entries.size() - 50].flags |= Disk2NibbleRecord::kSpinUp;

            auto  events = FeedAll (tracker, b);

            Assert::AreEqual ((size_t) 1, CountOf (events, DiskFieldEventKind::AddressField));
            Assert::AreEqual ((size_t) 0, CountOf (events, DiskFieldEventKind::DataChecksum));
        }

        TEST_METHOD (APrologueWrittenAfterAnAddressField)
        {
            RecordBuilder     b;
            DiskFieldTracker  tracker;



            b.AddressField (DiskFieldKind::Sixteen, 9, 4, kRdadr16);

            for (Byte nibble : { (Byte) 0xFF, (Byte) 0xFF, (Byte) 0xD5, (Byte) 0xAA, (Byte) 0xAD })
            {
                b.Write (nibble, kWriter);
            }

            auto  written = Last (FeedAll (tracker, b), DiskFieldEventKind::WritePrologue);

            Assert::AreEqual ((Byte) 9,  written.track);
            Assert::AreEqual ((Byte) 4,  written.sector);
            Assert::AreEqual (kWriter,   written.lastInstruction);
        }

        //  A tracker primed from a record that ends partway through a data
        //  field reports the same events after it as one fed every nibble,
        //  one at a time.
        TEST_METHOD (PrimingPartwayThroughAFieldGivesTheSameEvents)
        {
            constexpr size_t             kBeforeEnd = 200;     // inside sector 2's data field
            RecordBuilder                b;
            DiskFieldTracker             fed;
            DiskFieldTracker             primed;
            Disk2NibbleRecord            record;
            std::vector<DiskFieldEvent>  before;
            std::vector<DiskFieldEvent>  fedEvents;
            std::vector<DiskFieldEvent>  primedEvents;
            size_t                       cut        = 0;



            b.Sector (DiskFieldKind::Sixteen, 3, 1, s_kSearch);
            b.Sector (DiskFieldKind::Sixteen, 3, 2, s_kRead);
            cut = b.entries.size() - kBeforeEnd;

            for (size_t i = 0; i < b.entries.size(); i++)
            {
                record.Add (0, b.entries[i].nibble, b.entries[i].flags, b.entries[i].address);
                fed.Feed (record, (i < cut) ? before : fedEvents);

                if (i + 1 == cut)
                {
                    primed.Prime (record);
                }

                if (i >= cut)
                {
                    primed.Feed (record, primedEvents);
                }
            }

            Assert::AreEqual ((size_t) 1, CountOf (primedEvents, DiskFieldEventKind::DataChecksum), L"sector 2's data field, primed partway through");
            Assert::AreEqual (fedEvents.size(), primedEvents.size());

            for (size_t i = 0; i < fedEvents.size(); i++)
            {
                Assert::IsTrue   (fedEvents[i].kind == primedEvents[i].kind);
                Assert::AreEqual (fedEvents[i].sector,             primedEvents[i].sector);
                Assert::AreEqual (fedEvents[i].isPassed,           primedEvents[i].isPassed);
                Assert::AreEqual (fedEvents[i].expectedDataChecksum, primedEvents[i].expectedDataChecksum);
            }
        }    };
}
