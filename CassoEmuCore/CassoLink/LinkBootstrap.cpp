#include "Pch.h"

#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::BuildLines
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::BuildLines (DWORD baud, std::vector<std::string> & outLines, int slot)
{
    HRESULT            hr = S_OK;
    std::vector<Byte>  shim;
    LinkShimOptions    options;



    outLines.clear();

    options.baud = baud;
    options.slot = slot;

    hr = LinkProgram::BuildShim (options, shim);
    CHR (hr);

    outLines.push_back (kCancelLine);
    outLines.push_back ("CALL -151\r");

    for (size_t at = 0; at < shim.size(); at += kBytesPerLine)
    {
        std::string  line = std::format ("{:04X}:", LinkProgram::kShimAddress + at);

        for (size_t i = at; i < at + kBytesPerLine && i < shim.size(); i++)
        {
            line += std::format (" {:02X}", shim[i]);
        }

        outLines.push_back (line + "\r");
    }

    outLines.push_back (std::format ("{:04X}G\r", LinkProgram::kShimAddress));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::Send
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::Send (IByteStream & stream, const std::vector<std::string> & lines, DWORD pauseMs, const std::function<bool()> & canceled, DWORD charPauseUs)
{
    HRESULT  hr    = S_OK;
    bool     going = true;



    for (const std::string & line : lines)
    {
        going = !canceled || !canceled();
        CBREx (going, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

        //  A character at a time, with a wait after each, as nothing on the
        //  line paces it: the Apple takes each one from a 6551 that holds one
        //  byte, and with DOS's input hook in the way it falls behind
        //  characters sent back to back.
        for (size_t at = 0; at < line.size(); at++)
        {
            hr = stream.Write (reinterpret_cast<const Byte *> (line.data() + at), 1);
            CHR (hr);

            WaitMicroseconds (charPauseUs);
        }

        std::this_thread::sleep_for (std::chrono::milliseconds (pauseMs));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::WaitMicroseconds
//
//  A sleep is as coarse as the system timer, about 15 ms, so a wait this
//  short spins.
//
////////////////////////////////////////////////////////////////////////////////

void LinkBootstrap::WaitMicroseconds (DWORD microseconds)
{
    auto  until = std::chrono::steady_clock::now() + std::chrono::microseconds (microseconds);



    while (std::chrono::steady_clock::now() < until)
    {
        YieldProcessor();
    }
}