#include "Pch.h"

#include "EmuTests/TestMachine.h"
#include "EmuTests/TextScreenScraper.h"
#include "Machines/Apple2/Common/AppleKeyboard.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorRomFactsTests
    //
    //  FR-029: the two things Monitor mode was about to be built on top of,
    //  asserted against the ROMs rather than assumed.
    //
    //  WHERE THE MINI-ASSEMBLER LIVES decides whether `!` can be a command at
    //  all and why `F666G` is an alias rather than a jump. $F666 is the
    //  mini-assembler only in the original ]['s Integer BASIC ROM; on every
    //  Applesoft machine those bytes are Applesoft, and the Enhanced //e and
    //  //c reach their assembler through the internal $Cxxx firmware.
    //
    //  WHETHER THE ROM ACCEPTS LOWERCASE decides whether FR-021 is describing
    //  the machines or only Casso's parser. The test types at the real `*`
    //  prompt and reads the real screen, because the answer is in the ROM's
    //  input path and nowhere else.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorRomFactsTests)
    {
    public:
        static constexpr Word      kCommandTable   = 0xFFCC;
        static constexpr Word      kHandlerTable   = 0xFFE3;
        static constexpr size_t    kEntryCount     = 23;
        static constexpr Word      kDispatchPage   = 0xFE00;
        static constexpr Word      kIntegerAsm     = 0xF666;
        static constexpr Byte      kBangCharacter  = '!';

        //  A cold boot to the BASIC prompt, then a line at a time. Ceilings,
        //  not targets: MachineIdle stops as soon as the machine goes quiet.
        static constexpr uint64_t  kBootCycles     = 20'000'000;
        static constexpr uint64_t  kLineCycles     = 5'000'000;



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        static Byte Decode (Byte entry)
        {
            static constexpr Byte  kBias = 0x89;
            static constexpr Byte  kMask = 0xB0;

            return (Byte) ((((entry - kBias) & 0xFF) ^ kMask) & 0x7F);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  FindHandler
        //
        //  The address the table dispatches `character` to, or zero.
        //
        //  The dispatcher at $FFBE pushes $FE, then the offset byte from
        //  $FFE3, and returns -- so the handler is $FE00 plus the offset plus
        //  the one the RTS adds.
        //
        ////////////////////////////////////////////////////////////////////////

        static Word FindHandler (MachineHost & machine, Byte character)
        {
            for (size_t i = 0; i < kEntryCount; ++i)
            {
                Byte  entry = machine.GetMemoryBus().ReadByte ((Word) (kCommandTable + i));



                if (Decode (entry) == character)
                {
                    return (Word) (kDispatchPage + machine.GetMemoryBus().ReadByte ((Word) (kHandlerTable + i)) + 1);
                }
            }

            return 0;
        }



        //  An absolute operand in the internal firmware's $Cxxx window, in the
        //  handler's first few instructions. Read as bytes rather than
        //  disassembled: what matters is that the handler leaves the Monitor
        //  ROM for the firmware, and both machines do it in the first three
        //  instructions.
        static bool ReachesInternalFirmware (MachineHost & machine, Word handler)
        {
            static constexpr size_t  kScanBytes   = 24;
            static constexpr Word    kFirmware    = 0xC000;
            static constexpr Word    kFirmwareEnd = 0xCFFF;



            for (size_t i = 0; i + 2 < kScanBytes; ++i)
            {
                Byte  opcode = machine.GetMemoryBus().ReadByte ((Word) (handler + i));
                Word  target = (Word) (machine.GetMemoryBus().ReadByte ((Word) (handler + i + 1))
                                     | (machine.GetMemoryBus().ReadByte ((Word) (handler + i + 2)) << 8));



                //  JMP absolute, JSR absolute, STA absolute.
                if ((opcode == 0x4C || opcode == 0x20 || opcode == 0x8D)
                    && target >= kFirmware && target <= kFirmwareEnd)
                {
                    return true;
                }
            }

            return false;
        }



        //  On both machines that have `!`, its handler reaches the internal
        //  $Cxxx firmware: the //c jumps straight there, and the Enhanced //e
        //  pages the firmware in with $C007 first. The prompt itself is
        //  printed inside that firmware, not in the handler the table names.
        TEST_METHOD (TheBangHandler_ReachesTheInternalFirmware)
        {
            for (const char * machineId : { "Apple2eEnhanced", "Apple2c" })
            {
                TestMachine  machine (machineId, TestMachine::Slots::Empty);
                Word         handler = 0;



                machine.PowerCycle();
                handler = FindHandler (machine, kBangCharacter);

                Assert::IsTrue (handler != 0, Widen (std::string (machineId) + ": no ! entry in the command table").c_str());
                Assert::IsTrue (ReachesInternalFirmware (machine, handler),
                                Widen (std::format ("{}: the ! handler at ${:04X} does not reach $Cxxx", machineId, handler)).c_str());
            }
        }



        //  And the three machines without it, so the union Casso offers is
        //  measured against what each ROM really carries.
        TEST_METHOD (TheEarlierRoms_HaveNoBangEntry)
        {
            for (const char * machineId : { "Apple2", "Apple2Plus", "Apple2e" })
            {
                TestMachine  machine (machineId, TestMachine::Slots::Empty);



                machine.PowerCycle();

                Assert::AreEqual ((Word) 0, FindHandler (machine, kBangCharacter),
                                  Widen (std::string (machineId) + ": unexpected ! entry").c_str());
            }
        }



        //  $F666 IS AN ENTRY POINT ON ONE MACHINE ONLY. The original ][ holds
        //  a JMP there into the mini-assembler; every Applesoft machine holds
        //  Applesoft. That is why `F666G` is Casso's alias for `!` rather than
        //  a jump to $F666: on four of the five machines, jumping there runs
        //  the middle of a floating-point routine.
        TEST_METHOD (F666_IsTheMiniAssembler_OnTheOriginalTwoOnly)
        {
            static constexpr Byte  kJmpAbsolute = 0x4C;
            TestMachine            original ("Apple2", TestMachine::Slots::Empty);



            original.PowerCycle();

            Assert::AreEqual (kJmpAbsolute, original.GetMemoryBus().ReadByte (kIntegerAsm),
                              L"the original ][ jumps from $F666 into its mini-assembler");

            for (const char * machineId : { "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" })
            {
                TestMachine  machine (machineId, TestMachine::Slots::Empty);



                machine.PowerCycle();

                Assert::AreNotEqual (kJmpAbsolute, machine.GetMemoryBus().ReadByte (kIntegerAsm),
                                     Widen (std::string (machineId) + ": $F666 is Applesoft on this machine, not an entry point").c_str());
            }
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ReachMonitorPrompt
        //
        //  THROUGH THE MONITOR'S OWN RESET ENTRY, not through BASIC.
        //
        //  `CALL -151` is how a person reaches the Monitor, and it is not
        //  available here: with no disk the //e spins in its startup firmware
        //  showing only its banner, and the //c stops at "Check Disk Drive",
        //  so neither machine ever offers a BASIC prompt to type it at.
        //
        //  $FF59 is byte-identical on all five shipped ROMs -- SETNORM, INIT,
        //  SETVID, SETKBD, CLD, BELL, then MONZ, whose `LDA #$AA` is the `*`
        //  prompt. Entering there sets up the screen and the input and output
        //  hooks exactly as a reset does, with no operating system in the way.
        //
        ////////////////////////////////////////////////////////////////////////

        static void ReachMonitorPrompt (TestMachine & machine)
        {
            static constexpr Word  kMonitorReset = 0xFF59;



            machine.PowerCycle();
            machine.RunCycles (kBootCycles);

            machine.GetCpu()->SetPC (kMonitorReset);
            machine.RunCycles (kLineCycles);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Type
        //
        //  One key, held on the latch until the ROM takes it.
        //
        //  On the base AppleKeyboard rather than the //e one: the //e-specific
        //  pointer is what KeystrokeInjector waits on, and waiting on it here
        //  landed no keys at all. This is the path MachineDebugTarget::
        //  InjectKey already uses.
        //
        ////////////////////////////////////////////////////////////////////////

        static void Type (TestMachine & machine, Byte character)
        {
            static constexpr uint64_t  kSlice     = 20'000;
            static constexpr int       kMaxSlices = 200;
            AppleKeyboard *            keyboard   = machine.GetRefs().keyboard;
            int                        slice      = 0;



            Assert::IsNotNull (keyboard, L"the machine must have a keyboard");

            keyboard->PressKey (character);

            for (slice = 0; slice < kMaxSlices && !keyboard->IsStrobeClear(); ++slice)
            {
                machine.RunCycles (kSlice);
            }

            Assert::IsTrue (keyboard->IsStrobeClear(),
                            std::format (L"the ROM did not take ${:02X} off the keyboard latch", character).c_str());
        }



        static void TypeLine (TestMachine & machine, const std::string & text)
        {
            static constexpr Byte  kReturn = 0x0D;



            for (char character : text)
            {
                Type (machine, (Byte) character);
            }

            Type (machine, kReturn);
            machine.RunCycles (kLineCycles);
        }



        static std::string Screen (MachineHost & machine)
        {
            std::string  text;

            for (const std::string & row : TextScreenScraper::Scrape (machine))
            {
                text += row;
                text += '\n';
            }

            return text;
        }



        //  LOWERCASE REACHES THE MONITOR on the machines whose keyboards can
        //  produce it. Driven through the ROM's own input path -- typed at the
        //  real `*` prompt, read off the real screen -- because upshifting is
        //  the ROM's behavior and nothing in Casso can stand in for it.
        TEST_METHOD (LowercaseInput_ListsOnTheEnhancedRoms)
        {
            for (const char * machineId : { "Apple2eEnhanced", "Apple2c" })
            {
                TestMachine  machine (machineId, TestMachine::Slots::Empty);
                std::string  screen;



                ReachMonitorPrompt (machine);
                TypeLine (machine, "300l");

                screen = Screen (machine);

                Assert::IsTrue (screen.find ("0300-") != std::string::npos,
                                Widen (std::string (machineId) + ": lowercase 300l produced no listing\n" + screen).c_str());
                //  The echo is the finding itself: the Monitor upshifted the
                //  command without upshifting what it printed back.
                Assert::IsTrue (screen.find ("*300l") != std::string::npos,
                                Widen (std::string (machineId) + ": the Monitor did not echo the lowercase line\n" + screen).c_str());
            }
        }
    };
}
