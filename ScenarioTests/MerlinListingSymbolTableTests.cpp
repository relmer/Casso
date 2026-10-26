#include "Pch.h"

#include "EhmTestHelper.h"
#include "TestHelpers.h"
#include "Assembler.h"
#include "CommandLineOptions.h"
#include "Cli/ArtifactWriter.h"
#include "DialectRegistry.h"
#include "DialectProfile.h"
#include "TestCpu65C02.h"
#include "DialectReporting.h"





using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace MerlinListingSymbolTableTests
{
    ////////////////////////////////////////////////////////////////////////////////
    //
    //  MerlinListingGateTests
    //
    //  WHICH DIALECT GETS THE TABLE, asserted through the real writer rather than
    //  through the formatter. The unit suite's formatter tests prove the layout;
    //  only the writer proves that an AS65 listing is left alone, because the
    //  gate is the writer's and a formatter cannot fail it.
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
