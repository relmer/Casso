#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateReader
//
//  Reads a blob StateWriter produced. Every read is bounds-checked against the
//  innermost open section, so a part can never read past its own payload into
//  the next part's, and EndSection fails unless the part consumed its payload
//  exactly. See IMachineState.h for the pattern.
//
//  Errors are sticky, like a stream's fail bit: the first failed read records
//  its HRESULT, sets its output to zero, and every later read does the same
//  without touching the stream. BeginSection and EndSection return the recorded
//  failure, so a LoadState checks those two calls and nothing in between.
//
//  Failures:
//    ERROR_INVALID_DATA      wrong tag, truncated stream, a read past the
//                            section, a section not fully consumed, a bool
//                            other than 0 or 1
//    ERROR_REVISION_MISMATCH a section version of 0 or newer than the reader
//                            supports
//
//  The reader does not own the bytes; they must outlive it.
//
////////////////////////////////////////////////////////////////////////////////

class StateReader
{
public:
                         StateReader (const Byte * data, size_t size);
    explicit             StateReader (const std::vector<Byte> & bytes);

    HRESULT              BeginSection (uint32_t tag, uint16_t maxVersion, uint16_t & outVersion);
    HRESULT              EndSection   ();

    void                 ReadByte     (Byte & out);
    void                 ReadBool     (bool & out);
    void                 ReadWord     (Word & out);
    void                 ReadUInt32   (uint32_t & out);
    void                 ReadUInt64   (uint64_t & out);
    void                 ReadBytes    (Byte * out, size_t count);

    HRESULT              GetResult    () const { return m_hr; }
    bool                 IsAtEnd      () const { return m_offset == m_size && m_sectionEnds.empty(); }

private:
    uint64_t             ReadLittleEndian (size_t byteCount);
    size_t               GetLimit         () const;
    bool                 TryConsume       (size_t byteCount);

    const Byte         * m_data   = nullptr;
    size_t               m_size   = 0;
    size_t               m_offset = 0;
    HRESULT              m_hr     = S_OK;
    std::vector<size_t>  m_sectionEnds;     // stream offset where each open section's payload ends
};
