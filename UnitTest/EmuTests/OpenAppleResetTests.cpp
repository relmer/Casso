#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2eKeyboard.h"

#include "TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  OpenAppleResetTests
//
//  Ctrl-Open-Apple-Reset restarts the machine; Ctrl-Reset alone keeps the
//  program. The ROM tells the two apart by reading Open Apple during its
//  reset handler, so the key has to still be down at that moment.
//
//  Applesoft's end-of-program pointer is the witness: a cold start puts it
//  back to the start of program memory, and a warm reset leaves whatever
//  was there.
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


    TEST_METHOD (TheFirmwareReadsOpenAppleWithinTheFirstThousandCycles)
    {
        //  Measured: released after 100 cycles the reset is warm, after 1,000
        //  it is cold. That window is under a millisecond, which is why the
        //  key has to be held through the reset by the machine rather than
        //  left to the host's key state around a click.
        Assert::IsFalse (ColdStartsWhenReleasedAfter (100),   L"100 cycles is too soon");
        Assert::IsTrue  (ColdStartsWhenReleasedAfter (1'000), L"a thousand cycles is enough");
    }


    TEST_METHOD (AKeyHeldThroughTheResetColdStartsThoughTheHostReleasedIt)
    {
        TestMachine  machine ("Apple2e", TestMachine::Slots::Empty);

        BootToPrompt (machine);
        PretendAProgramIsLoaded (machine);

        //  What the shell does: the keys as they were at the click are held
        //  through the reset, and the host state has already gone up.
        machine.GetRefs().iieKeyboard->HoldAppleKeysThroughReset (true, false);
        machine.GetRefs().iieKeyboard->SetOpenApple (false);
        machine.SoftReset();
        machine.RunCycles (kSettleCycles);

        Assert::AreEqual<Word> (kEmptyProgramEnd, ProgramEnd (machine), L"cold start");
    }


    TEST_METHOD (TheHoldRunsOutAndTheKeyReadsAsTheHostSaysAgain)
    {
        TestMachine       machine ("Apple2e", TestMachine::Slots::Empty);
        Apple2eKeyboard * keyboard = machine.GetRefs().iieKeyboard;

        keyboard->HoldAppleKeysThroughReset (true, true);
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC061) & 0x80, L"held down");
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC062) & 0x80);

        keyboard->TickResetHold (Apple2eKeyboard::kResetHoldUs / 2);
        Assert::AreEqual<Byte> (0x80, machine.GetMemoryBus().ReadByte (0xC061) & 0x80, L"still held halfway");

        keyboard->TickResetHold (Apple2eKeyboard::kResetHoldUs / 2);
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
    static constexpr Word      kProgramEndLo    = 0x00AF;   // Applesoft PRGEND
    static constexpr Word      kEmptyProgramEnd = 0x0803;   // right after the empty program at $0801
    static constexpr Word      kFakeProgramEnd  = 0x2000;


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


    static bool ColdStartsWhenReleasedAfter (uint32_t cycles)
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


    static Word ProgramEnd (MachineHost & machine)
    {
        return (Word) (machine.GetMemoryBus().ReadByte (kProgramEndLo)
                     | (machine.GetMemoryBus().ReadByte (kProgramEndLo + 1) << 8));
    }
};
