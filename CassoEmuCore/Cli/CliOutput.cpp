#include "Pch.h"

#include "CliOutput.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CliOutput::PrintLine
//
//  An empty line, the one form std::println (stream, "") had that a format
//  string cannot express without an argument list to go with it.
//
////////////////////////////////////////////////////////////////////////////////

void CliOutput::PrintLine (std::FILE * stream)
{
    bool  isWritten = TryWrite (stream, "\n");



    IGNORE_RETURN_VALUE (isWritten, false);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CliOutput::TryWrite
//
//  The same fwrite std::print makes, with the result reported rather than
//  thrown. A text-mode stream still translates the line ending, so the bytes
//  that arrive are the ones std::println produced.
//
////////////////////////////////////////////////////////////////////////////////

bool CliOutput::TryWrite (std::FILE * stream, std::string_view text)
{
    HRESULT  hr        = S_OK;
    size_t   wanted    = text.size();
    size_t   written   = 0;
    bool     isWritten = false;



    CBRAEx (stream, E_INVALIDARG);

    written = std::fwrite (text.data(), 1, wanted, stream);
    CBR (written == wanted);

    isWritten = true;

Error:
    return isWritten;
}
