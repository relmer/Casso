#include "Pch.h"

#include "Core/Prng.h"
#include "Devices/RamDevice.h"
#include "Devices/RomDevice.h"
#include "Shell/MachineHost.h"
#include "TestMachine.h"
#include "TextScreenScraper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


//
//  A machine small enough to reason about: RAM over the whole user space,
//  ROM over the top page carrying a reset vector and one instruction at it.
//
static constexpr Word  s_kRamEnd      = 0xBFFF;
static constexpr Word  s_kRomStart    = 0xFF00;
static constexpr Word  s_kRomEnd      = 0xFFFF;
static constexpr Word  s_kResetVector = 0xFFFC;
static constexpr Word  s_kEntryPoint  = 0xFF00;

//  NOP, then INX. Two instructions of different lengths and costs, so a
//  step that ran two of them cannot look like a step that ran one.
static constexpr Byte  s_kNop = 0xEA;
static constexpr Byte  s_kInx = 0xE8;

static constexpr uint64_t  s_kSeed = 0xCA550031ULL;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHostLifecycleTests
//
//  Reset, power cycle and single-step, asserted against the production
//  lifecycle rather than a copy of it in the test tree.
//
//  These three have been described in the audit and in comments for a long
//  time and never asserted anywhere, because the code implementing them sat
//  on a class that needs an HINSTANCE, an HWND, a swap chain and an audio
//  endpoint to exist. It does not need those to run a machine, and this is
//  the evidence.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MachineHostLifecycleTests)
{
public:

    TEST_METHOD (SoftResetPreservesUserRamAndTakesTheResetVector)
    {
        MachineHost  host;

        Build (host);

        host.GetMemoryBus().WriteByte (0x1234, 0xAB);
        host.GetMemoryBus().WriteByte (0x5678, 0xCD);

        host.SoftReset();

        Assert::AreEqual<Byte> (0xAB, host.GetMemoryBus().ReadByte (0x1234),
            L"a soft reset must not disturb user RAM");
        Assert::AreEqual<Byte> (0xCD, host.GetMemoryBus().ReadByte (0x5678));

        Assert::AreEqual<Word> (s_kEntryPoint, host.GetCpu()->GetPC(),
            L"and must leave the CPU at the address the reset vector names");
    }


    TEST_METHOD (APowerCycleReseedsRamAndASoftResetDoesNot)
    {
        MachineHost  host;
        Byte         beforeCycle[16] = {};
        bool         anyChanged      = false;
        Word         addr            = 0;
        int          i               = 0;

        Build (host);

        //  A known pattern, so "re-seeded" means something other than
        //  "happened to already hold this".
        for (i = 0; i < 16; i++)
        {
            host.GetMemoryBus().WriteByte (static_cast<Word> (0x2000 + i), 0x5A);
        }

        host.SoftReset();

        for (i = 0; i < 16; i++)
        {
            addr = static_cast<Word> (0x2000 + i);
            Assert::AreEqual<Byte> (0x5A, host.GetMemoryBus().ReadByte (addr),
                L"a soft reset leaves DRAM alone");
            beforeCycle[i] = host.GetMemoryBus().ReadByte (addr);
        }

        host.PowerCycle();

        for (i = 0; i < 16; i++)
        {
            addr = static_cast<Word> (0x2000 + i);
            if (host.GetMemoryBus().ReadByte (addr) != beforeCycle[i])
            {
                anyChanged = true;
            }
        }

        //  Sixteen bytes of a pinned-seed reseed matching 0x5A everywhere is
        //  a 1-in-2^128 event, so this distinguishes the two resets rather
        //  than merely observing one of them.
        Assert::IsTrue (anyChanged,
            L"a power cycle re-seeds DRAM, which is what makes it not a soft reset");
    }


    //  Whatever the fill left, a power cycle must not leave the power-up byte
    //  valid, or the autostart ROM takes a warm reset through a vector that
    //  was never set up.
    TEST_METHOD (APowerCycleInvalidatesThePowerUpByte)
    {
        MachineHost  host;
        Byte         entryHi = 0;
        Byte         powerUp = 0;



        Build (host);

        host.PowerCycle();

        entryHi = host.GetMemoryBus().ReadByte (0x03F3);
        powerUp = host.GetMemoryBus().ReadByte (0x03F4);

        Assert::AreNotEqual<Byte> (static_cast<Byte> (entryHi ^ 0xA5), powerUp,
            L"the power-up byte must fail the autostart ROM's warm-reset check");
        Assert::AreEqual<Byte> (0, host.GetMemoryBus().ReadByte (0x03F2));
        Assert::AreEqual<Byte> (0, entryHi);
        Assert::AreEqual<Byte> (0, powerUp);
    }


    //  The monitor's random seed is read before anything writes it when a
    //  disk autostarts, and a zero seed hangs Pooyan.
    TEST_METHOD (APowerCycleLeavesTheRandomSeedNonzero)
    {
        MachineHost  host;
        Byte         seedLo = 0;
        Byte         seedHi = 0;



        Build (host);

        host.PowerCycle();

        seedLo = host.GetMemoryBus().ReadByte (0x004E);
        seedHi = host.GetMemoryBus().ReadByte (0x004F);

        Assert::AreNotEqual<Byte> (0, seedLo, L"$4E must be nonzero after a power cycle");
        Assert::AreNotEqual<Byte> (0, seedHi, L"$4F must be nonzero after a power cycle");
    }


    //  GH #157, end to end. Under the uniform random fill this seed left the
    //  power-up byte valid, and a //e Enhanced jumped into garbage with the
    //  power-on screen never cleared. A cold boot must reach the banner.
    TEST_METHOD (AColdBootReachesTheBannerUnderAFormerlyHangingSeed)
    {
        constexpr uint64_t  kFormerlyHangingSeed = 0x46305DE659C7976DULL;
        constexpr uint64_t  kBootCycles          = 1'500'000ULL;

        TestMachine               machine ("Apple2eEnhanced");
        std::vector<std::string>  rows;



        machine.SetPrng (std::make_unique<Prng> (kFormerlyHangingSeed));
        machine.PowerCycle();
        machine.RunCycles (kBootCycles);

        rows = TextScreenScraper::Scrape (machine);

        Assert::IsTrue (rows[0].find ("Apple //e") != std::string::npos,
            std::format (L"row 0 must show the banner, got '{}'",
                         std::wstring (rows[0].begin(), rows[0].end())).c_str());
    }


    TEST_METHOD (SteppingRetiresExactlyOneInstruction)
    {
        MachineHost  host;
        Word         pc     = 0;
        Byte         cycles = 0;

        Build (host);
        host.SoftReset();

        pc = host.GetCpu()->GetPC();
        Assert::AreEqual<Word> (s_kEntryPoint, pc);

        cycles = host.StepOne();

        Assert::AreEqual<Word> (static_cast<Word> (s_kEntryPoint + 1), host.GetCpu()->GetPC(),
            L"NOP is one byte, so the program counter advances by one");
        Assert::AreEqual<Byte> (2, cycles,
            L"and costs two cycles");

        cycles = host.StepOne();

        Assert::AreEqual<Word> (static_cast<Word> (s_kEntryPoint + 2), host.GetCpu()->GetPC(),
            L"INX is also one byte");
        Assert::AreEqual<Byte> (2, cycles);
    }


    TEST_METHOD (SteppingAdvancesTheMachineClock)
    {
        MachineHost  host;
        uint64_t     before = 0;

        Build (host);
        host.SoftReset();

        before = host.GetCpu()->GetTotalCycles();

        host.StepOne();

        //  The clock is what the //e video timing and the //c mouse measure
        //  themselves against. A step that ticked the disk controller but
        //  left this standing is the asymmetry that consolidating the run
        //  loop and the single step removed.
        Assert::AreEqual<uint64_t> (before + 2, host.GetCpu()->GetTotalCycles(),
            L"a stepped instruction costs the machine the time it took");
    }


    TEST_METHOD (RunCyclesSpendsAtLeastItsBudget)
    {
        MachineHost  host;
        uint64_t     spent = 0;

        Build (host);
        host.SoftReset();

        spent = host.RunCycles (100);

        //  The machine cannot stop mid-opcode, so the budget is a floor and
        //  the overshoot is bounded by the longest instruction. Callers
        //  pacing against a frame carry the remainder rather than expecting
        //  an exact count.
        Assert::IsTrue (spent >= 100, L"the budget is spent in full");
        Assert::IsTrue (spent < 100 + 8, L"and overshot by at most one instruction");

        Assert::AreEqual<uint64_t> (spent, host.GetCpu()->GetTotalCycles(),
            L"every cycle it reports is a cycle the clock took");
    }


    TEST_METHOD (AMachineWithNoCpuIsInertRatherThanFatal)
    {
        MachineHost  host;

        //  Nothing built. Every lifecycle entry point is reachable from the
        //  menu before a machine finishes coming up, so each has to answer.
        Assert::AreEqual<Byte> (0, host.StepOne());
        Assert::AreEqual<uint64_t> (0, host.RunCycles (100));

        host.SoftReset();
        host.PowerCycle();
    }


private:

    //  RAM, ROM, a reset vector pointing at two one-byte instructions, and a
    //  CPU seeded from a pinned Prng so two runs agree.
    static void Build (MachineHost & host)
    {
        std::vector<Byte>  rom (s_kRomEnd - s_kRomStart + 1, s_kNop);

        rom[s_kEntryPoint - s_kRomStart]     = s_kNop;
        rom[s_kEntryPoint + 1 - s_kRomStart] = s_kInx;

        rom[s_kResetVector - s_kRomStart]     = static_cast<Byte> (s_kEntryPoint & 0xFF);
        rom[s_kResetVector + 1 - s_kRomStart] = static_cast<Byte> (s_kEntryPoint >> 8);

        host.SetPrng (std::make_unique<Prng> (s_kSeed));

        auto  ram    = std::make_unique<RamDevice> (0x0000, s_kRamEnd);
        auto  romDev = std::make_unique<RomDevice> (s_kRomStart, s_kRomEnd, std::move (rom));

        host.GetMemoryBus().AddDevice (ram.get());
        host.GetMemoryBus().AddDevice (romDev.get());

        host.GetOwnedDevices().push_back (std::move (ram));
        host.GetOwnedDevices().push_back (std::move (romDev));

        host.SetCpu (std::make_unique<EmuCpu> (host.GetMemoryBus()));
        host.GetCpu()->InitForEmulation (*host.GetPrng());
    }
};
