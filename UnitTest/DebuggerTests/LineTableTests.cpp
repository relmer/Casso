#include "Pch.h"

#include "Debugger/DebugFileReader.h"
#include "Debugger/LineTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LineTableTests
//
//  Addresses to source lines and back, from a debug file (FR-033, FR-033a,
//  FR-055, SC-010).
//
//  A MACRO'S BYTES BELONG TO TWO LINES: the invocation and the body line that
//  produced them. The table gives both, outermost first, which is what lets
//  the source view show the invocation and still step into the body.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    //  main.a65 line 12 assembles two bytes at $0300 and line 13 three at
    //  $0302; line 14 invokes a macro whose body, macros.inc line 4, produced
    //  both spans again.
    static const char * const  s_kLines =
        "version\tmajor=2,minor=0\n"
        "file\tid=0,name=\"main.a65\",size=100,mtime=0,mod=0\n"
        "file\tid=1,name=\"macros.inc\",size=50,mtime=0,mod=0\n"
        "seg\tid=0,name=\"CODE\",start=0x0300,size=5,addrsize=absolute,type=rw\n"
        "span\tid=0,seg=0,start=0,size=2\n"
        "span\tid=1,seg=0,start=2,size=3\n"
        "line\tid=0,file=0,line=12,span=0\n"
        "line\tid=1,file=0,line=13,span=1\n"
        "line\tid=2,file=1,line=4,type=2,count=1,span=0+1\n";



    TEST_CLASS (LineTableTests)
    {
    public:

        static LineTable Build (const std::string & text)
        {
            DebugFile    file;
            std::string  error;
            LineTable    table;



            Assert::AreEqual (S_OK, DebugFileReader::Read (text, file, error));
            table.Build (file);
            return table;
        }



        TEST_METHOD (AnAddressGivesItsLinesOutermostFirst)
        {
            LineTable                            table     = Build (s_kLines);
            const std::vector<SourcePosition>  & positions = table.GetPositionsAt (0x0301);



            Assert::AreEqual ((size_t) 2, positions.size());
            Assert::AreEqual (0,  positions[0].file);
            Assert::AreEqual (12, positions[0].line);
            Assert::IsTrue   (positions[0].type == DebugLineType::Asm);
            Assert::AreEqual (1,  positions[1].file);
            Assert::AreEqual (4,  positions[1].line);
            Assert::AreEqual (1,  positions[1].depth, L"the macro body, one level in");
        }


        TEST_METHOD (AnAddressNoLineProducedHasNone)
        {
            Assert::IsTrue (Build (s_kLines).GetPositionsAt (0x0310).empty());
        }


        TEST_METHOD (ALineGivesTheAddressesItProduced)
        {
            LineTable                              table  = Build (s_kLines);
            std::vector<std::pair<Word, Word>>     ranges = table.GetRanges (0, 13);



            Assert::AreEqual ((size_t) 1,        ranges.size());
            Assert::AreEqual ((Word) 0x0302,     ranges[0].first);
            Assert::AreEqual ((Word) 0x0304,     ranges[0].second);
            Assert::AreEqual ((size_t) 2,        table.GetRanges (1, 4).size(), L"a macro body line produced both spans");
        }


        TEST_METHOD (ALineWithoutCodeMovesToTheNextOne)
        {
            LineTable  table = Build (s_kLines);



            Assert::AreEqual (12, *table.GetNextLineWithCode (0, 1));
            Assert::AreEqual (13, *table.GetNextLineWithCode (0, 13), L"a line with code is its own");
            Assert::IsFalse  (table.GetNextLineWithCode (0, 14).has_value(), L"nothing after the last line");
        }


        TEST_METHOD (ASegmentPlacedElsewhereMovesItsLines)
        {
            std::string  text = s_kLines;



            text.replace (text.find ("start=0x0300"), 12, "start=0x2000");

            Assert::AreEqual (12, Build (text).GetPositionsAt (0x2000).at (0).line, L"spans are relative to their segment");
            Assert::IsTrue   (Build (text).GetPositionsAt (0x0300).empty());
        }


        TEST_METHOD (EveryLineWithCodeResolvesBothWays)
        {
            DebugFile    file;
            std::string  error;
            LineTable    table;
            size_t       checked = 0;



            Assert::AreEqual (S_OK, DebugFileReader::Read (s_kLines, file, error));
            table.Build (file);

            for (const DebugLine & line : file.lines)
            {
                for (const std::pair<Word, Word> & range : table.GetRanges (line.file, line.line))
                {
                    for (uint32_t address = range.first; address <= range.second; address++)
                    {
                        const std::vector<SourcePosition> & at = table.GetPositionsAt ((Word) address);

                        Assert::IsTrue (std::any_of (at.begin(), at.end(),
                                        [&] (const SourcePosition & p) { return p.file == line.file && p.line == line.line; }));
                        checked++;
                    }
                }
            }

            Assert::AreEqual ((size_t) 10, checked, L"five bytes for main.a65's lines and the same five for the macro body");
        }
    };
}
