#include "Pch.h"

#include "Debugger/SymbolFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReaderTests
//
//  The five symbol file formats, detection from content, and an error for
//  a file in none of them. Every read asserts a non-zero count first.
//
////////////////////////////////////////////////////////////////////////////////

namespace DebuggerTests
{
    TEST_CLASS (SymbolFileReaderTests)
    {
    public:

        static std::vector<SymbolFileEntry> ReadOk (const std::string & content, SymbolFileFormat expected)
        {
            std::vector<SymbolFileEntry>  symbols;
            SymbolFileFormat              format = SymbolFileFormat::Unknown;
            std::string                   error;
            HRESULT                       hr     = SymbolFileReader::Read (content, symbols, format, error);



            Assert::AreEqual (S_OK, hr, std::wstring (error.begin(), error.end()).c_str());
            Assert::AreEqual ((int) expected, (int) format);
            Assert::IsTrue   (!symbols.empty(), L"a symbol file yields symbols");
            return symbols;
        }

        static Word Find (const std::vector<SymbolFileEntry> & symbols, const std::string & name)
        {
            for (const SymbolFileEntry & symbol : symbols)
            {
                if (symbol.name == name)
                {
                    return symbol.address;
                }
            }

            Assert::Fail ((L"missing " + std::wstring (name.begin(), name.end())).c_str());
            return 0;
        }



        TEST_METHOD (CassoDebug_BothSections_OneSymbolEach)
        {
            static constexpr const char * kFile =
                "; by address\n"
                "zebra=$0300\n"
                "mid=$0308\n"
                "ALPHA=$0310\n"
                "\n"
                "; by symbol\n"
                "ALPHA=$0310\n"
                "mid=$0308\n"
                "zebra=$0300\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::CassoDebug);



            Assert::AreEqual ((size_t) 3,    symbols.size(), L"the second section repeats the first");
            Assert::AreEqual ((Word) 0x0300, Find (symbols, "zebra"));
            Assert::AreEqual ((Word) 0x0310, Find (symbols, "ALPHA"));
        }



        TEST_METHOD (Cc65Debug_TopLevelSymbolsOnly)
        {
            static constexpr const char * kFile =
                "version\tmajor=2,minor=0\n"
                "file\tid=0,name=\"main.a65\",size=10,mtime=0,mod=0\n"
                "mod\tid=0,name=\"main\",file=0\n"
                "seg\tid=0,name=\"CODE\",start=0x0300,size=4,addrsize=absolute,type=rw\n"
                "sym\tid=0,name=\"start\",addrsize=absolute,scope=0,val=0x0300,seg=0,type=lab\n"
                "sym\tid=1,name=\"start.loop\",addrsize=absolute,scope=1,val=0x0302,seg=0,type=lab\n"
                "sym\tid=2,name=\"limit\",addrsize=absolute,scope=0,val=0x0007,type=equ\n"
                "scope\tid=0,name=\"\",mod=0\n"
                "scope\tid=1,name=\"local\",mod=0,parent=0\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::Cc65Debug);



            Assert::AreEqual ((size_t) 2,    symbols.size(), L"the local label stays out");
            Assert::AreEqual ((Word) 0x0300, Find (symbols, "start"));
            Assert::AreEqual ((Word) 0x0007, Find (symbols, "limit"));
        }



        TEST_METHOD (Cc65Debug_WrongVersionSaysWhich)
        {
            std::vector<SymbolFileEntry>  symbols;
            SymbolFileFormat              format = SymbolFileFormat::Unknown;
            std::string                   error;
            HRESULT                       hr     = SymbolFileReader::Read ("version\tmajor=3,minor=0\n", symbols, format, error);



            Assert::IsTrue   (FAILED (hr));
            Assert::AreEqual ((int) SymbolFileFormat::Cc65Debug, (int) format);
            Assert::IsTrue   (error.find ("version 3") != std::string::npos);
        }


        TEST_METHOD (AppleWinSym_AddressThenName)
        {
            static constexpr const char * kFile =
                "; IO Map\n"
                "C000 KEYBOARD\n"
                "FDED COUT\r\n"
                "\n"
                "FC58 HOME\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::AppleWinSym);



            Assert::AreEqual ((size_t) 3,    symbols.size());
            Assert::AreEqual ((Word) 0xC000, Find (symbols, "KEYBOARD"));
            Assert::AreEqual ((Word) 0xFDED, Find (symbols, "COUT"));
        }



        TEST_METHOD (ViceLabels_AlAddressDotName)
        {
            static constexpr const char * kFile =
                "al C:0300 .start\n"
                "al 0310 .loop\n"
                "al FDED .cout\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk ("al 0310 .loop\nal FDED .cout\n", SymbolFileFormat::ViceLabels);



            Assert::AreEqual ((size_t) 2,    symbols.size());
            Assert::AreEqual ((Word) 0x0310, Find (symbols, "loop"));
            Assert::AreEqual ((Word) 0xFDED, Find (symbols, "cout"));
            Assert::AreEqual ((int) SymbolFileFormat::ViceLabels, (int) SymbolFileReader::Detect (kFile));
        }



        //  Merlin Pro 2.23 assembling LABELS.S, captured off the emulated screen.
        //  The layout is recorded in research R-009.
        //
        //  An earlier version of this test carried a `]LOOP` entry, which no
        //  Merlin listing can contain: Merlin lists no `]` variable and no local
        //  label. The reader accepted it, so the test passed while documenting a
        //  format that does not exist.
        TEST_METHOD (MerlinListing_EntriesAfterTheHeading)
        {
            static constexpr const char * kFile =
                "83D7: 00        113  END      BRK              ;table end\n"
                "\n"
                "--End assembly, 984 bytes, Errors: 0\n"
                "\n"
                "Symbol table - alphabetical order:\n"
                "\n"
                "   END     =$83D7      LABTBL  =$8000\n"
                "\n"
                "Symbol table - numerical order:\n"
                "\n"
                "   LABTBL  =$8000      END     =$83D7\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::MerlinListing);



            Assert::AreEqual ((size_t) 2,    symbols.size(), L"the numerical section repeats the alphabetical one");
            Assert::AreEqual ((Word) 0x83D7, Find (symbols, "END"));
            Assert::AreEqual ((Word) 0x8000, Find (symbols, "LABTBL"));
        }





        //  A real listing carries a two-character flag field before the name:
        //  `MD` for a macro definition, `M ` for a label a macro expansion
        //  produced. Rows from Merlin Pro 2.23 assembling MAKE DUMP.S.
        //
        //  THE FLAG MUST NOT BECOME A SYMBOL. It is a bare token sitting exactly
        //  where a name sits, so a reader pairing tokens off has every chance of
        //  binding `MD` to the address that follows it.
        TEST_METHOD (MerlinListing_FlagFieldIsNotASymbol)
        {
            static constexpr const char * kFile =
                "Symbol table - alphabetical order:\n"
                "\n"
                "   CALLMAIN=$0A92      ZPFLAGA =$0AA5   MD MOV     =$8000   MD MOVD    =$8000\n"
                "MD INCD    =$8000   MD DECD    =$8000      AMPER   =$03F5   M  ND      =$09C4\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::MerlinListing);



            //  The names on either side of a flag both survive it.
            Assert::AreEqual ((Word) 0x0A92, Find (symbols, "CALLMAIN"));
            Assert::AreEqual ((Word) 0x0AA5, Find (symbols, "ZPFLAGA"));
            Assert::AreEqual ((Word) 0x03F5, Find (symbols, "AMPER"));

            //  Including the flagged entries themselves, under their own names.
            Assert::AreEqual ((Word) 0x8000, Find (symbols, "MOV"));
            Assert::AreEqual ((Word) 0x8000, Find (symbols, "INCD"));
            Assert::AreEqual ((Word) 0x09C4, Find (symbols, "ND"));

            //  And the flags are not symbols.
            for (const SymbolFileEntry & entry : symbols)
            {
                Assert::AreNotEqual (std::string ("MD"), entry.name, L"MD is a flag, not a symbol");
                Assert::AreNotEqual (std::string ("M"),  entry.name, L"M is a flag, not a symbol");
            }
        }



        TEST_METHOD (Unrecognized_IsAnError)
        {
            std::vector<SymbolFileEntry>  symbols;
            SymbolFileFormat              format = SymbolFileFormat::CassoDebug;
            std::string                   error;
            HRESULT                       hr     = SymbolFileReader::Read ("hello world\nnot a symbol file\n", symbols, format, error);



            Assert::IsTrue   (FAILED (hr));
            Assert::AreEqual ((int) SymbolFileFormat::Unknown, (int) format);
            Assert::IsFalse  (error.empty());
            Assert::IsTrue   (symbols.empty());

            hr = SymbolFileReader::Read ("", symbols, format, error);
            Assert::IsTrue   (FAILED (hr), L"an empty file is not a symbol file");
        }
    };
}
