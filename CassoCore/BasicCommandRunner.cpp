#include "Pch.h"

#include "BasicCommandRunner.h"
#include "BasicCrossReference.h"
#include "BasicProgram.h"
#include "BasicRenumber.h"
#include "IntegerBasicDetokenizer.h"
#include "IntegerBasicTokenizer.h"



static constexpr Byte  s_kHighBit         = 0x80;
static constexpr Byte  s_kUtf8Mark[]      = { 0xEF, 0xBB, 0xBF };
static constexpr char  s_kNotAProgram[]   = "Error: the file is not a tokenized BASIC program: ";





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::Fail
//
////////////////////////////////////////////////////////////////////////////////

void BasicCommandRunner::Fail (const std::string & message, BasicCommandResult & ioResult)
{
    ioResult.diagnostics += message;
    ioResult.exitStatus   = BasicCommandResult::kNoOutput;
    ioResult.hasFile      = false;

    ioResult.fileBytes.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::ReadListingText
//
//  The high bit is cleared when most of the bytes have it set, which is a
//  text file read from an Apple disk as it is stored.
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicCommandRunner::ReadListingText (const std::vector<Byte> & input)
{
    std::string  text;
    size_t       start    = 0;
    size_t       highBits = 0;
    size_t       index    = 0;
    bool         isApple  = false;



    if (input.size() >= sizeof (s_kUtf8Mark) && memcmp (input.data(), s_kUtf8Mark, sizeof (s_kUtf8Mark)) == 0)
    {
        start = sizeof (s_kUtf8Mark);
    }

    for (index = start; index < input.size(); index++)
    {
        highBits += (input[index] & s_kHighBit) ? 1 : 0;
    }

    isApple = highBits * 2 > (input.size() - start);

    for (index = start; index < input.size(); index++)
    {
        text += (char) (isApple ? (input[index] & ~s_kHighBit) : input[index]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::DescribeLine
//
//  The line number when the line has one, its position in the file when it
//  does not, the reason, and the line as written.
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicCommandRunner::DescribeLine (const char * leadIn, const ApplesoftListingError & error)
{
    std::string  message = leadIn;



    message += ": ";

    if (error.hasLineNumber)
    {
        message += "line " + std::to_string (error.lineNumber) + " ";
    }
    else if (error.sourceLineIndex > 0)
    {
        message += "the line at file position " + std::to_string (error.sourceLineIndex) + " ";
    }

    message += error.reason;
    message += "\n";

    if (!error.sourceLine.empty())
    {
        message += "    " + error.sourceLine + "\n";
    }

    return message;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::Tokenize
//
////////////////////////////////////////////////////////////////////////////////

void BasicCommandRunner::Tokenize (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input, BasicCommandResult & ioResult)
{
    HRESULT                hr        = S_OK;
    std::string            text      = ReadListingText (input);
    bool                   isInteger = options.dialect == CommandLineOptions::BasicOptions::Dialect::Integer;
    ApplesoftListingError  error;



    if (isInteger)
    {
        hr = IntegerBasicTokenizer::Tokenize (text, ioResult.fileBytes, error);
    }
    else
    {
        hr = ApplesoftTokenizer::Tokenize (text, ioResult.fileBytes, error);
    }

    if (FAILED (hr))
    {
        Fail (DescribeLine (isInteger ? "Error: the listing is not an Integer BASIC program"
                                      : "Error: the listing is not an Applesoft BASIC program", error), ioResult);
        return;
    }

    ioResult.hasFile = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::List
//
////////////////////////////////////////////////////////////////////////////////

void BasicCommandRunner::List (const std::vector<Byte> & input, BasicCommandResult & ioResult)
{
    HRESULT                   hr = S_OK;
    BasicProgram::Program     program;
    std::string               reason;
    std::string               text;
    ApplesoftListingError     applesoftError;
    IntegerBasicListingError  integerError;



    hr = BasicProgram::Parse (input, program, reason);

    if (FAILED (hr))
    {
        Fail (s_kNotAProgram + reason + "\n", ioResult);
        return;
    }

    if (program.dialect == BasicProgram::Dialect::Integer)
    {
        hr     = IntegerBasicDetokenizer::Detokenize (input, text, integerError);
        reason = "byte " + std::to_string (integerError.offset) + ": " + integerError.reason + "\n";
    }
    else
    {
        hr     = ApplesoftTokenizer::Detokenize (input, text, applesoftError);
        reason = DescribeLine ("", applesoftError);
    }

    if (FAILED (hr))
    {
        Fail ("Error: the program cannot be listed" + ((program.dialect == BasicProgram::Dialect::Integer) ? ": " + reason : reason), ioResult);
        return;
    }

    ioResult.fileBytes.assign (text.begin(), text.end());
    ioResult.hasFile = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::Renumber
//
////////////////////////////////////////////////////////////////////////////////

void BasicCommandRunner::Renumber (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input, BasicCommandResult & ioResult)
{
    HRESULT      hr = S_OK;
    std::string  reason;



    hr = BasicRenumber::Renumber (input, options.start, options.step, ioResult.fileBytes, reason);

    if (FAILED (hr))
    {
        Fail ("Error: the program cannot be renumbered: " + reason + "\n", ioResult);
        return;
    }

    ioResult.hasFile = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::CrossReference
//
////////////////////////////////////////////////////////////////////////////////

void BasicCommandRunner::CrossReference (const std::vector<Byte> & input, BasicCommandResult & ioResult)
{
    HRESULT                      hr = S_OK;
    BasicCrossReference::Report  report;
    std::string                  reason;



    hr = BasicCrossReference::Build (input, report, reason);

    if (FAILED (hr))
    {
        Fail (s_kNotAProgram + reason + "\n", ioResult);
        return;
    }

    ioResult.output = BasicCrossReference::Format (report);
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner::Run
//
////////////////////////////////////////////////////////////////////////////////

BasicCommandResult BasicCommandRunner::Run (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input)
{
    BasicCommandResult  result;



    switch (options.command)
    {
    case CommandLineOptions::BasicOptions::Command::Tokenize:
        Tokenize (options, input, result);
        break;

    case CommandLineOptions::BasicOptions::Command::List:
        List (input, result);
        break;

    case CommandLineOptions::BasicOptions::Command::Renumber:
        Renumber (options, input, result);
        break;

    case CommandLineOptions::BasicOptions::Command::Xref:
        CrossReference (input, result);
        break;

    default:
        Fail ("Error: no basic command was given\n", result);
        break;
    }

    return result;
}
