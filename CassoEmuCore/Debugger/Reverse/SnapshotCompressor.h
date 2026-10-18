#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotCompressor
//
//  Fast lossless compression of machine snapshots through the Windows
//  Compression API (cabinet.dll, part of Windows since 8; no third-party
//  code): XPRESS in raw mode, or the algorithm and mode passed to the
//  constructor. Raw mode stores no header, so the uncompressed size is stored
//  by the caller and passed back to Decompress. MSZIP has no raw mode.
//
////////////////////////////////////////////////////////////////////////////////

class SnapshotCompressor
{
public:
                SnapshotCompressor  () = default;
    explicit    SnapshotCompressor  (DWORD algorithm);
                ~SnapshotCompressor ();

                SnapshotCompressor  (const SnapshotCompressor &)             = delete;
    SnapshotCompressor & operator=  (const SnapshotCompressor &)             = delete;

    HRESULT     Compress            (const Byte * data, size_t size, std::vector<Byte> & out);
    HRESULT     Decompress          (const std::vector<Byte> & packed, size_t size, std::vector<Byte> & out);
    HRESULT     Decompress          (const Byte * packed, size_t packedSize, size_t size, std::vector<Byte> & out);

private:
    HRESULT     CreateHandles       ();

    DWORD                m_algorithm    = COMPRESS_ALGORITHM_XPRESS | COMPRESS_RAW;
    COMPRESSOR_HANDLE    m_compressor   = nullptr;
    DECOMPRESSOR_HANDLE  m_decompressor = nullptr;
    std::vector<Byte>    m_buffer;                   // worst-case output, reused
};
