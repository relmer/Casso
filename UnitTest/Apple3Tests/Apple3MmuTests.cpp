#include "Pch.h"

#include "Apple3TestFixture.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3MmuTests
//
//  The Apple ///'s memory map: the environment register's ROM, I/O, write
//  protection and stack bits, the bank register, the zero page register,
//  the VIAs at $FFD0-$FFEF and the Apple II lockout.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3MmuTests)
{
public:

    static constexpr Byte  kEnvRom         = Apple3Mmu::kEnvRomSelect;
    static constexpr Byte  kEnvStack       = Apple3Mmu::kEnvPrimaryStack;
    static constexpr Byte  kEnvProtect     = Apple3Mmu::kEnvWriteProtect;
    static constexpr Byte  kEnvIo          = Apple3Mmu::kEnvIoEnable;
    static constexpr Byte  kSystemBank128K = 3;


    TEST_METHOD (EveryEnvironmentValue_MapsRomIoAndProtection)
    {
        Apple3TestFixture  f;



        for (int value = 0; value <= 0xFF; value++)
        {
            Byte  env       = (Byte) value;
            bool  rom       = (env & kEnvRom) != 0;
            bool  io        = (env & kEnvIo) != 0;
            bool  protect   = (env & kEnvProtect) != 0;
            Byte  before    = 0;



            f.mmu.SetEnvironment (0);
            f.bus.WriteByte (0xF000, 0x11);
            f.bus.WriteByte (0xD000, 0x22);
            f.bus.WriteByte (0xC080, 0x33);
            f.mmu.SetEnvironment (env);

            Assert::AreEqual ((Byte) (rom ? 0x01 : 0x11), f.bus.ReadByte (0xF000), L"$F000 by ROM select");

            f.io.m_readCount = 0;
            Assert::AreEqual ((Byte) (io ? RecordingDevice::kReadValue : 0x33), f.bus.ReadByte (0xC080), L"$C080 by I/O enable");
            Assert::AreEqual (io ? 1 : 0, f.io.m_readCount, L"I/O reads");

            before = f.mmu.PeekSystem (0xD000);
            f.bus.WriteByte (0xD000, (Byte) (before + 1));
            Assert::AreEqual ((Byte) (protect ? before : before + 1), f.mmu.PeekSystem (0xD000), L"$D000 by write protection");
        }
    }


    TEST_METHOD (RomSelected_WriteGoesToRamBeneath)
    {
        Apple3TestFixture  f;



        f.mmu.SetEnvironment (kEnvRom);
        f.bus.WriteByte (0xF123, 0x77);

        Assert::AreEqual (Apple3TestFixture::kRomFill, f.bus.ReadByte (0xF123));
        Assert::AreEqual ((Byte) 0x77, f.mmu.PeekSystem (0xF123));
    }


    TEST_METHOD (RomSelected_FfcxReadsRam)
    {
        Apple3TestFixture  f;



        f.bus.WriteByte (0xFFC4, 0x66);
        f.mmu.SetEnvironment (kEnvRom);

        Assert::AreEqual ((Byte) 0x66, f.bus.ReadByte (0xFFC4));
        Assert::AreEqual ((Byte) 0x02, f.bus.ReadByte (0xFFFF));
    }


    TEST_METHOD (WriteProtect_AppliesFromC000Up)
    {
        Apple3TestFixture  f;



        f.mmu.SetEnvironment (kEnvProtect | kEnvStack);
        f.bus.WriteByte (0xBFFF, 0x44);
        f.bus.WriteByte (0xC000, 0x45);

        Assert::AreEqual ((Byte) 0x44, f.mmu.PeekSystem (0xBFFF));
        Assert::AreNotEqual ((Byte) 0x45, f.mmu.PeekSystem (0xC000));
    }


    TEST_METHOD (IoEnabled_C500ThroughC7FFIsRam)
    {
        Apple3TestFixture  f;



        f.mmu.SetEnvironment (kEnvIo);
        f.bus.WriteByte (0xC600, 0x5E);
        f.bus.WriteByte (0xC400, 0x5F);

        Assert::AreEqual ((Byte) 0x5E, f.bus.ReadByte (0xC600));
        Assert::AreEqual ((Word) 0xC400, f.io.m_lastWrite);
        Assert::AreEqual (RecordingDevice::kReadValue, f.bus.ReadByte (0xC401));
        Assert::AreEqual ((Word) 0xC401, f.io.m_lastRead);
    }


    TEST_METHOD (Vias_AccessedWithIoOff_RamWhenLockedOut)
    {
        Apple3TestFixture  f;



        f.mmu.SetEnvironment (0);
        f.bus.WriteByte (0xFFDF, 0x12);

        Assert::AreEqual ((Word) 0xFFDF, f.vias.m_lastWrite);
        Assert::AreEqual (RecordingDevice::kReadValue, f.bus.ReadByte (0xFFEF));

        f.mmu.SetLockout (true);
        f.bus.WriteByte (0xFFE0, 0x34);

        Assert::AreEqual ((Byte) 0x34, f.bus.ReadByte (0xFFE0));
        Assert::AreEqual (1, f.vias.m_writeCount);
    }


    TEST_METHOD (Bank_SelectsTheSwitchedRegion)
    {
        Apple3TestFixture  f;



        for (Byte bank = 0; bank < 3; bank++)
        {
            f.mmu.SetBank (bank);
            f.bus.WriteByte (0x2000, (Byte) (0x40 + bank));
            f.bus.WriteByte (0x9FFF, (Byte) (0x50 + bank));
        }

        for (Byte bank = 0; bank < 3; bank++)
        {
            Assert::AreEqual ((Byte) (0x40 + bank), f.mmu.PeekPhysical (bank, 0x0000));
            Assert::AreEqual ((Byte) (0x50 + bank), f.mmu.PeekPhysical (bank, 0x7FFF));
        }

        Assert::AreEqual ((size_t) 3, f.mmu.GetBankCount());
    }


    TEST_METHOD (Bank_256KHasSevenBanks_PastTheLastWraps)
    {
        Apple3TestFixture  f (Apple3Mmu::kRam256K);



        f.mmu.SetBank (6);
        f.bus.WriteByte (0x2000, 0x66);
        f.mmu.SetBank (7);
        f.bus.WriteByte (0x2001, 0x72);

        Assert::AreEqual ((size_t) 7, f.mmu.GetBankCount());
        Assert::AreEqual ((Byte) 0x66, f.mmu.PeekPhysical (6, 0x0000));
        Assert::AreEqual ((Byte) 0x72, f.mmu.PeekPhysical (2, 0x0001));
    }


    TEST_METHOD (Bank_128KPastTheLastWraps)
    {
        Apple3TestFixture  f;



        f.mmu.SetBank (5);
        f.bus.WriteByte (0x2000, 0x55);
        f.mmu.SetBank (3);
        f.bus.WriteByte (0x2001, 0x33);

        Assert::AreEqual ((Byte) 0x55, f.mmu.PeekPhysical (1, 0x0000));
        Assert::AreEqual ((Byte) 0x33, f.mmu.PeekPhysical (2, 0x0001));
    }


    TEST_METHOD (SystemBank_HasLowAndHighRegions)
    {
        Apple3TestFixture  f;



        f.bus.WriteByte (0x1FFF, 0x1F);
        f.bus.WriteByte (0xA000, 0xA0);

        Assert::AreEqual ((Byte) 0x1F, f.mmu.PeekPhysical (kSystemBank128K, 0x1FFF));
        Assert::AreEqual ((Byte) 0xA0, f.mmu.PeekPhysical (kSystemBank128K, 0x2000));
    }


    TEST_METHOD (ZeroPage_FollowsTheRegister)
    {
        Apple3TestFixture  f;



        f.mmu.SetZeroPage (0x1A);
        f.bus.WriteByte (0x0005, 0x1A);
        f.mmu.SetZeroPage (0x18);
        f.bus.WriteByte (0x0005, 0x18);

        Assert::AreEqual ((Byte) 0x1A, f.mmu.PeekSystem (0x1A05));
        Assert::AreEqual ((Byte) 0x18, f.mmu.PeekSystem (0x1805));
        Assert::AreEqual ((Byte) 0x18, f.bus.ReadByte (0x0005));
    }


    TEST_METHOD (ZeroPage_InTheBankedRegionUsesTheCurrentBank)
    {
        Apple3TestFixture  f;



        f.mmu.SetBank (1);
        f.mmu.SetZeroPage (0x30);
        f.bus.WriteByte (0x0007, 0x37);
        f.mmu.SetZeroPage (0xB0);
        f.bus.WriteByte (0x0008, 0xB8);

        Assert::AreEqual ((Byte) 0x37, f.mmu.PeekPhysical (1, 0x1007));
        Assert::AreEqual ((Byte) 0xB8, f.mmu.PeekSystem (0xB008));
    }


    TEST_METHOD (Stack_PrimaryIsPageOne_AlternateIsZeroPageXorOne)
    {
        Apple3TestFixture  f;



        f.mmu.SetZeroPage (0x1A);
        f.mmu.SetEnvironment (kEnvStack);
        f.bus.WriteByte (0x0105, 0x01);
        f.mmu.SetEnvironment (0);
        f.bus.WriteByte (0x0105, 0x1B);
        f.mmu.SetZeroPage (0x1B);
        f.bus.WriteByte (0x0106, 0x1A);

        Assert::AreEqual ((Byte) 0x01, f.mmu.PeekSystem (0x0105));
        Assert::AreEqual ((Byte) 0x1B, f.mmu.PeekSystem (0x1B05));
        Assert::AreEqual ((Byte) 0x1A, f.mmu.PeekSystem (0x1A06));
    }


    static HRESULT CopyState (const Apple3Mmu & from, Apple3Mmu & to)
    {
        StateWriter  writer;
        HRESULT      hr = S_OK;



        hr = from.SaveState (writer);
        CHR (hr);

        {
            StateReader  reader (writer.GetBytes());



            hr = to.LoadState (reader);
            CHR (hr);

            Assert::IsTrue (reader.IsAtEnd());
        }

    Error:
        return hr;
    }


    TEST_METHOD (StateRoundTrip_RestoresRamAndRegisters)
    {
        Apple3TestFixture  f;
        Apple3TestFixture  g;
        HRESULT            hr = S_OK;



        f.mmu.SetBank (2);
        f.mmu.SetZeroPage (0x1A);
        f.mmu.SetEnvironment (kEnvRom | kEnvStack);
        f.bus.WriteByte (0x3000, 0x9C);

        hr = CopyState (f.mmu, g.mmu);
        Assert::AreEqual (S_OK, hr);

        Assert::AreEqual ((Byte) 2,    g.mmu.GetBank());
        Assert::AreEqual ((Byte) 0x1A, g.mmu.GetZeroPage());
        Assert::AreEqual ((Byte) 0x9C, g.bus.ReadByte (0x3000));
        Assert::AreEqual ((Byte) 0x01, g.bus.ReadByte (0xF000));
    }
};
