#include "Pch.h"

#include "../EhmTestHelper.h"
#include "IigsTestCpu.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ShimSccTests
//
//  The shim's 8530 routines on a IIgs's ports, on a bare CPU: the assembled
//  shim at $3A00 in flat memory, a fake 8530 at $C036 to $C03B, and the
//  bytes the shim checks for a IIgs. Those are the enhanced //e's
//  identification bytes, an ID call at $FE1F that clears the C flag, and a
//  port's firmware in slots 1 and 2 with the Pascal signature, the Super
//  Serial Card's ID byte and no 6551. Port 1 is the 8530's channel A and
//  port 2 its channel B.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ShimSccTests)
{
public:

    static constexpr Word      kShim       = LinkProgram::kShimAddress;
    static constexpr Word      kSlot       = kShim + 0x38;
    static constexpr Word      kMode       = kShim + 0x39;
    static constexpr Word      kStatus     = kShim + 0x3A;
    static constexpr Word      kRead       = kShim + 0x3D;
    static constexpr Word      kWrite      = kShim + 0x40;
    static constexpr Word      kSetSpeed   = kShim + 0x44;
    static constexpr Word      kStub       = 0x0300;
    static constexpr Byte      kIigsPort   = 0xC0;
    static constexpr Byte      kFirmware   = 0x80;
    static constexpr Byte      k9600       = 0x1E;
    static constexpr uint32_t  kStartSteps = 2'000'000;
    static constexpr uint32_t  kCallSteps  = 100'000;



    //  The shim on a IIgs, or with iigs false on an enhanced //e, whose ID
    //  call is an RTS, for the port in `slot`.
    static void  Load (IigsTestCpu & cpu, Byte slot, bool iigs = true)
    {
        std::vector<Byte>  shim;
        std::string        errors;



        AssertSucceeded (LinkProgram::AssembleShim (shim, errors));

        cpu.InitForTest (kShim);

        for (size_t i = 0; i < shim.size(); i++)
        {
            cpu.Poke ((Word) (kShim + i), shim[i]);
        }

        cpu.Poke (kSlot, slot);
        cpu.SetUpIigs (iigs);

        cpu.scc.SetLineBaud (9600);
    }


    //  The shim from its start until it has sent `count` bytes on `channel`.
    static void  StartUntilSent (IigsTestCpu & cpu, int channel, size_t count)
    {
        uint32_t  steps = 0;



        cpu.RegPC() = kShim;

        while (cpu.scc.GetSent (channel).size() < count && steps < kStartSteps)
        {
            cpu.Step();
            steps++;
        }

        Assert::IsTrue (cpu.scc.GetSent (channel).size() >= count, L"the shim sends on the port's channel");
    }


    //  A JSR to `entry` with A as given, run until it returns.
    static void  Call (IigsTestCpu & cpu, Word entry, Byte a)
    {
        cpu.Poke ((Word) kStub,       0x20);
        cpu.PokeWord ((Word) (kStub + 1), entry);
        cpu.RegA()  = a;
        cpu.RegPC() = kStub;
        cpu.RegSP() = 0xFF;

        Assert::IsTrue (cpu.RunUntil ((Word) (kStub + 3), kCallSteps) == TestCpu::StopReason::ReachedTarget, L"the call returns");
    }


    //  The shim on port `slot` sets up the 8530's channel for it at the
    //  starting speed and sends Ready there, and doesn't touch the other channel.
    static void  SendsReadyOn (Byte slot, int channel, int other)
    {
        IigsTestCpu                cpu;
        const std::vector<Byte>  * sent = nullptr;



        Load (cpu, slot);
        StartUntilSent (cpu, channel, 4);

        sent = &cpu.scc.GetSent (channel);

        Assert::AreEqual (kIigsPort, cpu.Peek (kMode), L"the port is run as a IIgs's");
        Assert::AreEqual ((uint32_t) 9600, cpu.scc.GetBaud (channel), L"at the starting speed");
        Assert::AreEqual (CassoLinkCodec::kStart, (*sent)[0], L"Ready's envelope");
        Assert::AreEqual ((Byte) 0xD2, (*sent)[3], L"Ready's command");
        Assert::AreEqual (0, cpu.scc.GetGarbled (channel), L"every byte at the line's rate");
        Assert::IsTrue (cpu.scc.GetSent (other).empty(), L"nothing on the other port");
        Assert::AreEqual ((Byte) 0, cpu.scc.GetRegister (other, 11), L"the other port's clocks left as they were");
    }




    TEST_METHOD (OnPort1_TheShimUsesChannelA_AndSendsReady)
    {
        SendsReadyOn (1, FakeScc8530::kA, FakeScc8530::kB);
    }


    TEST_METHOD (OnPort2_TheShimUsesChannelB_AndSendsReady)
    {
        SendsReadyOn (2, FakeScc8530::kB, FakeScc8530::kA);
    }


    //  The 8530 runs from a crystal: the shim sets register 11's crystal bit,
    //  without which the channel has no clock.
    TEST_METHOD (TheShim_ClocksThe8530FromItsCrystal)
    {
        IigsTestCpu  cpu;



        Load (cpu, 2);
        StartUntilSent (cpu, FakeScc8530::kB, 4);

        Assert::AreEqual ((Byte) 0x80, (Byte) (cpu.scc.GetRegister (FakeScc8530::kB, 11) & 0x80));
    }


    //  On an enhanced //e, a card with the same firmware is run through it
    //  and the 8530's registers aren't touched.
    TEST_METHOD (OnAnEnhancedIIe_ThePortIsRunThroughItsFirmware)
    {
        IigsTestCpu  cpu;
        uint32_t     steps = 0;



        Load (cpu, 2, false);

        while (cpu.Peek (kMode) == 0 && steps < kStartSteps)
        {
            cpu.Step();
            steps++;
        }

        Assert::AreEqual (kFirmware, cpu.Peek (kMode));
        Assert::AreEqual ((Byte) 0, cpu.scc.GetRegister (FakeScc8530::kA, 11));
        Assert::AreEqual ((Byte) 0, cpu.scc.GetRegister (FakeScc8530::kB, 11));
    }


    //  The status, read and write entries on the port's channel: status
    //  with A 0 sets the C flag for the transmitter ready, and with A 1
    //  for a byte arrived; read returns it and write sends A.
    TEST_METHOD (TheEntries_ReadWriteAndReportStatus_OnThePortsChannel)
    {
        IigsTestCpu  cpu;



        Load (cpu, 1);
        StartUntilSent (cpu, FakeScc8530::kA, 4);

        Call (cpu, kStatus, 1);
        Assert::IsFalse (cpu.Status().flags.carry != 0, L"no byte yet");

        cpu.scc.Receive (FakeScc8530::kA, { 0x5A });
        Call (cpu, kStatus, 1);
        Assert::IsTrue (cpu.Status().flags.carry != 0, L"a byte arrived");

        Call (cpu, kRead, 0);
        Assert::AreEqual ((Byte) 0x5A, cpu.RegA());

        Call (cpu, kStatus, 0);
        Assert::IsTrue (cpu.Status().flags.carry != 0, L"the transmitter ready");

        Call (cpu, kWrite, 0xA5);
        Assert::AreEqual ((Byte) 0xA5, cpu.scc.GetSent (FakeScc8530::kA).back());
        Assert::IsTrue (cpu.scc.GetSent (FakeScc8530::kB).empty());
    }


    //  The speed entry sets the channel to each 6551 rate the shim has, and
    //  a byte written then goes out at that rate.
    TEST_METHOD (TheSpeedEntry_SetsEachRate)
    {
        struct Rate { Byte control; uint32_t baud; };

        static constexpr Rate  kRates[] =
        {
            { 0x18,   1200 },
            { 0x1A,   2400 },
            { 0x1C,   4800 },
            { 0x1E,   9600 },
            { 0x1F,  19200 },
            { 0x10, 115200 },
            { 0x11, 230400 },
        };

        IigsTestCpu  cpu;



        Load (cpu, 2);
        StartUntilSent (cpu, FakeScc8530::kB, 4);

        for (const Rate & rate : kRates)
        {
            Call (cpu, kSetSpeed, rate.control);
            Assert::AreEqual (rate.baud, cpu.scc.GetBaud (FakeScc8530::kB), std::format (L"control byte {:02X}", rate.control).c_str());

            cpu.scc.SetLineBaud (rate.baud);
            Call (cpu, kWrite, rate.control);
            Assert::AreEqual (rate.control, cpu.scc.GetSent (FakeScc8530::kB).back(), std::format (L"a byte at {}", rate.baud).c_str());
        }

        Assert::AreEqual (0, cpu.scc.GetGarbled (FakeScc8530::kB));
    }
};
