#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "Assembler.h"
#include "CommandLineOptions.h"
#include "Cli/ArtifactWriter.h"
#include "DialectRegistry.h"
#include "DialectProfile.h"
#include "TestCpu65C02.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinListingSymbolTableTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinSymbolTableTests
    //
    //  The table a Merlin listing ends with, against the layout research R-009
    //  records from Merlin Pro 2.23 assembling `MAKE DUMP.S` under Casso.
    //
    //  THE EXPECTATIONS ARE INLINE RATHER THAN A FIXTURE, and that is a decision
    //  rather than a shortcut. A Merlin listing cannot be got off the emulated
    //  machine into a file: the listing scrolls past a 24-line screen faster than
    //  it can be captured, and Casso's printer renders to a dot raster and then to
    //  an image, never to text. What IS capturable is the table itself, which sits
    //  static at the end of the assembly, and that capture is what R-009 holds and
    //  what these strings quote.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinSymbolTableTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////
        //
        //  BothSectionsAppear
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BothSectionsAppear)
        {
            std::unordered_map<std::string, Word>  symbols = { { "LABTBL", 0x8000 }, { "END", 0x83D7 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);



            Assert::IsTrue (table.find ("Symbol table - alphabetical order:") != std::string::npos,
                            L"the alphabetical heading");
            Assert::IsTrue (table.find ("Symbol table - numerical order:") != std::string::npos,
                            L"the numerical heading");
            Assert::IsTrue (table.find ("Symbol table - alphabetical order:") <
                            table.find ("Symbol table - numerical order:"),
                            L"alphabetical comes first");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  EntryLayoutMatchesMerlin
        //
        //  The row Merlin printed for this source, reproduced exactly: three
        //  columns of flag field, the name padded to eight, then `=$` and the
        //  address.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (EntryLayoutMatchesMerlin)
        {
            std::unordered_map<std::string, Word>  symbols = { { "LABTBL", 0x8000 }, { "END", 0x83D7 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);



            //  Merlin Pro 2.23 printed exactly this for LABELS.S.
            Assert::IsTrue (table.find ("   END     =$83D7      LABTBL  =$8000") != std::string::npos,
                            L"the alphabetical row as Merlin printed it");
            Assert::IsTrue (table.find ("   LABTBL  =$8000      END     =$83D7") != std::string::npos,
                            L"the numerical row as Merlin printed it");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  ZeroPageValuesPrintTwoDigits
        //
        //  `SOURCE  =$0A`, not `=$000A`. The width says which page the address is
        //  on, so widening it loses what Merlin was saying.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ZeroPageValuesPrintTwoDigits)
        {
            std::unordered_map<std::string, Word>  symbols = { { "SOURCE", 0x0A }, { "INTRFACE", 0x0300 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);



            Assert::IsTrue (table.find ("SOURCE  =$0A")    != std::string::npos, L"zero page in two digits");
            Assert::IsTrue (table.find ("INTRFACE=$0300")  != std::string::npos, L"above it in four");
            Assert::IsTrue (table.find ("SOURCE  =$000A")  == std::string::npos, L"never widened to four");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  FourEntriesToARow
        //
        //  Merlin's grid, and the reason the stride is padded to rather than
        //  counted from: a row mixing zero-page and higher addresses still has its
        //  names aligned.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (FourEntriesToARow)
        {
            std::unordered_map<std::string, Word>  symbols;
            std::vector<std::string>               rows;
            std::string                            table;
            std::istringstream                     stream;
            std::string                            line;



            //  Five symbols, so the grid has to wrap exactly once.
            symbols["AA"] = 0x0A;
            symbols["BB"] = 0x0300;
            symbols["CC"] = 0x0C;
            symbols["DD"] = 0x0400;
            symbols["EE"] = 0x0500;

            table = Assembler::FormatMerlinSymbolTable (symbols);
            stream.str (table);

            while (std::getline (stream, line))
            {
                if (line.find ("=$") != std::string::npos)
                {
                    rows.push_back (line);
                }
            }

            //  Two sections, each wrapping five entries into four then one.
            Assert::AreEqual ((size_t) 4, rows.size(), L"two sections of two rows each");

            for (const std::string & row : rows)
            {
                Assert::IsTrue (row.size() <= 80, L"a row fits the 80-column screen Merlin lists to");
                Assert::AreNotEqual (' ', row[row.size() - 1], L"no row carries trailing spaces");
            }

            //  The first row of a section is full and the second holds the
            //  remainder, which is what proves the wrap point rather than merely
            //  that some wrapping happened.
            Assert::AreEqual ((size_t) 4, (size_t) std::count (rows[0].begin(), rows[0].end(), '$'), L"four to the first row");
            Assert::AreEqual ((size_t) 1, (size_t) std::count (rows[1].begin(), rows[1].end(), '$'), L"one to the second");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  NamesAlignAcrossMixedWidths
        //
        //  The property the padding exists for, asserted directly: a zero-page
        //  entry and a higher one in the same row start their names at the same
        //  columns.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NamesAlignAcrossMixedWidths)
        {
            std::unordered_map<std::string, Word>  symbols = { { "AAA", 0x0A }, { "BBB", 0x0300 },
                                                               { "CCC", 0x0C }, { "DDD", 0x0400 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);
            std::istringstream                     stream (table);
            std::string                            line;
            std::string                            row;



            while (std::getline (stream, line))
            {
                if (line.find ("AAA") != std::string::npos)
                {
                    row = line;
                    break;
                }
            }

            Assert::IsFalse (row.empty(), L"found the row");

            //  Every name begins a stride apart whatever its neighbour's value
            //  width, which is what a reader parsing by column depends on.
            Assert::AreEqual ((size_t) 3,  row.find ("AAA"), L"first name at the flag field's end");
            Assert::AreEqual ((size_t) 23, row.find ("BBB"), L"second a stride along");
            Assert::AreEqual ((size_t) 43, row.find ("CCC"), L"third");
            Assert::AreEqual ((size_t) 63, row.find ("DDD"), L"fourth");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  NumericalSectionOrdersByAddress
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (NumericalSectionOrdersByAddress)
        {
            std::unordered_map<std::string, Word>  symbols = { { "ZZZ", 0x0A }, { "AAA", 0x9000 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);
            size_t                                 numeric = table.find ("Symbol table - numerical order:");
            std::string                            section = table.substr (numeric);



            //  ZZZ sorts last by name and first by address, so the two sections
            //  disagree -- which is the whole point of printing both.
            Assert::IsTrue (section.find ("ZZZ") < section.find ("AAA"),
                            L"the numerical section is ordered by address, not by name");

            Assert::IsTrue (table.find ("AAA") < table.find ("ZZZ"),
                            L"and the alphabetical section still leads with AAA");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  BuiltinsAreLeftOut
        //
        //  The assembler defines ERRORS, __65SC02__ and __6502X__ so `IFDEF` is
        //  answerable. They are not the source's symbols and Merlin never printed
        //  them, so a table claiming to be Merlin's must not carry them.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (BuiltinsAreLeftOut)
        {
            std::unordered_map<std::string, Word>  symbols = { { "LABTBL", 0x8000 }, { "END", 0x83D7 },
                                                               { "ERRORS", 0 }, { "__65SC02__", 0 },
                                                               { "__6502X__", 0 } };
            std::set<std::string>                  omit    = { "ERRORS", "__65SC02__", "__6502X__" };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols, omit);



            Assert::IsTrue (table.find ("ERRORS")     == std::string::npos, L"no ERRORS");
            Assert::IsTrue (table.find ("__65SC02__") == std::string::npos, L"no __65SC02__");
            Assert::IsTrue (table.find ("__6502X__")  == std::string::npos, L"no __6502X__");

            //  What real Merlin printed for this source, and only that.
            Assert::IsTrue (table.find ("   END     =$83D7      LABTBL  =$8000") != std::string::npos,
                            L"the two symbols the source defined, laid out as Merlin laid them");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  AnOverlongNameStillSeparatesFromTheNext
        //
        //  A name wider than the eight-column field loses the grid for its row.
        //  It must not also lose the gap, which would run two entries together
        //  into one unreadable token.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (AnOverlongNameStillSeparatesFromTheNext)
        {
            std::unordered_map<std::string, Word>  symbols = { { "AVERYLONGNAMEINDEED", 0x0300 },
                                                               { "ZZ", 0x0400 } };
            std::string                            table   = Assembler::FormatMerlinSymbolTable (symbols);



            Assert::IsTrue (table.find ("AVERYLONGNAMEINDEED=$0300 ") != std::string::npos,
                            L"the overlong entry is followed by a separator");
            Assert::IsTrue (table.find ("=$0300ZZ") == std::string::npos,
                            L"and never runs into the entry after it");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinListingGateTests
    //
    //  WHICH DIALECT GETS THE TABLE, asserted through the real writer rather than
    //  through the formatter. The formatter tests above prove the layout; only the
    //  writer proves that an AS65 listing is left alone, because the gate is the
    //  writer's and a formatter cannot fail it.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinListingGateTests)
    {
    public:

        ////////////////////////////////////////////////////////////////////////////
        //
        //  WriteAndRead
        //
        //  One listing written to a temporary file and read back, so the assertion
        //  is against the bytes a caller of the tool would get.
        //
        ////////////////////////////////////////////////////////////////////////////

        static std::string WriteAndRead (DialectId dialect)
        {
            AssemblyResult      result;
            CommandLineOptions  options;
            AssemblyLine        line;
            std::string         path = (std::filesystem::temp_directory_path() /
                                        ("casso-merlin-listing-test.lst")).string();
            std::ifstream       reader;
            std::ostringstream  contents;



            line.lineNumber = 1;
            line.address    = 0x0300;
            line.hasAddress = true;
            line.sourceText = "START    LDA #$41";

            result.listing.push_back (line);
            result.symbols["START"] = 0x0300;

            options.dialect     = dialect;
            options.listingFile = path;

            ArtifactWriter::WriteListing (result, options, {});

            reader.open (path);
            contents << reader.rdbuf();
            reader.close();

            std::filesystem::remove (path);

            return contents.str();
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  MerlinListingCarriesTheTable
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (MerlinListingCarriesTheTable)
        {
            std::string  listing = WriteAndRead (DialectId::Merlin);



            Assert::IsTrue (listing.find ("Symbol table - alphabetical order:") != std::string::npos,
                            L"a Merlin listing ends with its symbol table");
            Assert::IsTrue (listing.find ("START   =$0300") != std::string::npos,
                            L"and the symbol is in it");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  As65ListingIsUnchanged
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (As65ListingIsUnchanged)
        {
            std::string  listing = WriteAndRead (DialectId::As65);



            Assert::IsTrue (listing.find ("Symbol table") == std::string::npos,
                            L"an AS65 listing gains no symbol table");

            //  The listing itself is still written, so the assertion above is
            //  about the table rather than about an empty file.
            Assert::IsTrue (listing.find ("LDA #$41") != std::string::npos,
                            L"and is otherwise a listing");
        }
    };





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinLocalLabelTests
    //
    //  WHAT MERLIN'S TABLE HOLDS: the source's top-level symbols, and no local
    //  label. Measured against Merlin Pro 2.23 assembling `MAKE DUMP.S`, whose
    //  table lists none of that source's twelve locals.
    //
    //  The classification is recorded where the stored name is built, because
    //  nothing about the finished name says what it was built from -- the scope
    //  separator is a legal label character in some dialects, and a source symbol
    //  may end in digits like a per-invocation macro label does.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (MerlinLocalLabelTests)
    {
    public:

        static AssemblyResult AssembleMerlin (const std::string & source)
        {
            TestCpu           cpu;
            AssemblerOptions  options = {};



            cpu.InitForTest();
            options.dialect = DialectId::Merlin;

            Assembler  assembler (cpu.GetInstructionSet(), options);

            return assembler.Assemble (source);
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  ALocalLabelIsRecordedAsOne
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (ALocalLabelIsRecordedAsOne)
        {
            AssemblyResult  result = AssembleMerlin ("OWNER    LDA #$01\n"
                                                     ":SPIN    BNE :SPIN\n"
                                                     "         RTS\n");



            //  Stored under the scope it belongs to, and known to be a local.
            Assert::IsTrue (result.symbols.count ("OWNER.SPIN") > 0,
                            L"the local is bound under its owner");
            Assert::IsTrue (result.localSymbols.count ("OWNER.SPIN") > 0,
                            L"and is recorded as a local");

            //  The owner is an ordinary symbol and must not be swept up with it.
            Assert::IsTrue (result.symbols.count ("OWNER") > 0,      L"the owner is a symbol");
            Assert::IsTrue (result.localSymbols.count ("OWNER") == 0, L"and is not a local");
        }





        ////////////////////////////////////////////////////////////////////////////
        //
        //  TheTableLeavesLocalsOut
        //
        //  Through the writer, because the omission is the writer's decision and
        //  the assembler only supplies the classification.
        //
        ////////////////////////////////////////////////////////////////////////////

        TEST_METHOD (TheTableLeavesLocalsOut)
        {
            AssemblyResult      result  = AssembleMerlin ("OWNER    LDA #$01\n"
                                                          ":SPIN    BNE :SPIN\n"
                                                          "         RTS\n");
            CommandLineOptions  options;
            std::string         path    = (std::filesystem::temp_directory_path() /
                                           "casso-merlin-locals-test.lst").string();
            std::ifstream       reader;
            std::ostringstream  contents;
            std::string         listing;



            options.dialect     = DialectId::Merlin;
            options.listingFile = path;

            ArtifactWriter::WriteListing (result, options, {});

            reader.open (path);
            contents << reader.rdbuf();
            reader.close();
            std::filesystem::remove (path);

            listing = contents.str();

            Assert::IsTrue (listing.find ("Symbol table") != std::string::npos, L"the table is there");
            Assert::IsTrue (listing.find ("OWNER   =")     != std::string::npos, L"the owner is listed");
            Assert::IsTrue (listing.find ("OWNER.SPIN")    == std::string::npos, L"the local is not");
        }
    };
}
