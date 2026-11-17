#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "ApplesoftTokenizer.h"
#include "BasicCommandRunner.h"
#include "CommandLineHelp.h"
#include "CommandLineParser.h"
#include "IntegerBasicTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandTests
//
//  The `basic` subcommand: the parser on argument lists, and the runner on the
//  bytes of an input file.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BasicCommandTests)
{
public:
    using Command = CommandLineOptions::BasicOptions::Command;
    using Dialect = CommandLineOptions::BasicOptions::Dialect;

    static CommandLineOptions Parse (std::vector<std::string> words)
    {
        std::vector<char *>  argv;



        words.insert (words.begin(), { "CassoCli", "basic" });

        for (std::string & word : words)
        {
            argv.push_back (word.data());
        }

        return CommandLineParser::Parse ((int) argv.size(), argv.data(), [] (const std::string &) { return false; });
    }


    static void AssertNotParsed (std::vector<std::string> words, const std::string & fragment)
    {
        CommandLineOptions  options = Parse (words);



        Assert::IsTrue (options.parseVerdict == CommandLineOptions::ParseVerdict::Refused);
        Assert::IsTrue (options.refusalMessage.find (fragment) != std::string::npos, TextEncoding::NarrowToWide (options.refusalMessage).c_str());
    }


    static std::vector<Byte> ToBytes (const std::string & text)
    {
        return std::vector<Byte> (text.begin(), text.end());
    }


    static BasicCommandResult RunCommand (std::vector<std::string> words, const std::vector<Byte> & input)
    {
        CommandLineOptions  options = Parse (words);



        Assert::IsTrue (options.parseVerdict != CommandLineOptions::ParseVerdict::Refused, TextEncoding::NarrowToWide (options.refusalMessage).c_str());

        return BasicCommandRunner::Run (options.basic, input);
    }


    TEST_METHOD (Parse_Tokenize_ReadsOperandsAndDialect)
    {
        CommandLineOptions  options = Parse ({ "tokenize", "in.txt", "out.bas", "--integer" });



        Assert::IsTrue (options.subcommand == CommandLineOptions::Subcommand::Basic);
        Assert::IsTrue (options.basic.command == Command::Tokenize);
        Assert::IsTrue (options.basic.dialect == Dialect::Integer);
        Assert::AreEqual (std::string ("in.txt"),  options.basic.inputFile);
        Assert::AreEqual (std::string ("out.bas"), options.basic.outputFile);
    }


    TEST_METHOD (Parse_Renumber_ReadsStartAndStepWithEitherPrefix)
    {
        CommandLineOptions  dashes  = Parse ({ "renumber", "--start", "100", "--step", "5", "a", "b" });
        CommandLineOptions  slashes = Parse ({ "renumber", "a", "b", "/start", "200", "/step", "20" });



        Assert::IsTrue (dashes.basic.command == Command::Renumber);
        Assert::AreEqual (100u, dashes.basic.start);
        Assert::AreEqual (5u,   dashes.basic.step);
        Assert::AreEqual (200u, slashes.basic.start);
        Assert::AreEqual (20u,  slashes.basic.step);
        Assert::AreEqual ('/',  slashes.flagPrefix);
    }


    TEST_METHOD (Parse_Renumber_DefaultsToTenAndTen)
    {
        CommandLineOptions  options = Parse ({ "renumber", "a", "b" });



        Assert::AreEqual (10u, options.basic.start);
        Assert::AreEqual (10u, options.basic.step);
    }


    TEST_METHOD (Parse_Help_IsACommandOfItsOwn)
    {
        Assert::IsTrue (Parse ({ "--help" }).basic.command == Command::Help);
        Assert::IsTrue (Parse ({ "list", "/?" }).basic.command == Command::Help);
        Assert::AreEqual ('/', Parse ({ "/help" }).flagPrefix);
    }


    TEST_METHOD (Parse_BadCommandLines_AreErrors)
    {
        AssertNotParsed ({ },                                          "<command>");
        AssertNotParsed ({ "compile", "a", "b" },                      "unknown basic command 'compile'");
        AssertNotParsed ({ "tokenize", "a", "b" },                     "--applesoft or --integer");
        AssertNotParsed ({ "tokenize", "a", "b", "--applesoft", "--integer" }, "cannot be combined");
        AssertNotParsed ({ "tokenize", "a", "--applesoft" },           "<program>");
        AssertNotParsed ({ "list", "a", "b", "--integer" },            "tokenize command");
        AssertNotParsed ({ "list", "a", "b", "--start", "5" },         "renumber command");
        AssertNotParsed ({ "list", "a", "b", "c" },                    "'c'");
        AssertNotParsed ({ "xref" },                                   "<program>");
        AssertNotParsed ({ "xref", "a", "b" },                         "'b'");
        AssertNotParsed ({ "renumber", "a", "b", "--start", "ten" },   "illegal --start value");
        AssertNotParsed ({ "renumber", "a", "b", "--bogus" },          "unknown option '--bogus'");
    }


    TEST_METHOD (Tokenize_Applesoft_MatchesTheTokenizer)
    {
        std::string            listing = "10 PRINT \"HI\"\n20 GOTO 10\n";
        std::vector<Byte>      correct;
        ApplesoftListingError  error;
        BasicCommandResult     result = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes (listing));



        AssertSucceeded (ApplesoftTokenizer::Tokenize (listing, correct, error));
        Assert::AreEqual (0, result.exitStatus);
        Assert::IsTrue (result.hasFile);
        Assert::IsTrue (correct == result.fileBytes);
    }


    TEST_METHOD (Tokenize_Integer_AcceptsAppleTextAndAByteOrderMark)
    {
        std::string            listing = "10 PRINT \"HI\"\n20 GOTO 10\n";
        std::vector<Byte>      correct;
        std::vector<Byte>      apple;
        std::vector<Byte>      marked  = { 0xEF, 0xBB, 0xBF };
        ApplesoftListingError  error;



        for (char ch : listing)
        {
            apple.push_back ((Byte) ((ch == '\n' ? '\r' : ch) | 0x80));
            marked.push_back ((Byte) ch);
        }

        AssertSucceeded (IntegerBasicTokenizer::Tokenize (listing, correct, error));
        Assert::IsTrue (correct == RunCommand ({ "tokenize", "a", "b", "--integer" }, apple).fileBytes);
        Assert::IsTrue (correct == RunCommand ({ "tokenize", "a", "b", "--integer" }, marked).fileBytes);
    }


    TEST_METHOD (Tokenize_BadListing_FailsWithTheLine)
    {
        BasicCommandResult  result = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes ("10 PRINT\nPRINT\n"));



        Assert::AreEqual (2, result.exitStatus);
        Assert::IsFalse (result.hasFile);
        Assert::IsTrue (result.diagnostics.find ("not an Applesoft BASIC program") != std::string::npos, TextEncoding::NarrowToWide (result.diagnostics).c_str());
    }


    TEST_METHOD (List_DetectsTheDialectAndRoundTrips)
    {
        std::string         listing   = "10 PRINT \"HI\"\n20 GOTO 10\n";
        BasicCommandResult  applesoft = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes (listing));
        BasicCommandResult  integer   = RunCommand ({ "tokenize", "a", "b", "--integer" },   ToBytes (listing));
        BasicCommandResult  listedA   = RunCommand ({ "list", "a", "b" }, applesoft.fileBytes);
        BasicCommandResult  listedI   = RunCommand ({ "list", "a", "b" }, integer.fileBytes);



        Assert::IsTrue (listedA.hasFile && listedI.hasFile);
        Assert::IsTrue (applesoft.fileBytes == RunCommand ({ "tokenize", "a", "b", "--applesoft" }, listedA.fileBytes).fileBytes);
        Assert::IsTrue (integer.fileBytes   == RunCommand ({ "tokenize", "a", "b", "--integer" },   listedI.fileBytes).fileBytes);
    }


    TEST_METHOD (List_NotAProgram_Fails)
    {
        BasicCommandResult  result = RunCommand ({ "list", "a", "b" }, ToBytes ("this is not a tokenized program at all"));



        Assert::AreEqual (2, result.exitStatus);
        Assert::IsFalse (result.hasFile);
        Assert::IsTrue (result.diagnostics.find ("not a tokenized BASIC program") != std::string::npos);
    }


    TEST_METHOD (Renumber_RewritesLinesAndReferences)
    {
        BasicCommandResult  program    = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes ("5 PRINT 1\n7 GOTO 5\n"));
        BasicCommandResult  correct    = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes ("100 PRINT 1\n150 GOTO 100\n"));
        BasicCommandResult  renumbered = RunCommand ({ "renumber", "a", "b", "--start", "100", "--step", "50" }, program.fileBytes);



        Assert::AreEqual (0, renumbered.exitStatus);
        Assert::IsTrue (correct.fileBytes == renumbered.fileBytes);
    }


    TEST_METHOD (Renumber_PastTheHighestLine_FailsWithTheLine)
    {
        BasicCommandResult  program    = RunCommand ({ "tokenize", "a", "b", "--integer" }, ToBytes ("5 PRINT 1\n7 PRINT 2\n"));
        BasicCommandResult  renumbered = RunCommand ({ "renumber", "a", "b", "--start", "32767" }, program.fileBytes);



        Assert::AreEqual (2, renumbered.exitStatus);
        Assert::IsFalse (renumbered.hasFile);
        Assert::IsTrue (renumbered.diagnostics.find ("line 7") != std::string::npos, TextEncoding::NarrowToWide (renumbered.diagnostics).c_str());
        Assert::IsTrue (renumbered.diagnostics.find ("32767") != std::string::npos);
    }


    TEST_METHOD (Xref_PrintsTheReportAndWritesNoFile)
    {
        BasicCommandResult  program = RunCommand ({ "tokenize", "a", "b", "--applesoft" }, ToBytes ("10 A = 1\n20 GOTO 10\n30 CALL 768\n"));
        BasicCommandResult  result  = RunCommand ({ "xref", "a" }, program.fileBytes);



        Assert::AreEqual (0, result.exitStatus);
        Assert::IsFalse (result.hasFile);
        Assert::IsTrue (result.output.find ("Applesoft BASIC") != std::string::npos);
        Assert::IsTrue (result.output.find ("CALL 768") != std::string::npos, TextEncoding::NarrowToWide (result.output).c_str());
    }


    TEST_METHOD (Help_ListsEveryCommandWithTheTypedPrefix)
    {
        std::string  dashes  = CommandLineHelp::BuildBasicHelp ("banner\n", '-');
        std::string  slashes = CommandLineHelp::BuildBasicHelp ("banner\n", '/');
        std::string  general = CommandLineHelp::BuildGeneralHelp ("banner\n", '-');



        for (const char * word : { "tokenize", "list", "renumber", "xref", "--applesoft", "--integer", "--start", "--step" })
        {
            Assert::IsTrue (dashes.find (word) != std::string::npos, TextEncoding::NarrowToWide (word).c_str());
        }

        Assert::IsTrue (slashes.find ("/start") != std::string::npos);
        Assert::IsTrue (slashes.find ("--") == std::string::npos);
        Assert::IsTrue (general.find ("CassoCli basic --help") != std::string::npos);
    }
};
