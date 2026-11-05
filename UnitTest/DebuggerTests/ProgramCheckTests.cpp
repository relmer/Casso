#include "Pch.h"

#include "Debugger/DebugFileReader.h"
#include "Debugger/Handlers/SymbolHandlers.h"
#include "Debugger/ProgramCheck.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"
#include "Ui/Debugger/Panes/DisassemblyViews.h"
#include "Ui/Debugger/Panes/SourcePane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProgramCheckTests
    //
    //  The entries a debug or symbol file is checked by, and whether memory
    //  matches them.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ProgramCheckTests)
    {
    public:

        static ProgramCheck::PeekFn MakePeek (const std::vector<Byte> & memory)
        {
            return [&memory] (Word address, Byte & value) { value = memory[address]; return true; };
        }


        TEST_METHOD (TheMnemonicIsTheFirstOneOnTheLine)
        {
            Assert::AreEqual (std::string ("LDA"), ProgramCheck::GetMnemonic ("START   lda #$01"));
            Assert::AreEqual (std::string ("JSR"), ProgramCheck::GetMnemonic ("        JSR COUT"));
            Assert::AreEqual (std::string (""),    ProgramCheck::GetMnemonic ("MSG     .byte \"HI\""), L"a directive");
            Assert::AreEqual (std::string (""),    ProgramCheck::GetMnemonic ("* a comment"));
        }


        //  A debug file's instruction lines are entries by mnemonic and size;
        //  a data line and a line of a macro with two spans aren't, and a
        //  file without its source doesn't have any.
        TEST_METHOD (ADebugFilesEntriesAreItsInstructionLines)
        {
            DebugFile                                 file;
            std::map<int, std::vector<std::string>>  sources;
            std::vector<ProgramCheck::Entry>          entries;



            file.segments.push_back ({ 0, "CODE", 0x6000, 16 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 3 }, { 2, 0, 5, 2 }, { 3, 0, 7, 1 }, { 4, 0, 8, 1 } };
            file.lines = { { 0, 0, 1, DebugLineType::Asm, 0, { 0 } },
                           { 1, 0, 2, DebugLineType::Asm, 0, { 1 } },
                           { 2, 0, 3, DebugLineType::Asm, 0, { 2 } },
                           { 3, 0, 4, DebugLineType::Asm, 0, { 3, 4 } } };

            Assert::IsTrue (ProgramCheck::GetDebugFileEntries (file, sources).empty(), L"no source");

            sources[0] = { "START   LDA #$01", "        JSR COUT", "MSG     .byte \"HI\"", "        PRINT" };
            entries    = ProgramCheck::GetDebugFileEntries (file, sources);

            Assert::AreEqual ((size_t) 2,      entries.size());
            Assert::AreEqual ((Word) 0x6000,   entries[0].address);
            Assert::AreEqual (std::string ("LDA"), entries[0].mnemonic);
            Assert::AreEqual ((size_t) 2,      entries[0].size);
            Assert::AreEqual ((Word) 0x6002,   entries[1].address);
            Assert::AreEqual (std::string ("JSR"), entries[1].mnemonic);
            Assert::AreEqual ((size_t) 3,      entries[1].size);
        }


        //  A Merlin listing's lines have the bytes they assembled to, so its
        //  entries are those bytes.
        TEST_METHOD (AListingsEntriesAreItsBytes)
        {
            DebugFile                         file;
            std::string                       error;
            std::vector<ProgramCheck::Entry>  entries;
            HRESULT                           hr      = DebugFileReader::ReadMerlinListing ("6000: A9 01         1 START    LDA   #$01\n"
                                                                                            "6002: 20 ED FD      2          JSR   $FDED\n",
                                                                                            "prog.lst", file, error);



            Assert::IsTrue (SUCCEEDED (hr));

            entries = ProgramCheck::GetDebugFileEntries (file, {});

            Assert::AreEqual ((size_t) 2, entries.size());
            Assert::IsTrue   (entries[0].bytes == std::vector<Byte> { 0xA9, 0x01 });
            Assert::IsTrue   (entries[1].bytes == std::vector<Byte> { 0x20, 0xED, 0xFD });
            Assert::AreEqual ((Word) 0x6002, entries[1].address);
        }


        //  A label's entry is the four bytes from it, clipped at the end of
        //  RAM; labels on zero page, the stack, I/O and ROM have none, and
        //  two labels at one address have one.
        TEST_METHOD (ALabelsEntryIsTheBytesFromIt)
        {
            std::vector<Byte>                 memory (0x10000, 0x5A);
            std::vector<ProgramCheck::Entry>  entries;



            entries = ProgramCheck::GetLabelEntries ({ 0x0010, 0x0150, 0x6000, 0x6000, 0xC030, 0xF800, 0xBFFE }, MakePeek (memory));

            Assert::AreEqual ((size_t) 2,    entries.size());
            Assert::AreEqual ((Word) 0x6000, entries[0].address);
            Assert::AreEqual ((size_t) 4,    entries[0].bytes.size());
            Assert::AreEqual ((Word) 0xBFFE, entries[1].address);
            Assert::AreEqual ((size_t) 2,    entries[1].bytes.size(), L"clipped at the end of RAM");
        }


        //  A long program is checked at a bounded number of its entries.
        TEST_METHOD (ALongProgramIsSampled)
        {
            std::vector<Byte>  memory (0x10000, 0);
            std::vector<Word>  labels;



            for (Word label = 0x2000; label < 0x2000 + 2000; label++)
            {
                labels.push_back (label);
            }

            Assert::AreEqual (ProgramCheck::kMaxEntries, ProgramCheck::GetLabelEntries (labels, MakePeek (memory)).size());
        }


        //  A file matches while at least a quarter of its entries do, so a
        //  variable or a patch doesn't hide it and another program does.
        TEST_METHOD (AQuarterOfTheEntriesMatching)
        {
            std::vector<Byte>                 memory (0x10000, 0);
            std::vector<ProgramCheck::Entry>  entries;
            TestCpu                           cpu;



            cpu.InitForTest();

            memory[0x6000] = 0xA9;
            memory[0x6001] = 0x01;

            entries.push_back ({ 0x6000, "LDA", 2, {} });
            entries.push_back ({ 0x7000, "",    0, { 0x11 } });
            entries.push_back ({ 0x7001, "",    0, { 0x22 } });
            entries.push_back ({ 0x7002, "",    0, { 0x33 } });

            Assert::IsTrue  (ProgramCheck::IsMatch ({}, MakePeek (memory), cpu.GetInstructionSet()), L"nothing to compare");
            Assert::IsTrue  (ProgramCheck::IsMatch (entries, MakePeek (memory), cpu.GetInstructionSet()), L"one of four");

            memory[0x6000] = 0xA2;
            Assert::IsFalse (ProgramCheck::IsMatch (entries, MakePeek (memory), cpu.GetInstructionSet()), L"LDX isn't LDA");

            memory[0x6000] = 0xAD;
            Assert::IsFalse (ProgramCheck::IsMatch (entries, MakePeek (memory), cpu.GetInstructionSet()), L"LDA absolute is three bytes");
        }


        TEST_METHOD (TheBannersHaveTheFilesNames)
        {
            Assert::AreEqual (std::wstring(), DisassemblyViewPane::GetStaleText ({}, false));
            Assert::AreEqual (std::wstring (L"The program in memory doesn't match main.dbg, so its source lines and labels are hidden."),
                              DisassemblyViewPane::GetStaleText ({ "main.dbg" }, true));
            Assert::AreEqual (std::wstring (L"The program in memory doesn't match a.sym or b.sym, so their labels are hidden."),
                              DisassemblyViewPane::GetStaleText ({ "a.sym", "b.sym" }, false));
            Assert::AreEqual (std::wstring (L"The program in memory doesn't match main.dbg, so its source lines and labels are hidden.\n"
                                            L"The program in memory doesn't match a.sym, so its labels are hidden."),
                              DisassemblyViewPane::GetStaleText ({ "main.dbg", "a.sym" }, true));
            Assert::AreEqual (std::wstring (L"The program in memory doesn't match main.dbg, so this file's lines aren't mapped to it."),
                              SourcePane::GetStaleText (L"C:\\Work\\main.dbg"));
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  ProgramCheckSessionTests
    //
    //  At a stop where memory doesn't have a loaded file's program, the
    //  file's source lines and labels are hidden, and at a stop where it
    //  does, they're back.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (ProgramCheckSessionTests)
    {
    public:

        using Rig = HandlerRig<SymbolHandlers>;

        //  $6000: LDA #$01 / JSR $FDED / RTS, then "HI".
        static constexpr Byte  kProgram[] = { 0xA9, 0x01, 0x20, 0xED, 0xFD, 0x60, 0x48, 0x49 };

        static void Store (Rig & rig, Byte value)
        {
            for (size_t i = 0; i < std::size (kProgram); i++)
            {
                rig.target.memory[0x6000 + i] = (value == 0) ? kProgram[i] : value;
            }
        }

        static void Stop (Rig & rig, Word pc)
        {
            StopEvent  stop;



            rig.target.registers.pc = pc;
            stop.reason             = StopReason::Pause;
            stop.pc                 = pc;
            rig.session.OnStopped (stop);
        }

        static bool HasLabel (Rig & rig, Word address)
        {
            std::string    name;
            SymbolTableId  table = SymbolTableId::Main;



            return rig.session.GetSymbols().TryFindName (address, name, table);
        }

        static void Prepare (Rig & rig, TestCpu & cpu)
        {
            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();
            rig.session.GetSymbols().SetEnabled (SymbolTableId::Main,  false);
            rig.session.GetSymbols().SetEnabled (SymbolTableId::Basic, false);
        }


        TEST_METHOD (ADebugFileIsHiddenWhileAnotherProgramIsInMemory)
        {
            Rig            rig;
            TestCpu        cpu;
            std::string    file;
            int            line    = 0;
            Word           address = 0;
            SymbolTableId  table   = SymbolTableId::Main;



            Prepare (rig, cpu);
            Store   (rig, 0);

            rig.files.WriteAllText (L"C:\\Work\\main.a65", "COUT    = $FDED\nSTART   LDA #$01\n        JSR COUT\n        RTS\nMSG     .byte \"HI\"\n");
            rig.files.WriteAllText (L"C:\\Work\\main.dbg",
                "version\tmajor=2,minor=0\n"
                "file\tid=0,name=\"main.a65\",size=0,mtime=0,mod=0\n"
                "mod\tid=0,name=\"main\",file=0\n"
                "seg\tid=0,name=\"CODE\",start=0x6000,size=8,addrsize=absolute,type=rw\n"
                "span\tid=0,seg=0,start=0,size=2\n"
                "span\tid=1,seg=0,start=2,size=3\n"
                "span\tid=2,seg=0,start=5,size=1\n"
                "span\tid=3,seg=0,start=6,size=2\n"
                "line\tid=0,file=0,line=2,span=0\n"
                "line\tid=1,file=0,line=3,span=1\n"
                "line\tid=2,file=0,line=4,span=2\n"
                "line\tid=3,file=0,line=5,span=3\n"
                "sym\tid=0,name=\"START\",addrsize=absolute,scope=0,val=0x6000,seg=0,type=lab\n"
                "sym\tid=1,name=\"COUT\",addrsize=absolute,scope=0,val=0xFDED,type=equ\n"
                "scope\tid=0,name=\"\",mod=0\n");
            rig.RunOk ("SYMUSER LOAD main.dbg");

            Assert::IsFalse (rig.session.IsDebugFileStale(), L"the program is in memory");
            Assert::IsTrue  (HasLabel (rig, 0x6000));
            Assert::IsTrue  (rig.session.TryGetSourceLine (0x6000, file, line));

            Store (rig, 0xEA);
            Stop  (rig, 0x0300);

            Assert::IsTrue   (rig.session.IsDebugFileStale(), L"another program");
            Assert::IsFalse  (HasLabel (rig, 0x6000), L"its label is hidden");
            Assert::IsFalse  (rig.session.TryGetSourceLine (0x6000, file, line), L"and its lines");
            Assert::IsTrue   (rig.session.GetSymbols().TryResolve ("COUT", address, table), L"a name in the file resolves");
            Assert::AreEqual ((size_t) 1, rig.session.GetStaleFileNames().size());
            Assert::AreEqual (std::string ("main.dbg"), rig.session.GetStaleFileNames().front());

            Store (rig, 0);
            Stop  (rig, 0x0300);

            Assert::IsFalse (rig.session.IsDebugFileStale(), L"the program again");
            Assert::IsTrue  (HasLabel (rig, 0x6000));
            Assert::IsTrue  (rig.session.TryGetSourceLine (0x6000, file, line));
        }


        TEST_METHOD (ASymbolFileIsHiddenWhileItsLabelsBytesChange)
        {
            Rig      rig;
            TestCpu  cpu;



            Prepare (rig, cpu);
            Store   (rig, 0);
            rig.target.registers.pc = 0x6000;

            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "6000 START\n6005 DONE\n");
            rig.RunOk ("SYMUSER LOAD prog.sym");

            Store (rig, 0xEA);
            Stop  (rig, 0x6000);

            Assert::IsFalse  (HasLabel (rig, 0x6000), L"another program");
            Assert::AreEqual (std::string ("prog.sym"), rig.session.GetStaleFileNames().at (0));

            rig.RunOk ("SYMUSER LOAD prog.sym");
            Assert::IsTrue (HasLabel (rig, 0x6000), L"loaded again over the program in memory");

            rig.RunOk ("SYMUSER CLEAR");
            Stop (rig, 0x6000);
            Assert::IsTrue (rig.session.GetStaleFileNames().empty(), L"a cleared table has nothing to check");
        }


        //  Symbols loaded before their program, as a project opening loads
        //  them, are matched to the program at the first stop in it.
        TEST_METHOD (ASymbolFileLoadedBeforeItsProgramIsMatchedAtAStopInIt)
        {
            Rig      rig;
            TestCpu  cpu;



            Prepare (rig, cpu);
            rig.target.registers.pc = 0x0300;

            rig.files.WriteAllText (L"C:\\Work\\prog.sym", "6000 START\n6005 DONE\n");
            rig.RunOk ("SYMUSER LOAD prog.sym");

            Store (rig, 0);
            Stop  (rig, 0x0300);
            Assert::IsTrue (HasLabel (rig, 0x6000), L"the program loaded, and nothing to compare with yet");

            Stop  (rig, 0x6002);
            Store (rig, 0xEA);
            Stop  (rig, 0x0300);
            Assert::IsFalse (HasLabel (rig, 0x6000), L"matched at the stop in it, then replaced");
        }
    };
}
