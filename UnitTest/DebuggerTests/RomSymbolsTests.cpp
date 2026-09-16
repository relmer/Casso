#include "Pch.h"

#include "Debugger/RomSymbols.h"
#include "Debugger/SymbolTable.h"

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



            Assert::AreEqual (S_OK, hr, std::wstring (error.begin(), error.end()).c_str());
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
    };
}
