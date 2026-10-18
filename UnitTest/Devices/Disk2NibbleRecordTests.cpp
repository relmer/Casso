#include "Pch.h"
#include "Core/Prng.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleRecordTests
//
//  The recent nibbles: one entry per nibble the CPU takes from the latch,
//  with the address of the instruction that took it, and one per latch load
//  while the motor runs; the spin-up, dropped and new-disk flags; nothing
//  for a drive with no disk; and a power cycle starting it again.
//
////////////////////////////////////////////////////////////////////////////////

namespace Disk2NibbleRecordTests
{
    static constexpr Word    kMotorOn      = 0xC0E9;
    static constexpr Word    kQ6Off        = 0xC0EC;
    static constexpr Word    kQ6On         = 0xC0ED;
    static constexpr Word    kQ7Off        = 0xC0EE;
    static constexpr Word    kQ7On         = 0xC0EF;
    static constexpr Word    kSearch       = 0xB94F;    // RDADR16's latch read
    static constexpr Word    kRead         = 0xB925;    // READ16's
    static constexpr int     kSlot         = 6;
    static constexpr int     kDrive1       = 0;
    static constexpr int     kReadLoops    = 4000;
    static constexpr size_t  kTrackBits    = 4096;



    //  A controller reading a blank DOS 3.3 disk mounted from the store, with
    //  the instruction address a test sets.
    class Rig
    {
    public:
        DiskImageStore                     store;
        std::unique_ptr<Disk2Controller>   ctrl    = std::make_unique<Disk2Controller> (kSlot);
        DiskImage                        * image   = nullptr;
        Word                               address = 0;

        Rig()
        {
            std::vector<Byte>  blank (NibblizationLayer::kImageByteSize, 0);
            HRESULT            hr = S_OK;



            store.SetFlushSink ([] (const std::string &, const std::vector<Byte> &) { return S_OK; });

            hr = store.MountFromBytes (kSlot, kDrive1, "blank.dsk", DiskFormat::Dsk, blank);
            Assert::AreEqual (S_OK, hr);

            image = store.GetImage (kSlot, kDrive1);
            Assert::IsNotNull (image);

            ctrl->SetExternalDisk (kDrive1, image);
            ctrl->SetInstructionAddressSource (&address);
        }

        const Disk2NibbleRecord &  GetRecord() const { return ctrl->GetNibbleRecord(); }

        void StartReading (bool waitForSpinUp)
        {
            ctrl->Read (kMotorOn);
            ctrl->Tick (waitForSpinUp ? Disk2Controller::kMotorSpinupCycles : 0);
            ctrl->Read (kQ7Off);
            ctrl->Read (kQ6Off);
        }

        void StartWriting()
        {
            ctrl->Read (kMotorOn);
            ctrl->Tick (Disk2Controller::kMotorSpinupCycles);
            ctrl->Read (kQ6On);
            ctrl->Read (kQ7On);
        }

        //  A read of the latch at `at` after one bit cell, then the same read
        //  again with no time between; returns how many entries each added.
        void ReadTwice (Word at, uint64_t & outFirst, uint64_t & outSecond)
        {
            uint64_t  before = 0;



            address = at;
            ctrl->Tick (Disk2NibbleEngine::kCyclesPerBit);

            before = GetRecord().GetCount();
            ctrl->Read (kQ6Off);
            outFirst = GetRecord().GetCount() - before;

            before = GetRecord().GetCount();
            ctrl->Read (kQ6Off);
            outSecond = GetRecord().GetCount() - before;
        }
    };



