#include "Pch.h"

#include "Devices/IInputEventSink.h"
#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"
#include "Machines/Apple2/Common/Disk2Controller.h"

#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  OpenAppleResetTests
//
//  Ctrl-Open-Apple-Reset restarts the machine; Ctrl-Reset alone keeps the
//  program. The firmware tells the two apart by reading Open Apple after its
//  reset handler starts, so the key has to still be down at that moment --
//  and WHEN that moment comes differs by machine. The //e reads it 577
//  cycles in. The //c first strobes the drive motor on and off, waits for
//  the IWM's motor flag to clear, and reads it about 200,000 cycles in.
//  Both are measured here, because the length of the hold the shell
//  applies is chosen to cover the later one with room to spare.
//
//  Applesoft's end-of-program pointer is the witness on the //e: a cold start
//  puts it back to the start of program memory, a warm reset leaves what was
//  there. On the //c the drive motor is the witness: a cold start reboots
//  through the built-in drive and a warm reset returns to BASIC.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (OpenAppleResetTests)
{
public:

    TEST_METHOD (AResetWithOpenAppleHeldColdStarts)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);

        machine.GetRefs().iieKeyboard->SetOpenApple (true);
        machine.SoftReset();
        machine.RunCycles (kSettleCycles);
        machine.GetRefs().iieKeyboard->SetOpenApple (false);

        Assert::AreEqual<Word> (kEmptyProgramEnd, ProgramEnd (machine),
            L"Open Apple at reset is a cold start, and the program is gone");
    }


    TEST_METHOD (AResetWithoutOpenAppleKeepsTheProgram)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);

        machine.SoftReset();
        machine.RunCycles (kSettleCycles);

        Assert::AreEqual<Word> (kFakeProgramEnd, ProgramEnd (machine),
            L"a plain reset is a warm start, and the program stays");
    }


    TEST_METHOD (TheIIeFirmwareReadsOpenAppleWithinTheFirstThousandCycles)
    {
        //  Measured: released after 100 cycles the reset is warm, after 1,000
        //  it is cold. That window is under a millisecond, which is why the
        //  key has to be held through the reset by the machine rather than
        //  left to the host's key state around a click.
        Assert::IsFalse (IIeColdStartsWhenReleasedAfter (100),   L"100 cycles is too soon");
        Assert::IsTrue  (IIeColdStartsWhenReleasedAfter (1'000), L"a thousand cycles is enough");
    }


    TEST_METHOD (TheIIcFirmwareReadsOpenAppleAfterItsDriveCheck)
    {
        //  The //c reset routine strobes the drive motor on and off, writes
        //  the IWM mode register and reads it back until the two agree, and
        //  only then reads the Apple keys. The IWM takes that write as soon
        //  as the motor is commanded off, spin-down or not, so the read lands
        //  a fifth of a second in rather than a second in -- a real //c
        //  reboots at once, and so must this one. The hold the shell applies
        //  must still cover it.
        uint64_t  firstRead = CyclesUntilTheIIcReadsTheButtons();

        Assert::IsTrue (firstRead > 100'000,
            std::format (L"the //c read the buttons at cycle {}, before its drive check", firstRead).c_str());
        Assert::IsTrue (firstRead < Disk2Controller::kMotorSpindownCycles,
            std::format (L"the //c read the buttons at cycle {}, only after a spin-down it should not wait for",
                         firstRead).c_str());
        Assert::IsTrue (firstRead < Apple2eKeyboard::kResetHoldCycles,
            std::format (L"the //c read the buttons at cycle {}, after the hold would have ended", firstRead).c_str());
    }


    TEST_METHOD (AKeyHeldThroughTheResetColdStartsAIIeThoughTheHostReleasedIt)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);

        //  What the shell does: the keys as they were at the click are held
        //  through the reset, and the host state has already gone up.
        machine.GetRefs().iieKeyboard->HoldAppleKeysThroughReset (true, false);
        machine.GetRefs().iieKeyboard->SetOpenApple (false);
        machine.SoftReset();
        RunTickingTheHold (machine, kSettleCycles);

        Assert::AreEqual<Word> (kEmptyProgramEnd, ProgramEnd (machine), L"cold start");
    }


    TEST_METHOD (AKeyHeldThroughTheResetRebootsAIIcThoughTheHostReleasedIt)
    {
        TestMachine  plain ("Apple2c", TestMachine::Slots::Empty);
        TestMachine  held  ("Apple2c", TestMachine::Slots::Empty);

        BootToPrompt (plain);
        BootToPrompt (held);

        plain.SoftReset();
        RunTickingTheHold (plain, kIIcSettleCycles);
        Assert::IsFalse (plain.GetRefs().diskController->IsMotorOn(),
            L"a plain reset returns a //c to BASIC without touching the drive");

        held.GetRefs().iieKeyboard->HoldAppleKeysThroughReset (true, false);
        held.GetRefs().iieKeyboard->SetOpenApple (false);
        held.SoftReset();
        RunTickingTheHold (held, kIIcSettleCycles);
        Assert::IsTrue (held.GetRefs().diskController->IsMotorOn(),
            L"with Open Apple held through the reset the //c reboots through its drive");
    }


    TEST_METHOD (TheHoldRunsOutAndTheKeyReadsAsTheHostSaysAgain)
    {
        TestMachine       machine ("Apple2e", TestMachine::Slots::Empty);
        Apple2eKeyboard * keyboard = machine.GetRefs().iieKeyboard;

        keyboard->HoldAppleKeysThroughReset (true, true);
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC061) & 0x80, L"held down");
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC062) & 0x80);

        keyboard->TickResetHold (Apple2eKeyboard::kResetHoldCycles / 2);
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC061) & 0x80, L"still held halfway");

        keyboard->TickResetHold (Apple2eKeyboard::kResetHoldCycles / 2);
        Assert::AreEqual<Byte> (0x00, machine.GetMemoryBus().ReadByte (0xC061) & 0x80, L"the hold ran out");
        Assert::AreEqual<Byte> (0x00, machine.GetMemoryBus().ReadByte (0xC062) & 0x80);
    }


    TEST_METHOD (OpenAppleSurvivesTheResetItself)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);

        //  A /RESET pulse does not lift a finger off a key. The firmware
        //  reads $C061 after the reset to decide what kind it is, so a reset
        //  that cleared the key would never see a cold start.
        machine.GetRefs().iieKeyboard->SetOpenApple (true);
        machine.SoftReset();

        Assert::IsTrue (machine.GetRefs().iieKeyboard->IsOpenApplePressed());
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC061) & 0x80,
            L"and the firmware reads it as down");
    }


