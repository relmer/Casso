#include "Pch.h"

#include "Debugger/DebugFileReader.h"

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
}
