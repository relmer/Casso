#include "Pch.h"

#include "Debugger/MonitorParser.h"
#include "EmuTests/TestMachine.h"

#include "CppUnitTest.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorCommandTableTests
    //
    //  FR-027: every command in the Monitor command table of every Apple II
    //  ROM Casso ships is a command Monitor mode implements.
    //
    //  THE TABLE IS READ THROUGH A BUILT MACHINE, not at a file offset. The
    //  //c's ROM file is two 16 KB banks flipped by $C028, and the Monitor's
    //  table is in bank 0; seeking to the end of that file reads bank 1 and
    //  finds 23 zero entries, which would pass a sweep that excluded them as
    //  filler and prove nothing. Reading $FFCC through the bus gets whichever
    //  bank the machine powers on with, on every machine, with no per-ROM
    //  arithmetic to get wrong.
    //
    //  The decode is the inverse of the accumulator's path through
    //  $FFA7-$FFBD, which is where the Monitor decides an input character is
    //  not a hex digit: EOR #$B0, then ADC #$88 with the carry the CMP #$0A
    //  left set. Known entries are decoded first, so a transform that had
    //  drifted fails on `L` rather than on some entry nobody recognizes.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorCommandTableTests)
    {
    public:
        static constexpr Word    kCommandTable = 0xFFCC;
        static constexpr Word    kHandlerTable = 0xFFE3;
        static constexpr size_t  kEntryCount   = 23;

        //  The //c's last entry, and the only entry on any shipped ROM whose
        //  handler is $00. Declared here as the data it is rather than
        //  skipped quietly.
        static constexpr Byte    kFillerCharacter = 0xEA;
        static constexpr Byte    kFillerHandler   = 0x00;

        static constexpr const char *  kMachines[] =
        {
            "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c",
        };



        ////////////////////////////////////////////////////////////////////////
        //
        //  Decode
        //
        //  A table byte to the character the user types, high bit stripped.
        //
        ////////////////////////////////////////////////////////////////////////

        static Byte Decode (Byte entry)
        {
            static constexpr Byte  kBias = 0x89;
            static constexpr Byte  kMask = 0xB0;

            return (Byte) ((((entry - kBias) & 0xFF) ^ kMask) & 0x7F);
        }



        static std::wstring Widen (const std::string & text)
        {
            return std::wstring (text.begin(), text.end());
        }



        //  A character as it reads in a failure message: ^C for a control
        //  code, the character itself otherwise.
        static std::string Describe (Byte character)
        {
            static constexpr Byte  kFirstPrintable = 0x20;

            return (character < kFirstPrintable)
                 ? std::string ("^") + (char) (character + 0x40)
                 : std::string (1, (char) character);
        }



        static void ReadTable (const char * machineId, std::vector<Byte> & characters, std::vector<Byte> & handlers)
        {
            TestMachine  machine (machineId, TestMachine::Slots::Empty);



            machine.PowerCycle();

            for (size_t i = 0; i < kEntryCount; ++i)
            {
                characters.push_back (machine.GetMemoryBus().ReadByte ((Word) (kCommandTable + i)));
                handlers.push_back   (machine.GetMemoryBus().ReadByte ((Word) (kHandlerTable + i)));
            }
        }



        //  THE TRANSFORM, PINNED BEFORE ANYTHING IS SWEPT WITH IT. `L`, `G`
        //  and `M` are in every shipped ROM's table, so a decode that no
        //  longer produces them is wrong however plausible its output looks.
        TEST_METHOD (KnownCommands_DecodeOnEveryRom)
        {
            for (const char * machineId : kMachines)
            {
                std::vector<Byte>  characters;
                std::vector<Byte>  handlers;
                std::set<Byte>     decoded;



                //  NOT asserted here: that 23 entries were read. ReadTable
                //  loops kEntryCount times, so comparing its result against
                //  kEntryCount compares a number with itself -- a line that
                //  reads like a check and holds under any value. The count is
                //  documented data rather than something the ROM discloses,
                //  and what makes it load-bearing is the filler test below,
                //  which finds the //c's last entry only when all 23 are
                //  read.
                ReadTable (machineId, characters, handlers);

                for (Byte entry : characters)
                {
                    decoded.insert (Decode (entry));
                }

                for (char known : { 'L', 'G', 'M' })
                {
                    Assert::IsTrue (decoded.count ((Byte) known) == 1,
                                    Widen (std::string (machineId) + ": the table decodes no '" + known + "'").c_str());
                }
            }
        }



        //  FR-027 itself.
        TEST_METHOD (EveryTableEntry_IsAMonitorCommand)
        {
            for (const char * machineId : kMachines)
            {
                std::vector<Byte>  characters;
                std::vector<Byte>  handlers;



                ReadTable (machineId, characters, handlers);

                for (size_t i = 0; i < kEntryCount; ++i)
                {
                    bool  isFiller = characters[i] == kFillerCharacter && handlers[i] == kFillerHandler;
                    Byte  command  = Decode (characters[i]);



                    if (isFiller)
                    {
                        continue;
                    }

                    Assert::IsTrue (MonitorParser::IsCommandCharacter (command),
                                    Widen (std::format ("{}: entry {} is '{}' (${:02X}), which Monitor mode does not implement",
                                                        machineId, i, Describe (command), characters[i])).c_str());
                }
            }
        }



        //  The filler rule is one entry on one machine, and saying so is what
        //  keeps it from becoming a license to drop anything inconvenient.
        TEST_METHOD (OnlyTheApple2cLastEntry_IsFiller)
        {
            for (const char * machineId : kMachines)
            {
                std::vector<Byte>  characters;
                std::vector<Byte>  handlers;
                size_t             fillerCount = 0;
                bool               isApple2c   = std::string (machineId) == "Apple2c";



                ReadTable (machineId, characters, handlers);

                for (size_t i = 0; i < kEntryCount; ++i)
                {
                    if (characters[i] == kFillerCharacter && handlers[i] == kFillerHandler)
                    {
                        ++fillerCount;
                        Assert::AreEqual (kEntryCount - 1, i, Widen (std::string (machineId) + ": filler is not the last entry").c_str());
                    }
                }

                Assert::AreEqual (isApple2c ? size_t (1) : size_t (0), fillerCount, Widen (machineId).c_str());
            }
        }



        //  WHICH COMMANDS A ROM CARRIES IS NOT THE SAME ON EVERY MACHINE, and
        //  Casso offers the union on all of them (FR-016). The ][+ and //e
        //  dropped step and trace, leaving three `^Y` entries whose later two
        //  the table scan can never reach; the Enhanced //e and //c put `!`
        //  and `S` back. Pinned because it is the reason the union exists.
        TEST_METHOD (TheRomsDisagreeAboutStepTraceAndTheAssembler)
        {
            auto  commandsOf = [] (const char * machineId)
            {
                std::vector<Byte>  characters;
                std::vector<Byte>  handlers;
                std::set<Byte>     commands;

                ReadTable (machineId, characters, handlers);

                for (Byte entry : characters)
                {
                    commands.insert (Decode (entry));
                }

                return commands;
            };

            std::set<Byte>  original = commandsOf ("Apple2");
            std::set<Byte>  plus     = commandsOf ("Apple2Plus");
            std::set<Byte>  enhanced = commandsOf ("Apple2eEnhanced");



            Assert::IsTrue (original.count ('S') == 1 && original.count ('T') == 1, L"the original ][ steps and traces");
            Assert::IsTrue (original.count ('!') == 0,                              L"and reaches its assembler at $F666, not through !");

            Assert::IsTrue (plus.count ('S') == 0 && plus.count ('T') == 0,         L"the ][+ dropped both");
            Assert::IsTrue (plus.count ('!') == 0,                                  L"and has no ! either");

            Assert::IsTrue (enhanced.count ('!') == 1 && enhanced.count ('S') == 1, L"the Enhanced //e has ! and S");
            Assert::IsTrue (enhanced.count ('T') == 0,                              L"but not T");
        }
    };
}
