#include "Pch.h"
#include "EhmTestHelper.h"
#include "GuestSession.h"
#include "KeystrokeInjector.h"
#include "MachineIdle.h"
#include "TestMachine.h"
#include "ApplesoftTokenizer.h"
#include "BasicRenumber.h"
#include "IntegerBasicDetokenizer.h"
#include "IntegerBasicTokenizer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleBasicToolsTests
//
//  The BASIC tools checked against the BASIC in the machine.
//
//  A listing typed at the prompt is tokenized by the ROM, and the bytes in the
//  guest's memory are compared with the tokenizer's bytes for the same text.
//
//  A renumbered program is listed, typed into a second machine and run. Its
//  output is compared with the output of the program it was made from. The
//  sample programs branch with GOTO, GOSUB and IF ... THEN, so a reference
//  left pointing at an old line number changes the output or stops the run.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleBasicToolsTests)
{
public:

    //  Applesoft's start of program and end of program.
    static constexpr Word  kTxtTab = 0x0067;
    static constexpr Word  kVarTab = 0x0069;

    //  Integer BASIC's start of program and end of memory.
    static constexpr Word  kProgramPointer = 0x00CA;
    static constexpr Word  kHimem          = 0x004C;

    static constexpr uint64_t  kResetCycles = 4'000'000ULL;
    static constexpr uint64_t  kLineCycles  = 20'000'000ULL;

    static constexpr uint32_t  kRenumberStart = 1000;
    static constexpr uint32_t  kRenumberStep  = 5;

    static constexpr const char *  kEnterIntegerBasic = "\x02";


    static std::string ApplesoftListing()
    {
        return "10 T = 0\n"
               "20 FOR I = 1 TO 3\n"
               "30 GOSUB 100\n"
               "40 NEXT I\n"
               "50 IF T > 5 THEN 70\n"
               "60 PRINT \"LOW\"\n"
               "70 PRINT \"SUM \";T\n"
               "80 ON 2 GOTO 60,200\n"
               "100 T = T + I\n"
               "110 RETURN\n"
               "200 PRINT \"DONE\"\n"
               "210 END\n";
    }


    static std::string IntegerListing()
    {
        return "10 A=0\n"
               "20 FOR I=1 TO 3\n"
               "30 GOSUB 100\n"
               "40 NEXT I\n"
               "50 IF A>5 THEN 70\n"
               "60 PRINT \"LOW\"\n"
               "70 PRINT \"SUM \";A\n"
               "80 GOTO 200\n"
               "100 A=A+I\n"
               "110 RETURN\n"
               "200 PRINT \"DONE\"\n"
               "210 END\n";
    }


    static std::vector<std::string> SplitLines (const std::string & listing)
    {
        std::vector<std::string>  lines;
        std::string               line;

        for (char c : listing)
        {
            if (c == '\n')
            {
                if (!line.empty()) { lines.push_back (line); }

                line.clear();
            }
            else if (c != '\r')
            {
                line += c;
            }
        }

        if (!line.empty()) { lines.push_back (line); }

        return lines;
    }


    static Word ReadGuestWord (MachineHost & machine, Word address)
    {
        std::vector<Byte>  bytes = GuestSession::GuestBytesAt (machine, address, 2);

        return (Word) (bytes[0] | (bytes[1] << 8));
    }


    static std::wstring Hex (const std::vector<Byte> & bytes)
    {
        std::wstring  out;
        wchar_t       buf[8] = {};

        for (size_t i = 0; i < bytes.size(); i++)
        {
            swprintf_s (buf, L"%02X ", bytes[i]);
            out += buf;
        }

        return out;
    }


    //  The rows printed after the last RUN, without the prompt rows.
    static std::vector<std::string> OutputOfRun (const std::vector<std::string> & rows)
    {
        std::vector<std::string>  output;
        size_t                    runRow = rows.size();

        for (size_t i = 0; i < rows.size(); i++)
        {
            std::string  row = GuestSession::TrimTrailingBlanks (rows[i]);

            if (row == "]RUN" || row == ">RUN") { runRow = i; }
        }

        for (size_t i = runRow + 1; i < rows.size(); i++)
        {
            std::string  row = GuestSession::TrimTrailingBlanks (rows[i]);

            if (!row.empty() && row[0] != ']' && row[0] != '>') { output.push_back (row); }
        }

        return output;
    }


    static void AssertSameBytes (const std::vector<Byte> & guest, const std::vector<Byte> & casso, const wchar_t * what)
    {
        std::wstring  message = what;

        message += L"\n  guest: ";
        message += Hex (guest);
        message += L"\n  Casso: ";
        message += Hex (casso);

        Assert::IsTrue (guest == casso, message.c_str());
    }


    static void AssertTheSampleRan (const std::vector<std::string> & output)
    {
        Assert::IsTrue (GuestSession::AnyRowIs (output, "SUM 6"), L"the loop and its subroutine ran");
        Assert::IsTrue (GuestSession::AnyRowIs (output, "DONE"),  L"the last branch was taken");
        Assert::IsFalse (GuestSession::AnyRowIs (output, "LOW"),  L"the IF skipped its line");
    }


    //
    //  ------------------------------------------------------------------
    //  Applesoft.
    //  ------------------------------------------------------------------
    //

    //  Types a listing into a booted Apple //e and returns the program bytes
    //  in the guest's memory and the output of RUN. The byte count is the
    //  tokenizer's, because the end of program pointer is past the last byte
    //  of the program.
    static void RunInApplesoft (const std::string         & listing,
                                size_t                      programBytes,
                                std::vector<Byte>         & outProgram,
                                std::vector<std::string>  & outOutput)
    {
        TestMachine        machine ("Apple2e");
        std::vector<Byte>  master = GuestSession::RequireDos33Master();
        Word               txtTab = 0;
        Word               varTab = 0;

        GuestSession::BootToPrompt (machine, master);
        GuestSession::TypeAndCollect (machine, "NEW");

        for (const std::string & line : SplitLines (listing))
        {
            GuestSession::TypeAndCollect (machine, line);
        }

        txtTab = ReadGuestWord (machine, kTxtTab);
        varTab = ReadGuestWord (machine, kVarTab);

        Assert::AreEqual ((int) ApplesoftTokenizer::kProgramBase, (int) txtTab, L"the program is at the tokenizer's base");
        Assert::IsTrue (varTab >= txtTab + programBytes, L"the guest's program is at least as long as the tokenizer's");
        Assert::IsTrue (varTab <= txtTab + programBytes + 1, L"and no longer");

        outProgram = GuestSession::GuestBytesAt (machine, txtTab, programBytes);
        outOutput  = OutputOfRun (GuestSession::TypeAndCollect (machine, "RUN"));
    }


    TEST_METHOD (Applesoft_TokenizedListingMatchesTheGuestAndARenumberedProgramRunsTheSame)
    {
        std::vector<Byte>         tokenized;
        std::vector<Byte>         renumbered;
        std::vector<Byte>         guestProgram;
        std::vector<Byte>         guestRenumbered;
        std::vector<std::string>  output;
        std::vector<std::string>  renumberedOutput;
        std::string               renumberedListing;
        std::string               reason;
        ApplesoftListingError     error;



        AssertSucceeded (ApplesoftTokenizer::Tokenize (ApplesoftListing(), tokenized, error), L"the sample tokenizes");

        RunInApplesoft (ApplesoftListing(), tokenized.size(), guestProgram, output);

        AssertSameBytes (guestProgram, tokenized, L"Applesoft's bytes for the sample are the tokenizer's bytes");
        AssertTheSampleRan (output);

        AssertSucceeded (BasicRenumber::Renumber (tokenized, kRenumberStart, kRenumberStep, renumbered, reason), L"the sample renumbers");
        AssertSucceeded (ApplesoftTokenizer::Detokenize (renumbered, renumberedListing, error), L"the renumbered program lists");

        Assert::IsTrue (renumbered != tokenized, L"the renumbered program differs from the sample");

        RunInApplesoft (renumberedListing, renumbered.size(), guestRenumbered, renumberedOutput);

        AssertSameBytes (guestRenumbered, renumbered, L"Applesoft's bytes for the renumbered listing are the renumbered bytes");
        Assert::IsTrue (output == renumberedOutput, L"the renumbered program prints what the sample prints");
    }


    //
    //  ------------------------------------------------------------------
    //  Integer BASIC.
    //  ------------------------------------------------------------------
    //

    //  Types a listing into an Apple ][ at the Integer BASIC prompt and
    //  returns the program bytes in the guest's memory and the output of RUN.
    static void RunInIntegerBasic (const std::string         & listing,
                                   std::vector<Byte>         & outProgram,
                                   std::vector<std::string>  & outOutput)
    {
        TestMachine               machine ("Apple2", TestMachine::Slots::Empty);
        std::vector<std::string>  rows;
        Word                      programPointer = 0;
        Word                      himem          = 0;

        machine.PowerCycle();
        MachineIdle::RunUntilIdle (machine, kResetCycles);
        KeystrokeInjector::InjectLine (machine, kEnterIntegerBasic, kLineCycles);

        for (const std::string & line : SplitLines (listing))
        {
            KeystrokeInjector::InjectLine (machine, line, kLineCycles);
        }

        programPointer = ReadGuestWord (machine, kProgramPointer);
        himem          = ReadGuestWord (machine, kHimem);

        Assert::IsTrue (himem > programPointer, L"the guest has a program");

        outProgram = GuestSession::GuestBytesAt (machine, programPointer, (size_t) (himem - programPointer));

        KeystrokeInjector::InjectLine (machine, "RUN", kLineCycles);
        GuestSession::CollectRows (machine, rows);

        outOutput = OutputOfRun (rows);
    }


    TEST_METHOD (IntegerBasic_TokenizedListingMatchesTheGuestAndARenumberedProgramRunsTheSame)
    {
        std::vector<Byte>         tokenized;
        std::vector<Byte>         renumbered;
        std::vector<Byte>         guestProgram;
        std::vector<Byte>         guestRenumbered;
        std::vector<std::string>  output;
        std::vector<std::string>  renumberedOutput;
        std::string               renumberedListing;
        std::string               reason;
        ApplesoftListingError     error;
        IntegerBasicListingError  listingError;



        AssertSucceeded (IntegerBasicTokenizer::Tokenize (IntegerListing(), tokenized, error), L"the sample tokenizes");

        RunInIntegerBasic (IntegerListing(), guestProgram, output);

        AssertSameBytes (guestProgram, tokenized, L"Integer BASIC's bytes for the sample are the tokenizer's bytes");
        AssertTheSampleRan (output);

        AssertSucceeded (BasicRenumber::Renumber (tokenized, kRenumberStart, kRenumberStep, renumbered, reason), L"the sample renumbers");
        AssertSucceeded (IntegerBasicDetokenizer::Detokenize (renumbered, renumberedListing, listingError), L"the renumbered program lists");

        Assert::IsTrue (renumbered != tokenized, L"the renumbered program differs from the sample");

        RunInIntegerBasic (renumberedListing, guestRenumbered, renumberedOutput);

        AssertSameBytes (guestRenumbered, renumbered, L"Integer BASIC's bytes for the renumbered listing are the renumbered bytes");
        Assert::IsTrue (output == renumberedOutput, L"the renumbered program prints what the sample prints");
    }
};