    TEST_CLASS (Disk2NibbleRecordTests)
    {
    public:

        //  A loop reading the latch at two addresses, one bit cell apart, so
        //  every nibble is read while fresh: each assembled nibble has an entry
        //  once, with the address of the read that took it, and reading the
        //  same latch again adds nothing.
        TEST_METHOD (EachNibbleTakenIsKeptOnceWithItsInstruction)
        {
            Rig       rig;
            uint64_t  assembled = 0;
            uint64_t  added     = 0;
            uint64_t  first     = 0;
            uint64_t  second    = 0;
            size_t    wrong     = 0;
            size_t    repeats   = 0;
            Word      at        = 0;



            rig.StartReading (true);
            assembled = rig.ctrl->GetEngine (kDrive1).GetReadNibbles();

            for (int i = 0; i < kReadLoops; i++)
            {
                at = (i & 1) ? kRead : kSearch;
                rig.ReadTwice (at, first, second);

                added    += first;
                repeats += second;
                wrong   += (first != 0 && rig.GetRecord().GetEntry (rig.GetRecord().GetCount() - 1).address != at) ? 1 : 0;
            }

            assembled = rig.ctrl->GetEngine (kDrive1).GetReadNibbles() - assembled;

            Assert::IsTrue   (added > 100, L"the loop read nibbles");
            Assert::AreEqual (assembled, added,        L"one entry per nibble assembled");
            Assert::AreEqual ((size_t) 0, repeats,     L"a second read of the same latch adds nothing");
            Assert::AreEqual ((size_t) 0, wrong,       L"each entry has the address of the read that took it");
            Assert::AreEqual ((Byte) 0, (Byte) (rig.GetRecord().GetEntry (rig.GetRecord().GetCount() - 1).flags & Disk2NibbleRecord::kSpinUp),
                              L"no spin-up flag once the motor is at speed");
        }

        //  With the latch read once every two nibbles' time, some nibbles the
        //  drive assembled go untaken: each entry after one is flagged.
        TEST_METHOD (ANibbleAfterAMissedOneIsFlagged)
        {
            constexpr uint32_t  kTwoNibbles = Disk2NibbleEngine::kCyclesPerBit * 8 * 2 + 4;
            Rig                 rig;
            size_t              added       = 0;
            size_t              flagged     = 0;
            uint64_t            before      = 0;



            rig.StartReading (true);
            rig.ctrl->Read (kQ6Off);

            for (int i = 0; i < kReadLoops / 10; i++)
            {
                rig.ctrl->Tick (kTwoNibbles);
                before = rig.GetRecord().GetCount();
                rig.ctrl->Read (kQ6Off);

                added   += (size_t) (rig.GetRecord().GetCount() - before);
                flagged += (rig.GetRecord().GetCount() != before && (rig.GetRecord().GetEntry (rig.GetRecord().GetCount() - 1).flags & Disk2NibbleRecord::kAfterGap) != 0) ? 1 : 0;
            }

            Assert::IsTrue   (added > 100, L"nibbles were taken");
            Assert::AreEqual (added, flagged, L"every one of them after a missed one");
        }

        TEST_METHOD (ANibbleDuringTheSpinUpIsFlagged)
        {
            Rig       rig;
            uint64_t  first   = 0;
            uint64_t  second  = 0;
            size_t    flagged = 0;
            size_t    added   = 0;



            rig.StartReading (false);

            for (uint32_t cycles = 0; cycles + Disk2NibbleEngine::kCyclesPerBit < Disk2Controller::kMotorSpinupCycles; cycles += Disk2NibbleEngine::kCyclesPerBit)
            {
                rig.ReadTwice (kSearch, first, second);

                added    += (size_t) first;
                flagged += (first != 0 && (rig.GetRecord().GetEntry (rig.GetRecord().GetCount() - 1).flags & Disk2NibbleRecord::kSpinUp) != 0) ? 1 : 0;
            }

            Assert::IsTrue   (added > 0, L"nibbles passed during the spin-up");
            Assert::AreEqual (added, flagged, L"every one of them is flagged");
        }

        TEST_METHOD (ALatchLoadIsKeptAndFlaggedWhenDropped)
        {
            for (bool isProtected : { false, true })
            {
                Rig   rig;
                Byte  expected = static_cast<Byte> (Disk2NibbleRecord::kWritten | (isProtected ? Disk2NibbleRecord::kDropped : 0));



                rig.image->SetWriteProtected (isProtected);
                rig.StartWriting();

                rig.address = 0xB82A;
                rig.ctrl->Write (kQ6On, 0xD5);

                Assert::AreEqual ((uint64_t) 1,   rig.GetRecord().GetCount());
                Assert::AreEqual ((Byte) 0xD5,    rig.GetRecord().GetEntry (0).nibble);
                Assert::AreEqual ((Word) 0xB82A,  rig.GetRecord().GetEntry (0).address);
                Assert::AreEqual (expected,       (Byte) (rig.GetRecord().GetEntry (0).flags & ~Disk2NibbleRecord::kNewDisk));
            }
        }

        TEST_METHOD (ADriveWithNoDiskAddsNothing)
        {
            auto      ctrl  = std::make_unique<Disk2Controller> (kSlot);



            ctrl->GetDisk (kDrive1)->ResizeTrack (0, kTrackBits);
            ctrl->Read (kMotorOn);
            ctrl->Tick (Disk2Controller::kMotorSpinupCycles);

            for (int i = 0; i < kReadLoops; i++)
            {
                ctrl->Tick (Disk2NibbleEngine::kCyclesPerBit);
                ctrl->Read (kQ6Off);
            }

            Assert::AreEqual ((uint64_t) 0, ctrl->GetNibbleRecord().GetCount());
        }

        TEST_METHOD (TheFirstEntryAfterADiskChangeIsFlagged)
        {
            Rig       rig;
            uint64_t  first  = 0;
            uint64_t  second = 0;



            rig.StartReading (true);

            while (rig.GetRecord().GetCount() < 2)
            {
                rig.ReadTwice (kSearch, first, second);
            }

            Assert::AreNotEqual ((Byte) 0, (Byte) (rig.GetRecord().GetEntry (0).flags & Disk2NibbleRecord::kNewDisk), L"the first entry after the mount");
            Assert::AreEqual    ((Byte) 0, (Byte) (rig.GetRecord().GetEntry (1).flags & Disk2NibbleRecord::kNewDisk), L"and not the next");

            rig.ctrl->SetExternalDisk (kDrive1, rig.image);

            while (rig.GetRecord().GetCount() < 3)
            {
                rig.ReadTwice (kSearch, first, second);
            }

            Assert::AreNotEqual ((Byte) 0, (Byte) (rig.GetRecord().GetEntry (2).flags & Disk2NibbleRecord::kNewDisk), L"the first entry after a swap");
        }

        TEST_METHOD (APowerCycleStartsTheRecordAgain)
        {
            Rig       rig;
            Prng      prng (1);
            uint64_t  first  = 0;
            uint64_t  second = 0;



            rig.StartReading (true);

            while (rig.GetRecord().GetCount() == 0)
            {
                rig.ReadTwice (kSearch, first, second);
            }

            rig.ctrl->PowerCycle (prng);

            Assert::AreEqual ((uint64_t) 0, rig.GetRecord().GetCount());
            Assert::AreEqual ((size_t) 0,   rig.GetRecord().GetSize());
        }

        TEST_METHOD (TheNewestEntriesAreKept)
        {
            constexpr int      kAdded = (int) Disk2NibbleRecord::kCapacity + 100;
            Disk2NibbleRecord  record;
            uint64_t           index  = 0;



            for (int i = 0; i < kAdded; i++)
            {
                record.Add (i & 1, (Byte) i, 0, (Word) i);
            }

            Assert::AreEqual ((uint64_t) kAdded, record.GetCount());
            Assert::AreEqual (Disk2NibbleRecord::kCapacity, record.GetSize());

            for (index = record.GetCount() - record.GetSize(); index < record.GetCount(); index++)
            {
                Assert::AreEqual ((Word) index, record.GetEntry (index).address);
                Assert::AreEqual ((Byte) ((index & 1) ? Disk2NibbleRecord::kDrive2 : 0), record.GetEntry (index).flags);
            }
        }
    };
}
