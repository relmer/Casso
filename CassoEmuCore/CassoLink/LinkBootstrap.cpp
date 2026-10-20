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
    HRESULT            hr = S_OK;
    std::vector<Byte>  shim;



    outLines.clear();

    hr = LinkProgram::BuildShim (baud, shim);
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

HRESULT LinkBootstrap::Send (IByteStream & stream, const std::vector<std::string> & lines, DWORD pauseMs, const std::function<bool()> & canceled)
{
    HRESULT  hr    = S_OK;
    bool     going = true;



    for (const std::string & line : lines)
    {
        going = !canceled || !canceled();
        CBREx (going, HRESULT_FROM_WIN32 (ERROR_CANCELLED));

        hr = stream.Write (reinterpret_cast<const Byte *> (line.data()), line.size());
        CHR (hr);

        std::this_thread::sleep_for (std::chrono::milliseconds (pauseMs));
    }

Error:
    return hr;
}
