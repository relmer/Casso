#include "Pch.h"

#include "EmuTests/TextScreenScraper.h"
#include "WatchMonitorRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace WatchMonitorTests;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorHandlersTests
    //
    //  Monitor mode's own commands over a real machine, grouped by command:
    //  examine, deposit, L, V, the arithmetic, I and N, the input and output
    //  hooks, ^E and the colon that sets the registers, and R and W. Each is
    //  typed as a reader types it at the `*` prompt, through the real parser,
    //  handlers and formatter, and each asserts what the machine or the host
    //  file holds afterward as well as what was printed.
    //
    //  The ROM itself is the oracle for the Monitor's rules; the tests that put
    //  a line to it are in MonitorRomOracleTests.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorHandlersTests)
    {
    public:

        //  A batch session switched to Monitor mode the way a reader switches.
        struct MonitorRig : SessionRig
        {
            explicit MonitorRig (const std::string & machineId = "Apple2e") :
                SessionRig (machineId)
            {
                RunOk ("MODE MONITOR");
            }
        };



        //  LDA #$00 / LDX #$01 / STA $1234, then NOPs from $0307 to $0346.
        static void DepositProgram (SessionRig & rig)
        {
            std::string  line = "300: A9 00 A2 01 8D 34 12";



            for (int i = 0; i < 0x40; ++i)
            {
                line += " EA";
            }

            rig.RunOk (line);
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Examine
        //
        ////////////////////////////////////////////////////////////////////////

        //  One address is one byte; a range breaks its rows on eight-byte
        //  boundaries; hex is read in either case.
        TEST_METHOD (Examine_AnAddressAndARange)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F });

            rig.RunIs ("300",     { "0300- 00" });
            rig.RunIs ("30f",     { "030F- 0F" });
            rig.RunIs ("303.30F", { "0303- 03 04 05 06 07", "0308- 08 09 0A 0B 0C 0D 0E 0F" });
            rig.RunIs ("300.30f", { "0300- 00 01 02 03 04 05 06 07", "0308- 08 09 0A 0B 0C 0D 0E 0F" });
        }



        //  Each address on a line is examined, in order.
        TEST_METHOD (Examine_SeveralOnOneLine)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x41, 0x42 });
            rig.Poke (0x0400, { 0x43, 0x44 });

            rig.RunIs ("300 400",         { "0300- 41", "0400- 43" });
            rig.RunIs ("300.301 400.401", { "0300- 41 42", "0400- 43 44" });
        }



        //  A bare Return goes on from the byte after the last one shown to the
        //  end of its row, then a whole row at a time; `.addr` goes on to addr;
        //  `.` alone shows one byte; and after $FFFF comes $0000.
        TEST_METHOD (Examine_GoesOnFromTheLastByteShown)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                0x10, 0x11, 0x12, 0x13, 0x14, 0x15 });
            rig.Poke (0x0000, { 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7 });

            rig.RunIs ("300.302", { "0300- 00 01 02" });
            rig.RunIs ("",        { "0303- 03 04 05 06 07" });
            rig.RunIs (" ",       { "0308- 08 09 0A 0B 0C 0D 0E 0F" });
            rig.RunIs (".313",    { "0310- 10 11 12 13" });
            rig.RunIs (".",       { "0314- 14" });

            rig.RunIs ("FFFE", { std::format ("FFFE- {:02X}", rig.Peek (0xFFFE)) });
            rig.RunIs ("",     { std::format ("FFFF- {:02X}", rig.Peek (0xFFFF)) });
            rig.RunIs ("",     { "0000- A0 A1 A2 A3 A4 A5 A6 A7" });
        }



        //  A byte the debugger cannot read without disturbing the machine is
        //  ??, and each row carries the region of its first address.
        TEST_METHOD (Examine_TheIoPage_IsUnread)
        {
            MonitorRig  rig;



            rig.Poke (0xBFF8, { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 });

            rig.RunIs ("C000.C007", { "C000- ?? ?? ?? ?? ?? ?? ?? ??" });
            rig.RunIs ("BFF8.C007", { "BFF8- 01 02 03 04 05 06 07 08", "C000- ?? ?? ?? ?? ?? ?? ?? ??" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"BFFE.C001","data":{"kind":"memory","rows":[{"address":49150,"bytes":[7,8],"region":"mainRam"},{"address":49152,"bytes":[null,null],"region":"io"}]},"text":["BFFE- 07 08","C000- ?? ??"]})json"),
                              rig.Json ("BFFE.C001"));
        }



        //  A RANGE THAT ENDS BEFORE IT STARTS SHOWS ITS FIRST BYTE, as the
        //  ROM's XAM does: it prints A1 before it compares A1 with A2.
        TEST_METHOD (Examine_ARangeThatEndsBeforeItStarts_ShowsItsFirstByte)
        {
            MonitorRig  rig;



            rig.Poke (0x0310, { 0x5A });

            rig.RunIs ("310.300",   { "0310- 5A" });
            rig.RunIs ("FFF8.0007", { std::format ("FFF8- {:02X}", rig.Peek (0xFFF8)) });
        }



        //  The AppleWin output format has its own layout for the same rows.
        TEST_METHOD (Examine_InTheAppleWinOutputFormat)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x41, 0x42, 0x43 });

            rig.RunOk ("/OUTPUT APPLEWIN");
            rig.RunIs ("300.302", { "0300: 41 42 43                 ABC" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Deposit
        //
        ////////////////////////////////////////////////////////////////////////

        //  The bytes land in memory, and the reply shows them. A byte is one
        //  or two hex digits in either case, with or without a space after
        //  the colon, and a colon with no address goes on where the last
        //  deposit stopped.
        TEST_METHOD (Deposit_WritesTheBytes)
        {
            MonitorRig  rig;



            rig.RunIs ("300: A9 41 60", { "0300- A9 41 60" });
            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0300));
            Assert::AreEqual ((Byte) 0x41, rig.Peek (0x0301));
            Assert::AreEqual ((Byte) 0x60, rig.Peek (0x0302));

            rig.RunIs (": 42",    { "0303- 42" });
            rig.RunIs (":7 e",    { "0304- 07 0E" });
            Assert::AreEqual ((Byte) 0x42, rig.Peek (0x0303));
            Assert::AreEqual ((Byte) 0x07, rig.Peek (0x0304));
            Assert::AreEqual ((Byte) 0x0E, rig.Peek (0x0305));

            rig.RunIs ("400:a9", { "0400- A9" });
            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0400));
        }



        TEST_METHOD (Deposit_Errors)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x11 });

            rig.RunFails ("300:",       "invalid arguments", "A deposit takes one or more bytes.");
            rig.RunFails (":",          "invalid arguments", "A deposit takes one or more bytes.");
            rig.RunFails ("300: GG",    "invalid arguments", "A deposit takes hex bytes.");
            rig.RunFails ("300: 123",   "invalid arguments", "A deposit takes hex bytes.");
            Assert::AreEqual ((Byte) 0x11, rig.Peek (0x0300), L"a line that did not parse wrote nothing");

            //  ROM and I/O cannot be written. A deposit writes what it can up
            //  to the first address that refuses it.
            rig.RunFails ("F800: 01",    "memory not writable", "$F800 cannot be written.");
            rig.RunFails ("C000: 01",    "memory not writable", "$C000 cannot be written.");
            rig.RunFails ("BFFF: 5A 02", "memory not writable", "$C000 cannot be written.");
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0xBFFF), L"the byte before the I/O page was written");

            //  And the next colon goes on after the failed deposit, as the
            //  ROM's store address does.
            rig.RunFails (": 05", "memory not writable", "$C001 cannot be written.");
        }



        //  A deposit that passes $FFFF goes on at $0000, and the reply shows
        //  both rows. $FFFF is writable while language-card RAM is banked in.
        TEST_METHOD (Deposit_PastFFFF_WrapsToZeroPage)
        {
            MonitorRig  rig;



            rig.RunOk ("/IN C083");
            rig.RunOk ("/IN C083");

            rig.RunIs ("FFFF: 01 02", { "FFFF- 01", "0000- 02" });
            Assert::AreEqual ((Byte) 0x01, rig.Peek (0xFFFF));
            Assert::AreEqual ((Byte) 0x02, rig.Peek (0x0000));
        }



        //  A deposit and an examine go where the //e's memory switches send
        //  the CPU: with RAMWRT on, a write lands in auxiliary memory while
        //  reads still come from main.
        TEST_METHOD (Deposit_AndExamine_FollowTheMemorySwitches)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x11 });

            rig.RunOk ("/OUT C005 00");
            rig.RunIs ("300: 55", { "0300- 11" });
            rig.RunIs ("300",     { "0300- 11" });

            rig.RunOk ("/OUT C004 00");
            rig.RunOk ("/OUT C003 00");
            rig.RunIs ("300", { "0300- 55" });

            rig.RunOk ("/OUT C002 00");
            rig.RunIs ("300", { "0300- 11" });
        }



        TEST_METHOD (Deposit_AsJson)
        {
            MonitorRig  rig;



            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"300: 01 02","data":{"kind":"memory","rows":[{"address":768,"bytes":[1,2],"region":"mainRam"}]},"text":["0300- 01 02"]})json"),
                              rig.Json ("300: 01 02"));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  List
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (List_TwentyInstructionsInTheMonitorsLayout)
        {
            MonitorRig  rig;
            Reply       listed;



            DepositProgram (rig);

            listed = rig.RunOk ("300L");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::AreEqual (std::string ("0300-   A9 00       LDA   #$00"),  listed.text[0]);
            Assert::AreEqual (std::string ("0302-   A2 01       LDX   #$01"),  listed.text[1]);
            Assert::AreEqual (std::string ("0304-   8D 34 12    STA   $1234"), listed.text[2]);
            Assert::AreEqual (std::string ("0307-   EA          NOP"),         listed.text[3]);
            Assert::AreEqual (std::string ("0317-   EA          NOP"),         listed.text[19]);

            Check::TextIs (listed.text, rig.RunOk ("300l"), "300l");
        }



        //  THE ROM'S L NEVER READS A2: `300.305L` lists twenty instructions
        //  from $0300.
        TEST_METHOD (List_IgnoresTheEndOfARange)
        {
            MonitorRig  rig;
            Reply       whole;



            DepositProgram (rig);

            whole = rig.RunOk ("300L");
            Check::TextIs (whole.text, rig.RunOk ("300.305L"), "300.305L");
        }



        //  L alone goes on from where the last listing ended; before any
        //  listing, from the program counter.
        TEST_METHOD (List_AloneGoesOn)
        {
            MonitorRig  rig;
            Reply       listed;



            DepositProgram (rig);
            rig.RunOk ("/R PC 30A");

            listed = rig.RunOk ("L");
            Assert::AreEqual (std::string ("030A-   EA          NOP"), listed.text.at (0), L"the first L lists from the PC");

            rig.RunOk ("300L");
            listed = rig.RunOk ("L");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::AreEqual (std::string ("0318-   EA          NOP"), listed.text.at (0), L"then on from the last listing");

            listed = rig.RunOk ("L");
            Assert::AreEqual (std::string ("032C-"), listed.text.at (0).substr (0, 5));
        }



        //  The ROM's program counter is sixteen bits, so a listing that
        //  reaches $FFFF goes on at $0000 and still lists twenty.
        TEST_METHOD (List_WrapsPastFFFF)
        {
            MonitorRig  rig;
            Reply       listed;



            rig.RunOk ("/IN C083");
            rig.RunOk ("/IN C083");
            rig.RunOk ("FFF8: EA EA EA EA EA EA EA EA");
            rig.RunOk ("0: EA EA EA EA EA EA EA EA EA EA EA EA");

            listed = rig.RunOk ("FFF8L");
            Assert::AreEqual ((size_t) 20, listed.text.size());
            Assert::AreEqual (std::string ("FFFF-   EA          NOP"), listed.text[7]);
            Assert::AreEqual (std::string ("0000-   EA          NOP"), listed.text[8]);
            Assert::AreEqual (std::string ("000B-   EA          NOP"), listed.text[19]);

            listed = rig.RunOk ("L");
            Assert::AreEqual (std::string ("000C-"), listed.text.at (0).substr (0, 5));
        }



        //  A symbol is a label on its own line and a data block is data, as U
        //  lists them.
        TEST_METHOD (List_ShowsSymbolsAndDataBlocks)
        {
            MonitorRig  rig;
            Reply       listed;



            listed = rig.RunOk ("FDEDL");
            Assert::AreEqual (std::string ("COUT:"), listed.text.at (0));
            Check::TextIs (listed.text, rig.RunOk ("/U FDED"), "/U FDED");

            DepositProgram (rig);
            rig.RunOk ("/DB 308:30F");

            listed = rig.RunOk ("300L");
            Check::TextIs (listed.text, rig.RunOk ("/U 300"), "/U 300");

            for (const std::string & line : listed.text)
            {
                Assert::IsFalse (line.starts_with ("0309-"), L"the data block is one line, not instructions");
            }
        }



        TEST_METHOD (List_InTheAppleWinFormat_AndJson)
        {
            MonitorRig  rig;
            std::string  json;



            DepositProgram (rig);

            rig.RunOk ("/OUTPUT APPLEWIN");
            Assert::AreEqual (std::string ("0300: A9 00    LDA  #$00"), rig.RunOk ("300L").text.at (0));

            json = rig.Json ("300L");
            Assert::IsTrue (json.starts_with (R"json({"type":"reply","status":"ok","command":"300L","data":{"kind":"disassembly","lines":[)json"),
                            Check::Widen (json).c_str());
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Verify
        //
        ////////////////////////////////////////////////////////////////////////

        //  One line per difference: the range's byte, then the destination's
        //  in parentheses. Two ranges that match print nothing.
        TEST_METHOD (Verify_ReportsEachDifference)
        {
            MonitorRig  rig;



            rig.RunOk ("300: 41 42 43");
            rig.RunOk ("400: 41 99 43");

            rig.RunIs ("400<300.302V", { "0301-42 (99)" });
            rig.RunIs ("400<300.302v", { "0301-42 (99)" });

            rig.RunOk ("401: 42");
            rig.RunIs ("400<300.302V", {});

            rig.RunOk ("401: 99");
            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"400<300.302V","data":{"kind":"compare","compared":3,"differences":[{"address":769,"value":66,"other":1025,"otherValue":153}]},"text":["0301-42 (99)"]})json"),
                              rig.Json ("400<300.302V"));

            rig.RunOk ("/OUTPUT APPLEWIN");
            rig.RunIs ("400<300.302V", { "Compared 3 bytes, 1 differ.", "0301: 42  0401: 99" });
        }



        //  Without a destination the range is compared with $0000 on; the
        //  destination wraps at $FFFF; a range that ends before it starts
        //  compares its first byte, as the ROM's VFY does; and an I/O byte
        //  compares as $00.
        TEST_METHOD (Verify_Boundaries)
        {
            MonitorRig  rig;
            Reply       reply;



            rig.RunOk ("0: 11 22");
            rig.RunOk ("300: 11 23");
            rig.RunIs ("300.301V", { "0301-23 (22)" });

            rig.RunIs ("V", {});

            rig.RunOk ("/IN C083");
            rig.RunOk ("/IN C083");
            rig.RunOk ("FFFE: 11 99");
            rig.RunIs ("0<FFFE.FFFFV", { "FFFF-99 (22)" });

            rig.RunOk ("310: 5A");
            rig.RunOk ("400: A5");
            reply = rig.RunIs ("400<310.300V", { "0310-5A (A5)" });
            Assert::AreEqual ((uint32_t) 1, std::get<CompareData> (reply.data).compared, L"one byte compared");

            rig.RunOk ("400: 00 00 01");
            rig.RunIs ("400<C000.C002V", { "C002-00 (01)" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Arithmetic
        //
        ////////////////////////////////////////////////////////////////////////

        //  Eight bits, on the low bytes only; a missing operand is zero.
        TEST_METHOD (Arithmetic_IsEightBit)
        {
            MonitorRig  rig;



            rig.RunIs ("FF+FF",  { "=FE" });
            rig.RunIs ("20-01",  { "=1F" });
            rig.RunIs ("00-01",  { "=FF" });
            rig.RunIs ("1234+1", { "=35" });
            rig.RunIs ("FF+",    { "=FF" });
            rig.RunIs ("+5",     { "=05" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"FF+FF","data":{"kind":"calc","value":254},"text":["=FE"]})json"),
                              rig.Json ("FF+FF"));

            rig.RunOk ("/OUTPUT APPLEWIN");
            rig.RunIs ("FF+FF", { "$00FE  0z11111110    254  '~' (High)" });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  SetInverse and SetNormal
        //
        ////////////////////////////////////////////////////////////////////////

        //  INVFLG at $32 is the mask the ROM's COUT ands each character with,
        //  so I makes the next character printed inverse and N puts it back.
        //  Run on the ROM's own COUT after the Monitor has set the screen up.
        TEST_METHOD (InverseAndNormal_ChangeWhatCoutPrints)
        {
            SessionRig  rig ("Apple2");
            Word        at = 0;



            //  Power-on runs the original ]['s RESET, which is the Monitor.
            rig.RunOk ("G");
            rig.RunOk ("MODE MONITOR");

            //  LDA #"A" / JSR COUT / RTS
            rig.RunOk ("300: A9 C1 20 ED FD 60");

            rig.RunIs ("I", { "Inverse" });
            Assert::AreEqual ((Byte) 0x3F, rig.Peek (0x0032));

            rig.RunOk ("300G");
            at = (Word) (rig.PeekWord (0x0028) + rig.Peek (0x0024) - 1);
            Assert::AreEqual ((Byte) 0x01, rig.Peek (at), L"COUT printed an inverse A");

            rig.RunIs ("n", { "Normal" });
            Assert::AreEqual ((Byte) 0xFF, rig.Peek (0x0032));

            rig.RunOk ("300G");
            at = (Word) (rig.PeekWord (0x0028) + rig.Peek (0x0024) - 1);
            Assert::AreEqual ((Byte) 0xC1, rig.Peek (at), L"COUT printed a normal A");

            rig.RunIs ("300i", { "Inverse" });
            Assert::AreEqual ((Byte) 0x3F, rig.Peek (0x0032), L"an address before I changes nothing");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  SetInputSlot and SetOutputSlot
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (Hooks_PointAtASlotOrBackAtTheRom)
        {
            MonitorRig  rig;



            rig.RunIs ("3^K", { "Input hook: $C300" });
            Assert::AreEqual ((Word) 0xC300, rig.PeekWord (0x0038));

            rig.RunIs ("3^P", { "Output hook: $C300" });
            Assert::AreEqual ((Word) 0xC300, rig.PeekWord (0x0036));

            rig.RunIs ("0^K", { "Input hook: $FD1B" });
            Assert::AreEqual ((Word) 0xFD1B, rig.PeekWord (0x0038));

            rig.RunIs ("0^P", { "Output hook: $FDF0" });
            Assert::AreEqual ((Word) 0xFDF0, rig.PeekWord (0x0036));

            rig.RunIs ("6\x0B", { "Input hook: $C600" });
            rig.RunIs ("^K",    { "Input hook: $FD1B" });
            rig.RunIs ("2^p",   { "Output hook: $C200" });
            rig.RunIs ("7\x10", { "Output hook: $C700" });
            Assert::AreEqual ((Word) 0xFD1B, rig.PeekWord (0x0038));
            Assert::AreEqual ((Word) 0xC700, rig.PeekWord (0x0036));
        }



        //  THE SLOT IS THE LOW HEX DIGIT, as the ROM's IOPRT takes it.
        TEST_METHOD (Hooks_TakeTheLowDigit)
        {
            MonitorRig  rig;



            rig.RunIs ("13^K", { "Input hook: $C300" });
            Assert::AreEqual ((Word) 0xC300, rig.PeekWord (0x0038));

            rig.RunIs ("8^P",  { "Output hook: $C800" });
            Assert::AreEqual ((Word) 0xC800, rig.PeekWord (0x0036));

            rig.RunIs ("A^K",  { "Input hook: $CA00" });
            rig.RunIs ("10^P", { "Output hook: $FDF0" });
            Assert::AreEqual ((Word) 0xFDF0, rig.PeekWord (0x0036));
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ShowRegistersForEdit and EditRegisters
        //
        ////////////////////////////////////////////////////////////////////////

        //  ^E shows the CPU's registers; the colon after it sets A, X, Y, P
        //  and S in that order, as many as are given, and stores them at
        //  $45-$49 as well.
        TEST_METHOD (Registers_ShowThenSet)
        {
            static constexpr Byte  kSet[] = { 0x01, 0x02, 0x03, 0xB1, 0xE0 };
            MonitorRig             rig;
            Cpu6502Registers       registers;



            rig.RunOk ("/R A 12");
            rig.RunOk ("/R X 34");
            rig.RunOk ("/R Y 56");
            rig.RunOk ("/R P 30");
            rig.RunOk ("/R S F0");

            rig.RunIs ("^E",            { "A=12 X=34 Y=56 P=30 S=F0" });
            rig.RunIs (": 01 02 03 B1 E0", { "A=01 X=02 Y=03 P=B1 S=E0" });

            registers = rig.target.GetRegisters();
            Assert::AreEqual ((Byte) 0x01, registers.a);
            Assert::AreEqual ((Byte) 0x02, registers.x);
            Assert::AreEqual ((Byte) 0x03, registers.y);
            Assert::AreEqual ((Byte) 0xB1, registers.p);
            Assert::AreEqual ((Byte) 0xE0, registers.sp);

            for (Word i = 0; i < std::size (kSet); ++i)
            {
                Assert::AreEqual (kSet[i], rig.Peek ((Word) (0x0045 + i)), L"and the ROM's copy at $45-$49");
            }
        }



        //  Fewer than five set that many; more than five set five; zero is a
        //  value like any other.
        TEST_METHOD (Registers_AsManyAsGiven)
        {
            MonitorRig        rig;
            Cpu6502Registers  registers;



            rig.RunOk ("/R X 34");
            rig.Poke (0x004A, { 0x77 });

            rig.RunOk ("^e");
            rig.RunOk (": 07");
            registers = rig.target.GetRegisters();
            Assert::AreEqual ((Byte) 0x07, registers.a);
            Assert::AreEqual ((Byte) 0x34, registers.x, L"one value sets A alone");

            rig.RunOk ("\x05");
            rig.RunOk (": 00 00");
            registers = rig.target.GetRegisters();
            Assert::AreEqual ((Byte) 0x00, registers.a);
            Assert::AreEqual ((Byte) 0x00, registers.x);

            rig.RunOk ("^E");
            rig.RunOk (": 11 22 33 34 F5 66");
            registers = rig.target.GetRegisters();
            Assert::AreEqual ((Byte) 0xF5, registers.sp);
            Assert::AreEqual ((Byte) 0x77, rig.Peek (0x004A), L"the sixth value goes nowhere");
        }



        //  The arming is spent by the one colon that follows it, so the next
        //  colon deposits again. A colon whose bytes do not parse spends
        //  nothing.
        TEST_METHOD (Registers_TheArmingIsSpentByOneColon)
        {
            MonitorRig  rig;



            rig.Poke (0x0300, { 0x00, 0x00 });

            rig.RunOk ("^E");
            rig.RunFails (": GG", "invalid arguments", "A deposit takes hex bytes.");
            rig.RunIs (": 09", { std::format ("A=09 X={:02X} Y={:02X} P={:02X} S={:02X}",
                                             rig.target.GetRegisters().x, rig.target.GetRegisters().y,
                                             rig.target.GetRegisters().p, rig.target.GetRegisters().sp) });

            rig.RunIs ("300: 41", { "0300- 41" });
            rig.RunIs (": 42",    { "0301- 42" });
            Assert::AreEqual ((Byte) 0x09, rig.target.GetRegisters().a, L"the deposits did not touch A");

            rig.RunOk ("^E");
            rig.RunIs (":", { std::format ("A=09 X={:02X} Y={:02X} P={:02X} S={:02X}",
                                          rig.target.GetRegisters().x, rig.target.GetRegisters().y,
                                          rig.target.GetRegisters().p, rig.target.GetRegisters().sp) });
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  WriteFile
        //
        ////////////////////////////////////////////////////////////////////////

        TEST_METHOD (WriteFile_WritesTheRange)
        {
            MonitorRig  rig;



            rig.RunOk ("300: 01 02 03");
            rig.RunIs ("300.302W out.bin", { "out.bin: 3 of 3 bytes" });
            Assert::AreEqual (std::string ("\x01\x02\x03", 3), rig.files.PeekContent (L"out.bin"));

            rig.RunIs ("300w one.bin", { "one.bin: 1 of 1 bytes" });
            Assert::AreEqual (std::string ("\x01", 1), rig.files.PeekContent (L"one.bin"));

            //  A range that ends before it starts is its first byte.
            rig.RunIs ("302.300W back.bin", { "back.bin: 1 of 1 bytes" });
            Assert::AreEqual (std::string ("\x03", 1), rig.files.PeekContent (L"back.bin"));

            //  An I/O byte is not read, and is written as $00.
            rig.Poke (0xBFFE, { 0x7E, 0x7F });
            rig.RunIs ("BFFE.C001W io.bin", { "io.bin: 4 of 4 bytes" });
            Assert::AreEqual (std::string ("\x7E\x7F\x00\x00", 4), rig.files.PeekContent (L"io.bin"));

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"300.301W j.bin","data":{"kind":"fileIo","path":"j.bin","requested":2,"transferred":2,"mismatch":false},"text":["j.bin: 2 of 2 bytes"]})json"),
                              rig.Json ("300.301W j.bin"));
        }



        //  Quotes, double or single, keep a name with spaces together; a
        //  relative name is taken from the CD directory.
        TEST_METHOD (WriteFile_PathForms)
        {
            MonitorRig  rig;



            rig.RunOk ("300: 01 02");

            rig.RunIs ("300.301W \"my file.bin\"", { "my file.bin: 2 of 2 bytes" });
            Assert::IsTrue (rig.files.Exists (L"my file.bin"));

            rig.RunIs ("300.301W 'two words.bin'", { "two words.bin: 2 of 2 bytes" });
            Assert::IsTrue (rig.files.Exists (L"two words.bin"));

            rig.RunOk ("/CD C:\\Work");
            rig.RunOk ("300.301W f.bin");
            Assert::AreEqual (std::string ("\x01\x02", 2), rig.files.PeekContent (L"C:\\Work\\f.bin"));
        }



        TEST_METHOD (WriteFile_Errors)
        {
            MonitorRig  rig;



            rig.RunFails ("300.301W", "invalid arguments", "W takes a file name here. Only the debugger window can prompt for one.");
            rig.RunFails ("W out.bin", "invalid arguments", "W takes the range to write: first.lastW name.");

            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"ro.bin", "kept"));
            Assert::AreEqual (S_OK, rig.files.SetReadOnlyAttribute (L"ro.bin", true));
            rig.RunFails ("300.301W ro.bin", "file not written", "ro.bin could not be written.");
            Assert::AreEqual (std::string ("kept"), rig.files.PeekContent (L"ro.bin"));

            Assert::AreEqual ((size_t) 1, rig.files.FileCount(), L"no failed W wrote a file");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  ReadFile
        //
        ////////////////////////////////////////////////////////////////////////

        //  The smaller of the file and the range is read, and the reply says
        //  when their sizes differ, whichever is the larger.
        TEST_METHOD (ReadFile_ReadsIntoTheRange)
        {
            MonitorRig  rig;



            rig.RunOk ("400: 00 00 00 00 00 00 00 00 00 00 00 00");
            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"abc.bin", "abc"));
            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"ten.bin", "0123456789"));
            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"empty.bin", ""));

            rig.RunIs ("400.402R abc.bin", { "abc.bin: 3 of 3 bytes" });
            rig.RunIs ("400.402", { "0400- 61 62 63" });

            rig.RunIs ("404.407R abc.bin", { "abc.bin: 3 of 4 bytes; the file and the range differ in size" });
            rig.RunIs ("404.407", { "0404- 61 62 63 00" });

            rig.RunIs ("408.40Br ten.bin", { "ten.bin: 4 of 4 bytes; the file and the range differ in size" });
            rig.RunIs ("408.40F", { "0408- 30 31 32 33 00 00 00 00" });

            rig.RunIs ("40C.40FR empty.bin", { "empty.bin: 0 of 4 bytes; the file and the range differ in size" });

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"400.403R abc.bin","data":{"kind":"fileIo","path":"abc.bin","requested":4,"transferred":3,"mismatch":true},"text":["abc.bin: 3 of 4 bytes; the file and the range differ in size"]})json"),
                              rig.Json ("400.403R abc.bin"));
        }



        //  A RANGE THAT ENDS BEFORE IT STARTS IS ONE BYTE, never the whole
        //  file poked on through $FFFF into zero page.
        TEST_METHOD (ReadFile_ARangeThatEndsBeforeItStarts_ReadsOneByte)
        {
            MonitorRig  rig;



            rig.RunOk ("300: 00 00");
            rig.Poke (0x0000, { 0xEE });
            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"big.bin", std::string (0x10000, 'Z')));

            rig.RunIs ("300.200R big.bin", { "big.bin: 1 of 1 bytes; the file and the range differ in size" });
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x0300));
            Assert::AreEqual ((Byte) 0x00, rig.Peek (0x0301));
            Assert::AreEqual ((Byte) 0xEE, rig.Peek (0x0000), L"zero page was not reached");
        }



        TEST_METHOD (ReadFile_Errors)
        {
            MonitorRig  rig;



            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"x.bin", "\x01\x02\x03\x04"));

            rig.RunFails ("300.303R",             "invalid arguments",   "R takes a file name here. Only the debugger window can prompt for one.");
            rig.RunFails ("R x.bin",              "invalid arguments",   "R takes the range to read into: first.lastR name.");
            rig.RunFails ("300.303R missing.bin", "file not found",      "missing.bin could not be read.");
            rig.RunFails ("F800.F803R x.bin",     "memory not writable", "$F800 cannot be written.");

            rig.Poke (0xBFFE, { 0x00, 0x00 });
            rig.RunFails ("BFFE.C001R x.bin", "memory not writable", "$C000 cannot be written.");
            Assert::AreEqual ((Byte) 0x01, rig.Peek (0xBFFE), L"what fit before the I/O page was read");
            Assert::AreEqual ((Byte) 0x02, rig.Peek (0xBFFF));
        }



        //  Quotes are not part of the name, and a relative name is taken from
        //  the CD directory.
        TEST_METHOD (ReadFile_PathForms)
        {
            MonitorRig  rig;



            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"my file.bin", "\x11"));
            Assert::AreEqual (S_OK, rig.files.WriteAllText (L"C:\\Work\\w.bin", "\x22"));

            rig.RunIs ("300R \"my file.bin\"", { "my file.bin: 1 of 1 bytes" });
            Assert::AreEqual ((Byte) 0x11, rig.Peek (0x0300));

            rig.RunOk ("/CD C:\\Work");
            rig.RunIs ("301R w.bin", { "w.bin: 1 of 1 bytes" });
            Assert::AreEqual ((Byte) 0x22, rig.Peek (0x0301));
        }



        //  NO PRODUCTION SESSION LACKS A FILE SYSTEM, so a bare session is the
        //  only way to reach what R and W say without one.
        TEST_METHOD (FileCommands_WithNoFileSystem_SaySo)
        {
            MockDebugTarget            target;
            RecordingNotificationSink  sink;
            DebugSession               session (target, sink, RunState::Paused);
            DebugHandlerSet            handlers;



            handlers.Attach (session);
            session.ExecuteLine ("MODE MONITOR");

            Check::Fails (session.ExecuteLine ("300.301W f"), "W", CommandStatus::Error, "no file access", "This session cannot read or write host files.");
            Check::Fails (session.ExecuteLine ("300.301R f"), "R", CommandStatus::Error, "no file access", "This session cannot read or write host files.");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  Lines, modes and paths
        //
        ////////////////////////////////////////////////////////////////////////

        //  Several commands on a line run in order, their text in one reply.
        //  The merged reply has no data of its own.
        TEST_METHOD (Lines_SeveralCommandsRunInOrder)
        {
            MonitorRig  rig;



            rig.RunOk ("300: 41");
            rig.RunOk ("400: 42");

            rig.RunIs ("300 400 I 3^P", { "0300- 41", "0400- 42", "Inverse", "Output hook: $C300" });
            Assert::AreEqual ((Byte) 0x3F, rig.Peek (0x0032));
            Assert::AreEqual ((Word) 0xC300, rig.PeekWord (0x0036));

            Assert::AreEqual (std::string (R"json({"type":"reply","status":"ok","command":"300 400","data":{"kind":"message"},"text":["0300- 41","0400- 42"]})json"),
                              rig.Json ("300 400"));
        }



        //  A line that does not parse runs nothing on it.
        TEST_METHOD (Lines_ALineThatDoesNotParse_RunsNothing)
        {
            MonitorRig  rig;



            rig.Poke (0x0032, { 0xFF });

            rig.RunFails ("I Z", "invalid arguments", "Z is not a Monitor command.");
            Assert::AreEqual ((Byte) 0xFF, rig.Peek (0x0032), L"the I before the Z did not run");

            rig.RunFails ("I ^", "invalid arguments", "^ must be followed by a letter.");
            Assert::AreEqual ((Byte) 0xFF, rig.Peek (0x0032));
        }



        //  A `/` line is an AppleWin line, and `/` alone is nothing at all.
        TEST_METHOD (Lines_ASlashLineIsAnAppleWinLine)
        {
            MonitorRig  rig;
            Reply       reply;



            rig.RunIs ("/BPL", { "No breakpoints." });

            reply = rig.RunOk ("/");
            Assert::IsTrue (reply.text.empty());

            reply = rig.RunOk ("/   ");
            Assert::IsTrue (reply.text.empty());
        }



        //  THE MONITOR'S COMMANDS ARE MONITOR MODE'S. No other mode reads its
        //  syntax, and GSSquared's verify does nothing, as in GSSquared.
        TEST_METHOD (Modes_OnlyMonitorModeReadsTheMonitorsSyntax)
        {
            SessionRig  rig;



            Check::Fails (rig.Run ("300.30F"), "applewin", CommandStatus::Unknown, "unknown command", "300.30F is not a command.");

            rig.RunOk ("MODE CASSO");
            Check::Fails (rig.Run ("FF+FF"), "casso", CommandStatus::Unknown, "unknown command", "FF+FF is not a command.");

            rig.RunOk ("MODE GSSQUARED");
            Check::Fails (rig.Run ("verify"), "gssquared", CommandStatus::NotAvailable, "command not available",
                          "VERIFY is accepted by GSSquared and does nothing there either.");

            rig.RunOk ("MODE MONITOR");
            rig.RunOk ("FF+FF");
        }



        //  Batch mode started with --mode monitor reads Monitor lines from
        //  the first one.
        TEST_METHOD (Paths_ABatchScriptInMonitorMode)
        {
            ScriptRig  rig;



            rig.options.mode = "monitor";

            Assert::AreEqual (std::string ("*300: A9 41 60\n"
                                           "0300- A9 41 60\n"
                                           "*300.302W out.bin\n"
                                           "out.bin: 3 of 3 bytes\n"
                                           "*400.402R out.bin\n"
                                           "out.bin: 3 of 3 bytes\n"
                                           "*3^P\n"
                                           "Output hook: $C300\n"),
                              rig.Run ("300: A9 41 60\n300.302W out.bin\n400.402R out.bin\n3^P\n"));

            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0400));
            Assert::AreEqual ((Byte) 0x60, rig.Peek (0x0402));
            Assert::AreEqual ((Byte) 0xC3, rig.Peek (0x0037));
            Assert::AreEqual (std::string ("\xA9\x41\x60", 3), rig.files.PeekContent (L"out.bin"));
        }



        //  The window's console, in Monitor mode, runs the line as batch does.
        TEST_METHOD (Paths_TheConsoleInMonitorMode)
        {
            WindowRig  rig;



            rig.TypeOk ("MODE MONITOR");

            rig.TypeIs ("800: 12 34", { "0800- 12 34" });
            rig.TypeIs ("800.801",    { "0800- 12 34" });
            rig.TypeIs ("",           { "0802- " + std::format ("{:02X} {:02X} {:02X} {:02X} {:02X} {:02X}",
                                                                rig.Peek (0x0802), rig.Peek (0x0803), rig.Peek (0x0804),
                                                                rig.Peek (0x0805), rig.Peek (0x0806), rig.Peek (0x0807)) });
            rig.TypeIs ("5^K",        { "Input hook: $C500" });
            Assert::AreEqual ((Byte) 0xC5, rig.Peek (0x0039));
        }



        //  A channel request in Monitor mode runs as a Monitor line whatever
        //  the session's mode, and a bare Return from the channel goes on
        //  examining.
        TEST_METHOD (Paths_AChannelRequestInMonitorMode)
        {
            WindowRig  rig;



            Assert::AreEqual (std::string (R"json({"type":"reply","id":1,"status":"ok","command":"800: 12 34","data":{"kind":"memory","rows":[{"address":2048,"bytes":[18,52],"region":"mainRam"}]},"text":["0800- 12 34"]})json"),
                              rig.Request ("800: 12 34", "monitor"));
            Assert::AreEqual ((Byte) 0x34, rig.Peek (0x0801));

            Assert::IsTrue (rig.Request ("", "monitor").find (R"json("text":["0802- )json") != std::string::npos,
                            L"Return goes on from $0802");

            Assert::IsTrue (rig.Session().GetMode() == CommandMode::AppleWin, L"the session's own mode is unchanged");
        }



        ////////////////////////////////////////////////////////////////////////
        //
        //  While the machine runs
        //
        ////////////////////////////////////////////////////////////////////////

        //  A deposit, the register edit and R change the machine, and wait
        //  for it to stop; the commands that only read it run.
        TEST_METHOD (Running_WritesWaitForAStop_ReadsDoNot)
        {
            WindowRig  rig;



            rig.TypeOk ("MODE MONITOR");
            rig.TypeOk ("900: 5A");
            rig.rig.files.WriteAllText (L"f.bin", "\x77");

            rig.TypeOk ("G");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun);

            Check::Fails (rig.Type ("900: 01"), "900: 01", CommandStatus::Error, "machine running", ": changes registers or memory. Pause the machine first.");
            Check::Fails (rig.Type ("900R f.bin"), "900R", CommandStatus::Error, "machine running", "R changes registers or memory. Pause the machine first.");
            Assert::AreEqual ((Byte) 0x5A, rig.Peek (0x0900), L"nothing was written");

            rig.TypeIs ("900",          { "0900- 5A" });
            rig.TypeIs ("FF+FF",        { "=FE" });
            rig.TypeIs ("900<900.900V", {});
            rig.TypeIs ("900W g.bin",   { "g.bin: 1 of 1 bytes" });
            rig.TypeOk ("900L");
            rig.TypeOk ("^E");
            Check::Fails (rig.Type (": 01"), ": 01", CommandStatus::Error, "machine running", ": changes registers or memory. Pause the machine first.");

            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"none of it stopped the run");

            rig.Pause();
            Assert::IsTrue (rig.Session().GetRunState() == RunState::Paused);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MonitorRunTests
    //
    //  The Monitor's runs: G, the BASIC entries ^B and ^C, and the user vector
    //  ^Y. Each pushes the Monitor's return address and runs, so a program
    //  that ends in RTS comes back to $FF69 and stops there.
    //
    //  Nothing about the machine is set by hand: a batch session runs from
    //  power-on, and the window's runs go through the emulator's run driver,
    //  sliced as its frame loop slices them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MonitorRunTests)
    {
    public:

        static constexpr Word  kMonitorReentry = 0xFF69;



        //  JMP $0300 at $03F8, and LDA #$41 / RTS at $0300.
        static void SetUserVector (SessionRig & rig)
        {
            rig.RunOk ("3F8: 4C 00 03");
            rig.RunOk ("300: A9 41 60");
        }



        TEST_METHOD (Go_ReturnsToTheMonitorOnRts)
        {
            SessionRig  rig;
            Byte        sp = 0;



            rig.RunOk ("MODE MONITOR");
            rig.RunOk ("300: A9 41 60");
            sp = rig.target.GetRegisters().sp;

            rig.RunOk ("300G");

            Assert::AreEqual (kMonitorReentry, rig.target.GetRegisters().pc, L"stopped where the Monitor is re-entered");
            Assert::AreEqual ((Byte) 0x41, rig.target.GetRegisters().a, L"the program ran");
            Assert::AreEqual (sp, rig.target.GetRegisters().sp, L"the RTS took back what G pushed");
            Assert::AreEqual (kMonitorReentry, rig.sink.stops.back().pc);
        }



        //  THE CPU IS THE TRUTH. The ROM's own G reloads A, X, Y, P and S from
        //  $45-$49 before running; Casso's does not, so a register set in
        //  AppleWin mode survives a Monitor G.
        TEST_METHOD (Go_DoesNotReloadRegistersFromZeroPage)
        {
            SessionRig  rig;



            rig.RunOk ("MEB 300 EA 60");
            rig.RunOk ("R A 41");
            rig.RunOk ("MEB 45 99");
            rig.RunOk ("MODE MONITOR");

            rig.RunOk ("300G");

            Assert::AreEqual ((Byte) 0x41, rig.target.GetRegisters().a, L"the register the reader set, not the zero-page byte");
        }



        //  ^Y runs whatever the user vector holds, as a G does.
        TEST_METHOD (UserVector_RunsTheVectorAndComesBack)
        {
            SessionRig  rig;
            Reply       reply;
            Byte        sp = 0;



            rig.RunOk ("MODE MONITOR");
            SetUserVector (rig);
            sp = rig.target.GetRegisters().sp;

            reply = rig.RunOk ("^Y");
            Assert::IsTrue (reply.verb == DebugVerb::Go, L"^Y replies as the G it is");
            Assert::AreEqual (std::string ("^Y"), reply.command);

            Assert::AreEqual (kMonitorReentry, rig.target.GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x41, rig.target.GetRegisters().a);
            Assert::AreEqual (sp, rig.target.GetRegisters().sp);

            for (const char * form : { "^y", "\x19", "300^Y" })
            {
                rig.RunOk ("/R A 00");
                rig.RunOk (form);
                Assert::AreEqual ((Byte) 0x41, rig.target.GetRegisters().a, Check::Widen (form).c_str());
                Assert::AreEqual (kMonitorReentry, rig.target.GetRegisters().pc);
            }
        }



        //  ^B and ^C enter BASIC at $E000 and $E003, each a JMP into the
        //  interpreter. A breakpoint on where that JMP goes stops the run
        //  there, with the Monitor's return address on the stack under it.
        TEST_METHOD (BasicEntries_JumpThroughE000AndE003)
        {
            static constexpr std::pair<const char *, Word>  kEntries[] = { { "^B", 0xE000 }, { "^C", 0xE003 }, { "\x02", 0xE000 }, { "^c", 0xE003 } };



            for (const auto & [form, entry] : kEntries)
            {
                SessionRig  rig;
                Word        target = 0;
                Byte        sp     = 0;



                rig.RunOk ("MODE MONITOR");
                Assert::AreEqual ((Byte) 0x4C, rig.Peek (entry), L"the entry is a JMP");
                target = rig.PeekWord ((Word) (entry + 1));
                sp     = rig.target.GetRegisters().sp;

                rig.RunOk (std::format ("/BP {:04X}", target));
                rig.RunOk (form);

                Assert::AreEqual (target, rig.target.GetRegisters().pc, Check::Widen (form).c_str());
                Assert::IsTrue   (rig.sink.stops.back().breakpointId.has_value(), L"the breakpoint stopped it");
                Assert::AreEqual ((Byte) (sp - 2), rig.target.GetRegisters().sp, L"the return address is on the stack");
                Assert::AreEqual ((Word) (kMonitorReentry - 1), (Word) (rig.Peek ((Word) (0x0100 + (Byte) (sp - 1))) | (rig.Peek ((Word) (0x0100 + sp)) << 8)));
            }
        }



        //  With no breakpoint, BASIC runs to its own prompt and waits there,
        //  never returning, so the run ends on the budget as any batch run
        //  can. The original ]['s power-on runs its Monitor first, which sets
        //  up the screen BASIC prints on.
        TEST_METHOD (BasicEntries_RunBasicToItsPrompt)
        {
            SessionRig                rig ("Apple2");
            std::vector<std::string>  screen;



            rig.RunOk ("G");
            rig.RunOk ("MODE MONITOR");

            for (const char * form : { "^B", "^C" })
            {
                for (Word address = TextScreenScraper::kTextPage1; address < TextScreenScraper::kTextPage2; ++address)
                {
                    rig.Poke (address, { 0xA0 });
                }

                rig.RunOk (form);

                Assert::IsTrue (rig.sink.stops.back().reason == StopReason::Budget, Check::Widen (form).c_str());

                screen = TextScreenScraper::Scrape (rig.machine);
                Assert::IsTrue (std::any_of (screen.begin(), screen.end(), [] (const std::string & row) { return row.starts_with (">"); }),
                                Check::Widen (std::string (form) + ": Integer BASIC's > prompt is on the screen\n" + Check::Join (screen)).c_str());
            }
        }



        //  THROUGH THE WINDOW, a run is the emulator's: ^Y starts it and the
        //  reply comes back at once, and the frame loop's slices carry it to
        //  the return.
        TEST_METHOD (UserVector_FromTheConsole)
        {
            WindowRig  rig;



            rig.TypeOk ("MODE MONITOR");
            rig.TypeOk ("3F8: 4C 00 03");
            rig.TypeOk ("300: A9 41 60");

            rig.TypeOk ("^Y");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"the run is on foot");

            rig.RunSlices (100000);

            Assert::IsTrue   (rig.Session().GetRunState() == RunState::Paused, L"and ended");
            Assert::AreEqual (kMonitorReentry, rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x41, rig.Session().GetTarget().GetRegisters().a);
        }



        //  From a channel client, the reply says the run is still going, and
        //  the stop that ends it is sent as caused by that request.
        TEST_METHOD (UserVector_FromThePipe)
        {
            WindowRig    rig;
            std::string  reply;
            bool         isStopped = false;



            rig.TypeOk ("MODE MONITOR");
            rig.TypeOk ("3F8: 4C 00 03");
            rig.TypeOk ("300: A9 41 60");
            rig.TypeOk ("/MODE APPLEWIN");

            reply = rig.Request ("^Y", "monitor");
            Assert::IsTrue (reply.find (R"json("running":true)json") != std::string::npos, Check::Widen (reply).c_str());

            rig.RunSlices (100000);

            for (const std::string & record : rig.Received())
            {
                isStopped |= record.starts_with (R"json({"type":"stopped")json") && record.find (R"json("pc":65385)json") != std::string::npos
                             && record.find (R"json("causeId":1)json") != std::string::npos;
            }

            Assert::IsTrue (isStopped, L"the stop at $FF69 was sent, caused by the ^Y");
        }



        //  A run in progress is not started again, and a machine the user let
        //  run is adopted into the run.
        TEST_METHOD (Runs_WhileRunning)
        {
            WindowRig  rig;



            rig.TypeOk ("MODE MONITOR");
            rig.TypeOk ("3F8: 4C 00 03");
            rig.TypeOk ("300: A9 41 60");

            rig.TypeOk ("G");
            Check::Fails (rig.Type ("^Y"), "^Y", CommandStatus::Error, "already running", "A run is in progress. Use PAUSE to stop it.");
            Check::Fails (rig.Type ("^B"), "^B", CommandStatus::Error, "already running", "A run is in progress. Use PAUSE to stop it.");
            Check::Fails (rig.Type ("^C"), "^C", CommandStatus::Error, "already running", "A run is in progress. Use PAUSE to stop it.");
            rig.Pause();

            //  The user resumes the emulator; the host tells the session, as
            //  the emulator's pause toggle does.
            rig.rig.cpuManager.SetPaused (false);
            rig.Session().OnUserResumed();
            Assert::IsTrue (rig.Session().GetRunState() == RunState::FreeRunning);

            rig.TypeOk ("^Y");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"the free run became the debugger's");

            rig.RunSlices (100000);
            Assert::AreEqual (kMonitorReentry, rig.Session().GetTarget().GetRegisters().pc);
            Assert::AreEqual ((Byte) 0x41, rig.Session().GetTarget().GetRegisters().a);
        }



        //  IN THE WINDOW THE RUN IS ASYNCHRONOUS, so a command after ^Y on the
        //  same line meets a running machine: a deposit there waits for the
        //  stop, and writes nothing.
        TEST_METHOD (Runs_ALaterCommandOnTheLineMeetsTheRun)
        {
            WindowRig  rig;
            Reply      reply;



            rig.TypeOk ("MODE MONITOR");
            rig.TypeOk ("3F8: 4C 00 03");
            rig.TypeOk ("300: A9 41 60");

            reply = rig.Type ("^Y 300: 01");
            Assert::AreEqual ((int) CommandStatus::Error, (int) reply.status);
            Assert::AreEqual (std::string ("machine running"), reply.error.label);
            Assert::AreEqual (std::string (": changes registers or memory. Pause the machine first."), reply.error.detail);
            Assert::AreEqual ((Byte) 0xA9, rig.Peek (0x0300), L"the deposit did not happen");
            Assert::IsTrue (rig.Session().GetRunState() == RunState::DebugRun, L"the ^Y run is on foot");

            rig.RunSlices (100000);
            Assert::AreEqual (kMonitorReentry, rig.Session().GetTarget().GetRegisters().pc);
        }
    };
}
