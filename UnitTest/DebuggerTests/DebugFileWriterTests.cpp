#include "Pch.h"

#include "Assembler.h"
#include "Debugger/DebugFileReader.h"
#include "Debugger/DebugFileWriter.h"
#include "Debugger/LineTable.h"
#include "Sha1.h"
#include "TestHelpers.h"
#include "MockFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugFileWriterTests
//
//  The debug file both assemblers write for `-g`: cc65's format, version 2,
//  with a SHA-1 on every file (FR-033, FR-033a, contracts/debug-file-format).
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    static const char * const  s_kMain =
        "    .org $0300\n"          //  1
        "    include \"defs.inc\"\n" //  2
        "inner macro\n"              //  3
        "    nop\n"                  //  4
        "    endm\n"                 //  5
        "outer macro\n"              //  6
        "    lda #1\n"               //  7
        "    inner\n"                //  8
        "    endm\n"                 //  9
        "start\n"                    // 10
        "    outer\n"                // 11
        "    rts\n"                  // 12
        "limit = 7\n";               // 13

    static const char * const  s_kDefs = "    ldx #2\n";



    TEST_CLASS (DebugFileWriterTests)
    {
    public:

        static AssemblyResult AssembleSample()
        {
            TestCpu           cpu;
            MockFileReader    reader;
            AssemblerOptions  opts;
            AssemblyResult    result;



            reader.files["defs.inc"] = s_kDefs;
            opts.fileReader          = &reader;
            result                   = Assembler (cpu.GetInstructionSet(), opts).Assemble (s_kMain);

            Assert::IsTrue (result.success);
            return result;
        }



        static DebugFile BuildSample()
        {
            std::map<std::string, DebugSourceName>  names = { { "", { "src/main.a65", 0x66E9A1B2 } },
                                                              { "defs.inc", { "src/defs.inc", 0 } } };



            return DebugFileWriter::Build (AssembleSample(), names);
        }



        static DebugFile ReadBack (const std::string & text)
        {
            DebugFile    file;
            std::string  error;



            Assert::AreEqual (S_OK, DebugFileReader::Read (text, file, error));
            return file;
        }



        TEST_METHOD (TheRecordsComeInCc65Order)
        {
            std::string               text  = DebugFileWriter::Format (BuildSample());
            std::vector<std::string>  order = { "version\t", "info\t", "file\t", "mod\t", "seg\t", "span\t",
                                                "line\t", "sym\t", "scope\t" };
            size_t                    last  = 0;



            Assert::IsTrue (text.starts_with ("version\tmajor=2,minor=0\n"));

            for (const std::string & keyword : order)
            {
                size_t  at = text.find ("\n" + keyword);

                if (keyword == "version\t")
                {
                    continue;
                }

                Assert::AreNotEqual (std::string::npos, at, std::wstring (keyword.begin(), keyword.end()).c_str());
                Assert::IsTrue (at > last, L"each kind of record follows the one before it");
                last = at;
            }
        }


        TEST_METHOD (AFileRecordCarriesItsSizeTimeAndHash)
        {
            DebugFile  file = ReadBack (DebugFileWriter::Format (BuildSample()));



            Assert::AreEqual ((size_t) 2, file.files.size());
            Assert::AreEqual (std::string ("src/main.a65"),                 file.files[0].name);
            Assert::AreEqual ((uint64_t) strlen (s_kMain),                  file.files[0].size);
            Assert::AreEqual ((uint64_t) 0x66E9A1B2,                        file.files[0].mtime);
            Assert::AreEqual (Sha1::ComputeTextHex (s_kMain),               file.files[0].sha1);
            Assert::AreEqual (std::string ("src/defs.inc"),                 file.files[1].name);
            Assert::AreEqual (Sha1::ComputeTextHex (s_kDefs),               file.files[1].sha1);
            Assert::AreEqual (std::string ("main"),                         file.modules.at (0).name);
        }


        TEST_METHOD (SpansAreRelativeToTheirSegment)
        {
            DebugFile  file = ReadBack (DebugFileWriter::Format (BuildSample()));



            Assert::AreEqual ((size_t) 1,       file.segments.size());
            Assert::AreEqual ((uint32_t) 0x0300, file.segments[0].start);
            Assert::AreEqual ((uint32_t) 6,      file.segments[0].size);
            Assert::AreEqual ((uint32_t) 0,      file.spans[0].start);
            Assert::AreEqual ((uint32_t) 5,      file.spans.back().start, L"rts, five bytes in");
        }


        TEST_METHOD (AMacroLineIsRecordedAtEveryDepth)
        {
            std::string  text  = DebugFileWriter::Format (BuildSample());
            LineTable    table;



            table.Build (ReadBack (text));

            const std::vector<SourcePosition> & at = table.GetPositionsAt (0x0304);

            Assert::AreEqual ((size_t) 3, at.size(), L"the invocation, the outer body line, the inner body line");
            Assert::AreEqual (11, at[0].line);
            Assert::AreEqual (8,  at[1].line);
            Assert::AreEqual (4,  at[2].line);
            Assert::AreEqual (2,  at[2].depth);
            Assert::IsTrue   (at[2].type == DebugLineType::Macro);
            Assert::IsTrue   (text.find (",type=2,count=2,") != std::string::npos);
        }


        TEST_METHOD (AnInvocationLineCoversEveryByteItsExpansionMade)
        {
            LineTable  table;



            table.Build (ReadBack (DebugFileWriter::Format (BuildSample())));

            std::vector<std::pair<Word, Word>> ranges = table.GetRanges (0, 11);

            Assert::AreEqual ((size_t) 2,    ranges.size(), L"lda and nop");
            Assert::AreEqual ((Word) 0x0302, ranges[0].first);
            Assert::AreEqual ((Word) 0x0304, ranges[1].second);
        }


        TEST_METHOD (SymbolsCarryTheirKindAndSegment)
        {
            DebugFile  file  = ReadBack (DebugFileWriter::Format (BuildSample()));
            auto       find  = [&] (const char * name) -> const DebugSymbol &
                               {
                                   for (const DebugSymbol & each : file.symbols)
                                   {
                                       if (each.name == name) { return each; }
                                   }

                                   Assert::Fail();
                                   return file.symbols.front();
                               };



            Assert::AreEqual ((uint32_t) 0x0302, find ("start").value);
            Assert::AreEqual (std::string ("lab"), find ("start").type);
            Assert::AreEqual (0,                   find ("start").segment);
            Assert::AreEqual (std::string ("equ"), find ("limit").type);
            Assert::AreEqual (-1,                  find ("limit").segment);

            for (const DebugSymbol & each : file.symbols)
            {
                Assert::IsFalse (each.name == "__65SC02__", L"the assembler's own names are left out");
            }
        }


        TEST_METHOD (LocalLabelsGoInTheirOwnScope)
        {
            AssemblyResult  result;
            DebugFile       file;



            result.success = true;
            result.symbols["main"]               = 0x0300;
            result.symbols["main.loop"]          = 0x0302;
            result.symbolKinds["main"]           = SymbolKind::Label;
            result.symbolKinds["main.loop"]      = SymbolKind::Label;
            result.localSymbols.insert ("main.loop");
            result.debugLines.push_back ({ 0x0300, 4, 0, { { "", 1 } } });

            file = ReadBack (DebugFileWriter::Format (DebugFileWriter::Build (result, {})));

            Assert::AreEqual ((size_t) 2,                    file.scopes.size());
            Assert::AreEqual (DebugFileWriter::kModuleScope, file.symbols[0].scope);
            Assert::AreEqual (DebugFileWriter::kLocalScope,  file.symbols[1].scope);
            Assert::AreEqual (DebugFileWriter::kModuleScope, file.scopes[1].parent);
        }


        TEST_METHOD (EachOutputIsItsOwnSegment)
        {
            AssemblyResult  result;
            DebugFile       file;



            result.success = true;
            result.debugLines.push_back ({ 0x0300, 3, 0, { { "", 1 } } });
            result.debugLines.push_back ({ 0x0300, 2, 1, { { "", 5 } } });

            file = ReadBack (DebugFileWriter::Format (DebugFileWriter::Build (result, {})));

            Assert::AreEqual ((size_t) 2, file.segments.size(), L"two outputs that both begin at $0300");
            Assert::AreEqual (0, file.spans[0].segment);
            Assert::AreEqual (1, file.spans[1].segment);
        }


        TEST_METHOD (TheFileReadsBackAsWritten)
        {
            DebugFile  written = BuildSample();
            DebugFile  read    = ReadBack (DebugFileWriter::Format (written));



            Assert::AreEqual (DebugFileWriter::Format (written), DebugFileWriter::Format (read));
        }


        TEST_METHOD (ANameWithAQuoteSurvives)
        {
            AssemblyResult  result;
            DebugFile       file;



            result.success = true;
            result.debugLines.push_back ({ 0x0300, 1, 0, { { "", 1 } } });

            file = ReadBack (DebugFileWriter::Format (DebugFileWriter::Build (result, { { "", { "a \"b\\c\".s", 0 } } })));

            Assert::AreEqual (std::string ("a \"b\\c\".s"), file.files[0].name);
        }
    };
}
