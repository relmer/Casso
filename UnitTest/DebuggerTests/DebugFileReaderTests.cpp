#include "Pch.h"

#include "Debugger/DebugFileReader.h"
#include "Debugger/LineTable.h"
#include "EmuTests/FixtureProvider.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileReaderTests
//
//  cc65's debug-info format, version 2, as contracts/debug-file-format.md
//  describes it: one record a line, a keyword, a tab, and key=value pairs.
//
//  THE READER IS AS FORGIVING AS CC65'S OWN. An unknown key or an unknown
//  record is skipped, so a file from a newer cc65, or one carrying Casso's
//  sha1 key, loads. What it will not do is load a file whose records point at
//  records that are not there: a line naming a missing span would place code
//  nowhere, so the whole file is refused rather than half of it trusted.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static const char * const  s_kSample =
        "version\tmajor=2,minor=0\n"
        "info\tcsym=0,file=2,lib=0,line=3,mod=1,scope=1,seg=1,span=2,sym=1,type=0\n"
        "file\tid=0,name=\"main.a65\",size=1234,mtime=0x66E9A1B2,mod=0,sha1=\"2fd4e1c67a2d28fced849ee1bb76e7391b93eb12\"\n"
        "file\tid=1,name=\"macros.inc\",size=321,mtime=0x66E9A1B2,mod=0\n"
        "mod\tid=0,name=\"main\",file=0\n"
        "seg\tid=0,name=\"CODE\",start=0x0300,size=0x00F2,addrsize=absolute,type=rw\n"
        "span\tid=0,seg=0,start=0,size=2\n"
        "span\tid=1,seg=0,start=2,size=3\n"
        "line\tid=0,file=0,line=12,span=0\n"
        "line\tid=1,file=0,line=13,span=1\n"
        "line\tid=2,file=1,line=4,type=2,count=1,span=0+1\n"
        "sym\tid=0,name=\"start\",addrsize=absolute,scope=0,def=0,val=0x0300,seg=0,type=lab\n"
        "scope\tid=0,name=\"\",mod=0,size=0x00F2\n";



    TEST_CLASS (DebugFileReaderTests)
    {
    public:

        static DebugFile ReadOk (const std::string & text)
        {
            DebugFile    file;
            std::string  error;



            Assert::AreEqual (S_OK, DebugFileReader::Read (text, file, error), std::wstring (error.begin(), error.end()).c_str());
            return file;
        }

        static std::string ReadFails (const std::string & text)
        {
            DebugFile    file;
            std::string  error;
            HRESULT      hr = DebugFileReader::Read (text, file, error);



            Assert::IsTrue  (FAILED (hr));
            Assert::IsTrue  (file.lines.empty() && file.files.empty() && file.symbols.empty(), L"a refused file loads nothing");
            Assert::IsFalse (error.empty());
            return error;
        }



        TEST_METHOD (EveryRecordKindIsRead)
        {
            DebugFile  file = ReadOk (s_kSample);



            Assert::AreEqual ((size_t) 2, file.files.size());
            Assert::AreEqual (std::string ("main.a65"),  file.files[0].name);
            Assert::AreEqual ((uint64_t) 1234,           file.files[0].size);
            Assert::AreEqual (std::string ("2fd4e1c67a2d28fced849ee1bb76e7391b93eb12"), file.files[0].sha1);

            Assert::AreEqual ((size_t) 1, file.segments.size());
            Assert::AreEqual ((uint32_t) 0x0300, file.segments[0].start);

            Assert::AreEqual ((size_t) 2, file.spans.size());
            Assert::AreEqual ((uint32_t) 2, file.spans[1].start, L"relative to its segment");

            Assert::AreEqual ((size_t) 3, file.lines.size());
            Assert::AreEqual (13, file.lines[1].line);

            Assert::AreEqual ((size_t) 1, file.symbols.size());
            Assert::AreEqual (std::string ("start"), file.symbols[0].name);
            Assert::AreEqual ((uint32_t) 0x0300,     file.symbols[0].value);

            Assert::AreEqual ((size_t) 1, file.modules.size());
            Assert::AreEqual ((size_t) 1, file.scopes.size());
        }


        TEST_METHOD (AFileWithoutSha1HasAnEmptyHash)
        {
            Assert::IsTrue (ReadOk (s_kSample).files[1].sha1.empty(), L"cc65 itself writes none");
        }


        TEST_METHOD (MacroLinesCarryTheirTypeAndDepth)
        {
            DebugFile  file = ReadOk (s_kSample);



            Assert::IsTrue   (file.lines[0].type == DebugLineType::Asm, L"no type key is an assembler line");
            Assert::IsTrue   (file.lines[2].type == DebugLineType::Macro);
            Assert::AreEqual (1, file.lines[2].depth);
        }


        TEST_METHOD (ALineCanListSeveralSpans)
        {
            DebugFile  file = ReadOk (s_kSample);



            Assert::AreEqual ((size_t) 2, file.lines[2].spans.size());
            Assert::AreEqual (1, file.lines[2].spans[1]);
        }


        TEST_METHOD (UnknownKeysAndRecordsAreSkipped)
        {
            std::string  text = std::string (s_kSample) +
                                "csym\tid=0,name=\"x\",scope=0,type=0,sc=auto,offs=0\n"
                                "tomorrow\tid=0,shape=round\n";
            DebugFile    file;



            text.replace (text.find ("mtime=0x66E9A1B2,mod=0\n"), 0, "fresh=1,");
            file = ReadOk (text);

            Assert::AreEqual ((size_t) 3, file.lines.size());
        }


        TEST_METHOD (CrLfLineEndingsRead)
        {
            std::string  text = s_kSample;
            size_t       at   = 0;



            while ((at = text.find ('\n', at)) != std::string::npos)
            {
                text.insert (at, "\r");
                at += 2;
            }

            Assert::AreEqual ((size_t) 3, ReadOk (text).lines.size());
        }


        TEST_METHOD (AnotherMajorVersionIsRefusedByName)
        {
            std::string  text = s_kSample;



            text.replace (text.find ("major=2"), 7, "major=3");

            Assert::AreNotEqual (std::string::npos, ReadFails (text).find ("3"), L"the error says which version it found");
        }


        TEST_METHOD (ALineNamingAMissingFileIsRefused)
        {
            std::string  text = s_kSample;



            text.replace (text.find ("id=0,file=0,line=12"), 19, "id=0,file=9,line=12");

            ReadFails (text);
        }


        TEST_METHOD (ALineNamingAMissingSpanIsRefused)
        {
            std::string  text = s_kSample;



            text.replace (text.find ("span=0+1"), 8, "span=0+7");

            ReadFails (text);
        }


        TEST_METHOD (ASpanNamingAMissingSegmentIsRefused)
        {
            std::string  text = s_kSample;



            text.replace (text.find ("span\tid=1,seg=0"), 15, "span\tid=1,seg=4");

            ReadFails (text);
        }


        TEST_METHOD (NoVersionRecordIsRefused)
        {
            std::string  text = s_kSample;



            text.erase (0, text.find ('\n') + 1);

            ReadFails (text);
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinListingTests
    //
    //  A Merlin 8/16 listing read as a debug file whose one source is the
    //  listing itself (FR-033b). The layout is the one captured from Merlin
    //  Pro 2.23 (research R-022): address and bytes, then the line number, with
    //  `>` in front of it on a line from a PUT file, whose numbers restart at 1.
    //
    ////////////////////////////////////////////////////////////////////////////////

    static const char * const  s_kListing =
        "                1  * sample\n"                                 //  1
        "                2           ORG   $8000\n"                     //  2
        "8000: A9 41     3  START    LDA   #$41\n"                      //  3
        "8002: 20 9B 80  4           JSR   SENDMSG\n"                   //  4
        "8005: C5 B0 B0  5           DCI   \"E003BASIC\"\n"             //  5
        "8008: B3 C2 C1 D3 C9 C3\n"                                     //  6
        "                6           PUT   SENDMSG\n"                   //  7
        "809B: 68       >1  SENDMSG  PLA\n"                             //  8
        "809C: 60       >2           RTS\n"                             //  9
        "\n"                                                            // 10
        "--End assembly, 14 bytes, Errors: 0\n"                         // 11
        "\n"
        "Symbol table - alphabetical order:\n"
        "\n"
        "   SENDMSG =$809B      START   =$8000\n";



    TEST_CLASS (MerlinListingTests)
    {
    public:

        static DebugFile Read (const std::string & text)
        {
            DebugFile    file;
            std::string  error;



            Assert::AreEqual (S_OK, DebugFileReader::ReadMerlinListing (text, "SAMPLE.LST", file, error),
                              std::wstring (error.begin(), error.end()).c_str());
            return file;
        }



        TEST_METHOD (TheListingIsItsOneSourceFile)
        {
            DebugFile  file = Read (s_kListing);



            Assert::AreEqual ((size_t) 1,                 file.files.size());
            Assert::AreEqual (std::string ("SAMPLE.LST"), file.files[0].name);
            Assert::AreEqual ((uint64_t) strlen (s_kListing), file.files[0].size);
            Assert::IsFalse  (file.files[0].sha1.empty(), L"hashed, so the source service finds the listing itself");
        }


        TEST_METHOD (EachLineWithBytesIsARecordAtItsListingLine)
        {
            DebugFile  file = Read (s_kListing);
            LineTable  table;



            Assert::AreEqual ((size_t) 5, file.lines.size(), L"LDA, JSR, DCI, and the two PUT lines");
            table.Build (file);
            Assert::AreEqual (3, table.GetPositionsAt (0x8000).at (0).line);
            Assert::AreEqual (4, table.GetPositionsAt (0x8004).at (0).line);
        }


        TEST_METHOD (ABytesOnlyLineContinuesTheLineBefore)
        {
            DebugFile  file = Read (s_kListing);
            LineTable  table;



            table.Build (file);
            Assert::AreEqual (5, table.GetPositionsAt (0x8008).at (0).line, L"the DCI's second row of bytes");
            Assert::AreEqual (5, table.GetPositionsAt (0x800D).at (0).line);
        }


        TEST_METHOD (APutLineIsALineOfTheListing)
        {
            DebugFile  file = Read (s_kListing);
            LineTable  table;



            table.Build (file);
            Assert::AreEqual (8, table.GetPositionsAt (0x809B).at (0).line, L"its own listing line, not the PUT file's line 1");
            Assert::AreEqual (9, table.GetPositionsAt (0x809C).at (0).line);
            Assert::AreEqual (0, table.GetPositionsAt (0x809B).at (0).file);
        }


        TEST_METHOD (TheSymbolTableGivesTheSymbols)
        {
            DebugFile  file = Read (s_kListing);



            Assert::AreEqual ((size_t) 2, file.symbols.size());
        }


        TEST_METHOD (AListingWithNoBytesIsRefused)
        {
            DebugFile    file;
            std::string  error;
            HRESULT      hr    = DebugFileReader::ReadMerlinListing ("   1  * nothing\n", "X", file, error);



            Assert::IsTrue (FAILED (hr));
            Assert::IsFalse (error.empty());
        }
    };




    ////////////////////////////////////////////////////////////////////////////////
    //
    //  Cc65FixtureTests
    //
    //  A debug file written by cc65's own linker (Fixtures/Debugger/DebugFiles,
    //  see its LICENSE.md): records out of Casso's order, line records with no
    //  span, keys Casso does not write (ref, oname, ooffs), six-digit hex, and
    //  ca65's own macro records, which name the invocation and each body line.
    //
    ////////////////////////////////////////////////////////////////////////////////

    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinFixtureTests
    //
    //  Merlin Pro 2.23's own listing of PI.ADD.S, printed by the emulated
    //  assembler. It carries what a captured listing has that a written one
    //  does not: macro expansion lines, PUT and USE files with their own
    //  numbering, and a symbol table after the end of the assembly.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinFixtureTests)
    {
    public:

        static DebugFile Read()
        {
            FixtureProvider       provider;
            std::vector<uint8_t>  bytes;
            HRESULT               hr    = provider.OpenFixture ("Debugger/Merlin/PI.ADD.LST", bytes);
            DebugFile             file;
            std::string           error;



            Assert::AreEqual (S_OK, hr, L"fixture missing: Debugger/Merlin/PI.ADD.LST");
            hr = DebugFileReader::ReadMerlinListing (std::string (bytes.begin(), bytes.end()), "PI.ADD.LST", file, error);
            Assert::AreEqual (S_OK, hr, std::wstring (error.begin(), error.end()).c_str());
            return file;
        }



        TEST_METHOD (TheListingIsReadAsItsOwnSource)
        {
            DebugFile  file = Read();
            LineTable  table;



            Assert::AreEqual ((size_t) 1, file.files.size(), L"the listing is the one source");
            Assert::IsFalse  (file.lines.empty());

            table.Build (file);

            //  The RTS that ends the OUTPUT routine, at $80CE, is one line of
            //  the listing and starts there.
            Assert::AreEqual ((size_t) 1, table.GetPositionsAt (0x80CE).size());
            Assert::AreEqual (0x80CE, (int) table.GetRanges (0, table.GetPositionsAt (0x80CE).at (0).line).at (0).first);
        }


        TEST_METHOD (EveryRecordedLineHoldsCode)
        {
            DebugFile  file = Read();



            for (const DebugLine & line : file.lines)
            {
                Assert::IsTrue (line.line > 0,        L"a record carries the listing line it came from");
                Assert::IsFalse (line.spans.empty(),  L"and the bytes that line assembled to");
                Assert::AreEqual (0, line.file,       L"the listing is the only source");
            }
        }
    };

    TEST_CLASS (Cc65FixtureTests)
    {
    public:

        static DebugFile Read()
        {
            FixtureProvider       provider;
            std::vector<uint8_t>  bytes;
            HRESULT               hr    = provider.OpenFixture ("Debugger/DebugFiles/hello.dbg", bytes);
            DebugFile             file;
            std::string           error;



            Assert::AreEqual (S_OK, hr, L"fixture missing: Debugger/DebugFiles/hello.dbg");
            hr = DebugFileReader::Read (std::string (bytes.begin(), bytes.end()), file, error);
            Assert::AreEqual (S_OK, hr, std::wstring (error.begin(), error.end()).c_str());
            return file;
        }



        TEST_METHOD (EveryRecordKindIsRead)
        {
            DebugFile  file = Read();



            Assert::AreEqual ((size_t) 2,  file.files.size());
            Assert::AreEqual ((size_t) 10, file.lines.size());
            Assert::AreEqual ((size_t) 9,  file.spans.size());
            Assert::AreEqual ((size_t) 6,  file.segments.size());
            Assert::AreEqual ((size_t) 31, file.symbols.size());
            Assert::AreEqual ((uint32_t) 0x0300, file.segments[0].start, L"six hex digits");
            Assert::IsTrue   (file.files[0].sha1.empty(), L"cc65 writes no hash");
        }


        TEST_METHOD (AMacroMapsToTheInvocationAndItsBody)
        {
            DebugFile  file = Read();
            LineTable  table;



            table.Build (file);

            const std::vector<SourcePosition> & at = table.GetPositionsAt (0x0302);

            Assert::AreEqual ((size_t) 2, at.size(), L"the invocation and one body line");
            Assert::AreEqual (8, at[0].line, L"twoinx");
            Assert::AreEqual (3, at[1].line, L"the first inx in the body");
            Assert::AreEqual (1, at[1].depth);
            Assert::AreEqual (4, table.GetPositionsAt (0x0303).back().line);
        }


        TEST_METHOD (SymbolsResolveToTheirAddresses)
        {
            DebugFile  file  = Read();
            bool       found = false;



            for (const DebugSymbol & symbol : file.symbols)
            {
                if (symbol.name == "sub")
                {
                    Assert::AreEqual ((uint32_t) 0x0308, symbol.value);
                    Assert::AreEqual (std::string ("lab"), symbol.type);
                    found = true;
                }
            }

            Assert::IsTrue (found);
        }
    };
}