#include "Pch.h"

#include "Debugger/Basic/ApplesoftProgram.h"
#include "Core/TextEncoding.h"
#include "EmuTests/GuestSession.h"
#include "EmuTests/MachineIdle.h"
#include "EmuTests/TestMachine.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ApplesoftProgramTests
//
//  The program reader against Applesoft itself: an enhanced //e with no
//  disk comes up in Applesoft, programs are typed into it, and the listing,
//  variables and frames read from memory are compared with what LIST and
//  PRINT write on the screen.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (ApplesoftProgramTests)
    {
    public:

        struct BasicRig
        {
            TestMachine  machine { "Apple2eEnhanced", TestMachine::Slots::Empty };

            BasicRig (std::initializer_list<const char *> lines)
            {
                machine.PowerCycle();
                MachineIdle::RunUntilIdle (machine, 20'000'000);

                for (const char * line : lines)
                {
                    GuestSession::TypeAndCollect (machine, line);
                }
            }

            ApplesoftProgram Program()
            {
                return ApplesoftProgram ([this] (Word address) { return machine.GetMemoryBus().ReadByte (address); });
            }

            //  The rows a command prints, on a cleared screen, between its own
            //  line and the next prompt.
            std::vector<std::string> Run (const std::string & command)
            {
                std::vector<std::string>  rows;
                std::vector<std::string>  printed;
                bool                      isAfter = false;



                GuestSession::TypeAndCollect (machine, "HOME");
                rows = GuestSession::TypeAndCollect (machine, command);

                for (const std::string & row : rows)
                {
                    if (isAfter && row.starts_with ("]"))
                    {
                        break;
                    }

                    if (isAfter)
                    {
                        printed.push_back (Trim (row));
                    }

                    isAfter = isAfter || row == "]" + command;
                }

                return printed;
            }
        };

        static std::string Trim (const std::string & text)
        {
            size_t  last = text.find_last_not_of (' ');



            return (last == std::string::npos) ? std::string() : text.substr (0, last + 1);
        }


        //  SC-047: line for line what LIST prints, with its spacing and its
        //  wrapping, for tokens, strings, REM, DATA and long lines.
        TEST_METHOD (TheListingIsWhatListPrints)
        {
            BasicRig                  rig ({ "10 A = 1/3: B% = -7: C$ = \"HI\": D = 1E10: E = .001: F = 123456789",
                                             "20 DIM X(2,3), Y%(4): REM  A REMARK: WITH COLONS",
                                             "30 FOR I = 1 TO 10 STEP 2: GOSUB 100: NEXT I: END",
                                             "40 DATA 1, TWO ,\"THREE\": PRINT \"DONE\";",
                                             "100 IF A > 0 THEN PRINT \"YES\": RETURN" });
            std::vector<BasicLine>    lines;
            std::vector<std::string>  rows;



            Assert::IsTrue (rig.Program().TryGetListing (lines));
            Assert::AreEqual ((size_t) 5, lines.size());
            Assert::AreEqual ((Word) 100, lines.back().number);

            rows = ApplesoftProgram::GetListRows (lines);
            std::ranges::transform (rows, rows.begin(), Trim);

            Assert::IsTrue (rows == rig.Run ("LIST"), TextEncoding::NarrowToWide (std::format ("{}", rows)).c_str());
        }

        //  SC-048: each simple variable's value is what PRINT writes for it.
        TEST_METHOD (EachVariableIsWhatPrintWrites)
        {
            BasicRig                    rig ({ "10 A = 1/3: B = 2/3: C = 1E10: D = .001: E = .01: F = 123456789: G = -2.5",
                                               "20 H = 1E9: I = 999999999: J = 0: K = -1E-20: L = 3.14159265: M = 100: N% = -7",
                                               "30 O$ = \"HELLO\": PI = 4 * ATN (1): Q = -.5: R = 1E38: S = 255 / 7",
                                               "RUN" });
            std::vector<BasicVariable>  variables;



            rig.Program().GetVariables (variables);

            Assert::AreEqual ((size_t) 19, variables.size());

            for (const BasicVariable & variable : variables)
            {
                std::vector<std::string>  printed = rig.Run ("PRINT " + variable.name);



                Assert::IsFalse  (printed.empty(), TextEncoding::NarrowToWide (variable.name).c_str());
                Assert::AreEqual (printed.front(), variable.value, TextEncoding::NarrowToWide (variable.name).c_str());
            }
        }

        TEST_METHOD (EachArrayElementIsWhatPrintWrites)
        {
            BasicRig                 rig ({ "10 DIM X(2,3), Y%(4), Z$(1)",
                                            "20 FOR I = 0 TO 2: FOR J = 0 TO 3: X(I,J) = I * 10 + J / 4: NEXT J, I",
                                            "30 Y%(3) = 9: Y%(4) = -300: Z$(1) = \"Q\"",
                                            "RUN" });
            std::vector<BasicArray>  arrays;



            rig.Program().GetArrays (arrays);

            Assert::AreEqual ((size_t) 3, arrays.size());
            Assert::AreEqual (std::string ("X"), arrays[0].name);
            Assert::IsTrue   (arrays[0].sizes == std::vector<int> { 3, 4 });
            Assert::AreEqual ((size_t) 12, arrays[0].values.size());

            for (int j = 0; j < 4; j++)
            {
                for (int i = 0; i < 3; i++)
                {
                    Assert::AreEqual (rig.Run (std::format ("PRINT X({},{})", i, j)).front(), arrays[0].values[(size_t) (i + 3 * j)]);
                }
            }

            Assert::AreEqual (std::string ("Y%"),   arrays[1].name);
            Assert::AreEqual (std::string ("9"),    arrays[1].values[3]);
            Assert::AreEqual (std::string ("-300"), arrays[1].values[4]);
            Assert::AreEqual (std::string ("Z$"),   arrays[2].name);
            Assert::AreEqual (std::string ("Q"),    arrays[2].values[1]);
        }

        //  After a STOP in a subroutine called from a FOR loop both frames are on
        //  the stack: the GOSUB's, newest, then the loop's.
        TEST_METHOD (TheGosubAndForFramesAreRead)
        {
            BasicRig                 rig ({ "10 X = 3",
                                            "30 FOR I = 1 TO 10 STEP 2: FOR J = 5 TO 0 STEP -.5: GOSUB 100: NEXT J, I: END",
                                            "100 STOP",
                                            "RUN" });
            std::vector<BasicFrame>  frames;



            rig.Program().GetFrames (frames);

            Assert::AreEqual ((size_t) 3, frames.size());
            Assert::IsTrue   (frames[0].kind == BasicFrameKind::Gosub);
            Assert::AreEqual ((Word) 30, frames[0].line);
            Assert::IsTrue   (frames[1].kind == BasicFrameKind::For);
            Assert::AreEqual (std::string ("J"),   frames[1].variable);
            Assert::AreEqual (std::string ("0"),   frames[1].limit);
            Assert::AreEqual (std::string ("-.5"), frames[1].step);
            Assert::AreEqual (std::string ("I"),   frames[2].variable);
            Assert::AreEqual (std::string ("10"),  frames[2].limit);
            Assert::AreEqual (std::string ("2"),   frames[2].step);
            Assert::AreEqual ((Word) 30, frames[2].line);
            Assert::AreEqual ((Word) 100, rig.Program().GetCurrentLine().value_or (0), L"the line STOP left, for CONT");

            rig.Run ("PRINT 1");
            Assert::IsFalse (rig.Program().GetCurrentLine().has_value(), L"direct mode");
        }

        TEST_METHOD (RealsFormatAsPrintDoes)
        {
            Assert::AreEqual (std::string ("0"),          ApplesoftProgram::FormatReal ({ 0x00, 0x00, 0x00, 0x00, 0x00 }));
            Assert::AreEqual (std::string ("1"),          ApplesoftProgram::FormatReal ({ 0x81, 0x00, 0x00, 0x00, 0x00 }));
            Assert::AreEqual (std::string ("-2.5"),       ApplesoftProgram::FormatReal ({ 0x82, 0xA0, 0x00, 0x00, 0x00 }));
            Assert::AreEqual (std::string (".333333333"), ApplesoftProgram::FormatReal ({ 0x7F, 0x2A, 0xAA, 0xAA, 0xAB }));
            Assert::AreEqual (std::string ("1E+10"),      ApplesoftProgram::FormatReal ({ 0xA2, 0x15, 0x02, 0xF9, 0x00 }));
        }
    };
}