private:

    static constexpr uint32_t  kBootCycles      = 5'000'000;
    static constexpr uint32_t  kSettleCycles    = 2'000'000;
    static constexpr uint32_t  kIIcSettleCycles = 3'000'000;   // past the spin-down wait and the read
    static constexpr uint32_t  kTickCycles      = 1'000;
    static constexpr Word      kProgramEndLo    = 0x00AF;      // Applesoft PRGEND
    static constexpr Word      kEmptyProgramEnd = 0x0803;      // right after the empty program at $0801
    static constexpr Word      kFakeProgramEnd  = 0x2000;


    //  A sink that notes the cycle of every read of the Apple keys.
    class ButtonReads : public IInputEventSink
    {
    public:
        MachineHost *          machine   = nullptr;
        std::vector<uint64_t>  readAt;

        void OnKbdDataRead (Word, Byte, bool) override {}
        void OnKbdStrobe (Word, Byte, bool) override {}
        void OnButtonRead (Word address, Byte) override
        {
            if (address == 0xC061 && readAt.size() < 16)
            {
                readAt.push_back (machine->GetCpu()->GetTotalCycles());
            }
        }

        void OnPaddleTrigger (Word) override {}
        void OnPaddleRead (Word, Byte) override {}
        void OnHostAutoRepeat (Byte) override {}
        void OnHostKeyDown (Byte) override {}
        void OnHostKeyUp (Byte) override {}
    };


    static void BootToPrompt (MachineHost & machine)
    {
        machine.PowerCycle();
        machine.RunCycles (kBootCycles);
    }


    static void PretendAProgramIsLoaded (MachineHost & machine)
    {
        machine.GetMemoryBus().WriteByte (kProgramEndLo,     (Byte) (kFakeProgramEnd & 0xFF));
        machine.GetMemoryBus().WriteByte (kProgramEndLo + 1, (Byte) (kFakeProgramEnd >> 8));
    }


    //  Runs the machine the way the CPU thread does: a slice at a time, with
    //  the reset hold counted down by the cycles each slice ran.
    static void RunTickingTheHold (MachineHost & machine, uint32_t cycles)
    {
        for (uint32_t ran = 0; ran < cycles; ran += kTickCycles)
        {
            machine.RunCycles (kTickCycles);
            machine.GetRefs().iieKeyboard->TickResetHold (kTickCycles);
        }
    }


    static Word ProgramEnd (MachineHost & machine)
    {
        return (Word) (machine.GetMemoryBus().ReadByte (kProgramEndLo)
                     | (machine.GetMemoryBus().ReadByte (kProgramEndLo + 1) << 8));
    }


    static bool IIeColdStartsWhenReleasedAfter (uint32_t cycles)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);

        machine.GetRefs().iieKeyboard->SetOpenApple (true);
        machine.SoftReset();
        machine.RunCycles (cycles);
        machine.GetRefs().iieKeyboard->SetOpenApple (false);
        machine.RunCycles (kSettleCycles);

        return ProgramEnd (machine) == kEmptyProgramEnd;
    }


    static uint64_t CyclesUntilTheIIcReadsTheButtons()
    {
        TestMachine       machine ("Apple2c", TestMachine::Slots::Empty);
        ButtonReads       sink;
        MachineObservers  observers;
        uint64_t          resetAt = 0;

        sink.machine    = &machine;
        observers.input = &sink;

        BootToPrompt (machine);
        machine.AttachObservers (observers);

        resetAt = machine.GetCpu()->GetTotalCycles();
        machine.SoftReset();
        machine.RunCycles (kIIcSettleCycles);

        Assert::IsFalse (sink.readAt.empty(), L"the //c firmware reads $C061 after a reset");

        return sink.readAt[0] - resetAt;
    }
};
