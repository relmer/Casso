#include "Pch.h"

#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;



namespace AddressingModeTests
{


    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ImmediateModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ImmediateModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Immediate_LoadsValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Immediate_LoadsValue)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0xA9, 0x42 });   // LDA #$42

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDX_Immediate_LoadsValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDX_Immediate_LoadsValue)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0xA2, 0x10 });   // LDX #$10

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x10, cpu.RegX ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDY_Immediate_LoadsValue
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDY_Immediate_LoadsValue)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, { 0xA0, 0x20 });   // LDY #$20

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x20, cpu.RegY ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ZeroPageModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ZeroPageModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPage_LoadsFromZP
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPage_LoadsFromZP)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Poke (0x42, 0xBB);
            cpu.WriteBytes (0x8000, { 0xA5, 0x42 });   // LDA $42

            cpu.Step ();

            Assert::AreEqual ((Byte) 0xBB, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_ZeroPage_Stores
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_ZeroPage_Stores)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xA9, 0x77,     // LDA #$77
                0x85, 0x30      // STA $30
            });

            cpu.StepN (2);

            Assert::AreEqual ((Byte) 0x77, cpu.Peek (0x30));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ZeroPageXModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ZeroPageXModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZeroPageX_IndexesWithX
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZeroPageX_IndexesWithX)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x05;
            cpu.Poke (0x15, 0xCC);
            cpu.WriteBytes (0x8000, { 0xB5, 0x10 });   // LDA $10,X

            cpu.Step ();

            Assert::AreEqual ((Byte) 0xCC, cpu.RegA ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AbsoluteModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AbsoluteModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_Absolute_LoadsFromAddress
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_Absolute_LoadsFromAddress)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.Poke (0x1234, 0xAA);
            cpu.WriteBytes (0x8000, { 0xAD, 0x34, 0x12 });   // LDA $1234

            cpu.Step ();

            Assert::AreEqual ((Byte) 0xAA, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  STA_Absolute_Stores
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (STA_Absolute_Stores)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xA9, 0x55,             // LDA #$55
                0x8D, 0x00, 0x20        // STA $2000
            });

            cpu.StepN (2);

            Assert::AreEqual ((Byte) 0x55, cpu.Peek (0x2000));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  AbsoluteIndexedModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (AbsoluteIndexedModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_AbsoluteX_IndexesWithX
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_AbsoluteX_IndexesWithX)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x06;
            cpu.Poke (0x123A, 0x99);
            cpu.WriteBytes (0x8000, { 0xBD, 0x34, 0x12 });   // LDA $1234,X

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x99, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_AbsoluteY_IndexesWithY
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_AbsoluteY_IndexesWithY)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegY () = 0x0C;
            cpu.Poke (0x1240, 0x88);
            cpu.WriteBytes (0x8000, { 0xB9, 0x34, 0x12 });   // LDA $1234,Y

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x88, cpu.RegA ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ZeroPageXIndirectModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ZeroPageXIndirectModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZpXIndirect_DoubleIndirection
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZpXIndirect_DoubleIndirection)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegX () = 0x04;
            cpu.PokeWord (0x14, 0x2345);    // ($10 + X) -> pointer at $14
            cpu.Poke (0x2345, 0x77);         // Target value

            cpu.WriteBytes (0x8000, { 0xA1, 0x10 });   // LDA ($10,X)

            cpu.Step ();

            Assert::AreEqual ((Byte) 0x77, cpu.RegA ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ZeroPageIndirectYModeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ZeroPageIndirectYModeTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  LDA_ZpIndirectY_PostIndexed
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (LDA_ZpIndirectY_PostIndexed)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.RegY () = 0x10;
            cpu.PokeWord (0x20, 0x3000);    // Pointer at ZP $20
            cpu.Poke (0x3010, 0xEE);         // $3000 + Y

            cpu.WriteBytes (0x8000, { 0xB1, 0x20 });   // LDA ($20),Y

            cpu.Step ();

            Assert::AreEqual ((Byte) 0xEE, cpu.RegA ());
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  BranchIntegrationTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (BranchIntegrationTests)
    {
    public:


        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BNE_Forward_SkipsBytes
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BNE_Forward_SkipsBytes)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xA9, 0x01,     // LDA #$01   (clears Z)
                0xD0, 0x02,     // BNE +2     (skip next 2 bytes)
                0xA9, 0xFF,     // LDA #$FF   (skipped)
                0xA9, 0x42      // LDA #$42   (lands here)
            });

            cpu.StepN (3);      // LDA, BNE, LDA

            Assert::AreEqual ((Byte) 0x42, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  BEQ_NotTaken_FallsThrough
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BEQ_NotTaken_FallsThrough)
        {
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xA9, 0x01,     // LDA #$01   (Z clear)
                0xF0, 0x02,     // BEQ +2     (not taken)
                0xA9, 0xAA      // LDA #$AA   (executes)
            });

            cpu.StepN (3);      // LDA, BEQ (fall-through), LDA

            Assert::AreEqual ((Byte) 0xAA, cpu.RegA ());
        }





        ////////////////////////////////////////////////////////////////////////////////
        //
        //  DEX_BMI_Loop
        //
        ////////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (DEX_BMI_Loop)
        {
            // LDX #3, then DEX loop until negative
            TestCpu cpu;
            cpu.InitForTest ();
            cpu.WriteBytes (0x8000, {
                0xA2, 0x03,                 // LDX #$03
                0xCA,                       // DEX
                0x10, (Byte) (~0x03 + 1)    // BPL -3 (back to DEX)
            });

            // LDX, DEX(2), BPL, DEX(1), BPL, DEX(0), BPL, DEX(FF), BPL(not taken)
            cpu.StepN (9);

            Assert::AreEqual ((Byte) 0xFF, cpu.RegX ());
            Assert::IsTrue ((bool) cpu.Status ().flags.negative);
        }
    };
}
