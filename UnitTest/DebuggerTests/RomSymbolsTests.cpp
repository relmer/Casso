#include "Pch.h"

#include "Debugger/RomSymbols.h"
#include "Debugger/SymbolTable.h"
#include "Core/TextEncoding.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RomSymbolsTests
//
//  The shipped tables load, are non-empty, and hold the entry points spot
//  checked here.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (RomSymbolsTests)
    {
    public:

        static size_t Load (SymbolTable & table, SymbolTableId id, const char * text)
        {
            std::string  error;
            size_t       loaded = 0;
            HRESULT      hr     = table.LoadFrom (id, text, 0, loaded, error);



            Assert::AreEqual (S_OK, hr, TextEncoding::NarrowToWide (error).c_str());
            Assert::IsTrue   (loaded > 0, L"a shipped table is not empty");
            return loaded;
        }

        static Word Resolve (const SymbolTable & table, SymbolTableId id, const char * name)
        {
            Word  address = 0;



            Assert::IsTrue (table.TryResolveIn (id, name, address), std::wstring (name, name + strlen (name)).c_str());
            return address;
        }



        TEST_METHOD (Monitor_SpotEntries_EveryMachine)
        {
            static constexpr const char * kMachines[] = { "Apple2", "Apple2Plus", "Apple2e", "Apple2eEnhanced", "Apple2c" };
            size_t                        checked     = 0;



            for (const char * machine : kMachines)
            {
                SymbolTable  table;



                Load (table, SymbolTableId::Main, RomSymbols::GetMain (machine));

                Assert::AreEqual ((Word) 0xFDED, Resolve (table, SymbolTableId::Main, "COUT"));
                Assert::AreEqual ((Word) 0xFD6A, Resolve (table, SymbolTableId::Main, "GETLN"));
                Assert::AreEqual ((Word) 0xFF69, Resolve (table, SymbolTableId::Main, "MONZ"));
                Assert::AreEqual ((Word) 0xFC58, Resolve (table, SymbolTableId::Main, "HOME"));
                Assert::AreEqual ((Word) 0xC030, Resolve (table, SymbolTableId::Main, "SPKR"));
                Assert::AreEqual ((Word) 0x0036, Resolve (table, SymbolTableId::Main, "CSWL"));
                ++checked;
            }

            Assert::AreEqual (std::size (kMachines), checked);
        }



        TEST_METHOD (Iie_AddsTheMemorySwitches_IiDoesNot)
        {
            SymbolTable  iie;
            SymbolTable  ii;
            Word         address = 0;



            Load (iie, SymbolTableId::Main, RomSymbols::GetMain ("Apple2e"));
            Load (ii,  SymbolTableId::Main, RomSymbols::GetMain ("Apple2Plus"));

            Assert::AreEqual ((Word) 0xC019, Resolve (iie, SymbolTableId::Main, "RDVBL"));
            Assert::AreEqual ((Word) 0xC003, Resolve (iie, SymbolTableId::Main, "RAMRDON"));
            Assert::IsFalse  (ii.TryResolveIn (SymbolTableId::Main, "RDVBL", address), L"a ][+ has no MMU");
            Assert::IsTrue   (iie.GetCount (SymbolTableId::Main) > ii.GetCount (SymbolTableId::Main));
            Assert::AreEqual (iie.GetCount (SymbolTableId::Main), [] { SymbolTable t; Load (t, SymbolTableId::Main, RomSymbols::GetMain ("Apple //e Enhanced")); return t.GetCount (SymbolTableId::Main); } (),
                              L"the display name picks the same table as the id");
        }



        //  The //c has no cassette port: its $C028 flips the ROM bank and its
        //  $C060 reads the 80/40 switch, so it has ROMBANK and RD80SW where
        //  the //e has TAPEOUT and TAPEIN.
        TEST_METHOD (Iic_NamesItsRomBankAndSwitchInPlaceOfTheCassette)
        {
            SymbolTable  iic;
            SymbolTable  iie;
            Word         address = 0;



            Load (iic, SymbolTableId::Main, RomSymbols::GetMain ("Apple2c"));
            Load (iie, SymbolTableId::Main, RomSymbols::GetMain ("Apple2e"));

            Assert::AreEqual ((Word) 0xC028, Resolve (iic, SymbolTableId::Main, "ROMBANK"));
            Assert::AreEqual ((Word) 0xC060, Resolve (iic, SymbolTableId::Main, "RD80SW"));
            Assert::IsFalse  (iic.TryResolveIn (SymbolTableId::Main, "TAPEIN",  address), L"no cassette input");
            Assert::IsFalse  (iic.TryResolveIn (SymbolTableId::Main, "TAPEOUT", address), L"no cassette output");
            Assert::AreEqual ((Word) 0xC060, Resolve (iie, SymbolTableId::Main, "TAPEIN"));
            Assert::IsFalse  (iie.TryResolveIn (SymbolTableId::Main, "RD80SW", address), L"the //e has the cassette there");
            Assert::AreEqual ((Word) 0xC019, Resolve (iic, SymbolTableId::Main, "RDVBL"), L"and the //e's switches");
        }



        TEST_METHOD (Basic_Dos33_ProDos_SpotEntries)
        {
            SymbolTable  table;



            Load (table, SymbolTableId::Basic,  RomSymbols::GetBasic());
            Load (table, SymbolTableId::Dos33,  RomSymbols::GetDos33());
            Load (table, SymbolTableId::ProDos, RomSymbols::GetProDos());

            Assert::AreEqual ((Word) 0x00B1, Resolve (table, SymbolTableId::Basic,  "CHRGET"));
            Assert::AreEqual ((Word) 0xDD7B, Resolve (table, SymbolTableId::Basic,  "FRMEVL"));
            Assert::AreEqual ((Word) 0xE000, Resolve (table, SymbolTableId::Basic,  "COLDSTART"));
            Assert::AreEqual ((Word) 0x03D0, Resolve (table, SymbolTableId::Dos33,  "DOSWARM"));
            Assert::AreEqual ((Word) 0x03D9, Resolve (table, SymbolTableId::Dos33,  "RWTS"));
            Assert::AreEqual ((Word) 0xBF00, Resolve (table, SymbolTableId::ProDos, "MLI"));
            Assert::AreEqual ((Word) 0xBFFF, Resolve (table, SymbolTableId::ProDos, "KVERSION"));
        }



        //  The even annunciator address turns the output off and the odd one
        //  on; FADD and FSUB are Applesoft's memory entries, as FMULT and
        //  FDIV are; the ProDOS global page ends IBAKVER, IVERSION, KBAKVER,
        //  KVERSION.
        TEST_METHOD (EachSymbolIsAtTheAddressItsDescriptionMatches)
        {
            SymbolTable  table;



            Load (table, SymbolTableId::Main,   RomSymbols::GetMain ("Apple2e"));
            Load (table, SymbolTableId::Basic,  RomSymbols::GetBasic());
            Load (table, SymbolTableId::ProDos, RomSymbols::GetProDos());

            Assert::AreEqual ((Word) 0xC058, Resolve (table, SymbolTableId::Main,   "CLRAN0"));
            Assert::AreEqual ((Word) 0xC059, Resolve (table, SymbolTableId::Main,   "SETAN0"));
            Assert::AreEqual ((Word) 0xC05E, Resolve (table, SymbolTableId::Main,   "CLRAN3"));
            Assert::AreEqual ((Word) 0xC05F, Resolve (table, SymbolTableId::Main,   "SETAN3"));
            Assert::AreEqual ((Word) 0xE7BE, Resolve (table, SymbolTableId::Basic,  "FADD"));
            Assert::AreEqual ((Word) 0xE7A7, Resolve (table, SymbolTableId::Basic,  "FSUB"));
            Assert::AreEqual ((Word) 0xBFFC, Resolve (table, SymbolTableId::ProDos, "IBAKVER"));
            Assert::AreEqual ((Word) 0xBFFD, Resolve (table, SymbolTableId::ProDos, "IVERSION"));
            Assert::AreEqual ((Word) 0xBFFE, Resolve (table, SymbolTableId::ProDos, "KBAKVER"));
        }
    };
}
