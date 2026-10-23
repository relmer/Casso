#include "Pch.h"

#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::BuildLines
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::BuildLines (DWORD baud, std::vector<std::string> & outLines)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  image;
    LinkShimOptions    options;
    Word               start = 0;



    outLines.clear();

    options.baud = baud;

    hr = LinkProgram::BuildBootstrapImage (options, image, start);
    CHR (hr);

    outLines.push_back (kCancelLine);
    outLines.push_back ("CALL -151\r");

    for (size_t at = 0; at < image.size(); at += kBytesPerLine)
    {
        size_t       end  = std::min (at + kBytesPerLine, image.size());
        bool         gap  = start + at < LinkProgram::kShimAddress && std::all_of (image.begin() + (ptrdiff_t) at, image.begin() + (ptrdiff_t) end, [] (Byte b) { return b == 0; });
        std::string  line = std::format ("{:04X}:", start + at);

        for (size_t i = at; i < end; i++)
        {
            line += std::format (" {:02X}", image[i]);
        }

        if (!gap)
        {
            outLines.push_back (line + "\r");
        }
    }

    outLines.push_back (std::format ("{:04X}G\r", start));

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