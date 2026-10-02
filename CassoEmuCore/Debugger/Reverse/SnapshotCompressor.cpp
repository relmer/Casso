#include "Pch.h"

#include "Debugger/Reverse/SnapshotCompressor.h"

#pragma comment(lib, "cabinet.lib")





////////////////////////////////////////////////////////////////////////////////
//
//  ~SnapshotCompressor
//
////////////////////////////////////////////////////////////////////////////////

SnapshotCompressor::~SnapshotCompressor()
{
    if (m_compressor != nullptr)
    {
        CloseCompressor (m_compressor);
    }

    if (m_decompressor != nullptr)
    {
        CloseDecompressor (m_decompressor);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CreateHandles
//
//  Opens both handles on first use, so a store that never records costs no
//  more than two null pointers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SnapshotCompressor::CreateHandles()
{
    HRESULT  hr      = S_OK;
    BOOL     created = FALSE;



    if (m_compressor == nullptr)
    {
        created = CreateCompressor (COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW, nullptr, &m_compressor);
        CWRA (created);
    }

    if (m_decompressor == nullptr)
    {
        created = CreateDecompressor (COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW, nullptr, &m_decompressor);
        CWRA (created);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Compress
//
//  Packs size bytes into out, replacing what out held. The first call asks
//  the API for the bound on the packed size; the second packs into it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SnapshotCompressor::Compress (
    const Byte         * data,
    size_t               size,
    std::vector<Byte>  & out)
{
    HRESULT  hr         = S_OK;
    SIZE_T   needed     = 0;
    SIZE_T   packedSize = 0;
    BOOL     packed     = FALSE;
    DWORD    lastError  = ERROR_SUCCESS;
    bool     isEmpty    = size == 0;



    CBRAEx (data != nullptr || size == 0, E_INVALIDARG);

    hr = CreateHandles();
    CHR (hr);

    out.clear();

    BAIL_OUT_IF (isEmpty, S_OK);

    packed    = ::Compress (m_compressor, data, size, nullptr, 0, &needed);
    lastError = GetLastError();
    CBRAEx (!packed && lastError == ERROR_INSUFFICIENT_BUFFER, HRESULT_FROM_WIN32 (lastError));

    out.resize (needed);

    packed = ::Compress (m_compressor, data, size, out.data(), needed, &packedSize);
    CWRA (packed);

    out.resize (packedSize);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Decompress
//
//  Unpacks what Compress produced back to exactly size bytes. A stream that
//  does not unpack, or unpacks to any other length, is corrupt and fails with
//  ERROR_INVALID_DATA.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SnapshotCompressor::Decompress (
    const std::vector<Byte>  & packed,
    size_t                     size,
    std::vector<Byte>        & out)
{
    HRESULT  hr           = S_OK;
    SIZE_T   unpackedSize = 0;
    BOOL     unpacked     = FALSE;
    bool     hasPacked    = !packed.empty();
    bool     isEmpty      = size == 0;



    hr = CreateHandles();
    CHR (hr);

    out.resize (size);

    CBREx       (!isEmpty || !hasPacked, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    BAIL_OUT_IF (isEmpty, S_OK);

    unpacked = ::Decompress (m_decompressor, packed.data(), packed.size(), out.data(), size, &unpackedSize);
    CWREx (unpacked,             HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (unpackedSize == size, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}
