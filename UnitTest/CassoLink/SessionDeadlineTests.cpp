#include "Pch.h"

#include "../EhmTestHelper.h"
#include "../EmuTests/GuestSession.h"
#include "CassoLink/CassoLinkSearch.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/Session/CassoLinkSessionTypes.h"
#include "ProgramRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SessionDeadlineTests
//
//  The PC's deadlines against the time the Apple's code takes, measured in
//  the machine's cycles on an emulated //c: the session's silence limit
//  against the idle program's longest gap, the search's reply time against
//  the shim's answer to Idle, and the connect timeout against the link
//  disk's boot. Each failure has both numbers. The bootstrap's pace, the
//  prompt probe's wait and the loader's wait are checked by the tests that
//  type the bootstrap at those values.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (SessionDeadlineTests)
{
public:

    static constexpr double    kCyclesPerMs = CassoLinkHost::kCpuCyclesPerMs;
    static constexpr uint64_t  kStepCycles  = 1'000;
    static constexpr uint64_t  kIdleCycles  = 5'000'000;
    static constexpr Byte      kFastControl = 0x10;     // 115,200 baud

    static uint64_t  ToCycles (DWORD ms)
    {
        return (uint64_t) ((double) ms * kCyclesPerMs);
    }

    //  The machine run until a byte arrives from it, and the cycles that took;
    //  `limit` when none arrives within it.
    static uint64_t  CyclesToNextByte (TestMachine & machine, InProcessByteStream & pcEnd, uint64_t limit)
    {
        Byte      got[256] = {};
        size_t    read     = 0;
        uint64_t  spent    = 0;



        while (pcEnd.GetReadableCount() == 0 && spent < limit)
        {
            machine.RunCycles (kStepCycles);
            spent += kStepCycles;
        }

        AssertSucceeded (pcEnd.Read (got, sizeof (got), 0, read));
        return spent;
    }

    static std::wstring  Measured (const wchar_t * what, uint64_t cycles, uint64_t limit)
    {
        return std::format (L"{}: {} cycles ({:.0f} ms), the limit {} cycles ({:.0f} ms)", what, cycles, cycles / kCyclesPerMs, limit, limit / kCyclesPerMs);
    }



    //  The session counts an Apple silent for kSilenceMs as gone. With no Idle
    //  from the PC, the program sends Ready once a second, the longest it is
    //  silent; the session's polls only shorten that.
    TEST_METHOD (TheIdleProgramsLongestGap_IsUnderHalfTheSessionsSilenceLimit)
    {
        ProgramRig  rig ({});
        uint64_t    limit   = ToCycles (CassoLinkSession::kSilenceMs);
        uint64_t    longest = 0;
        uint64_t    spent   = 0;



        AssertSucceeded (rig.host->SetSpeed (kFastControl));

        while (spent < kIdleCycles)
        {
            uint64_t  gap = CyclesToNextByte (rig.machine, *rig.pcEnd, limit);

            longest = std::max (longest, gap);
            spent  += std::max (gap, kStepCycles);
        }

        Logger::WriteMessage (Measured (L"longest idle gap", longest, limit).c_str());
        Assert::IsTrue (longest * 2 < limit, Measured (L"the longest gap, under half the limit", longest, limit).c_str());
    }



    //  A port search sends Idle and waits kReplyTimeoutMs for a Ready.
    TEST_METHOD (TheShimsAnswerToIdle_IsWithinTheSearchsReplyTime)
    {
        ProgramRig  rig ({});
        uint64_t    limit = ToCycles (CassoLinkSearch::kReplyTimeoutMs);
        uint64_t    taken = 0;



        rig.machine.RunCycles (limit);
        CyclesToNextByte (rig.machine, *rig.pcEnd, limit);

        AssertSucceeded (rig.host->PromptReady());
        taken = CyclesToNextByte (rig.machine, *rig.pcEnd, limit);

        Assert::IsTrue (taken < limit / 2, Measured (L"the shim's Ready after Idle, under half the limit", taken, limit).c_str());
    }



    //  A link waits kConnectTimeoutMs for the Apple's Ready: the link disk,
    //  booted, sends it well within that.
    TEST_METHOD (TheLinkDisksFirstReady_IsWithinTheConnectTimeout)
    {
        TestMachine                           machine { "Apple2c" };
        std::shared_ptr<InProcessByteStream>  machineEnd;
        std::shared_ptr<InProcessByteStream>  pcEnd;
        std::vector<Byte>                     boot;
        uint64_t                              limit = ToCycles (CassoLinkSessionTypes::kConnectTimeoutMs);
        uint64_t                              taken = 0;



        AssertSucceeded (LinkProgram::BuildBootDisk (CassoLinkSettings::kDefaultBaud, boot));

        InProcessByteStream::CreatePair (machineEnd, pcEnd);
        AssertSucceeded (machine.AttachSerialStream (2, machineEnd));
        GuestSession::Mount (machine, boot);

        taken = CyclesToNextByte (machine, *pcEnd, limit);

        Assert::IsTrue (taken < limit / 2, Measured (L"the link disk's first Ready from boot, under half the limit", taken, limit).c_str());
    }
};
