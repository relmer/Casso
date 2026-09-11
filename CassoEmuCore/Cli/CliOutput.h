#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  CliOutput
//
//  Formatted writes to a C stream that cannot throw. Every std::print and
//  std::println the console tool used goes through here instead.
//
//  std::print THROWS std::system_error WHEN ITS WRITE FAILS, and nothing in
//  the tool catches it, so the exception reached the runtime and aborted the
//  process. The common way to fail that write is a reader that stops reading:
//  `CassoCli disk --help | Select-Object -First 40` closes the pipe after 40
//  of 229 lines, and a Debug build then raised a modal abort dialog that hangs
//  an unattended script.
//
//  A reader that has gone away is not an error the tool can report, since the
//  report would go down the same closed pipe. The writes are therefore dropped
//  and the command runs to completion, which is what std::cout already does
//  with the same failure: an assembly whose log reader quit still writes its
//  binary.
//
////////////////////////////////////////////////////////////////////////////////

class CliOutput
{
public:
    template <class... Args>
    static void Print     (std::FILE * stream, std::format_string<Args...> format, Args &&... args);

    template <class... Args>
    static void PrintLine (std::FILE * stream, std::format_string<Args...> format, Args &&... args);

    static void PrintLine (std::FILE * stream);

    //  Whether every byte reached the stream. False on a closed pipe rather
    //  than a throw.
    static bool TryWrite  (std::FILE * stream, std::string_view text);
};





////////////////////////////////////////////////////////////////////////////////
//
//  CliOutput::Print
//
////////////////////////////////////////////////////////////////////////////////

template <class... Args>
void CliOutput::Print (std::FILE * stream, std::format_string<Args...> format, Args &&... args)
{
    std::string  text      = std::format (format, std::forward<Args> (args)...);
    bool         isWritten = false;



    isWritten = TryWrite (stream, text);
    IGNORE_RETURN_VALUE (isWritten, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CliOutput::PrintLine
//
////////////////////////////////////////////////////////////////////////////////

template <class... Args>
void CliOutput::PrintLine (std::FILE * stream, std::format_string<Args...> format, Args &&... args)
{
    std::string  text      = std::format (format, std::forward<Args> (args)...);
    bool         isWritten = false;



    text += '\n';

    isWritten = TryWrite (stream, text);
    IGNORE_RETURN_VALUE (isWritten, false);
}
