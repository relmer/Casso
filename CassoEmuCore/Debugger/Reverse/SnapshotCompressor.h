#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SnapshotCompressor
//
//  Fast lossless compression of machine snapshots through the Windows
//  Compression API's XPRESS algorithm in raw mode (cabinet.dll, part of
//  Windows since 8; no third-party code). Raw mode stores no header, so the
//  caller keeps the uncompressed size and passes it back to Decompress.
//
////////////////////////////////////////////////////////////////////////////////

class SnapshotCompressor
{
public:
                SnapshotCompressor  () = default;
                ~SnapshotCompressor ();

                SnapshotCompressor  (const SnapshotCompressor &)             = delete;
    SnapshotCompressor & operator=  (const SnapshotCompressor &)             = delete;

    HRESULT     Compress            (const Byte * data, size_t size, std::vector<Byte> & out);
    HRESULT     Decompress          (const std::vector<Byte> & packed, size_t size, std::vector<Byte> & out);

private:
    HRESULT     CreateHandles       ();

    COMPRESSOR_HANDLE    m_compressor   = nullptr;
    DECOMPRESSOR_HANDLE  m_decompressor = nullptr;
    std::vector<Byte>    m_buffer;                   // worst-case output, reused
};
