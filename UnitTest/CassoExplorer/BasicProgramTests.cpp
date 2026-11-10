#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "ApplesoftTokenizer.h"
#include "BasicCrossReference.h"
#include "BasicProgram.h"
#include "BasicRenumber.h"
#include "IntegerBasicTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicProgramTests
//
//  Programs are tokenized from a listing, renumbered or cross-referenced, and
//  compared with the bytes of the listing that is the correct result.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BasicProgramTests)
{
public:
    using Dialect = BasicProgram::Dialect;

    static std::vector<Byte> Tokenize (Dialect dialect, const std::string & listing)
    {
        std::vector<Byte>      bytes;
        ApplesoftListingError  error;
        HRESULT                hr = S_OK;



        hr = (dialect == Dialect::Integer) ? IntegerBasicTokenizer::Tokenize (listing, bytes, error)
                                           : ApplesoftTokenizer::Tokenize    (listing, bytes, error);

        Assert::IsTrue (SUCCEEDED (hr), TextEncoding::NarrowToWide (listing + " : " + error.reason).c_str());

        return bytes;
    }


    static void AssertRenumbered (Dialect dialect, const std::string & listing, uint32_t start, uint32_t step, const std::string & correct)
    {
        std::vector<Byte>  bytes;
        std::string        reason;
        std::wstring       message = TextEncoding::NarrowToWide (listing);



        Assert::IsTrue (SUCCEEDED (BasicRenumber::Renumber (Tokenize (dialect, listing), start, step, bytes, reason)), message.c_str());
        Assert::IsTrue (Tokenize (dialect, correct) == bytes, message.c_str());
    }


    static std::string FindEntry (const std::vector<BasicCrossReference::Entry> & entries, const std::string & label)
    {
        std::string  lines = "absent";



        for (const BasicCrossReference::Entry & entry : entries)
        {
            if (entry.label != label)
            {
                continue;
            }

            lines.clear();

            for (uint32_t line : entry.lines)
            {
                lines += std::to_string (line) + " ";
            }

            lines += entry.isMissing ? "missing" : "";
        }

        return lines;
    }


    TEST_METHOD (Parse_ReadsTheDialectFromTheBytes)
    {
        BasicProgram::Program  program;
        std::string            reason;
        std::vector<Byte>      bytes;



        Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (Tokenize (Dialect::Applesoft, "10 PRINT \"HI\"\n20 END\n"), program, reason)));
        Assert::IsTrue (program.dialect == Dialect::Applesoft);
        Assert::AreEqual ((size_t) 2, program.lines.size());
        Assert::AreEqual ((uint32_t) 20, program.lines[1].number);
        Assert::AreEqual ((int) ApplesoftTokenizer::kProgramBase, (int) program.base);

        Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (Tokenize (Dialect::Integer, "10 PRINT \"HI\"\n20 END\n"), program, reason)));
        Assert::IsTrue (program.dialect == Dialect::Integer);
        Assert::AreEqual ((size_t) 2, program.lines.size());
        Assert::AreEqual ((uint32_t) 10, program.lines[0].number);

        Assert::IsTrue  (FAILED (BasicProgram::Parse (bytes, program, reason)));
        Assert::IsFalse (reason.empty());

        bytes = { 0x4C, 0x00, 0x20, 0x60 };
        Assert::IsTrue (FAILED (BasicProgram::Parse (bytes, program, reason)));
    }


    TEST_METHOD (Parse_ThenEmit_ReturnsTheBytesItRead)
    {
        const std::string      listing = "10 FOR I=1 TO 10\n20 PRINT I\n30 NEXT I\n40 GOTO 10\n";
        BasicProgram::Program  program;
        std::string            reason;
        std::vector<Byte>      source;
        std::vector<Byte>      bytes;
        uint32_t               longLine = 0;



        for (Dialect dialect : { Dialect::Applesoft, Dialect::Integer })
        {
            source = Tokenize (dialect, listing);

            Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (source, program, reason)));
            Assert::IsTrue (SUCCEEDED (BasicProgram::Emit (program, bytes, longLine)));
            Assert::IsTrue (source == bytes);
        }
    }


    TEST_METHOD (Parse_AnApplesoftProgramLinkedForAnotherAddress_HasThatBase)
    {
        std::vector<Byte>      source = Tokenize (Dialect::Applesoft, "10 GOTO 20\n20 END\n");
        BasicProgram::Program  program;
        std::string            reason;
        std::vector<Byte>      bytes;
        uint32_t               longLine = 0;



        Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (source, program, reason)));

        program.base = 0x4001;
        Assert::IsTrue (SUCCEEDED (BasicProgram::Emit (program, bytes, longLine)));
        Assert::IsTrue (source != bytes);

        Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (bytes, program, reason)));
        Assert::AreEqual (0x4001, (int) program.base);
        Assert::AreEqual ((size_t) 2, program.lines.size());

        //  Links that match no address are ignored.
        bytes[0] = 0x34;
        bytes[1] = 0x12;
        Assert::IsTrue (SUCCEEDED (BasicProgram::Parse (bytes, program, reason)));
        Assert::IsTrue (program.dialect == Dialect::Applesoft);
        Assert::AreEqual ((size_t) 2, program.lines.size());
    }


    TEST_METHOD (Renumber_Applesoft_RewritesEveryLineReference)
    {
        AssertRenumbered (Dialect::Applesoft,
                          "5 GOTO 100\n7 GOSUB 200:GOTO 5\n100 IF A=1 THEN 200\n150 IF A THEN GOTO 7\n200 ON A GOTO 5,7,100\n210 ON A GOSUB 200, 100\n",
                          10, 10,
                          "10 GOTO 30\n20 GOSUB 50:GOTO 10\n30 IF A=1 THEN 50\n40 IF A THEN GOTO 20\n50 ON A GOTO 10,20,30\n60 ON A GOSUB 50, 30\n");

        AssertRenumbered (Dialect::Applesoft,
                          "1 LIST 2,3\n2 LIST 1-3\n3 RUN 1\n4 DEL 1,2\n5 ONERR GOTO 1\n",
                          1000, 5,
                          "1000 LIST 1005,1010\n1005 LIST 1000-1010\n1010 RUN 1000\n1015 DEL 1000,1005\n1020 ONERR GOTO 1000\n");
    }


    TEST_METHOD (Renumber_Applesoft_LeavesTextAndOtherNumbersAlone)
    {
        AssertRenumbered (Dialect::Applesoft,
                          "5 PRINT \"GOTO 5\":REM GOTO 5\n7 DATA 5,GOTO 5\n9 A=5:POKE 5,7:GOTO 5\n",
                          10, 10,
                          "10 PRINT \"GOTO 5\":REM GOTO 5\n20 DATA 5,GOTO 5\n30 A=5:POKE 5,7:GOTO 10\n");

        //  A line the program does not have, and a GOTO with no number.
        AssertRenumbered (Dialect::Applesoft,
                          "5 GOTO 999\n7 GOTO\n9 GOTO 5\n",
                          10, 10,
                          "10 GOTO 999\n20 GOTO\n30 GOTO 10\n");
    }


    TEST_METHOD (Renumber_Integer_RewritesEveryLineReference)
    {
        AssertRenumbered (Dialect::Integer,
                          "5 GOTO 100\n7 GOSUB 200:GOTO 5\n100 IF A=1 THEN 200\n150 IF A THEN GOTO 7\n200 LIST 5,7\n210 LIST 100\n",
                          10, 10,
                          "10 GOTO 30\n20 GOSUB 50:GOTO 10\n30 IF A=1 THEN 50\n40 IF A THEN GOTO 20\n50 LIST 10,20\n60 LIST 30\n");
    }


    TEST_METHOD (Renumber_Integer_LeavesComputedTargetsAndOtherNumbersAlone)
    {
        AssertRenumbered (Dialect::Integer,
                          "5 GOTO 5+A\n7 GOTO A*5\n9 PRINT \"GOTO 5\":REM GOTO 5\n11 A=5:POKE 5,7:GOTO 5\n13 GOTO 999\n",
                          100, 100,
                          "100 GOTO 5+A\n200 GOTO A*5\n300 PRINT \"GOTO 5\":REM GOTO 5\n400 A=5:POKE 5,7:GOTO 100\n500 GOTO 999\n");
    }


    TEST_METHOD (Renumber_FailsWithAReason)
    {
        std::vector<Byte>  bytes;
        std::vector<Byte>  junk = { 1, 2, 3 };
        std::string        reason;



        Assert::IsTrue (FAILED (BasicRenumber::Renumber (Tokenize (Dialect::Applesoft, "10 END\n"), 10, 0, bytes, reason)));
        Assert::IsTrue (reason.find ("step") != std::string::npos);

        Assert::IsTrue (FAILED (BasicRenumber::Renumber (Tokenize (Dialect::Applesoft, "10 END\n20 END\n"), 63999, 1, bytes, reason)));
        Assert::IsTrue (reason.find ("20") != std::string::npos);
        Assert::IsTrue (bytes.empty());

        Assert::IsTrue (FAILED (BasicRenumber::Renumber (Tokenize (Dialect::Integer, "10 END\n20 END\n"), 32767, 1, bytes, reason)));
        Assert::IsTrue (reason.find ("32767") != std::string::npos);

        Assert::IsTrue (SUCCEEDED (BasicRenumber::Renumber (Tokenize (Dialect::Integer, "10 END\n20 END\n"), 32766, 1, bytes, reason)));

        Assert::IsTrue (FAILED (BasicRenumber::Renumber (junk, 10, 10, bytes, reason)));
        Assert::IsFalse (reason.empty());
    }


    TEST_METHOD (CrossReference_Applesoft_ListsVariablesLinesAndAddresses)
    {
        const std::string            listing = "10 A=1:B$=\"A\":C%=2:DIM D(5):TALLY=A\n"
                                               "20 DEF FN Q(X)=X*2:PRINT FN Q(A);D(1);1E5\n"
                                               "30 CALL -936:POKE 49168,0:A=PEEK(49152):POKE A,1\n"
                                               "40 GOTO 10:GOSUB 99:REM ZZ\n"
                                               "50 CALL 768:DATA YY\n"
                                               "60 CALL 768+1\n";
        BasicCrossReference::Report  report;
        std::string                  reason;
        std::string                  text;



        Assert::IsTrue (SUCCEEDED (BasicCrossReference::Build (Tokenize (Dialect::Applesoft, listing), report, reason)));
        Assert::IsTrue (report.dialect == Dialect::Applesoft);

        Assert::AreEqual (std::string ("10 20 30 "), FindEntry (report.variables, "A"));
        Assert::AreEqual (std::string ("10 "),       FindEntry (report.variables, "B$"));
        Assert::AreEqual (std::string ("10 "),       FindEntry (report.variables, "C%"));
        Assert::AreEqual (std::string ("10 20 "),    FindEntry (report.variables, "D("));
        Assert::AreEqual (std::string ("10 "),       FindEntry (report.variables, "TALLY"));
        Assert::AreEqual (std::string ("20 "),       FindEntry (report.variables, "FN Q"));
        Assert::AreEqual (std::string ("20 "),       FindEntry (report.variables, "X"));
        Assert::AreEqual (std::string ("absent"),    FindEntry (report.variables, "E"));
        Assert::AreEqual (std::string ("absent"),    FindEntry (report.variables, "ZZ"));
        Assert::AreEqual (std::string ("absent"),    FindEntry (report.variables, "YY"));

        Assert::AreEqual (std::string ("40 "),        FindEntry (report.lineReferences, "10"));
        Assert::AreEqual (std::string ("40 missing"), FindEntry (report.lineReferences, "99"));

        Assert::AreEqual (std::string ("30 "),     FindEntry (report.addresses, "CALL -936"));
        Assert::AreEqual (std::string ("50 "),     FindEntry (report.addresses, "CALL 768"));
        Assert::AreEqual (std::string ("30 "),     FindEntry (report.addresses, "POKE 49168"));
        Assert::AreEqual (std::string ("30 "),     FindEntry (report.addresses, "PEEK 49152"));
        Assert::AreEqual ((size_t) 4, report.addresses.size());

        text = BasicCrossReference::Format (report);

        Assert::IsTrue (text.find ("Applesoft BASIC")          != std::string::npos);
        Assert::IsTrue (text.find ("  99  40  (no such line)") != std::string::npos);
        Assert::IsTrue (text.find ("  A      10, 20, 30")      != std::string::npos);
    }


    TEST_METHOD (CrossReference_Integer_ListsVariablesLinesAndAddresses)
    {
        const std::string            listing = "10 A=1:DIM B$(10):B$=\"A\":C1=A\n"
                                               "20 CALL -936:POKE 50,63:A=PEEK(37):POKE A,1\n"
                                               "30 GOTO 10:GOSUB 99:REM ZZ\n"
                                               "40 IF A THEN 20:GOTO A+10\n";
        BasicCrossReference::Report  report;
        std::string                  reason;



        Assert::IsTrue (SUCCEEDED (BasicCrossReference::Build (Tokenize (Dialect::Integer, listing), report, reason)));
        Assert::IsTrue (report.dialect == Dialect::Integer);

        Assert::AreEqual (std::string ("10 20 40 "), FindEntry (report.variables, "A"));
        Assert::AreEqual (std::string ("10 "),       FindEntry (report.variables, "B$"));
        Assert::AreEqual (std::string ("10 "),       FindEntry (report.variables, "C1"));
        Assert::AreEqual (std::string ("absent"),    FindEntry (report.variables, "ZZ"));

        Assert::AreEqual (std::string ("30 "),        FindEntry (report.lineReferences, "10"));
        Assert::AreEqual (std::string ("40 "),        FindEntry (report.lineReferences, "20"));
        Assert::AreEqual (std::string ("30 missing"), FindEntry (report.lineReferences, "99"));
        Assert::AreEqual ((size_t) 3, report.lineReferences.size());

        Assert::AreEqual (std::string ("20 "), FindEntry (report.addresses, "CALL -936"));
        Assert::AreEqual (std::string ("20 "), FindEntry (report.addresses, "POKE 50"));
        Assert::AreEqual (std::string ("20 "), FindEntry (report.addresses, "PEEK 37"));
        Assert::AreEqual ((size_t) 3, report.addresses.size());

        Assert::IsTrue (BasicCrossReference::Format (report).find ("Integer BASIC") != std::string::npos);
    }


    TEST_METHOD (CrossReference_AnEmptySection_IsNone)
    {
        BasicCrossReference::Report  report;
        std::string                  reason;



        Assert::IsTrue (SUCCEEDED (BasicCrossReference::Build (Tokenize (Dialect::Applesoft, "10 END\n"), report, reason)));
        Assert::AreEqual (std::string ("Applesoft BASIC\n\nVariables\n  none\n\nLine references\n  none\n\nCALL, PEEK and POKE\n  none\n"),
                          BasicCrossReference::Format (report));
    }
};
