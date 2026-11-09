#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/FixtureProvider.h"
#include "../EmuTests/TestMachine.h"
#include "CassoLink/CassoLinkHost.h"
#include "CassoLink/LinkProgram.h"
#include "Core/MemoryDevice.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Seams/InProcessByteStream.h"
#include "InMemoryCassoLinkFolder.h"
#include "SteppedByteStream.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriverTimingTests
//
//  The program's Disk II driver against the timing measured on a real //c
//  (specs/033-casso-explorer/real-2c-findings.md). The program runs on an
//  emulated //c and the host on the test's thread, through a
//  SteppedByteStream. A proxy in front of the disk controller logs each
//  access to its soft switches with the CPU's cycle count, and the
//  assertions run on that log. The count is the cycle the access's
//  instruction starts at, so the spacing of two stores of one kind is exact.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskDriverTimingTests)
{
public:

    static constexpr Word      kQ6H         = 0xC08D;   // the data latch, indexed by the slot
    static constexpr Word      kQ7L         = 0xC08E;
    static constexpr Word      kQ7H         = 0xC08F;
    static constexpr Word      kSlotOffset  = 0x60;     // slot 6
    static constexpr uint64_t  kBitCell     = 4;
    static constexpr uint64_t  kDataSpacing = 32;
    static constexpr uint64_t  kSyncSpacing = 40;
    static constexpr uint64_t  kNibbleTime  = 32;
    static constexpr Byte      kTrack       = 30;
    static constexpr Byte      kVolume      = 254;

    struct Access
    {
        uint64_t  cycle;
        Word      pc;
        Word      address;
        bool      write;
        Byte      value;
    };

    //  In front of the disk controller on the bus: each access logged, then
    //  passed on.
    class SwitchLog : public MemoryDevice
    {
    public:
        SwitchLog (MemoryDevice & inner, EmuCpu & cpu) : m_inner (inner), m_cpu (cpu) {}

        Byte  Read (Word address) override
        {
            Byte  value = m_inner.Read (address);

            accesses.push_back ({ m_cpu.GetCycleCount(), m_cpu.GetPC(), address, false, value });
            return value;
        }

        void  Write (Word address, Byte value) override
        {
            accesses.push_back ({ m_cpu.GetCycleCount(), m_cpu.GetPC(), address, true, value });
            m_inner.Write (address, value);
        }

        Word  GetStart   () const override       { return m_inner.GetStart(); }
        Word  GetEnd     () const override       { return m_inner.GetEnd(); }
        void  Reset      () override             { m_inner.Reset(); }
        void  SoftReset  () override             { m_inner.SoftReset(); }
        void  PowerCycle (Prng & prng) override  { m_inner.PowerCycle (prng); }

        std::vector<Access>  accesses;

    private:
        MemoryDevice  & m_inner;
        EmuCpu        & m_cpu;
    };

    struct Rig
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        InMemoryCassoLinkFolder               folder;
        std::unique_ptr<CassoLinkHost>        host;
        std::unique_ptr<SwitchLog>            log;

        Rig()
        {
            FixtureProvider       fixtures;
            std::vector<uint8_t>  bytes;
            std::vector<Byte>     dos;
            std::vector<Byte>     shim;
            std::string           errors;
            Word                  at = LinkProgram::kShimAddress;
            MemoryDevice        * disk = machine.GetRefs().diskController;



            AssertSucceeded (fixtures.OpenFixture ("CassoExplorer/dos33.dsk", bytes));
            dos.assign (bytes.begin(), bytes.end());

            machine.SetVideoTiming (std::make_unique<VideoTiming>());
            machine.GetCpu()->SetVideoTiming (machine.GetVideoTiming());
            machine.PowerCycle();

            AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "", DiskFormat::Dsk, dos));

            Assert::IsNotNull (disk, L"the //c's disk controller");
            log = std::make_unique<SwitchLog> (*disk, *machine.GetCpu());
            machine.GetMemoryBus().RemoveDevice (disk);
            machine.GetMemoryBus().AddDevice (log.get());

            InProcessByteStream::CreatePair (machineEnd, pcEnd);
            Assert::AreEqual (S_OK, machine.AttachSerialStream (2, machineEnd));

            host = std::make_unique<CassoLinkHost> (std::make_shared<SteppedByteStream> (machine, pcEnd), folder, [] () { return std::array<Byte, 4> {}; });

            AssertSucceeded (LinkProgram::AssembleShim (shim, errors));

            for (Byte b : shim)
            {
                machine.GetCpu()->WriteByte (at++, b);
            }

            machine.GetCpu()->SetPC (LinkProgram::kShimAddress);
        }

        //  The stores to the data latch from the first write mode to the next
        //  read mode, and the access that selected read mode.
        void  CollectStream (std::vector<Access> & outStores, Access & outReadMode)
        {
            bool  writing = false;



            outStores.clear();
            outReadMode = {};

            for (const Access & a : log->accesses)
            {
                if (a.write && a.address == kQ7H + kSlotOffset)
                {
                    writing = true;
                }
                else if (writing && a.write && a.address == kQ6H + kSlotOffset)
                {
                    outStores.push_back (a);
                }
                else if (writing && !a.write && a.address == kQ7L + kSlotOffset)
                {
                    outReadMode = a;
                    return;
                }
            }
        }
    };

    static std::vector<Byte>  Pattern (size_t size, Byte seed)
    {
        std::vector<Byte>  bytes (size);



        for (size_t i = 0; i < size; i++)
        {
            bytes[i] = (Byte) (seed + i * 13 + (i >> 8));
        }

        return bytes;
    }

    //  Each store a whole number of bit cells after the first, and 32 or 40
    //  cycles after the store before it: 40 only to a self-sync byte, which
    //  is $FF. A data nibble can also be $FF, so a store of $FF can be at
    //  either spacing, and any other is at 32.
    static void  AssertSpacing (const std::vector<Access> & stores)
    {
        Assert::IsTrue (stores.size() > 2, L"a stream was written");

        for (size_t i = 1; i < stores.size(); i++)
        {
            uint64_t      gap  = stores[i].cycle - stores[i - 1].cycle;
            bool          sync = stores[i].value == 0xFF;
            std::wstring  what = std::format (L"store {} (${:02X}), {} cycles after store {} (${:02X})", i, stores[i].value, gap, i - 1, stores[i - 1].value);

            Assert::AreEqual<uint64_t> (0, (stores[i].cycle - stores[0].cycle) % kBitCell, (what + L": partway through a bit cell").c_str());
            Assert::IsTrue (gap == kDataSpacing || (sync && gap == kSyncSpacing), what.c_str());
        }
    }



    //  Findings items 21 and 29: 32 cycles between data bytes, 40 to and
    //  between self-sync bytes, every store on a bit cell, and no extra time
    //  where the stream crosses a page.
    TEST_METHOD (WriteTrack_EveryStore_IsOnABitCell_AtTheSpacingAnIwmTakes)
    {
        Rig                  rig;
        std::vector<Access>  stores;
        Access               readMode;
        CassoLinkStatus      status;



        AssertSucceeded (rig.host->WriteTrack (1, kTrack, kVolume, Pattern (16 * 256, 7), status));
        Assert::AreEqual<int> (CassoLinkHost::kErrorNone, status.error);

        rig.CollectStream (stores, readMode);
        Assert::IsTrue (stores.size() > 2 * 256, L"the stream crosses pages");
        AssertSpacing (stores);
    }



    //  Findings item 1: an indexed store reads its address in the cycle
    //  before it writes it, which starts the controller's write loop, and an
    //  absolute store writes nothing on the //c. The emulated 65C02 doesn't
    //  make that read on the bus, so each store's instruction is checked: a
    //  store's PC is past its three bytes, and STA abs,X is $9D.
    TEST_METHOD (WriteTrack_EveryStoreToTheDrive_IsIndexed)
    {
        static constexpr Byte  kStaAbsX = 0x9D;

        Rig              rig;
        CassoLinkStatus  status;
        size_t           stores = 0;



        AssertSucceeded (rig.host->WriteTrack (1, kTrack, kVolume, Pattern (16 * 256, 9), status));

        for (const Access & a : rig.log->accesses)
        {
            if (a.write)
            {
                Word  at     = (Word) (a.pc - 3);
                Byte  opcode = rig.machine.GetCpu()->ReadByte (at);

                stores++;
                Assert::AreEqual<int> (kStaAbsX, opcode, std::format (L"the store to ${:04X} at ${:04X}", a.address, at).c_str());
            }
        }

        Assert::IsTrue (stores > 0);
    }



    //  Findings item 2: the //c's IWM writes a stored byte only after the one
    //  before it, so read mode comes after two self-sync bytes and the time
    //  to write both.
    TEST_METHOD (WriteTrack_ReadMode_ComesAfterTheLastTwoNibblesAreWritten)
    {
        Rig                  rig;
        std::vector<Access>  stores;
        Access               readMode;
        CassoLinkStatus      status;



        AssertSucceeded (rig.host->WriteTrack (1, kTrack, kVolume, Pattern (16 * 256, 11), status));

        rig.CollectStream (stores, readMode);
        Assert::IsTrue (stores.size() >= 2);
        Assert::AreEqual<int> (0xFF, stores[stores.size() - 1].value, L"the last store is a self-sync byte");
        Assert::AreEqual<int> (0xFF, stores[stores.size() - 2].value, L"and the one before it");
        Assert::IsTrue (readMode.cycle >= stores.back().cycle + 2 * kNibbleTime,
                        std::format (L"read mode {} cycles after the last store", readMode.cycle - stores.back().cycle).c_str());
    }



    //  Findings items 3 and 27: drives measured at 204.67 ms with gap 27 and
    //  201.08 ms with gap 21. The format stream is 768 self-sync bytes of
    //  first gap and gaps, less the 16 gaps, then the 16 sectors; it takes
    //  longer than a revolution, so its end lands in the first gap. At each
    //  measured gap, the 16 sectors take less than that drive's revolution,
    //  and the stream's end lands before sector 0's address field.
    //
    //  The emulated drive turns at exactly 300 rpm, 200.69 ms, so a stream for
    //  a slower drive overlaps sector 0 there and the format's read-back
    //  fails. Only the stream's timing is checked.
    TEST_METHOD (FormatTrack_AtEachMeasuredGap_FitsThatDrivesRevolution)
    {
        struct Drive { double revolutionMs; Byte gap; };

        static constexpr Drive   kDrives[]    = { { 204.67, 27 }, { 201.08, 21 } };
        static constexpr size_t  kClosingSync = 2;     // the two self-sync bytes after the stream

        for (const Drive & drive : kDrives)
        {
            Rig                  rig;
            std::vector<Access>  stores;
            Access               readMode;
            CassoLinkStatus      status;
            size_t               firstGap   = 768 - 16 * (size_t) drive.gap;
            uint64_t             revolution = (uint64_t) (drive.revolutionMs * CassoLinkHost::kCpuCyclesPerMs);
            uint64_t             sectors    = 0;
            uint64_t             stream     = 0;



            Assert::AreEqual<int> (drive.gap, CassoLinkHost::ComputeFormatGap (drive.revolutionMs), L"the gap for that drive");

            AssertSucceeded (rig.host->FormatTrack (1, kTrack, kVolume, status, drive.gap));

            rig.CollectStream (stores, readMode);
            AssertSpacing (stores);
            Assert::IsTrue (stores.size() > firstGap + kClosingSync);

            sectors = stores[stores.size() - kClosingSync].cycle - stores[firstGap].cycle;
            stream  = stores[stores.size() - kClosingSync].cycle - stores.front().cycle;

            Assert::IsTrue (sectors < revolution, std::format (L"gap {}: the sectors take {} cycles of a revolution of {}", drive.gap, sectors, revolution).c_str());
            Assert::IsTrue (stream > revolution,  std::format (L"gap {}: the stream takes {} cycles, less than a revolution, so its end isn't in the first gap", drive.gap, stream).c_str());
        }
    }
};
