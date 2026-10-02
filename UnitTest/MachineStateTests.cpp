#include "Pch.h"

#include "Core/Cpu65C02.h"
#include "Core/EmuCpu.h"
#include "Core/InterruptController.h"
#include "Core/MemoryBus.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStateTests
//
//  Save and load of the CPU, the interrupt controller and the bus. Each round
//  trip sets every saved field to a distinct nonzero value, saves, loads into
//  a freshly built object, and compares every field, so a field missing from
//  either side fails here. The version and tag checks prove a blob from
//  another part, or from a newer build, is refused.
//
////////////////////////////////////////////////////////////////////////////////

namespace MachineState
{
    static constexpr HRESULT  kInvalidData = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    static constexpr HRESULT  kRevision    = HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH);

    // Offset of a section's version field.
    static constexpr size_t   kVersionOffset = sizeof (uint32_t);


    ////////////////////////////////////////////////////////////////////////////
    //
    //  StateProbeCpu
    //
    //  A 65C02 with its protected state opened up: Fill gives every saved
    //  field a value derived from a seed, and AssertSameState compares every
    //  saved field against another CPU. The bools take the seed's low four
    //  bits, so a source and target filled from seeds whose low nibbles are
    //  complements differ in every field, and a field the load skips keeps
    //  the target's value and fails the compare.
    //
    ////////////////////////////////////////////////////////////////////////////

    class StateProbeCpu : public Cpu65C02
    {
    public:
        explicit StateProbeCpu (MemoryBus & bus) : Cpu65C02 (bus) {}

        void Fill (Byte seed)
        {
            constexpr uint64_t  kCycleBase = 0x0000123456789A00ull;
            constexpr Word      kPcBase    = 0x8000;
            size_t              i          = 0;



            PC               = static_cast<Word> (kPcBase + seed);
            SP               = static_cast<Byte> (seed + 1);
            A                = static_cast<Byte> (seed + 2);
            X                = static_cast<Byte> (seed + 3);
            Y                = static_cast<Byte> (seed + 4);
            status.status    = static_cast<Byte> (seed + 5);
            m_irqLine        = (seed & 0x01) != 0;
            m_nmiLine        = (seed & 0x02) != 0;
            m_nmiPending     = (seed & 0x04) != 0;
            m_totalCycles    = kCycleBase + seed;
            m_busCycle       = kCycleBase + seed + 6;
            m_lastCycles     = static_cast<Byte> (seed + 7);
            m_lastPenalties  = static_cast<Byte> (seed + 8);
            m_lastBranchFrom = static_cast<Word> (kPcBase + seed + 9);
            m_hasLastBranch  = (seed & 0x08) != 0;

            for (i = 0; i < kStateRamSize; i++)
            {
                memory[i] = static_cast<Byte> (seed + i * 7);
            }
        }

        void AssertSameState (const StateProbeCpu & other) const
        {
            int  ramDiff = memcmp (memory.data(), other.memory.data(), kStateRamSize);



            Assert::AreEqual (PC,               other.PC,               L"PC");
            Assert::AreEqual (SP,               other.SP,               L"SP");
            Assert::AreEqual (A,                other.A,                L"A");
            Assert::AreEqual (X,                other.X,                L"X");
            Assert::AreEqual (Y,                other.Y,                L"Y");
            Assert::AreEqual (status.status,    other.status.status,    L"P");
            Assert::AreEqual (m_irqLine,        other.m_irqLine,        L"IRQ line");
            Assert::AreEqual (m_nmiLine,        other.m_nmiLine,        L"NMI line");
            Assert::AreEqual (m_nmiPending,     other.m_nmiPending,     L"NMI pending");
            Assert::AreEqual (m_totalCycles,    other.m_totalCycles,    L"total cycles");
            Assert::AreEqual (m_busCycle,       other.m_busCycle,       L"bus cycle");
            Assert::AreEqual (m_lastCycles,     other.m_lastCycles,     L"last cycles");
            Assert::AreEqual (m_lastPenalties,  other.m_lastPenalties,  L"last penalties");
            Assert::AreEqual (m_lastBranchFrom, other.m_lastBranchFrom, L"last branch from");
            Assert::AreEqual (m_hasLastBranch,  other.m_hasLastBranch,  L"has last branch");
            Assert::AreEqual (0,                ramDiff,                L"RAM $0000-$BFFF");
        }
    };


    static std::vector<Byte> SaveCpu (const StateProbeCpu & cpu)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = cpu.SaveState (writer);
        Assert::AreEqual (S_OK, hr);

        return writer.GetBytes();
    }


    // Loads a blob into a part; on success the part must have consumed it all.
    static HRESULT LoadFrom (IMachineState & target, const std::vector<Byte> & bytes)
    {
        StateReader  reader (bytes);
        HRESULT      hr = S_OK;



        hr = target.LoadState (reader);

        if (SUCCEEDED (hr))
        {
            Assert::IsTrue (reader.IsAtEnd());
        }

        return hr;
    }


    TEST_CLASS (MachineStateTests)
    {
    public:
        TEST_METHOD (CpuRoundTripsEveryField)
        {
            constexpr Byte     kSeed       = 0x35;
            constexpr Byte     kTargetSeed = 0xCA;
            auto               sourceBus   = std::make_unique<MemoryBus>();   // heap: the page tables overflow the frame (C6262)
            auto               targetBus   = std::make_unique<MemoryBus>();
            StateProbeCpu      source (*sourceBus);
            StateProbeCpu      target (*targetBus);
            std::vector<Byte>  bytes;
            HRESULT            hr          = S_OK;



            source.Fill (kSeed);
            target.Fill (kTargetSeed);
            bytes = SaveCpu (source);

            hr = LoadFrom (target, bytes);
            Assert::AreEqual (S_OK, hr);

            source.AssertSameState (target);
        }


        TEST_METHOD (CpuRoundTripsThroughEmuCpu)
        {
            constexpr Byte     kSeed       = 0x5A;
            constexpr Byte     kTargetSeed = 0xA5;
            auto               sourceBus   = std::make_unique<MemoryBus>();   // heap: the page tables overflow the frame (C6262)
            auto               targetBus   = std::make_unique<MemoryBus>();
            auto               owned       = std::make_unique<StateProbeCpu> (*sourceBus);
            StateProbeCpu    * source      = owned.get();
            EmuCpu             sourceEmu (*sourceBus, std::move (owned));
            auto               loaded      = std::make_unique<StateProbeCpu> (*targetBus);
            StateProbeCpu    * target      = loaded.get();
            EmuCpu             targetEmu (*targetBus, std::move (loaded));
            StateWriter        writer;
            HRESULT            hr          = S_OK;



            source->Fill (kSeed);
            target->Fill (kTargetSeed);

            hr = sourceEmu.SaveState (writer);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (targetEmu, writer.GetBytes());
            Assert::AreEqual (S_OK, hr);

            source->AssertSameState (*target);
        }


        TEST_METHOD (CpuNewerVersionFails)
        {
            MemoryBus          bus;
            StateProbeCpu      cpu (bus);
            std::vector<Byte>  bytes   = SaveCpu (cpu);
            StateReader        reader (bytes);
            HRESULT            hr      = S_OK;



            bytes[kVersionOffset] = static_cast<Byte> (MemoryBusCpu::kStateVersion + 1);

            hr = cpu.LoadState (reader);
            Assert::AreEqual (kRevision, hr);
        }


        TEST_METHOD (CpuShortRamFails)
        {
            MemoryBus          bus;
            StateProbeCpu      cpu (bus);
            std::vector<Byte>  bytes   = SaveCpu (cpu);
            StateReader        reader (bytes.data(), bytes.size() - 1);
            HRESULT            hr      = S_OK;



            hr = cpu.LoadState (reader);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (CpuStateIntoBusFails)
        {
            MemoryBus          bus;
            StateProbeCpu      cpu (bus);
            std::vector<Byte>  bytes   = SaveCpu (cpu);
            StateReader        reader (bytes);
            HRESULT            hr      = S_OK;



            hr = bus.LoadState (reader);
            Assert::AreEqual (kInvalidData, hr);
        }


        TEST_METHOD (BusRoundTripsFloatingBusValue)
        {
            constexpr Byte     kFloating = 0x5A;
            StateWriter        handMade;
            StateWriter        resaved;
            MemoryBus          bus;
            HRESULT            hr        = S_OK;



            handMade.BeginSection (MemoryBus::kStateTag, MemoryBus::kStateVersion);
            handMade.WriteByte    (kFloating);

            hr = handMade.EndSection();
            Assert::AreEqual (S_OK, hr);

            bus.ClearVideoDirty();

            hr = LoadFrom (bus, handMade.GetBytes());
            Assert::AreEqual (S_OK,      hr);
            Assert::AreEqual (kFloating, bus.GetFloatingBusValue());
            Assert::IsTrue   (bus.IsVideoDirty());

            hr = bus.SaveState (resaved);
            Assert::AreEqual (S_OK, hr);
            Assert::IsTrue   (handMade.GetBytes() == resaved.GetBytes());
        }


        TEST_METHOD (InterruptControllerRoundTripsAssertedSources)
        {
            constexpr uint32_t   kAsserted     = 0x5;
            constexpr int        kSources      = 3;
            InterruptController  source;
            InterruptController  target;
            StateWriter          writer;
            IrqSourceId          ids[kSources] = {};
            int                  i             = 0;
            HRESULT              hr            = S_OK;



            for (i = 0; i < kSources; i++)
            {
                hr = source.RegisterSource (ids[i]);
                Assert::AreEqual (S_OK, hr);
                hr = target.RegisterSource (ids[i]);
                Assert::AreEqual (S_OK, hr);
            }

            source.Assert (ids[0]);
            source.Assert (ids[2]);

            hr = source.SaveState (writer);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (target, writer.GetBytes());
            Assert::AreEqual (S_OK,      hr);
            Assert::AreEqual (kAsserted, target.GetAssertedSources());
            Assert::IsTrue   (target.IsAnyAsserted());
        }


        TEST_METHOD (InterruptControllerSourceCountMismatchFails)
        {
            InterruptController  source;
            InterruptController  target;
            StateWriter          writer;
            IrqSourceId          id = {};
            HRESULT              hr = S_OK;



            hr = source.RegisterSource (id);
            Assert::AreEqual (S_OK, hr);
            hr = source.RegisterSource (id);
            Assert::AreEqual (S_OK, hr);
            hr = target.RegisterSource (id);
            Assert::AreEqual (S_OK, hr);

            hr = source.SaveState (writer);
            Assert::AreEqual (S_OK, hr);

            hr = LoadFrom (target, writer.GetBytes());
            Assert::AreEqual (kInvalidData, hr);
        }
    };
}
