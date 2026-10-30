#include "Pch.h"

#include "CassoLink/LinkBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::BuildLines
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::BuildLines (std::vector<std::string> & outLines)
{
    HRESULT            hr     = S_OK;
    std::vector<Byte>  loader;
    std::string        errors;
    Word               start  = LinkProgram::kSerialLoaderAddress;



    outLines.clear();

    hr = LinkProgram::AssembleSerialLoader (loader, errors);
    CHR (hr);

    outLines.push_back (kCancelLine);
    outLines.push_back ("CALL -151\r");

    for (size_t at = 0; at < loader.size(); at += kBytesPerLine)
    {
        size_t       end  = std::min (at + kBytesPerLine, loader.size());
        std::string  line = std::format ("{:04X}:", start + at);

        for (size_t i = at; i < end; i++)
        {
            line += std::format (" {:02X}", loader[i]);
        }

        outLines.push_back (line + "\r");
    }

    outLines.push_back (std::format ("{:04X}G\r", start));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::BuildShimTransfer
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::BuildShimTransfer (DWORD baud, std::vector<Byte> & outBytes)
{
    HRESULT            hr    = S_OK;
    std::vector<Byte>  shim;
    size_t             pages = 0;
    Byte               sum   = 0;



    outBytes.clear();

    hr = LinkProgram::BuildShim (baud, shim);
    CHR (hr);

    pages = (shim.size() + 255) / 256;
    CBREx (pages > 0 && pages < 256, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    shim.resize (pages * 256, 0);

    for (Byte value : shim)
    {
        sum ^= value;
    }

    outBytes.push_back ((Byte) pages);
    outBytes.insert    (outBytes.end(), shim.begin(), shim.end());
    outBytes.push_back (sum);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LinkBootstrap::SendShim
//
//  The monitor echoes nothing to the port, so the ready byte is normally
//  the first one there.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LinkBootstrap::SendShim (IByteStream & stream, const std::vector<Byte> & transfer)
{
    HRESULT  hr       = S_OK;
    Byte     got      = 0;
    size_t   read     = 0;
    bool     ready    = false;
    auto     deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (kLoaderReadyWaitMs);



    while (!ready && std::chrono::steady_clock::now() < deadline)
    {
        hr = stream.Read (&got, 1, 100, read);
        CHR (hr);

        ready = read == 1 && got == kLoaderReady;
    }

    CBREx (ready, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));

    hr = stream.Write (transfer.data(), transfer.size());
    CHR (hr);

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
