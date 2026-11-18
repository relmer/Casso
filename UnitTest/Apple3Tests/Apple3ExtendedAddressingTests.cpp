#include "Pch.h"

#include "Apple3TestFixture.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ExtendedAddressingTests
//
//  The Apple ///'s enhanced indirect addressing through the CPU: the
//  extension byte at the pointer's high byte in the extension page, the
//  bank pair it selects, $8F, the current zero page for $0000-$00FF, the
//  zero page range that turns it on, and the mapping restored after the
//  instruction. Each case runs the (zp),Y and the (zp,X) form.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3ExtendedAddressingTests)
{
public:

    static constexpr Byte  kZeroPage     = 0x1A;
    static constexpr Byte  kExtensionXor = 0x0C;
    static constexpr Byte  kPointer      = 0x30;
    static constexpr Byte  kIndex        = 0x04;
    static constexpr Byte  kLdaIndY      = 0xB1;
    static constexpr Byte  kStaIndY      = 0x91;
    static constexpr Byte  kLdaXInd      = 0xA1;
    static constexpr Byte  kStaXInd      = 0x81;
    static constexpr Byte  kLdyImm       = 0xA0;
    static constexpr Byte  kLdxImm       = 0xA2;
    static constexpr Byte  kLdaImm       = 0xA9;
    static constexpr Byte  kSystemBank   = 3;

    enum class Form { IndirectY, XIndirect };


    //  Sets the zero page, the pointer at kPointer (or at kPointer + kIndex
    //  for the X form, where the pointer contains the full address) and
    //  its extension byte beside the pointer's high byte.
    static void SetPointer (Apple3TestFixture & f, Form form, Word address, Byte extension, Byte zeroPage = kZeroPage)
    {
        Byte  slot      = (form == Form::XIndirect) ? (Byte) (kPointer + kIndex) : kPointer;
        Word  target    = (form == Form::XIndirect) ? address : (Word) (address - kIndex);
        Byte  extPage   = (Byte) (zeroPage ^ kExtensionXor);



        f.mmu.SetZeroPage (zeroPage);
        f.bus.WriteByte ((Word) ((zeroPage << 8) | slot),              (Byte) (target & 0xFF));
        f.bus.WriteByte ((Word) ((zeroPage << 8) | (Byte) (slot + 1)), (Byte) (target >> 8));
        f.bus.WriteByte ((Word) ((extPage << 8)  | (Byte) (slot + 1)), extension);
    }


    //  Runs one load through the pointer and returns A.
    static Byte Load (Apple3TestFixture & f, Form form)
    {
        if (form == Form::IndirectY)
        {
            f.LoadCode ({ kLdyImm, kIndex, kLdaIndY, kPointer });
        }
        else
        {
            f.LoadCode ({ kLdxImm, kIndex, kLdaXInd, kPointer });
        }

        f.cpu.StepOne();
        f.cpu.StepOne();

        return f.cpu.GetA();
    }


    //  Runs one store of `value` through the pointer.
    static void Store (Apple3TestFixture & f, Form form, Byte value)
    {
        Byte  load  = (form == Form::IndirectY) ? kLdyImm  : kLdxImm;
        Byte  store = (form == Form::IndirectY) ? kStaIndY : kStaXInd;



        f.LoadCode ({ load, kIndex, kLdaImm, value, store, kPointer });

        f.cpu.StepOne();
        f.cpu.StepOne();
        f.cpu.StepOne();
    }


    //  Writes a byte at an offset of a switched bank, then switches bank 0
    //  back in.
    static void PokeBank (Apple3TestFixture & f, Byte bank, Word offset, Byte value)
    {
        f.mmu.SetBank (bank);
        f.bus.WriteByte ((Word) (0x2000 + offset), value);
        f.mmu.SetBank (0);
    }


    static void CheckLoadFromBankPair (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 1, 0x1234, 0x11);
        PokeBank   (f, 2, 0x1234, 0x22);
        SetPointer (f, form, 0x1234, 0x81);
        Assert::AreEqual ((Byte) 0x11, Load (f, form), L"$81:1234 is bank 1");

        SetPointer (f, form, 0x9234, 0x81);
        Assert::AreEqual ((Byte) 0x22, Load (f, form), L"$81:9234 is bank 2");
    }


    TEST_METHOD (Load_ReachesTheBankPair_IndirectY) { CheckLoadFromBankPair (Form::IndirectY); }
    TEST_METHOD (Load_ReachesTheBankPair_XIndirect) { CheckLoadFromBankPair (Form::XIndirect); }


    static void CheckExtensionOff (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 1, 0x1234, 0x11);
        f.bus.WriteByte (0x1234, 0x5B);
        SetPointer (f, form, 0x1234, 0x01);

        Assert::AreEqual ((Byte) 0x5B, Load (f, form));
    }


    TEST_METHOD (ExtensionBitClear_IsAnOrdinaryAccess_IndirectY) { CheckExtensionOff (Form::IndirectY); }
    TEST_METHOD (ExtensionBitClear_IsAnOrdinaryAccess_XIndirect) { CheckExtensionOff (Form::XIndirect); }


    static void CheckZeroPageOutsideRange (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 1, 0x1234, 0x11);
        f.bus.WriteByte (0x1234, 0x5C);

        //  Zero page $10 and $20 are outside $18-$1F; $20 is in the banked
        //  region, so its pointer is in bank 0.
        SetPointer (f, form, 0x1234, 0x81, 0x10);
        Assert::AreEqual ((Byte) 0x5C, Load (f, form), L"zero page $10");

        SetPointer (f, form, 0x1234, 0x81, 0x20);
        Assert::AreEqual ((Byte) 0x5C, Load (f, form), L"zero page $20");
    }


    TEST_METHOD (ZeroPageOutside18To1F_IsAnOrdinaryAccess_IndirectY) { CheckZeroPageOutsideRange (Form::IndirectY); }
    TEST_METHOD (ZeroPageOutside18To1F_IsAnOrdinaryAccess_XIndirect) { CheckZeroPageOutsideRange (Form::XIndirect); }


    static void CheckSystemArrangement (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 0, 0x1000, 0x30);
        PokeBank   (f, 1, 0x1000, 0x31);
        f.mmu.SetBank (1);
        f.mmu.SetEnvironment (Apple3Mmu::kEnvIoEnable | Apple3Mmu::kEnvPrimaryStack);

        SetPointer (f, form, 0x3000, 0x8F);
        Assert::AreEqual ((Byte) 0x30, Load (f, form), L"$8F:3000 is bank 0");

        SetPointer (f, form, 0xFFD4, 0x8F);
        Store      (f, form, 0x7E);
        Assert::AreEqual ((Byte) 0x7E, f.mmu.PeekSystem (0xFFD4), L"$8F:FFD4 is the RAM under the VIAs");
        Assert::AreEqual (0, f.vias.m_writeCount);

        SetPointer (f, form, 0xC080, 0x8F);
        Store      (f, form, 0x7F);
        Assert::AreEqual ((Byte) 0x7F, f.mmu.PeekSystem (0xC080), L"$8F:C080 is RAM, not I/O");
        Assert::AreEqual (0, f.io.m_writeCount);
    }


    TEST_METHOD (Extension8F_IsTheSystemBankAndBank0_IndirectY) { CheckSystemArrangement (Form::IndirectY); }
    TEST_METHOD (Extension8F_IsTheSystemBankAndBank0_XIndirect) { CheckSystemArrangement (Form::XIndirect); }


    static void CheckZeroPageTarget (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 1, 0x0040, 0x11);
        SetPointer (f, form, 0x0040, 0x81);
        f.bus.WriteByte ((Word) ((kZeroPage << 8) | 0x40), 0x4A);

        Assert::AreEqual ((Byte) 0x4A, Load (f, form));
    }


    TEST_METHOD (TargetInPageZero_IsTheCurrentZeroPage_IndirectY) { CheckZeroPageTarget (Form::IndirectY); }
    TEST_METHOD (TargetInPageZero_IsTheCurrentZeroPage_XIndirect) { CheckZeroPageTarget (Form::XIndirect); }


    static void CheckStoreAndRestore (Form form)
    {
        Apple3TestFixture  f;



        f.bus.WriteByte (0x5678, 0x00);
        SetPointer (f, form, 0x5678, 0x82);
        Store      (f, form, 0x6D);

        Assert::AreEqual ((Byte) 0x6D, f.mmu.PeekPhysical (2, 0x5678), L"stored in bank 2");
        Assert::AreEqual ((Byte) 0x00, f.bus.ReadByte (0x5678), L"bank 0 is switched in again");
    }


    TEST_METHOD (Store_ReachesTheBank_AndTheMapIsRestored_IndirectY) { CheckStoreAndRestore (Form::IndirectY); }
    TEST_METHOD (Store_ReachesTheBank_AndTheMapIsRestored_XIndirect) { CheckStoreAndRestore (Form::XIndirect); }


    static void CheckHighPageStore (Form form)
    {
        Apple3TestFixture  f;



        f.mmu.SetEnvironment (Apple3Mmu::kEnvIoEnable | Apple3Mmu::kEnvRomSelect | Apple3Mmu::kEnvPrimaryStack);
        SetPointer (f, form, 0xC123, 0x80);
        Store      (f, form, 0x5D);

        Assert::AreEqual ((Byte) 0x5D, f.mmu.PeekPhysical (1, 0x4123), L"$80:C123 is bank 1 at $4123");
        Assert::AreEqual (0, f.io.m_writeCount);
    }


    TEST_METHOD (StoreAtC000OrAbove_ReachesTheBank_IndirectY) { CheckHighPageStore (Form::IndirectY); }
    TEST_METHOD (StoreAtC000OrAbove_ReachesTheBank_XIndirect) { CheckHighPageStore (Form::XIndirect); }


    static void CheckAbsentBank (Form form)
    {
        Apple3TestFixture  f;



        PokeBank   (f, 1, 0x0444, 0x15);
        SetPointer (f, form, 0x0444, 0x85);
        Assert::AreEqual ((Byte) 0x15, Load (f, form), L"bank 5 wraps to bank 1 on 128K");

        SetPointer (f, form, 0x8000, 0x82);
        Store      (f, form, 0x2F);
        Assert::AreEqual ((Byte) 0x2F, f.mmu.PeekPhysical (kSystemBank, 0x0000), L"past bank 2 on 128K");
    }


    TEST_METHOD (PastTheHighestBank_Wraps_IndirectY) { CheckAbsentBank (Form::IndirectY); }
    TEST_METHOD (PastTheHighestBank_Wraps_XIndirect) { CheckAbsentBank (Form::XIndirect); }


    TEST_METHOD (OnlyIndirectInstructionsAreFlagged)
    {
        Apple3TestFixture  f;
        int                flagged = 0;



        for (int opcode = 0; opcode <= 0xFF; opcode++)
        {
            const Microcode  & microcode  = f.cpu.GetMicrocode ((Byte) opcode);
            bool               isIndirect = microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageXIndirect
                                         || microcode.globalAddressingMode == GlobalAddressingMode::ZeroPageIndirectY;



            Assert::AreEqual (microcode.isLegal && isIndirect, microcode.isExtendedIndirect);

            if (microcode.isExtendedIndirect)
            {
                flagged++;
            }
        }

        Assert::IsTrue (flagged > 0);
    }
};
