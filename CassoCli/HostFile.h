#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  HostFile
//
//  Whole-file reads against the host filesystem, for the two places that want
//  a file's bytes rather than a stream: the source the assembler is about to
//  read, and the binary `run` is about to load.
//
//  Separate from the assembler's own FileReader, which resolves an include
//  against a base directory and is the ASSEMBLER's notion of a file. These two
//  callers have a path already and want what is at it.
//
////////////////////////////////////////////////////////////////////////////////

class HostFile
{
public:
    //  The whole file, in binary. Fails rather than returning an empty string
    //  when the file will not open, so "absent" and "empty" stay distinct.
    static HRESULT  ReadAll (const std::string & path, std::string & contents);

    //  Whether a path names something openable. The parser uses it to tell a
    //  source file from a mistyped subcommand.
    static bool     Exists  (const std::string & path);
};
