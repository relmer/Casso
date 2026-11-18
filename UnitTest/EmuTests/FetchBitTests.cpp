#include "Pch.h"

#include "Core/MemoryBus.h"
#include "Core/MemoryBusCpu.h"
#include "Core/Prng.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FetchBitTests
//
//  The bus's fetch bits: a bit for each byte of each buffer a page maps,
//  set by an opcode fetch from the byte the read table maps and cleared by a
//  write to the byte the write table maps, or to an operand of the
//  instruction there. The banks are the reason the bits are kept by buffer:
//  a write to one bank leaves the other's bits, and a bank switch changes
//  which bits an address has.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (FetchBitTests)
{
public:

    static constexpr Byte  kLda = 0xAD;    // LDA abs, three bytes
    static constexpr Byte  kNop = 0xEA;    // one byte


    //  Main and aux pages for page 3, as the MMU maps them, with an opcode
    //  table where $AD is three bytes and $EA one.
    struct Rig
    {
        MemoryBus          bus;
        std::vector<Byte>  main      = std::vector<Byte> (0x1000, 0);
        std::vector<Byte>  aux       = std::vector<Byte> (0x1000, 0);
        std::vector<Byte>  rom       = std::vector<Byte> (0x1000, 0);
        std::vector<Byte>  mainFlags = std::vector<Byte> (0x10, 0);
        std::vector<Byte>  auxFlags  = std::vector<Byte> (0x10, 0);

        Rig()
        {
            std::array<uint8_t, 256>  lengths;



            lengths.fill (1);
            lengths[kLda] = 3;
            bus.SetOpcodeLengths (lengths);
            bus.RegisterRamPages (nullptr, main.data(), main.size(), mainFlags.data());
            bus.RegisterRamPages (nullptr, aux.data(),  aux.size(),  auxFlags.data());
            MapBoth (main);
        }

        void  MapBoth  (std::vector<Byte> & buffer) { bus.SetReadPage (3, &buffer[0x300]); bus.SetWritePage (3, &buffer[0x300]); }
        void  MapRead  (std::vector<Byte> & buffer) { bus.SetReadPage (3, &buffer[0x300]); }
        void  MapWrite (std::vector<Byte> & buffer) { bus.SetWritePage (3, &buffer[0x300]); }
    };


    //  An instruction at the end of page 2 whose operand is in page 3: a
    //  write to the operand's byte clears the instruction's bit.
    TEST_METHOD (AWriteToAnOperandInTheNextPageClearsTheInstruction)
    {
        Rig  rig;



        rig.bus.SetReadPage  (2, &rig.main[0x200]);
        rig.bus.SetWritePage (2, &rig.main[0x200]);
        rig.main[0x2FF] = kLda;
        rig.bus.NoteFetch (0x02FF);

        rig.bus.WriteByte (0x0301, kNop);

        Assert::IsFalse (rig.bus.IsFetched (0x02FF), L"the LDA at $02FF runs on to $0301");
    }


    //  A write to the bank the card writes while it reads ROM clears the RAM
    //  bank's bit, and the ROM's bit is there afterward.
    TEST_METHOD (AWriteToTheRamBankWhileReadingRomLeavesTheRomsBit)
    {
        Rig  rig;



        rig.MapBoth (rig.main);
        rig.bus.NoteFetch (0x0300);
        rig.MapRead (rig.rom);
        rig.bus.NoteFetch (0x0300);

        rig.bus.WriteByte (0x0300, kNop);

        Assert::IsTrue  (rig.bus.IsFetched (0x0300), L"the ROM's bit");
        rig.MapRead (rig.main);
        Assert::IsFalse (rig.bus.IsFetched (0x0300), L"the RAM bank's bit is cleared");
    }


    //  With RAMWRT on while reading main memory, a write clears the aux bit,
    //  and the main bit is there afterward.
    TEST_METHOD (AWriteWithRamwrtClearsTheAuxBitAlone)
    {
        Rig  rig;



        rig.MapBoth (rig.aux);
        rig.bus.NoteFetch (0x0300);
        rig.MapBoth (rig.main);
        rig.bus.NoteFetch (0x0300);
        rig.MapWrite (rig.aux);

        rig.bus.WriteByte (0x0300, kNop);

        Assert::IsTrue  (rig.bus.IsFetched (0x0300), L"main's bit");
        rig.MapRead (rig.aux);
        Assert::IsFalse (rig.bus.IsFetched (0x0300), L"aux's bit is cleared");
    }


    //  A write to an instruction's operand clears its opcode's bit; a write
    //  past the instruction doesn't.
    TEST_METHOD (AWriteToAnOperandClearsItsOpcodesBit)
    {
        Rig  rig;



        rig.main[0x300] = kLda;
        rig.main[0x310] = kNop;
        rig.bus.NoteFetch (0x0300);
        rig.bus.NoteFetch (0x0310);

        rig.bus.WriteByte (0x0303, 0);
        Assert::IsTrue  (rig.bus.IsFetched (0x0300), L"past the LDA's three bytes");

        rig.bus.WriteByte (0x0311, 0);
        Assert::IsTrue  (rig.bus.IsFetched (0x0310), L"past the NOP's one byte");

        rig.bus.WriteByte (0x0302, 0);
        Assert::IsFalse (rig.bus.IsFetched (0x0300), L"the LDA's operand");
    }


    //  A host write clears the bits as a guest write does; a power cycle
    //  clears the RAM bits and keeps the ROM's; a bank switch changes which
    //  bits an address has.
    TEST_METHOD (HostWritesPowerCyclesAndBankSwitches)
    {
        Rig   rig;
        Prng  prng (1);



        rig.bus.NoteFetch (0x0300);
        rig.bus.MarkWritten (0x0300);
        Assert::IsFalse (rig.bus.IsFetched (0x0300), L"a debugger edit clears it");

        rig.bus.NoteFetch (0x0301);
        rig.bus.MarkPointerWritten (&rig.main[0x301]);
        Assert::IsFalse (rig.bus.IsFetched (0x0301), L"a store straight into the buffer clears it");

        rig.bus.NoteFetch (0x0302);
        rig.MapRead (rig.rom);
        rig.bus.NoteFetch (0x0302);
        rig.bus.PowerCycleAll (prng);
        Assert::IsTrue  (rig.bus.IsFetched (0x0302), L"the ROM's bit stays");
        rig.MapRead (rig.main);
        Assert::IsFalse (rig.bus.IsFetched (0x0302), L"a power cycle clears the RAM's");

        rig.bus.NoteFetch (0x0304);
        rig.MapBoth (rig.aux);
        Assert::IsFalse (rig.bus.IsFetched (0x0304), L"aux's bit, which was never set");
        rig.MapBoth (rig.main);
        Assert::IsTrue  (rig.bus.IsFetched (0x0304), L"main's, back");

        rig.bus.ClearAllFetchBits();
        Assert::IsFalse (rig.bus.IsFetched (0x0304), L"a machine switch clears every bit");
    }


    //  The CPU sets the bit of each opcode it fetches, and not an operand's.
    TEST_METHOD (TheCpuSetsTheBitOfEachOpcodeItFetches)
    {
        MemoryBus     bus;
        MemoryBusCpu  cpu (bus);



        cpu.PokeByte (0x0300, kNop);
        cpu.PokeByte (0x0301, kLda);
        cpu.PokeByte (0x0302, 0x00);
        cpu.PokeByte (0x0303, 0x20);
        cpu.SetPC    (0x0300);

        cpu.StepOne();
        cpu.StepOne();

        Assert::IsTrue  (bus.IsFetched (0x0300));
        Assert::IsTrue  (bus.IsFetched (0x0301));
        Assert::IsFalse (bus.IsFetched (0x0302), L"an operand");
    }
};
