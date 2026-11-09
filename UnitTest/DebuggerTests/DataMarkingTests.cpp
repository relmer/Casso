#include "Pch.h"

#include "Debugger/DebugFileData.h"
#include "Debugger/Handlers/DataDirectiveHandlers.h"
#include "Debugger/Handlers/SymbolHandlers.h"
#include "Debugger/Handlers/WatchHandlers.h"
#include "HandlerTestRig.h"
#include "TestHelpers.h"
#include "Ui/Debugger/HeatStrip.h"
#include "Ui/Debugger/Panes/SourcePane.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DebuggerTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DebugFileDataTests
    //
    //  The data blocks a debug file's data directives lay down, and the kind
    //  of data each directive is.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DebugFileDataTests)
    {
    public:

        TEST_METHOD (EachDirectiveHasItsDataKind)
        {
            const std::pair<const char *, DataBlockKind>  cases[] =
            {
                { "count       .byte 0                 ; loop counter",  DataBlockKind::Bytes   },
                { "hex_chars   .byte \"0123456789ABCDEF\"",                DataBlockKind::Text    },
                { "title       .asciiz \"HELLO\"",                         DataBlockKind::Text    },
                { "table       .word $1234, $5678",                      DataBlockKind::Words   },
                { "vectors     .addr reset, irq",                        DataBlockKind::Address },
                { "buffer      .res 16",                                 DataBlockKind::Bytes   },
                { "MSG         ASC \"HELLO\"",                             DataBlockKind::Text    },
                { "PTRS        DA  START",                               DataBlockKind::Address },
            };
            DataBlockKind  kind = DataBlockKind::Float;



            for (const auto & [line, expected] : cases)
            {
                Assert::IsTrue   (DebugFileData::TryGetKind (line, kind), TextEncoding::NarrowToWide (line).c_str());
                Assert::AreEqual ((int) expected, (int) kind, TextEncoding::NarrowToWide (line).c_str());
            }

            Assert::IsFalse (DebugFileData::TryGetKind ("loop        lda #$01", kind), L"an instruction");
            Assert::IsFalse (DebugFileData::TryGetKind ("; .byte in a comment", kind), L"a comment");
        }


        //  Each span of a data line is a block at its address; an instruction
        //  line, a macro's line and a line whose source wasn't read aren't.
        TEST_METHOD (EachDataLinesSpanIsABlock)
        {
            DebugFile                                 file;
            std::map<int, std::vector<std::string>>  sources;
            std::vector<DataBlockEntry>               blocks;



            file.segments.push_back ({ 0, "CODE", 0x6000, 32 });
            file.spans = { { 0, 0, 0, 2 }, { 1, 0, 2, 1 }, { 2, 0, 3, 5 }, { 3, 0, 8, 4 }, { 4, 0, 12, 2 } };
            file.lines = { { 0, 0, 1, DebugLineType::Asm,   0, { 0 } },
                           { 1, 0, 2, DebugLineType::Asm,   0, { 1 } },
                           { 2, 0, 3, DebugLineType::Asm,   0, { 2 } },
                           { 3, 0, 4, DebugLineType::Asm,   0, { 3 } },
                           { 4, 0, 5, DebugLineType::Macro, 1, { 4 } },
                           { 5, 1, 1, DebugLineType::Asm,   0, { 4 } } };
            sources[0] = { "start   lda #$01", "count   .byte 0", "msg     .byte \"HELLO\"", "table   .word 1, 2", "        .byte 1, 2" };

            blocks = DebugFileData::GetBlocks (file, sources);

            Assert::AreEqual ((size_t) 3,     blocks.size());
            Assert::AreEqual ((Word) 0x6002,  blocks[0].first);
            Assert::AreEqual ((Word) 0x6002,  blocks[0].last);
            Assert::AreEqual ((int) DataBlockKind::Bytes, (int) blocks[0].kind);
            Assert::AreEqual ((Word) 0x6003,  blocks[1].first);
            Assert::AreEqual ((Word) 0x6007,  blocks[1].last);
            Assert::AreEqual ((int) DataBlockKind::Text,  (int) blocks[1].kind);
            Assert::AreEqual ((Word) 0x6008,  blocks[2].first);
            Assert::AreEqual ((int) DataBlockKind::Words, (int) blocks[2].kind);
            Assert::IsTrue   (blocks[0].name.empty(), L"a block takes its label from the symbols");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DataMarkingSessionTests
    //
    //  The disassembly reads a loaded debug file's data as data, with the
    //  symbol's name, while the file's program is in memory; a data directive
    //  marks bytes by a symbol's name; after X, either kind is disassembled
    //  as code.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DataMarkingSessionTests)
    {
    public:

        using Rig = HandlerRig<SymbolHandlers>;

        //  $6000: LDA #$01 / JSR $FDED / RTS, then "HI".
        static constexpr Byte  kProgram[] = { 0xA9, 0x01, 0x20, 0xED, 0xFD, 0x60, 0x48, 0x49 };

        static void Load (Rig & rig, TestCpu & cpu)
        {
            cpu.InitForTest();
            rig.target.instructionSet = cpu.GetInstructionSet();
            rig.session.GetSymbols().SetEnabled (SymbolTableId::Main,  false);
            rig.session.GetSymbols().SetEnabled (SymbolTableId::Basic, false);

            for (size_t i = 0; i < std::size (kProgram); i++)
            {
                rig.target.memory[0x6000 + i] = kProgram[i];
            }

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
                "sym\tid=2,name=\"MSG\",addrsize=absolute,scope=0,val=0x6006,seg=0,type=lab\n"
                "scope\tid=0,name=\"\",mod=0\n");
            rig.RunOk ("SYMUSER LOAD main.dbg");
        }

        static DisassemblyLine LineAt (Rig & rig, Word address)
        {
            DisassemblyData  data;



            (void) DataDirectiveHandlers::Disassemble (rig.session, address, std::nullopt, 1, data);
            return data.lines.at (0);
        }

        static void Stop (Rig & rig, Word pc)
        {
            StopEvent  stop;



            rig.target.registers.pc = pc;
            stop.reason             = StopReason::Pause;
            stop.pc                 = pc;
            rig.session.OnStopped (stop);
        }


        TEST_METHOD (ADebugFilesDataIsReadAsData)
        {
            Rig              rig;
            TestCpu          cpu;
            DisassemblyLine  line;



            Load (rig, cpu);

            line = LineAt (rig, 0x6006);
            Assert::AreEqual (std::string ("ASC"), line.instruction.mnemonic);
            Assert::AreEqual (std::string ("MSG"), line.label, L"the symbol labels the data");
            Assert::AreEqual ((int) DataOrigin::DebugFile, (int) line.dataOrigin);
            Assert::AreEqual ((int) DataOrigin::None, (int) LineAt (rig, 0x6000).dataOrigin, L"an instruction");

            for (Word address = 0x6000; address <= 0x6007; address++)
            {
                rig.target.memory[address] = 0xEA;
            }

            Stop (rig, 0x0300);
            Assert::AreEqual ((int) DataOrigin::None, (int) LineAt (rig, 0x6006).dataOrigin, L"not while another program is in memory");
        }


        //  The panes are built from a copy of the session, which has the
        //  debug file's data, and has it again after X.
        TEST_METHOD (ThePanesCopyHasTheDebugFilesData)
        {
            Rig               rig;
            TestCpu           cpu;
            DebugSession      copy   { rig.target, rig.sink, RunState::Paused };
            DebugSessionView  view;
            DataBlockEntry    block;
            DataOrigin        origin = DataOrigin::None;



            Load (rig, cpu);
            rig.session.TakeView (view);
            copy.InstallView (view);
            Assert::IsTrue   (copy.TryFindData (0x6006, block, origin));
            Assert::AreEqual ((int) DataOrigin::DebugFile, (int) origin);

            rig.session.RemoveData (0x6006, 0x6007);
            rig.session.TakeView (view);
            copy.InstallView (view);
            Assert::IsFalse (copy.TryFindData (0x6006, block, origin));
        }


        TEST_METHOD (AfterXADebugFilesDataIsCode)
        {
            Rig      rig;
            TestCpu  cpu;



            Load (rig, cpu);
            rig.session.RemoveData (0x6006, 0x6007);

            Assert::AreEqual ((int) DataOrigin::None, (int) LineAt (rig, 0x6006).dataOrigin);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  DataDirectiveNameTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (DataDirectiveNameTests)
    {
    public:

        using Rig = HandlerRig<DataDirectiveHandlers>;


        //  A block with no name given is called after the symbol at its start,
        //  and a symbol labels its line over a block's name.
        TEST_METHOD (TheSymbolAtABlocksStartIsItsName)
        {
            Rig              rig;
            DisassemblyData  data;



            rig.session.GetSymbols().Add (SymbolTableId::User, "count", 0x0300);

            Assert::AreEqual (std::string ("count        $0300-$0300  bytes"), rig.RunOk ("DB 300").text.at (0));
            Assert::AreEqual (std::string ("B_0310       $0310-$0317  bytes"), rig.RunOk ("DB 310:317").text.at (0));

            rig.session.GetSymbols().Add (SymbolTableId::User, "table", 0x0310);
            (void) DataDirectiveHandlers::Disassemble (rig.session, 0x0310, std::nullopt, 1, data);

            Assert::AreEqual (std::string ("table"), data.lines.at (0).label);
            Assert::AreEqual ((int) DataOrigin::Marked, (int) data.lines.at (0).dataOrigin);
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  HeatDataRangeTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (HeatDataRangeTests)
    {
    public:

        //  The bytes around an address that were read or written and never
        //  run, to the first byte that ran or that nothing touched.
        TEST_METHOD (TheRangeIsTheReadAndWrittenBytesThatNeverRan)
        {
            DebuggerViewSnapshot::HeatColumnLevels  levels;
            Word                                    first = 0;
            Word                                    last  = 0;



            levels.executeCount.assign (0x10000, 0);
            levels.readCount.assign    (0x10000, 0);
            levels.writeCount.assign   (0x10000, 0);

            levels.executeCount[0x60BF] = 9;
            levels.writeCount[0x60C1]   = 3;
            levels.readCount[0x60C2]    = 1;
            levels.readCount[0x60C3]    = 1;
            levels.readCount[0x60C4]    = 2;
            levels.executeCount[0x60C4] = 1;
            levels.readCount[0x60C0]    = 1;
            levels.executeCount[0x60C0] = 0;

            Assert::IsTrue   (HeatStrip::TryGetDataRange (levels, 0x60C2, first, last));
            Assert::AreEqual ((Word) 0x60C0, first, L"back to the byte after the one that ran");
            Assert::AreEqual ((Word) 0x60C3, last,  L"on to the byte before the one that ran");

            Assert::IsFalse (HeatStrip::TryGetDataRange (levels, 0x60BF, first, last), L"a byte that ran");
            Assert::IsFalse (HeatStrip::TryGetDataRange (levels, 0x7000, first, last), L"a byte nothing touched");
            Assert::IsFalse (HeatStrip::TryGetDataRange (DebuggerViewSnapshot::HeatColumnLevels(), 0x60C2, first, last), L"no counts");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  SourceSymbolTipTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (SourceSymbolTipTests)
    {
    public:

        TEST_METHOD (AWordThatIsASymbolHasItsValue)
        {
            std::vector<SymbolInfo>  symbols = { { "count_text", 0x60F2 }, { "BAR_LENGTH", 0x0028, SymbolTableId::User, true } };
            std::wstring             line    = L"            lda #>count_text        ; the text's page";
            std::wstring             text;



            Assert::IsTrue   (SourcePane::TryGetSymbolText (line, 22, symbols, text));
            Assert::AreEqual (std::wstring (L"count_text = $60F2\n#>count_text = #$60, its high byte"), text);

            Assert::IsTrue   (SourcePane::TryGetSymbolText (L"lda <count_text", 7, symbols, text));
            Assert::AreEqual (std::wstring (L"count_text = $60F2\n<count_text = $F2, its low byte"), text);

            Assert::IsTrue   (SourcePane::TryGetSymbolText (L"cmp #BAR_LENGTH", 15, symbols, text), L"just past the word");
            Assert::AreEqual (std::wstring (L"BAR_LENGTH = $0028, a constant"), text);

            Assert::IsTrue   (SourcePane::TryGetSymbolText (L"lda COUNT_TEXT", 6, symbols, text), L"in another case");

            Assert::IsFalse (SourcePane::TryGetSymbolText (line, 4, symbols, text), L"off a word");
            Assert::IsFalse (SourcePane::TryGetSymbolText (line, 13, symbols, text), L"a mnemonic, which is no symbol");
            Assert::IsFalse (SourcePane::TryGetSymbolText (L"nop ; count_text", 8, symbols, text), L"a comment");
        }





        //  FindSymbolAt returns the symbol under a character as the tip's
        //  lookup does, with the word's start.
        TEST_METHOD (TheSymbolUnderACharacterIsFound)
        {
            std::vector<SymbolInfo>  symbols = { { "draw_counter", 0x6100 }, { "BAR_LENGTH", 0x0028, SymbolTableId::User, true } };
            const SymbolInfo       * found   = nullptr;
            int                      start   = -1;



            found = SourcePane::FindSymbolAt (L"        jsr draw_counter", 16, symbols, start);
            Assert::IsNotNull (found);
            Assert::AreEqual  (std::string ("draw_counter"), found->name);
            Assert::AreEqual  (12, start);

            found = SourcePane::FindSymbolAt (L"cmp #BAR_LENGTH", 7, symbols, start);
            Assert::IsNotNull (found, L"a constant is found, and AddGoToDefinition omits it");
            Assert::IsTrue    (found->isConstant);

            Assert::IsNull (SourcePane::FindSymbolAt (L"        jsr draw_counter", 9, symbols, start), L"a mnemonic");
            Assert::IsNull (SourcePane::FindSymbolAt (L"rts ; draw_counter", 10, symbols, start), L"a comment");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  WatchTextTests
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (WatchTextTests)
    {
    public:

        using Rig = HandlerRig<WatchHandlers>;


        //  .s reads the text at a watch, as does a symbol at the start of a
        //  text block; & before a symbol is its address.
        TEST_METHOD (TextIsWatchedAsText)
        {
            Rig  rig;



            for (size_t i = 0; i < 6; i++)
            {
                rig.target.memory[0x60F2 + i] = (Byte) ("COUNT:"[i] | 0x80);
            }

            rig.target.memory[0x60F8] = 0;
            rig.session.GetSymbols().Add (SymbolTableId::User, "count_text", 0x60F2);

            Assert::AreEqual (std::string ("#0 count_text.s = \"COUNT:\""), rig.RunOk ("W EXPR count_text.s").text.at (0), L"the high bit dropped");
            Assert::AreEqual (std::string ("#1 count_text = $CFC3"),     rig.RunOk ("W EXPR count_text").text.at (0), L"a word without a text block");
            Assert::AreEqual (std::string ("#2 &count_text = $60F2"),    rig.RunOk ("W EXPR &count_text").text.at (0), L"the address");

            rig.session.GetDataBlocks().Add ("", 0x60F2, 0x60F8, DataBlockKind::Text, 32);
            Assert::AreEqual (std::string ("#1 count_text = \"COUNT:\""), rig.RunOk ("WL").text.at (1), L"text at a text block");
        }
    };
}
