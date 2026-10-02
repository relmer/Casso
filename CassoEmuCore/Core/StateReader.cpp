#include "Pch.h"

#include "StateReader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateReader
//
////////////////////////////////////////////////////////////////////////////////

StateReader::StateReader (const Byte * data, size_t size) :
    m_data (data),
    m_size (size)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateReader
//
////////////////////////////////////////////////////////////////////////////////

StateReader::StateReader (const std::vector<Byte> & bytes) :
    m_data (bytes.data()),
    m_size (bytes.size())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginSection
//
//  Reads a section header and opens the section. The tag must match, the
//  version must be from 1 to maxVersion, and the payload must fit inside the
//  enclosing section (or the stream). outVersion receives the version found,
//  so a reader can accept older field lists. A failure is recorded as the
//  reader's sticky error, so the reads that follow do nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StateReader::BeginSection (uint32_t tag, uint16_t maxVersion, uint16_t & outVersion)
{
    HRESULT   hr          = S_OK;
    uint32_t  foundTag    = 0;
    Word      version     = 0;
    uint32_t  payloadSize = 0;
    size_t    remaining   = 0;



    outVersion = 0;

    ReadUInt32 (foundTag);
    ReadWord   (version);
    ReadUInt32 (payloadSize);
    CHR (m_hr);

    CBREx (foundTag == tag,                       HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (version != 0 && version <= maxVersion, HRESULT_FROM_WIN32 (ERROR_REVISION_MISMATCH));

    remaining = GetLimit() - m_offset;
    CBREx (payloadSize <= remaining, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_sectionEnds.push_back (m_offset + payloadSize);
    outVersion = version;

Error:
    m_hr = hr;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndSection
//
//  Closes the innermost open section. Returns the sticky error if any read
//  in the section failed, and otherwise fails unless the payload was consumed
//  exactly: bytes left over mean the reader and writer disagree on the field
//  list, which a version bump should have caught.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StateReader::EndSection()
{
    HRESULT  hr         = S_OK;
    bool     isOpen     = !m_sectionEnds.empty();
    size_t   sectionEnd = 0;



    CHR    (m_hr);
    CBRAEx (isOpen, E_UNEXPECTED);

    sectionEnd = m_sectionEnds.back();
    CBREx (m_offset == sectionEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_sectionEnds.pop_back();

Error:
    m_hr = hr;
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadByte
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadByte (Byte & out)
{
    out = static_cast<Byte> (ReadLittleEndian (sizeof (Byte)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBool
//
//  One byte, which must be 0 or 1; anything else is corrupt data.
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadBool (bool & out)
{
    uint64_t  value = ReadLittleEndian (sizeof (Byte));



    if (value > 1 && SUCCEEDED (m_hr))
    {
        m_hr  = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
        value = 0;
    }

    out = value != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadWord
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadWord (Word & out)
{
    out = static_cast<Word> (ReadLittleEndian (sizeof (Word)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadUInt32
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadUInt32 (uint32_t & out)
{
    out = static_cast<uint32_t> (ReadLittleEndian (sizeof (uint32_t)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadUInt64
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadUInt64 (uint64_t & out)
{
    out = ReadLittleEndian (sizeof (uint64_t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBytes
//
//  A raw run of count bytes. On failure the output is zero-filled.
//
////////////////////////////////////////////////////////////////////////////////

void StateReader::ReadBytes (Byte * out, size_t count)
{
    if (!TryConsume (count))
    {
        memset (out, 0, count);
        return;
    }

    memcpy (out, m_data + m_offset, count);
    m_offset += count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadLittleEndian
//
//  Returns zero once the reader has failed.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t StateReader::ReadLittleEndian (size_t byteCount)
{
    uint64_t  value = 0;
    size_t    i     = 0;



    if (!TryConsume (byteCount))
    {
        return 0;
    }

    for (i = 0; i < byteCount; i++)
    {
        value |= static_cast<uint64_t> (m_data[m_offset + i]) << (i * CHAR_BIT);
    }

    m_offset += byteCount;
    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryConsume
//
//  True when byteCount more bytes may be read: the reader has not failed and
//  they fit before the limit. A read that does not fit records the failure.
//
////////////////////////////////////////////////////////////////////////////////

bool StateReader::TryConsume (size_t byteCount)
{
    size_t  remaining = 0;



    if (FAILED (m_hr))
    {
        return false;
    }

    remaining = GetLimit() - m_offset;

    if (byteCount > remaining)
    {
        m_hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetLimit
//
//  The stream offset no read may pass: the end of the innermost open section,
//  or the end of the stream.
//
////////////////////////////////////////////////////////////////////////////////

size_t StateReader::GetLimit() const
{
    return m_sectionEnds.empty() ? m_size : m_sectionEnds.back();
}
