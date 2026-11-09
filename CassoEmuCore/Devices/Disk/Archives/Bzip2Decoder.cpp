#include "Pch.h"

#include "Devices/Disk/Archives/Bzip2Decoder.h"

#define BZ_NO_STDIO
#include "../External/bzip2/bzlib.h"





////////////////////////////////////////////////////////////////////////////////
//
//  bz_internal_error
//
//  libbzip2 built with BZ_NO_STDIO calls this on an internal consistency
//  failure; the call that hit it returns an error.
//
////////////////////////////////////////////////////////////////////////////////

extern "C" void bz_internal_error (int errcode)
{
    UNREFERENCED_PARAMETER (errcode);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Bzip2Decoder::Expand
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Bzip2Decoder::Expand (std::span<const Byte> in, size_t outLength, std::vector<Byte> & outBytes, std::string & outError)
{
    HRESULT       hr        = S_OK;
    unsigned int  written   = (unsigned int) outLength;
    int           result    = BZ_OK;
    bool          fits      = outLength <= UINT_MAX && in.size() <= UINT_MAX;
    bool          expanded  = false;



    outBytes.clear();

    CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = "The bzip2 thread is too large.");

    outBytes.resize (outLength);

    result   = BZ2_bzBuffToBuffDecompress ((char *) outBytes.data(), &written, (char *) in.data(), (unsigned int) in.size(), 0, 0);
    expanded = result == BZ_OK && written == outLength;
    CBRFEx (expanded, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = std::format ("A bzip2 thread doesn't expand (error {}).", result));

Error:
    if (FAILED (hr))
    {
        outBytes.clear();
    }

    return hr;
}
