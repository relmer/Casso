#include "Pch.h"

#include "Seams/RawDevice/RawDeviceProtocol.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::ReadExact
//
//  Bytes written before the stream closed are read before the close is
//  reported.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceProtocol::ReadExact (IByteStream & stream, Byte * buffer, size_t count, DWORD timeoutMs)
{
    HRESULT    hr       = S_OK;
    size_t     total    = 0;
    ULONGLONG  started  = GetTickCount64();
    bool       ended    = false;
    bool       late     = false;



    while (total < count)
    {
        size_t  got = 0;

        hr = stream.Read (buffer + total, count - total, kSliceMs, got);
        CHR (hr);

        total += got;

        ended = got == 0 && stream.IsClosed() && stream.GetReadableCount() == 0;
        CBREx (!ended, HRESULT_FROM_WIN32 (ERROR_BROKEN_PIPE));

        late = got == 0 && timeoutMs != INFINITE && GetTickCount64() - started >= timeoutMs;
        CBREx (!late, HRESULT_FROM_WIN32 (ERROR_TIMEOUT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::AppendLong
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceProtocol::AppendLong (std::vector<Byte> & bytes, uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
    {
        bytes.push_back ((Byte) (value >> shift));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::AppendQuad
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceProtocol::AppendQuad (std::vector<Byte> & bytes, uint64_t value)
{
    for (int shift = 0; shift < 64; shift += 8)
    {
        bytes.push_back ((Byte) (value >> shift));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::AppendText
//
////////////////////////////////////////////////////////////////////////////////

void RawDeviceProtocol::AppendText (std::vector<Byte> & bytes, const std::string & text)
{
    size_t  length = std::min (text.size(), kMaxTextBytes);



    bytes.push_back ((Byte) (length & 0xFF));
    bytes.push_back ((Byte) (length >> 8));
    bytes.insert (bytes.end(), text.begin(), text.begin() + (ptrdiff_t) length);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::GetLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t RawDeviceProtocol::GetLong (const Byte * bytes)
{
    return (uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16) | ((uint32_t) bytes[3] << 24);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::GetQuad
//
////////////////////////////////////////////////////////////////////////////////

uint64_t RawDeviceProtocol::GetQuad (const Byte * bytes)
{
    return (uint64_t) GetLong (bytes) | ((uint64_t) GetLong (bytes + 4) << 32);
}





////////////////////////////////////////////////////////////////////////////////
//
//  RawDeviceProtocol::ReadText
//
////////////////////////////////////////////////////////////////////////////////

HRESULT RawDeviceProtocol::ReadText (IByteStream & stream, std::string & outText)
{
    HRESULT  hr        = S_OK;
    Byte     prefix[2] = {};
    size_t   length    = 0;
    bool     fits      = false;



    outText.clear();

    hr = ReadExact (stream, prefix, sizeof (prefix), kReplyTimeoutMs);
    CHR (hr);

    length = (size_t) prefix[0] | ((size_t) prefix[1] << 8);
    fits   = length <= kMaxTextBytes;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outText.resize (length);

    if (length > 0)
    {
        hr = ReadExact (stream, (Byte *) outText.data(), length, kReplyTimeoutMs);
        CHR (hr);
    }

Error:
    return hr;
}
