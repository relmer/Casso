#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Machines/Apple2/Common/AppleSoftSwitchBank.h"
#include "Machines/Apple2/Common/VideoScanner.h"
#include "Machines/Apple2/Common/VideoTiming.h"

#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  VideoScannerFloatingBusTests
//
//  A read that drives no data returns the byte the video scanner fetched on
//  that cycle (Sather, Understanding the Apple IIe, chapter 5). The parts
//  are checked against a screen whose every byte differs from its
//  neighbors, so a read at a known cycle can only match one address; then
//  a vapor-lock program, which polls the floating bus for a marker placed in
//  screen memory, must leave its loop on the marker's scanline.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (VideoScannerFloatingBusTests)
{
public:

    TEST_METHOD (UnmappedIoReadsTheByteBeingScanned)
    {
        Rig  rig;

        rig.Advance (100 * kCyclesPerLine + kFirstColumn + 7);

        Assert::AreEqual (rig.ram[0x0400 + 0x200 + 0x28 + 7], rig.bus.ReadByte (0xC0F0), L"text row 12, column 7");
    }


    TEST_METHOD (TheDisplaySwitchesReadTheByteFetchedBeforeTheyFlip)
    {
        Rig  rig;

        rig.switches.Read (0xC057);                     // hi-res, still text
        rig.Advance (kFirstColumn);

        Assert::AreEqual (rig.ram[0x0400], rig.switches.Read (0xC050), L"the text byte, fetched before graphics came on");
        Assert::AreEqual (rig.ram[0x2000], rig.switches.Read (0xC050), L"the hi-res byte once it is on");
    }


    TEST_METHOD (HiresAndBlankingReadTheirDocumentedBytes)
    {
        Rig  rig;

        rig.switches.Read (0xC050);
        rig.switches.Read (0xC057);

        rig.Advance (191 * kCyclesPerLine + kFirstColumn + 39);
        Assert::AreEqual (rig.ram[0x3FF7], rig.bus.ReadByte (0xC0F0), L"line 191, column 39");

        rig.Advance (kCyclesPerLine - (kFirstColumn + 39));
        Assert::AreEqual (rig.ram[0x2060], rig.bus.ReadByte (0xC0F0), L"line 192's first cycle, in both blankings");
    }


    TEST_METHOD (TheReadLandsOnItsCycleInTheInstruction)
    {
        Rig       rig;
        uint64_t  total    = 1000;
        uint64_t  busCycle = total + 3;                 // an absolute LDA reads on its fourth cycle

        rig.scanner.SetCycleCounters (&total, &busCycle);
        rig.Advance (kFirstColumn);

        Assert::AreEqual (rig.ram[0x0403], rig.bus.ReadByte (0xC0F0));
    }


    TEST_METHOD (WithNoScannerTheBusKeepsItsLastValue)
    {
        MemoryBus  bus;

        Assert::AreEqual ((Byte) 0xFF, bus.ReadByte (0xC0F0), L"the bare bus's power-on latch, unchanged");
    }


    TEST_METHOD (AVaporLockSyncsToTheMarkersScanline_IIe)
    {
        CheckVaporLock ("Apple2e");
    }


    TEST_METHOD (AVaporLockSyncsToTheMarkersScanline_IIPlus)
    {
        CheckVaporLock ("Apple2Plus");
    }


private:

    static constexpr uint32_t  kCyclesPerLine = 65;
    static constexpr uint32_t  kFirstColumn   = 25;

    struct Rig
    {
        std::vector<Byte>    ram = std::vector<Byte> (0x10000);
        VideoTiming          timing;
        AppleSoftSwitchBank  switches;
        VideoScanner         scanner;
        MemoryBus            bus;

        Rig()
        {
            for (size_t address = 0; address < ram.size(); address++)
            {
                ram[address] = static_cast<Byte> (address ^ (address >> 8) ^ (address >> 13));
            }

            scanner.SetTiming   (&timing);
            scanner.SetSwitches (&switches, nullptr);
            scanner.SetMainRam  (ram.data());
            switches.SetFloatingBusSource (&scanner);
            bus.SetFloatingBusSource      (&scanner);
        }

        void Advance (uint32_t cycles) { timing.Tick (cycles); }
    };


    //  Hi-res page 1 is cleared and the first ten bytes of line 100 set to
    //  $FF; nothing else the scanner fetches in a frame holds $FF. The
    //  program turns full-screen hi-res page 1 on and polls $C050 until it
    //  reads the marker:
    //
    //    $0300  LDA $C057 / LDA $C052 / LDA $C054
    //    $0309  LDA $C050 / CMP #$FF / BNE $0309
    //    $0310  JMP $0310
    //
    //  The marker is fetched on cycles 25-34 of line 100, and the loop's exit
    //  takes four more, so the program reaches $0310 on that line.
    static void CheckVaporLock (const std::string & machineId)
    {
        static constexpr Byte  s_kProgram[] =
        {
            0xAD, 0x57, 0xC0,  0xAD, 0x52, 0xC0,  0xAD, 0x54, 0xC0,
            0xAD, 0x50, 0xC0,  0xC9, 0xFF,  0xD0, 0xF9,
            0x4C, 0x10, 0x03,
        };

        constexpr Word      kOrigin      = 0x0300;
        constexpr Word      kSpin        = 0x0310;
        constexpr Word      kMarkerLine  = 0x2000 + 0x400 * (100 % 8) + 0x80 * ((100 / 8) % 8) + 0x28 * (100 / 64);
        constexpr int       kMarkerBytes = 10;
        constexpr int       kStepLimit   = 200000;
        constexpr uint32_t  kMarkerRow   = 100;

        TestMachine   machine (machineId, TestMachine::Slots::Empty);
        MemoryBus   & bus     = machine.GetMemoryBus();
        int           steps   = 0;



        // On the //e, a display switch has the MMU page main RAM in; until
        // one does, the build leaves the pages on the CPU's own array.
        bus.ReadByte (0xC054);

        for (Word address = 0x2000; address < 0x4000; address++)
        {
            bus.WriteByte (address, 0x00);
        }

        for (int index = 0; index < kMarkerBytes; index++)
        {
            bus.WriteByte (static_cast<Word> (kMarkerLine + index), 0xFF);
        }

        for (size_t index = 0; index < sizeof (s_kProgram); index++)
        {
            bus.WriteByte (static_cast<Word> (kOrigin + index), s_kProgram[index]);
        }

        // Start mid-frame, so the loop has to wait for the line.
        machine.GetVideoTiming()->Tick (150 * kCyclesPerLine);
        machine.GetCpu()->SetPC (kOrigin);

        while (machine.GetCpu()->GetPC() != kSpin && steps < kStepLimit)
        {
            machine.StepOne();
            steps++;
        }

        Assert::AreEqual (kSpin,      machine.GetCpu()->GetPC(),                        L"the loop saw the marker");
        Assert::AreEqual (kMarkerRow, machine.GetVideoTiming()->GetCurrentScanline(),   L"on the marker's scanline");
        Assert::IsTrue   (machine.GetVideoTiming()->GetHorizontalPos() < kFirstColumn + kMarkerBytes + 8, L"just after its bytes went by");
    }
};
