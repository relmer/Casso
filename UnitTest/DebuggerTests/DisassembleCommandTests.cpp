#include "Pch.h"

#include "Debugger/ReplyJson.h"
#include "SessionRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DisassembleCommandTests
//
//  U, and every other way to ask for a listing: AppleWin's `addrL`, the
//  Monitor's `L` and `/U`, WinDbg's `u` and `!U`, GSSquared's `l`, `list` and
//  `addrl`, in the window's console, over the channel and in batch. Each
//  listing is compared line for line against real memory on a real machine,
//  and the continuation a bare U reads is followed from command to command.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (DisassembleCommandTests)
    {
    public:

        using Lines = std::vector<std::string>;

        //  $0300: LDA #$41 / STA $0400 / JSR COUT / STA ($06),Y / BNE $0300 /
        //  JMP ($0036) / LDA #$06 / LDA $06,X / RTS, then NOPs to $03FF, and
        //  NOPs over $0000-$001F so a listing from zero is known too.
        static void LoadProgram (IDebugTarget & target)
        {
            static constexpr Byte  kProgram[] = { 0xA9, 0x41, 0x8D, 0x00, 0x04, 0x20, 0xED, 0xFD, 0x91, 0x06,
                                                  0xD0, 0xF4, 0x6C, 0x36, 0x00, 0xA9, 0x06, 0xB5, 0x06, 0x60 };
            static constexpr Byte  kNop       = 0xEA;
            Word                   at         = 0x0300;



            for (int address = 0x0300; address <= 0x03FF; ++address)
            {
                target.TryPoke ((Word) address, kNop);
            }

            for (int address = 0x0000; address <= 0x001F; ++address)
            {
                target.TryPoke ((Word) address, kNop);
            }

            for (Byte b : kProgram)
            {
                target.TryPoke (at++, b);
            }
        }

        //  NOP lines in the AppleWin layout with no label column.
        static void AddNops (Lines & lines, Word first, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                lines.push_back (std::format ("{:04X}: EA       NOP", first + i));
            }
        }

        //  The program's first 20 lines, as U 300 lists them with the ROM
        //  symbols alone.
        static Lines GetProgramListing()
        {
            Lines  lines =
            {
                "0300: A9 41    LDA  #$41",
                "0302: 8D 00 04 STA  $0400",
                "0305: 20 ED FD JSR  COUT",
                "0308: 91 06    STA  ($06),Y",
                "030A: D0 F4    BNE  $0300",
                "030C: 6C 36 00 JMP  (CSWL)",
                "030F: A9 06    LDA  #$06",
                "0311: B5 06    LDA  $06,X",
                "0313: 60       RTS",
            };



            AddNops (lines, 0x0314, 11);
            return lines;
        }

        static void AssertLines (const Lines & expected, const Lines & actual, const std::string & what)
        {
            std::wstring  where (what.begin(), what.end());



            for (size_t i = 0; i < std::min (expected.size(), actual.size()); ++i)
            {
                std::wstring  line = where + L" line " + std::to_wstring (i);



                Assert::AreEqual (expected[i], actual[i], line.c_str());
            }

            Assert::AreEqual (expected.size(), actual.size(), (where + L": line count").c_str());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  U addr: twenty lines, in every dialect's layout
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UAddr_ListsTwentyLinesFromTheAddress)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            AssertLines (GetProgramListing(), rig.RunOk ("U 300").text,                         "U 300");
            AssertLines (GetProgramListing(), rig.RunOk ("u 300").text,                         "names take any case");
            AssertLines (GetProgramListing(), rig.RunOk ("U $300").text,                        "U $300");
            AssertLines (GetProgramListing(), rig.RunOk ("U 300", CommandMode::Casso).text,     "Casso mode writes AppleWin's layout");
        }



        TEST_METHOD (UAddr_InGSSquaredMode_HasNoLabelsOrSymbols)
        {
            SessionRig  rig;
            Lines       expected =
            {
                "M=8 X=8",
                "0300: A9 41       LDA  #$41",
                "0302: 8D 00 04    STA  $0400",
                "0305: 20 ED FD    JSR  $FDED",
                "0308: 91 06       STA  ($06),Y",
                "030A: D0 F4       BNE  $0300",
                "030C: 6C 36 00    JMP  ($0036)",
                "030F: A9 06       LDA  #$06",
                "0311: B5 06       LDA  $06,X",
                "0313: 60          RTS",
            };



            LoadProgram (rig.target);
            rig.RunOk ("SYM LOOP = 300");

            for (int i = 0; i < 11; ++i)
            {
                expected.push_back (std::format ("{:04X}: EA          NOP", 0x0314 + i));
            }

            //  U is not a hex word, so GSSquared mode reaches it bare.
            AssertLines (expected, rig.RunOk ("U 300",    CommandMode::GSSquared).text, "U 300 in GSSquared mode");
            AssertLines (expected, rig.RunOk ("l 300",    CommandMode::GSSquared).text, "l 300");
            AssertLines (expected, rig.RunOk ("list 300", CommandMode::GSSquared).text, "list 300");
            AssertLines (expected, rig.RunOk ("300l",     CommandMode::GSSquared).text, "300l");
            AssertLines (expected, rig.RunOk ("300L",     CommandMode::GSSquared).text, "300L");
            AssertLines (expected, rig.RunOk ("00/300l",  CommandMode::GSSquared).text, "a bank-00 address");
        }



        TEST_METHOD (UAddr_InWinDbgMode_IsLowerCaseWithALabelLine)
        {
            SessionRig  rig;
            Lines       expected =
            {
                "LOOP:",
                "0300 a941    lda #$41",
                "0302 8d0004  sta $0400",
                "0305 20edfd  jsr COUT",
                "0308 9106    sta (PTR),Y",
                "030a d0f4    bne LOOP",
                "030c 6c3600  jmp (CSWL)",
                "030f a906    lda #$06",
                "0311 b506    lda PTR,X",
                "0313 60      rts",
            };



            LoadProgram (rig.target);
            rig.RunOk ("SYM LOOP = 300");
            rig.RunOk ("SYM PTR = 6");

            for (int i = 0; i < 11; ++i)
            {
                expected.push_back (std::format ("{:04x} ea      nop", 0x0314 + i));
            }

            //  A symbol keeps its own case; the rest of an operand is lowered.
            AssertLines (expected, rig.RunOk ("u 300",   CommandMode::WinDbg).text, "u 300");
            AssertLines (expected, rig.RunOk ("u 0x300", CommandMode::WinDbg).text, "u 0x300");
            AssertLines (expected, rig.RunOk ("u $300",  CommandMode::WinDbg).text, "u $300");
            AssertLines (expected, rig.RunOk ("!U 300",  CommandMode::WinDbg).text, "AppleWin's U through the marker");
        }



        TEST_METHOD (UAddr_InMonitorMode_UsesTheMonitorLayout)
        {
            SessionRig  rig;
            Lines       expected =
            {
                "LOOP:",
                "0300-   A9 41       LDA   #$41",
                "0302-   8D 00 04    STA   $0400",
                "0305-   20 ED FD    JSR   COUT",
                "0308-   91 06       STA   (PTR),Y",
                "030A-   D0 F4       BNE   LOOP",
                "030C-   6C 36 00    JMP   (CSWL)",
                "030F-   A9 06       LDA   #$06",
                "0311-   B5 06       LDA   PTR,X",
                "0313-   60          RTS",
            };



            LoadProgram (rig.target);
            rig.RunOk ("SYM LOOP = 300");
            rig.RunOk ("SYM PTR = 6");

            for (int i = 0; i < 11; ++i)
            {
                expected.push_back (std::format ("{:04X}-   EA          NOP", 0x0314 + i));
            }

            AssertLines (expected, rig.RunOk ("/U 300", CommandMode::Monitor).text, "/U 300");
            AssertLines (expected, rig.RunOk ("300L",   CommandMode::Monitor).text, "the Monitor's own L lists through U");
            AssertLines (expected, rig.RunOk ("300l",   CommandMode::Monitor).text, "lowercase");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  U first:last and U addr,len
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (URange_ListsEveryInstructionStartingByTheLastAddress)
        {
            SessionRig  rig;
            Lines       all  = GetProgramListing();
            Lines       nops;



            LoadProgram (rig.target);

            AssertLines (Lines (all.begin(), all.begin() + 3), rig.RunOk ("U 300:305").text, "the JSR at $0305 starts on the last address");
            AssertLines (Lines (all.begin(), all.begin() + 3), rig.RunOk ("U 300:306").text, "an instruction that runs past the last address is listed whole");
            AssertLines (Lines (all.begin(), all.begin() + 2), rig.RunOk ("U 300:304").text, "and one that starts after it is not");
            AssertLines (Lines (all.begin(), all.begin() + 3), rig.RunOk ("U 300:305", CommandMode::Casso).text, "Casso mode");
            AssertLines (Lines (all.begin(), all.begin() + 1), rig.RunOk ("U 300:300").text, "a range of one address");

            //  No line cap: $0300-$03FF is nine instructions and 236 NOPs.
            nops = rig.RunOk ("U 300:3FF").text;
            Assert::AreEqual ((size_t) 245,                   nops.size());
            Assert::AreEqual (std::string ("03FF: EA               NOP"), nops.back(), L"the page-3 vectors give the listing a label column");
        }



        TEST_METHOD (URange_LengthForm)
        {
            SessionRig  rig;
            Lines       all = GetProgramListing();



            LoadProgram (rig.target);

            AssertLines (Lines (all.begin(), all.begin() + 2), rig.RunOk ("U 300,5").text, "$0300-$0304");
            AssertLines (Lines (all.begin(), all.begin() + 3), rig.RunOk ("U 300,6").text, "$0300-$0305");
            AssertLines (Lines (all.begin(), all.begin() + 1), rig.RunOk ("U 300,1").text, "a length of one");

            rig.RunFails ("U 300,0",  "invalid arguments", "A range length must be at least 1.");
            rig.RunFails ("!U 300,0", "invalid arguments", "A range length must be at least 1.", CommandMode::WinDbg);
            rig.RunFails ("/U 300,0", "invalid arguments", "A range length must be at least 1.", CommandMode::Monitor);
        }



        TEST_METHOD (URange_InOtherModes)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            AssertLines ({ "0300 a941    lda #$41", "0302 8d0004  sta $0400", "0305 20edfd  jsr COUT" },
                         rig.RunOk ("!U 300:305", CommandMode::WinDbg).text, "!U range");
            AssertLines ({ "0300 a941    lda #$41", "0302 8d0004  sta $0400", "0305 20edfd  jsr COUT" },
                         rig.RunOk ("u 300:305",  CommandMode::WinDbg).text, "u with an AppleWin range");
            AssertLines ({ "0300 a941    lda #$41", "0302 8d0004  sta $0400" },
                         rig.RunOk ("u 300,5",    CommandMode::WinDbg).text, "u with an AppleWin length");
            AssertLines ({ "0300-   A9 41       LDA   #$41", "0302-   8D 00 04    STA   $0400" },
                         rig.RunOk ("/U 300,5",   CommandMode::Monitor).text, "/U with a length");
        }



        //  A range that ends before it begins, and a length that carries past
        //  $FFFF, list nothing; each is an error, and the continuation stays
        //  where it was.
        TEST_METHOD (URange_ThatEndsBeforeItBegins_IsAnError)
        {
            SessionRig  rig;



            LoadProgram (rig.target);
            rig.RunOk ("U 300:305");

            rig.RunFails ("U 400:300", "invalid arguments",
                          "$0400-$0300 ends before it begins. A range ends at or after its first address, and no later than $FFFF.");
            rig.RunFails ("U FFF0,20", "invalid arguments",
                          "$FFF0-$000F ends before it begins. A range ends at or after its first address, and no later than $FFFF.");
            rig.RunFails ("u 400:300", "invalid arguments",
                          "$0400-$0300 ends before it begins. A range ends at or after its first address, and no later than $FFFF.", CommandMode::WinDbg);

            Assert::AreEqual (std::string ("0308: 91 06    STA  ($06),Y"), rig.RunOk ("U").text.at (0), L"the continuation is where the last listing left it");
        }



        TEST_METHOD (UAddr_BadArguments)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            rig.RunFails ("U ZZZ",   "invalid arguments", "ZZZ is not a symbol, register, or hex number.");
            rig.RunFails ("U 12G",   "invalid arguments", "12G is not a hex number.");
            rig.RunFails ("U 10000", "invalid arguments", "10000 is outside $0000-$FFFF.");
            rig.RunFails ("U 300:ZZZ", "invalid arguments", "ZZZ is not a symbol, register, or hex number.");

            AssertLines (GetProgramListing(), rig.RunOk ("U 300 400").text, "a token after the address is ignored");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  U alone continues
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (UAlone_StartsAtZero_ThenContinues)
        {
            SessionRig  rig;
            Lines       expected;



            LoadProgram (rig.target);

            AddNops (expected, 0x0000, 20);
            AssertLines (expected, rig.RunOk ("U").text, "the first U of a session lists from $0000");

            Assert::AreEqual (std::string ("0014: EA              NOP"), rig.RunOk ("U").text.at (0), L"the next continues after it, with the label column WNDLFT and its neighbors open");

            rig.RunOk ("U 300");
            Assert::AreEqual (std::string ("031F: EA       NOP"), rig.RunOk ("U").text.at (0), L"after a 20-line listing");

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308: 91 06    STA  ($06),Y"), rig.RunOk ("U").text.at (0), L"after a range, from the address after its last instruction");

            rig.RunOk ("300L");
            Assert::AreEqual (std::string ("031F: EA       NOP"), rig.RunOk ("U").text.at (0), L"after the addrL shorthand");
        }



        //  The continuation belongs to the session, so every dialect's
        //  continue form reads the one the last listing left, whichever mode
        //  made it.
        TEST_METHOD (UAlone_EveryDialectsContinueForm_SharesOneContinuation)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308 9106    sta ($06),y"), rig.RunOk ("u", CommandMode::WinDbg).text.at (0), L"WinDbg's u");

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308: 91 06       STA  ($06),Y"), rig.RunOk ("l", CommandMode::GSSquared).text.at (1), L"GSSquared's l");

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308: 91 06       STA  ($06),Y"), rig.RunOk ("list", CommandMode::GSSquared).text.at (1), L"GSSquared's list");

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308 9106    sta ($06),y"), rig.RunOk ("!U", CommandMode::WinDbg).text.at (0), L"!U");

            rig.RunOk ("U 300:305");
            Assert::AreEqual (std::string ("0308-   91 06       STA   ($06),Y"), rig.RunOk ("/U", CommandMode::Monitor).text.at (0), L"/U");

            rig.RunOk ("u 300", CommandMode::WinDbg);
            Assert::AreEqual (std::string ("031F: EA       NOP"), rig.RunOk ("U").text.at (0), L"a WinDbg listing moves AppleWin's continuation");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The addrL shorthand
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AddrL_ListsTwentyLinesFromAnyHeadThatEvaluates)
        {
            SessionRig  rig;
            Lines       expected;
            Lines       cout;



            LoadProgram (rig.target);

            AssertLines (GetProgramListing(), rig.RunOk ("300L").text, "300L");
            AssertLines (GetProgramListing(), rig.RunOk ("300l").text, "300l");
            AssertLines (GetProgramListing(), rig.RunOk ("300L", CommandMode::Casso).text, "Casso mode");

            cout = rig.RunOk ("COUTL").text;
            Assert::AreEqual ((size_t) 20, cout.size());
            Assert::AreEqual (std::string ("FDED: 6C 36 00 COUT  JMP  (CSWL)"), cout[0], L"a symbol as the head");
            Assert::AreEqual (std::string ("FDF0: C9 A0    COUT1 CMP  #$A0"),   cout[1]);

            rig.RunOk ("R A 3");
            AddNops (expected, 0x0003, 20);
            AssertLines (expected, rig.RunOk ("AL").text, "a register as the head");

            rig.RunFails ("FROGL", "unknown command", "FROGL is not a command.");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The top of memory
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (U_StopsAtFFFF_AndContinuesFromZero)
        {
            SessionRig  rig;
            Lines       expected;



            LoadProgram (rig.target);

            //  The ROM's last three bytes: $FA of the reset vector, then the
            //  IRQ vector, $FA40.
            AssertLines ({ "FFFD: FA               NOP", "FFFE: 40       IRQVECT RTI", "FFFF: FA               NOP" },
                         rig.RunOk ("U FFFD").text, "a listing ends at $FFFF short of twenty lines");

            AddNops (expected, 0x0000, 20);
            AssertLines (expected, rig.RunOk ("U").text, "the next U starts again at $0000");

            AssertLines ({ "FFF0: 83 7F            SAX  ($7F,X)", "FFF2: 5D CC B5         EOR  $B5CC,X", "FFF5: FC 17 17         NOP  $1717,X",
                           "FFF8: F5 03            SBC  $03,X",   "FFFA: FB 03 62 NMIVECT ISC  $6203,Y", "FFFD: FA               NOP",
                           "FFFE: 40       IRQVECT RTI",          "FFFF: FA               NOP" },
                         rig.RunOk ("U FFF0:FFFF").text, "a range to $FFFF");
        }



        //  An instruction at $FFFF takes its operand from $0000 and $0001, and
        //  the next listing starts after it, at $0002.
        TEST_METHOD (U_AnInstructionAtFFFF_TakesItsOperandFromZeroPage)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            //  Language card RAM, read and write, so the top page can be written.
            rig.RunOk ("IN C083");
            rig.RunOk ("IN C083");
            rig.Poke (0xFFFF, { 0x20 });
            rig.Poke (0x0000, { 0x34, 0x12 });

            AssertLines ({ "FFFF: 20 34 12 JSR  $1234" }, rig.RunOk ("U FFFF").text, "the operand wraps to $0000");
            Assert::AreEqual (std::string ("0002: EA       NOP"), rig.RunOk ("U").text.at (0));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Labels and operand symbols
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (U_ShowsTheLabelAndTheOperandSymbolsSymCommandsAdd)
        {
            SessionRig  rig;
            Lines       expected =
            {
                "0300: A9 41    LOOP LDA  #$41",
                "0302: 8D 00 04      STA  $0400",
                "0305: 20 ED FD      JSR  COUT",
                "0308: 91 06         STA  (PTR),Y",
                "030A: D0 F4         BNE  LOOP",
                "030C: 6C 36 00      JMP  (CSWL)",
                "030F: A9 06         LDA  #$06",
                "0311: B5 06         LDA  PTR,X",
                "0313: 60            RTS",
            };



            LoadProgram (rig.target);
            rig.RunOk ("SYM LOOP = 300");
            rig.RunOk ("SYM PTR = 6");

            //  Indirect, branch, absolute and indexed operands take the name;
            //  the immediate #$06 is a value and keeps its digits.
            AssertLines (expected, rig.RunOk ("U 300:313").text, "labels and operand symbols");
        }



        //  The label is the first name at the address in table order: Main,
        //  Basic, Asm, User, User2 and on. A table that is off has no say.
        TEST_METHOD (U_TakesTheFirstNameInTableOrder_FromTablesThatAreOn)
        {
            SessionRig  rig;



            LoadProgram (rig.target);
            rig.RunOk ("SYMUSER2 ON");
            rig.RunOk ("SYMUSER2 SECOND = 300");
            rig.RunOk ("SYM FIRST = 300");
            rig.RunOk ("SYM MYCOUT = FDED");

            Assert::AreEqual (std::string ("0300: A9 41    FIRST LDA  #$41"), rig.RunOk ("U 300:300").text.at (0), L"User comes before User2");
            Assert::AreEqual (std::string ("0305: 20 ED FD JSR  COUT"),       rig.RunOk ("U 305:305").text.at (0), L"Main comes before User");

            rig.RunOk ("SYMUSER OFF");
            Assert::AreEqual (std::string ("0300: A9 41    SECOND LDA  #$41"), rig.RunOk ("U 300:300").text.at (0), L"User off, User2 names it");

            rig.RunOk ("SYMMAIN OFF");
            Assert::AreEqual (std::string ("0305: 20 ED FD JSR  $FDED"), rig.RunOk ("U 305:305").text.at (0), L"Main and User off, no name");
            Assert::AreEqual (std::string ("030C: 6C 36 00 JMP  ($0036)"), rig.RunOk ("U 30C:30C").text.at (0));

            rig.RunOk ("SYMMAIN ON");
            Assert::AreEqual (std::string ("0305: 20 ED FD JSR  COUT"), rig.RunOk ("U 305:305").text.at (0), L"on again, named again");
        }



        //  At $C000-$C0FF a read and a write are different switches.
        TEST_METHOD (U_ASoftSwitchOperandFollowsTheAccessDirection)
        {
            SessionRig  rig;



            rig.Poke (0x0300, { 0xAD, 0x00, 0xC0, 0x8D, 0x00, 0xC0, 0xEE, 0x00, 0xC0, 0xAD, 0x7E, 0xC0,
                                0x8D, 0x7E, 0xC0, 0xAD, 0x5E, 0xC0, 0x8D, 0x5E, 0xC0 });

            AssertLines ({ "0300: AD 00 C0 LDA  KBD",
                           "0303: 8D 00 C0 STA  80STOREOFF",
                           "0306: EE 00 C0 INC  80STOREOFF",
                           "0309: AD 7E C0 LDA  RDIOUDIS",
                           "030C: 8D 7E C0 STA  SETIOUDIS",
                           "030F: AD 5E C0 LDA  SETAN3",
                           "0312: 8D 5E C0 STA  SETAN3" },
                         rig.RunOk ("U 300:312").text, "the direction picks the switch");

            rig.RunOk ("SYMMAIN OFF");
            Assert::AreEqual (std::string ("0300: AD 00 C0 LDA  $C000"), rig.RunOk ("U 300:300").text.at (0), L"a table that is off offers neither name");
        }



        //  The I/O page cannot be read without touching its switches, so U
        //  shows its bytes as zero and leaves every switch as it was.
        TEST_METHOD (U_OverTheIoPage_TouchesNoSwitch)
        {
            SessionRig  rig;
            Lines       switches;



            AssertLines ({ "C050: 00       TXTCLR   BRK", "C051: 00       TXTSET   BRK", "C052: 00       MIXCLR   BRK",
                           "C053: 00       MIXSET   BRK", "C054: 00       TXTPAGE1 BRK", "C055: 00       TXTPAGE2 BRK",
                           "C056: 00       LORES    BRK", "C057: 00       HIRES    BRK" },
                         rig.RunOk ("U C050:C057").text, "the video switches");

            switches = rig.RunOk ("SWITCHES").text;
            Assert::AreEqual (std::string ("TEXT       on"),  switches.at (0), L"reading $C050 would have set graphics");
            Assert::AreEqual (std::string ("MIXED      off"), switches.at (1));
            Assert::AreEqual (std::string ("PAGE2      off"), switches.at (2));
            Assert::AreEqual (std::string ("HIRES      off"), switches.at (3));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The CPU's own table
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (U_DecodesWithTheMachinesCpu)
        {
            SessionRig  nmos;
            SessionRig  cmos ("Apple2eEnhanced");



            nmos.Poke (0x0300, { 0x80, 0x02, 0x64, 0x3E, 0x0F, 0x3E, 0x10, 0xA7, 0x53, 0x02, 0x9C, 0x00, 0x04 });
            cmos.Poke (0x0300, { 0x80, 0x02, 0x64, 0x3E, 0x0F, 0x3E, 0x10, 0xA7, 0x53, 0x02, 0x9C, 0x00, 0x04 });

            AssertLines ({ "0300: 80 02    NOP  #$02", "0302: 64 3E    NOP  A2L", "0304: 0F 3E 10 SLO  $103E", "0307: A7 53    LAX  $53",
                           "0309: 02       ???", "030A: 9C       ???", "030B: 00       BRK", "030C: 04 C8    NOP  $C8" },
                         nmos.RunOk ("U 300:30C").text, "a 6502 shows its undocumented and undefined opcodes");

            AssertLines ({ "0300: 80 02    BRA  $0304", "0302: 64 3E    STZ  A2L", "0304: 0F 3E 10 BBR0 $3E,$0317", "0307: A7 53    SMB2 $53",
                           "0309: 02 9C    NOP  #$9C", "030B: 00       BRK", "030C: 04 C8    TSB  $C8" },
                         cmos.RunOk ("U 300:30C").text, "a 65C02 decodes its own");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The dialects' own disassembly words
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (GSSquared_ListForms_Errors)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            rig.RunFails ("l 300 400", "invalid arguments",     "L takes an address or nothing.",                              CommandMode::GSSquared);
            rig.RunFails ("300l 5",    "invalid arguments",     "addrl takes nothing after it.",                               CommandMode::GSSquared);
            rig.RunFails ("l zz",      "invalid arguments",     "zz is not an address. An address is one to four hex digits.", CommandMode::GSSquared);
            rig.RunFails ("l e1/300",  "command not available", "E1/0300 is in bank E1. Only bank 00 exists on this machine.", CommandMode::GSSquared);
        }



        TEST_METHOD (WinDbg_UForms_Errors)
        {
            SessionRig  rig;



            LoadProgram (rig.target);

            rig.RunFails ("u 300 310", "invalid arguments", "u takes one address, or none to continue.", CommandMode::WinDbg);
            rig.RunFails ("u 300 l8",  "invalid arguments", "u takes one address, or none to continue.", CommandMode::WinDbg);
            rig.RunFails ("ub 300",    "unknown command",   "ub is not a WinDbg-mode command.",          CommandMode::WinDbg);
            rig.RunFails ("uf 300",    "unknown command",   "uf is not a WinDbg-mode command.",          CommandMode::WinDbg);
            rig.RunFails ("ln 300",    "unknown command",   "ln is not a WinDbg-mode command.",          CommandMode::WinDbg);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  The window's console and the channel
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (U_InTheWindowsConsole)
        {
            ControllerRig  rig;
            DebugSession & session = rig.controller.GetSession();
            Reply          reply;



            LoadProgram (session.GetTarget());

            reply = rig.view.ExecuteWindowLine (session, "U 300", CommandMode::AppleWin);
            AssertLines (GetProgramListing(), reply.text, "the console, AppleWin mode");

            reply = rig.view.ExecuteWindowLine (session, "U", CommandMode::AppleWin);
            Assert::AreEqual (std::string ("031F: EA       NOP"), reply.text.at (0), L"the console's U continues");

            reply = rig.view.ExecuteWindowLine (session, "u 300", CommandMode::WinDbg);
            Assert::AreEqual (std::string ("0305 20edfd  jsr COUT"), reply.text.at (2), L"the console, WinDbg mode");

            reply = rig.view.ExecuteWindowLine (session, "/U 300", CommandMode::Monitor);
            Assert::AreEqual (std::string ("0305-   20 ED FD    JSR   COUT"), reply.text.at (2), L"the console, Monitor mode");

            reply = rig.view.ExecuteWindowLine (session, "l 300", CommandMode::GSSquared);
            Assert::AreEqual (std::string ("0305: 20 ED FD    JSR  $FDED"), reply.text.at (3), L"the console, GSSquared mode");
        }



        TEST_METHOD (U_OverTheChannel_IsADisassemblyRecord)
        {
            ControllerRig      rig;
            PipeClient         client (rig);
            JsonValue          record;
            const JsonValue  * data   = nullptr;
            const JsonValue  * lines  = nullptr;
            std::string        text;
            int                value  = 0;
            bool               isTrue = false;



            LoadProgram (rig.controller.GetSession().GetTarget());
            rig.Run ("SYM LOOP = 300");

            record = client.Send ("U 300");
            Assert::AreEqual (std::string ("ok"), PipeClient::GetStringMember (record, "status"));
            Assert::IsTrue   (record.HasObject ("data", data));
            Assert::AreEqual (std::string ("disassembly"), PipeClient::GetStringMember (*data, "kind"));
            Assert::IsTrue   (data->HasArray ("lines", lines));
            Assert::AreEqual ((size_t) 20, lines->GetArraySize());

            const JsonValue & first = lines->GetArrayElement (0);
            Assert::AreEqual (S_OK, first.GetInt ("address", value));
            Assert::AreEqual (0x0300, value);
            Assert::AreEqual (std::string ("LDA"),  PipeClient::GetStringMember (first, "mnemonic"));
            Assert::AreEqual (std::string ("#$41"), PipeClient::GetStringMember (first, "operand"));
            Assert::AreEqual (std::string ("LOOP"), PipeClient::GetStringMember (first, "label"));
            Assert::IsTrue   (PipeClient::IsNullMember (first, "operandAddress"), L"an immediate has no operand address");
            Assert::IsTrue   (PipeClient::IsNullMember (first, "operandSymbol"));
            Assert::IsTrue   (first.HasArray ("bytes", data));
            Assert::AreEqual ((size_t) 2, data->GetArraySize());

            const JsonValue & jsr = lines->GetArrayElement (2);
            Assert::AreEqual (S_OK, jsr.GetInt ("operandAddress", value));
            Assert::AreEqual (0xFDED, value);
            Assert::AreEqual (std::string ("COUT"), PipeClient::GetStringMember (jsr, "operandSymbol"));
            Assert::AreEqual (S_OK, jsr.GetInt ("target", value));
            Assert::AreEqual (0xFDED, value);
            Assert::IsTrue   (PipeClient::IsNullMember (jsr, "label"), L"no symbol at $0305");
            Assert::AreEqual (S_OK, jsr.GetBool ("documented", isTrue));
            Assert::IsTrue   (isTrue);

            text = PipeClient::GetText (record).at (0);
            Assert::AreEqual (std::string ("0300: A9 41    LOOP LDA  #$41"), text, L"the text a client gets is the AppleWin layout");

            record = client.Send ("u", "windbg");
            Assert::AreEqual (std::string ("031f ea      nop"), PipeClient::GetText (record).at (0), L"a client's u in WinDbg mode continues the listing it made");
        }



        //  U reads memory without writing it, so it runs while the machine
        //  does, from the window and from a client.
        TEST_METHOD (U_WhileTheMachineRuns)
        {
            ControllerRig  rig;
            PipeClient     client (rig);
            DebugSession & session = rig.controller.GetSession();



            LoadProgram (session.GetTarget());

            //  An endless loop at $0300, run with no stop condition.
            session.GetTarget().TryPoke (0x0300, 0x4C);
            session.GetTarget().TryPoke (0x0301, 0x00);
            session.GetTarget().TryPoke (0x0302, 0x03);
            rig.Run ("G");
            WindowRun::RunSlices (rig, 3);
            Assert::IsTrue (rig.controller.GetRunDriver().IsRunning(), L"the run is going");

            Assert::AreEqual (std::string ("0300: 4C 00 03 JMP  $0300"), rig.Run ("U 300:300").text.at (0), L"the console during a run");
            Assert::AreEqual (std::string ("0300: 4C 00 03 JMP  $0300"), PipeClient::GetText (client.Send ("U 300:300")).at (0), L"a client during a run");

            rig.controller.RequestPause();
            WindowRun::Finish (rig);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Batch
        //
        ////////////////////////////////////////////////////////////////////////

        //  A batch run with --json writes a U's lines as JSON, with the
        //  fields the channel sends.
        TEST_METHOD (U_BatchJson_WritesTheSameRecord)
        {
            SessionRig   rig;
            Reply        reply;
            std::string  record;



            LoadProgram (rig.target);
            reply  = rig.RunOk ("U 305:305");
            record = ReplyJson::WriteReply (reply, std::nullopt);

            Assert::IsTrue (record.find (R"("mnemonic":"JSR","operand":"$FDED","operandAddress":65005,"operandSymbol":"COUT","target":65005,"label":null,"documented":true)") != std::string::npos,
                            std::wstring (record.begin(), record.end()).c_str());
        }
    };
}
