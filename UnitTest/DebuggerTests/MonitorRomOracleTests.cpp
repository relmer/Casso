#include "Pch.h"

#include "MonitorRomOracle.h"
#include "WatchMonitorRigs.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace WatchMonitorTests;





////////////////////////////////////////////////////////////////////////////////
//
//  MonitorRomOracleTests
//
//  MONITOR MODE IS HELD TO THE ROM. Each test types a line at the original
//  Apple II's own `*` prompt, reads what the ROM printed and did, then puts
//  the same line to Casso's Monitor mode over the same memory and asserts the
//  two agree. Where Casso departs from the ROM on purpose -- the register
//  line, which shows the CPU rather than $45-$49 -- nothing here asks.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (MonitorRomOracleTests)
    {
    public:

        //  LDA #$00 / LDX #$01 / STA $1234 at $0800, then NOPs.
        static void SetProgram (OracleRig & rig)
        {
            rig.Poke (0x0800, { 0xA9, 0x00, 0xA2, 0x01, 0x8D, 0x34, 0x12 });

            for (Word address = 0x0807; address < 0x0860; ++address)
            {
                rig.Poke (address, { 0xEA });
            }
        }



        static void AssertSame (OracleRig & rig, const std::string & line)
        {
            std::vector<std::string>  rom   = rig.Rom (line);
            std::vector<std::string>  casso = rig.Casso (line);



            Assert::IsFalse  (rom.empty(), Check::Widen ("the ROM printed nothing for " + line).c_str());
            Assert::AreEqual (Check::Join (rom), Check::Join (casso), Check::Widen (line).c_str());
        }



        TEST_METHOD (Examine_ARangeThatEndsBeforeItStarts)
        {
            OracleRig  rig;



            rig.Poke (0x0810, { 0x5A });
            AssertSame (rig, "810.800");
            AssertSame (rig, "FFF8.0007");
        }



        TEST_METHOD (Examine_RangesAndReturn)
        {
            OracleRig  rig;



            for (Word address = 0x0800; address < 0x0820; ++address)
            {
                rig.Poke (address, { (Byte) address });
            }

            AssertSame (rig, "803.80F");
            AssertSame (rig, "800.802");
            AssertSame (rig, "");
            AssertSame (rig, "");
            AssertSame (rig, ".813");
            AssertSame (rig, "800 810");
        }



        TEST_METHOD (List_TwentyInstructions_WhateverTheRangeEnd)
        {
            OracleRig  rig;



            SetProgram (rig);

            AssertSame (rig, "800L");
            AssertSame (rig, "800.805L");
        }



        TEST_METHOD (List_AloneGoesOnFromTheLastListing)
        {
            OracleRig  rig;



            SetProgram (rig);

            AssertSame (rig, "800L");
            AssertSame (rig, "L");
        }



        TEST_METHOD (Verify_TheSourceByteThenTheDestinations)
        {
            OracleRig  rig;



            rig.Poke (0x0810, { 0x5A, 0x11, 0x12 });
            rig.Poke (0x0900, { 0xA5, 0x11, 0x99 });

            AssertSame (rig, "900<810.812V");
            AssertSame (rig, "900<810.800V");
        }



        TEST_METHOD (Arithmetic_EightBits)
        {
            OracleRig  rig;



            for (const char * line : { "FF+FF", "20-01", "00-01", "1234+1", "FF+", "+5" })
            {
                AssertSame (rig, line);
            }
        }



        //  The ROM's own IN# and PR# routines, run with the value typed, set
        //  the hooks Casso's ^K and ^P must set: IOPRT keeps only the low hex
        //  digit.
        TEST_METHOD (Hooks_MatchTheRomsIoprt)
        {
            static constexpr std::pair<Byte, Word>  kCalls[] = { { 0x13, 0xFE8B }, { 0x08, 0xFE95 }, { 0x1A, 0xFE8B }, { 0x10, 0xFE95 } };
            OracleRig                               rig;



            for (const auto & [slot, routine] : kCalls)
            {
                bool         isInput = routine == 0xFE8B;
                Word         hook    = isInput ? 0x0038 : 0x0036;
                Word         rom     = 0;
                std::string  line    = std::format ("{:X}{}", slot, isInput ? "^K" : "^P");



                //  LDA #slot / JSR routine / RTS, run as a Monitor G.
                rig.Poke (0x0300, { 0xA9, slot, 0x20, (Byte) (routine & 0xFF), (Byte) (routine >> 8), 0x60 });
                rig.Casso ("300G");
                rom = (Word) (rig.Peek (hook) | (rig.Peek ((Word) (hook + 1)) << 8));

                rig.Poke (hook, { 0x00, 0x00 });
                rig.Casso (line);

                Assert::AreEqual (rom, (Word) (rig.Peek (hook) | (rig.Peek ((Word) (hook + 1)) << 8)), Check::Widen (line).c_str());
            }
        }



        //  I and N set INVFLG as the ROM's SETINV and SETNORM do.
        TEST_METHOD (InverseAndNormal_MatchSetinvAndSetnorm)
        {
            OracleRig  rig;
            Byte       rom = 0;



            for (const auto & [line, routine] : { std::pair<const char *, Word> { "I", 0xFE80 }, std::pair<const char *, Word> { "N", 0xFE84 } })
            {
                rig.Poke (0x0300, { 0x20, (Byte) (routine & 0xFF), (Byte) (routine >> 8), 0x60 });
                rig.Poke (0x0032, { 0x00 });
                rig.Casso ("300G");
                rom = rig.Peek (0x0032);

                rig.Poke (0x0032, { 0x00 });
                rig.Casso (line);

                Assert::AreEqual (rom, rig.Peek (0x0032), Check::Widen (line).c_str());
            }
        }
    };
}
