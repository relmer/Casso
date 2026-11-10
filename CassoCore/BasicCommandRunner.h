#pragma once

#include "Pch.h"

#include "ApplesoftTokenizer.h"
#include "CommandLineOptions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandResult
//
////////////////////////////////////////////////////////////////////////////////

struct BasicCommandResult
{
    static constexpr int  kDone     = 0;
    static constexpr int  kNoOutput = 2;

    int                exitStatus = kDone;
    bool               hasFile    = false;      // fileBytes is the output file
    std::vector<Byte>  fileBytes;
    std::string        output;                  // text for the output stream
    std::string        diagnostics;             // text for the error stream
};





////////////////////////////////////////////////////////////////////////////////
//
//  BasicCommandRunner
//
//  The `basic` subcommand on the bytes of its input file: tokenize a listing,
//  list a tokenized program, renumber one, or cross-reference one. No file is
//  opened here, so a test can call every command.
//
//  A tokenized program is a file as a raw Get returns it. The dialect of a
//  tokenized program is detected from its bytes. The dialect of a listing is
//  an option, because one listing can be valid in both.
//
//  A listing saved from an Apple has the high bit set on every character. It
//  is accepted as it is, and so is a UTF-8 byte order mark.
//
////////////////////////////////////////////////////////////////////////////////

class BasicCommandRunner
{
public:
    static BasicCommandResult  Run (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input);

private:
    static void         Tokenize        (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input, BasicCommandResult & ioResult);
    static void         List            (const std::vector<Byte> & input, BasicCommandResult & ioResult);
    static void         Renumber        (const CommandLineOptions::BasicOptions & options, const std::vector<Byte> & input, BasicCommandResult & ioResult);
    static void         CrossReference  (const std::vector<Byte> & input, BasicCommandResult & ioResult);

    static std::string  ReadListingText (const std::vector<Byte> & input);
    static std::string  DescribeLine    (const char * leadIn, const ApplesoftListingError & error);
    static void         Fail            (const std::string & message, BasicCommandResult & ioResult);
};
