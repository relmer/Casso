#include "Pch.h"

#include "Debugger/SymbolFileReader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  SymbolFileReaderTests
//
//  The four symbol file formats, detection from content, and an error for
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



        TEST_METHOD (MerlinListing_EntriesAfterTheHeading)
        {
            static constexpr const char * kFile =
                "0300: A9 41        1 START    LDA #$41\n"
                "0302: 60           2          RTS\n"
                "\n"
                "--End assembly, 3 bytes, Errors: 0\n"
                "\n"
                "Symbol table - alphabetical order:\n"
                "\n"
                "   COUT    =$FDED     START   =$0300     ]LOOP   =$0302\n"
                "\n"
                "Symbol table - numerical order:\n"
                "\n"
                "   START   =$0300     ]LOOP   =$0302     COUT    =$FDED\n";

            std::vector<SymbolFileEntry>  symbols = ReadOk (kFile, SymbolFileFormat::MerlinListing);



            Assert::AreEqual ((size_t) 3,    symbols.size(), L"the numerical section repeats the alphabetical one");
            Assert::AreEqual ((Word) 0x0300, Find (symbols, "START"));
            Assert::AreEqual ((Word) 0x0302, Find (symbols, "]LOOP"));
            Assert::AreEqual ((Word) 0xFDED, Find (symbols, "COUT"));
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
