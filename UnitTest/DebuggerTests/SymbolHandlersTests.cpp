#include "Pch.h"

#include "Debugger/Handlers/SymbolHandlers.h"
#include "HandlerTestRig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolHandlersTests
//
//  SYM and the SYM<table> forms over the session's symbol table, which
//  starts with the machine's ROM symbols.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SymbolHandlersTests)
    {
    public:

        using Rig = HandlerRig<SymbolHandlers>;



        TEST_METHOD (SYM_LookupByNameAndAddress_AcrossEnabledTables)
        {
            Rig  rig;



            Assert::AreEqual (std::string ("$FDED COUT (main)"),    rig.RunOk ("SYM cout").text.at (0), L"case does not matter");
            Assert::AreEqual (std::string ("$FC58 HOME (main)"),    rig.RunOk ("SYM FC58").text.at (0), L"an address finds its name");
            Assert::AreEqual (std::string ("$00B1 CHRGET (basic)"), rig.RunOk ("SYM CHRGET").text.at (0));
            Assert::AreEqual (std::string ("$FDED COUT (main)"),    rig.RunOk ("SYMMAIN COUT").text.at (0));
            rig.RunFails ("SYMBASIC COUT", "symbol not found");
            rig.RunFails ("SYM NOSUCH",    "symbol not found");
            rig.RunFails ("SYM MLI",       "symbol not found");
            Assert::AreEqual (std::string ("$BF00 MLI (prodos)"),   rig.RunOk ("SYMPRODOS MLI").text.at (0), L"a table answers whether or not it is on");
            Assert::AreEqual (std::string ("$BF00 MLI (prodos)"),   rig.RunOk ("SYMPRO MLI").text.at (0));
        }



        TEST_METHOD (SYM_Info_CountsAndState)
        {
            Rig                       rig;
            std::vector<std::string>  lines = rig.RunOk ("SYM").text;



            Assert::AreEqual ((size_t) 9, lines.size());
            Assert::IsTrue   (lines[0].starts_with ("main "),  L"main first");
            Assert::IsTrue   (lines[0].ends_with (" symbols, on"));
            Assert::IsTrue   (lines[3].starts_with ("user "));
            Assert::IsTrue   (lines[8].starts_with ("prodos ") && lines[8].ends_with (" symbols, off"));
            Assert::IsTrue   (rig.RunOk ("SYMINFO").text == lines);
        }



        TEST_METHOD (SYM_AddRemove_OnOff_Clear)
        {
            Rig   rig;
            Word  address = 0;



            Assert::AreEqual (std::string ("$0300 LIFE (user)"), rig.RunOk ("SYM LIFE = 300").text.at (0));
            Assert::IsTrue   (rig.session.TryResolveSymbol ("life", address), L"a user symbol resolves in expressions");
            Assert::AreEqual ((Word) 0x0300, address);
            Assert::AreEqual (std::string ("$0300 LIFE (user)"), rig.RunOk ("SYM 300").text.at (0));

            Assert::AreEqual (std::string ("user: off"), rig.RunOk ("SYMUSER OFF").text.at (0));
            Assert::IsFalse  (rig.session.TryResolveSymbol ("LIFE", address), L"a table that is off does not resolve");
            rig.RunOk ("SYMUSER ON");
            Assert::IsTrue   (rig.session.TryResolveSymbol ("LIFE", address));

            Assert::AreEqual (std::string ("$0400 TWO (user2)"), rig.RunOk ("SYMUSER2 TWO = 400").text.at (0));
            Assert::AreEqual (std::string ("Removed LIFE from user."), rig.RunOk ("SYM ! LIFE").text.at (0));
            rig.RunFails ("SYM ~ LIFE", "symbol not found");
            Assert::AreEqual (std::string ("Cleared user2."), rig.RunOk ("SYMUSER2 CLEAR").text.at (0));
            Assert::AreEqual ((size_t) 0, rig.session.GetSymbols().GetCount (SymbolTableId::User2));

            Assert::AreEqual (std::string ("Cleared main."), rig.RunOk ("SYMMAIN CLEAR").text.at (0));
            rig.RunFails ("SYM COUT", "symbol not found");
        }



        TEST_METHOD (SYM_LoadAndSave_Files)
        {
            Rig                       rig;
            std::vector<std::string>  lines;



            rig.files.WriteAllText (L"C:\\Work\\prog.dbg", "; by address\nstart=$0300\nloop=$0310\n");
            rig.files.WriteAllText (L"C:\\Work\\labels.sym", "0300 START\n0310 LOOP\n");
            rig.files.WriteAllText (L"C:\\Work\\bad.txt",    "nothing here\n");

            Assert::AreEqual (std::string ("Loaded 2 symbols into user from prog.dbg."), rig.RunOk ("SYMUSER LOAD \"prog.dbg\"").text.at (0));
            Assert::AreEqual (std::string ("$0310 loop (user)"), rig.RunOk ("SYM loop").text.at (0));

            Assert::AreEqual (std::string ("Loaded 2 symbols into src from labels.sym."), rig.RunOk ("SYMSRC LOAD labels.sym,1000").text.at (0));
            Assert::AreEqual (std::string ("$1300 START (src)"), rig.RunOk ("SYMSRC START").text.at (0), L"the offset moved every address");

            lines = rig.RunOk ("SYMLIST src").text;
            Assert::AreEqual ((size_t) 2, lines.size());
            Assert::AreEqual (std::string ("$1300 START (src)"), lines[0]);

            Assert::AreEqual (std::string ("Saved 2 symbols from src to out.dbg."), rig.RunOk ("SYMSRC SAVE out.dbg").text.at (0));
            Assert::AreEqual (std::string ("; by address\nSTART=$1300\nLOOP=$1310\n"), rig.files.PeekContent (L"C:\\Work\\out.dbg"));

            rig.RunFails ("SYMUSER LOAD bad.txt",     "not a symbol file");
            rig.RunFails ("SYMUSER LOAD missing.dbg", "file not found");
            rig.RunFails ("SYMUSER LOAD prog.dbg,zz", "invalid arguments");
            rig.RunFails ("SYMLIST bogus",            "invalid arguments");
            Assert::AreEqual ((int) CommandStatus::Error, (int) rig.Run ("SYMUSER LOAD").status);
        }
    };
}
