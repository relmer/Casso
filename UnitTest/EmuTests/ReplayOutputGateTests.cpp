#include "Pch.h"

#include "EmuTests/ReverseSessionRig.h"
#include "Devices/Printer/PrinterByteRing.h"
#include "Machines/Apple2/Common/PrinterCard.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;


static constexpr uint64_t  s_kGateLiveCycles  = KeyframeSettings::kFrameCycles * 30;
static constexpr uint64_t  s_kGateAfterCycles = KeyframeSettings::kFrameCycles * 2;
static constexpr Word      s_kGatePrintSite   = 0x0808;





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayOutputGateTests
//
//  Every replay reverse execution runs to reach a point is silent: a guest
//  that clicks the speaker on every pass and prints a byte every 256th is
//  moved through its history by a seek, a step back and a reverse continue,
//  and none of them leaves a speaker click for the audio pipeline or a byte
//  for the printer. Running on from the past plays the recorded future at
//  its own pace, so it sounds as it did live, while its printout, already
//  printed, is not printed again. Running live again after them clicks and
//  prints as before.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReplayOutputGateTests)
{
public:

    TEST_METHOD (SeekStepBackAndReverseContinueAreSilent)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseProbe       probe;
        ReverseResult      result;
        HRESULT            hr         = S_OK;
        uint64_t           oldest     = 0;
        uint64_t           live       = 0;



        Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kGateLiveCycles);

        Assert::IsFalse (machine.GetRefs().speaker->GetToggleTimestamps().empty(), L"live, the speaker clicks");
        Assert::AreNotEqual<uint32_t> (0, machine.GetRefs().printerCard->GetByteRing().GetApproxSize(), L"and the printer gets bytes");

        oldest = controller.GetOldestPosition();
        live   = machine.GetPosition();

        Drain (machine);

        hr = controller.SeekToPosition (oldest + (live - oldest) / 2, result);
        AssertSucceeded (hr, L"SeekToPosition halfway");
        AssertSilent (machine, L"seek");

        hr = controller.StepBack (result);
        AssertSucceeded (hr, L"StepBack");
        AssertSilent (machine, L"step back");

        probe.breakPc = s_kGatePrintSite;

        hr = controller.ReverseContinue (probe, result);
        AssertSucceeded (hr, L"ReverseContinue");
        Assert::AreEqual<int> (s_kGatePrintSite, machine.GetCpu()->GetPC(), L"reverse continue stopped at the print");
        AssertSilent (machine, L"reverse continue");

        Assert::IsFalse (machine.IsOutputMuted(), L"no replay leaves the output muted");

        //  Running on from the past replays the recorded future, heard but
        //  not printed, until the machine is live again.
        machine.RunCycles (s_kGateAfterCycles);
        Assert::IsTrue   (controller.IsInHistory(), L"still behind live");
        Assert::IsFalse  (machine.GetRefs().speaker->GetToggleTimestamps().empty(), L"running on behind live, the speaker clicks");
        Assert::AreEqual<uint32_t> (0, machine.GetRefs().printerCard->GetByteRing().GetApproxSize(), L"but the printer gets nothing");
        Drain (machine);

        while (controller.IsInHistory())
        {
            machine.RunCycles (s_kGateAfterCycles);
        }

        Drain (machine);
        machine.RunCycles (s_kGateAfterCycles);

        Assert::IsFalse (machine.GetRefs().speaker->GetToggleTimestamps().empty(), L"live again, the speaker clicks again");
        Assert::AreNotEqual<uint32_t> (0, machine.GetRefs().printerCard->GetByteRing().GetApproxSize(), L"and the printer gets bytes again");
    }


    //  A step from the past keeps the printer quiet. Stopping recording
    //  there, as loading a machine state does before it starts recording
    //  again, leaves nothing muted, and the printer prints again.
    TEST_METHOD (StoppingRecordingBehindLiveLeavesThePrinterPrinting)
    {
        TestMachine        machine    ("Apple2e");
        ReverseController  controller (machine);
        ReverseResult      result;
        HRESULT            hr         = S_OK;



        Prepare (machine);

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start");

        machine.RunCycles (s_kGateLiveCycles);

        hr = controller.SeekToPosition (controller.GetOldestPosition(), result);
        AssertSucceeded (hr, L"SeekToPosition back to the start");

        machine.StepOne();

        Assert::IsTrue  (controller.IsInHistory(), L"the step left the machine behind live");
        Assert::IsTrue  (machine.IsPrinterMuted(), L"with the printer quiet");

        controller.Stop();

        Assert::IsFalse (machine.IsOutputMuted(), L"stopping recording leaves nothing muted");

        hr = controller.Start (ReverseSessionRig::MakeSettings (1));
        AssertSucceeded (hr, L"Start again, as a state load does");

        Drain (machine);
        machine.RunCycles (s_kGateAfterCycles);

        Assert::AreNotEqual<uint32_t> (0, machine.GetRefs().printerCard->GetByteRing().GetApproxSize(), L"the printer gets bytes again");
    }


private:

    static void Prepare (TestMachine & machine)
    {
        static constexpr Byte  kProgram[] =
        {
            0xAD, 0x30, 0xC0,       // 0800  LDA $C030    click
            0xEE, 0x04, 0x03,       // 0803  INC $0304
            0xD0, 0x03,             // 0806  BNE $080B
            0x8D, 0x90, 0xC0,       // 0808  STA $C090    slot 1 printer data
            0x4C, 0x00, 0x08,       // 080B  JMP $0800
        };
        size_t  i = 0;



        machine.PowerCycle();

        Assert::IsNotNull (machine.GetRefs().speaker,     L"the //e has a speaker");
        Assert::IsNotNull (machine.GetRefs().printerCard, L"and a printer card in slot 1");

        for (i = 0; i < sizeof (kProgram); i++)
        {
            machine.GetMemoryBus().WriteByte (static_cast<Word> (ReverseSessionRig::kProgramStart + i), kProgram[i]);
        }

        machine.GetCpu()->SetPC (ReverseSessionRig::kProgramStart);
    }


    //  What the audio pipeline and the printer thread take each frame.
    static void Drain (TestMachine & machine)
    {
        Byte  value = 0;



        machine.GetRefs().speaker->ClearTimestamps();
        machine.GetRefs().speaker->BeginFrame();

        while (machine.GetRefs().printerCard->GetByteRing().TryPop (value))
        {
        }
    }


    static void AssertSilent (TestMachine & machine, const wchar_t * what)
    {
        Assert::AreEqual<size_t>   (0, machine.GetRefs().speaker->GetToggleTimestamps().size(),            (std::wstring (what) + L" left speaker clicks").c_str());
        Assert::AreEqual<uint32_t> (0, machine.GetRefs().printerCard->GetByteRing().GetApproxSize(),      (std::wstring (what) + L" left printer bytes").c_str());

        Drain (machine);
    }
};
