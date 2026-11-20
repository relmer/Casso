#include "Pch.h"

#include "../EhmTestHelper.h"
#include "IigsTestCpu.h"
#include "CassoLink/CassoLinkCodec.h"
#include "CassoLink/LinkProgram.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UniversalSccTests
//
//  The universal disk's first stage on a IIgs's ports, on a bare CPU: the
//  disk's payload at $2000 in flat memory, as the boot sector loads it, the
//  fake 8530, and the bytes the stage checks for a IIgs. The stage lists
//  ports 1 and 2, sets each to the shim's speed and waits on both; the start
//  byte on one starts the shim for it, which sends Ready on that port's
//  channel. Port 1 is channel A and port 2 channel B.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UniversalSccTests)
{
public:

    static constexpr Word      kSlot      = LinkProgram::kShimAddress + 0x38;
    static constexpr Word      kMode      = LinkProgram::kShimAddress + 0x39;
    static constexpr Byte      kIigsPort  = 0xC0;
    static constexpr uint32_t  kBaud      = 9600;
    static constexpr uint32_t  kWaitSteps = 5'000'000;



    //  The stage's payload in memory, run until both ports are set up and
    //  waited on.
    static void  Start (IigsTestCpu & cpu, const LinkShimOptions & options)
    {
        std::vector<Byte>  payload;



        AssertSucceeded (LinkProgram::BuildUniversalPayload (options, payload));

        cpu.InitForTest (LinkProgram::kUniversalAddress);

        for (size_t i = 0; i < payload.size(); i++)
        {
            cpu.Poke ((Word) (LinkProgram::kUniversalAddress + i), payload[i]);
        }

        cpu.SetUpIigs();
        cpu.scc.SetLineBaud (kBaud);
    }


    static LinkShimOptions  At9600()
    {
        LinkShimOptions  options;



        options.baud = kBaud;
        return options;
    }


    //  The start byte on port `slot` starts the shim there, which sends
    //  Ready on the port's channel and nothing on the other.
    static void  StartsTheShimOn (Byte slot, int channel, int other)
    {
        IigsTestCpu  cpu;



        Start (cpu, At9600());

        Assert::IsTrue (cpu.StepUntil ([&] () { return cpu.scc.GetBaud (FakeScc8530::kA) == kBaud && cpu.scc.GetBaud (FakeScc8530::kB) == kBaud; }, kWaitSteps), L"both ports set up");

        cpu.scc.Receive (channel, { CassoLinkCodec::kStart });

        Assert::IsTrue (cpu.StepUntil ([&] () { return cpu.scc.GetSent (channel).size() >= 4; }, kWaitSteps), L"Ready on the port's channel");
        Assert::AreEqual (slot,      cpu.Peek (kSlot), L"the shim's port");
        Assert::AreEqual (kIigsPort, cpu.Peek (kMode), L"run as a IIgs's");
        Assert::AreEqual (CassoLinkCodec::kStart, cpu.scc.GetSent (channel)[0]);
        Assert::AreEqual ((Byte) 0xD2, cpu.scc.GetSent (channel)[3], L"Ready's command");
        Assert::IsTrue (cpu.scc.GetSent (other).empty(), L"nothing on the other port");
        Assert::AreEqual (0, cpu.scc.GetGarbled (channel));
    }




    TEST_METHOD (AStartByteOnPort1_StartsTheShimOnChannelA)
    {
        StartsTheShimOn (1, FakeScc8530::kA, FakeScc8530::kB);
    }


    TEST_METHOD (AStartByteOnPort2_StartsTheShimOnChannelB)
    {
        StartsTheShimOn (2, FakeScc8530::kB, FakeScc8530::kA);
    }


    //  Before any start byte, both ports are clocked from the crystal at
    //  the shim's speed.
    TEST_METHOD (BothPorts_AreSetToTheShimsSpeed_FromTheCrystal)
    {
        IigsTestCpu  cpu;



        Start (cpu, At9600());

        Assert::IsTrue (cpu.StepUntil ([&] () { return cpu.scc.GetBaud (FakeScc8530::kA) == kBaud && cpu.scc.GetBaud (FakeScc8530::kB) == kBaud; }, kWaitSteps));

        for (int channel : { FakeScc8530::kA, FakeScc8530::kB })
        {
            Assert::AreEqual ((Byte) 0x80, (Byte) (cpu.scc.GetRegister (channel, 11) & 0x80), L"register 11's crystal bit");
        }
    }


    //  With a modem's dial command, the stage starts the shim at once on the
    //  last port found, port 2, with no start byte.
    TEST_METHOD (WithADialCommand_TheShimStartsAtOnceOnPort2)
    {
        IigsTestCpu      cpu;
        LinkShimOptions  options = At9600();



        options.dial = "ATDT10.0.0.1:6502";
        Start (cpu, options);

        Assert::IsTrue (cpu.StepUntil ([&] () { return cpu.scc.GetSent (FakeScc8530::kB).size() >= 4; }, kWaitSteps), L"Ready on port 2");
        Assert::AreEqual ((Byte) 2, cpu.Peek (kSlot));
        Assert::IsTrue (cpu.scc.GetSent (FakeScc8530::kA).empty());
    }
};
