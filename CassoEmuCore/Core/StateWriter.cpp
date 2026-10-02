#include "Pch.h"

#include "StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Reuse
//
//  Starts the stream over in buffer, keeping its capacity, so a writer that
//  saves the machine many times allocates once.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::Reuse (std::vector<Byte> && buffer)
{
    m_bytes = std::move (buffer);
    m_bytes.clear();
    m_openSections.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  BeginSection
//
//  Writes the section header with a placeholder size and opens the section.
//  Every BeginSection is matched by an EndSection.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::BeginSection (uint32_t tag, uint16_t version)
{
    WriteUInt32 (tag);
    WriteWord   (version);
    WriteUInt32 (0);

    m_openSections.push_back (m_bytes.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  EndSection
//
//  Closes the innermost open section and patches its payload size into the
//  header. Fails when no section is open, or when the payload would not fit
//  the 32-bit size field.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT StateWriter::EndSection()
{
    HRESULT  hr           = S_OK;
    bool     isOpen       = !m_openSections.empty();
    size_t   payloadStart = 0;
    size_t   payloadSize  = 0;
    size_t   sizeOffset   = 0;
    size_t   i            = 0;



    CBRAEx (isOpen, E_UNEXPECTED);

    payloadStart = m_openSections.back();
    m_openSections.pop_back();

    payloadSize = m_bytes.size() - payloadStart;
    CBRAEx (payloadSize <= UINT32_MAX, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

    sizeOffset = payloadStart - sizeof (uint32_t);

    for (i = 0; i < sizeof (uint32_t); i++)
    {
        m_bytes[sizeOffset + i] = static_cast<Byte> (payloadSize >> (i * CHAR_BIT));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteByte
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteByte (Byte value)
{
    m_bytes.push_back (value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBool
//
//  One byte, 0 or 1.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteBool (bool value)
{
    m_bytes.push_back (value ? 1 : 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteWord
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteWord (Word value)
{
    WriteLittleEndian (value, sizeof (Word));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteUInt32
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteUInt32 (uint32_t value)
{
    WriteLittleEndian (value, sizeof (uint32_t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteUInt64
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteUInt64 (uint64_t value)
{
    WriteLittleEndian (value, sizeof (uint64_t));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBytes
//
//  A raw run of bytes, such as a RAM bank. The count is not written; the
//  reader must know it, and a section size check catches a disagreement.
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteBytes (const Byte * data, size_t count)
{
    m_bytes.insert (m_bytes.end(), data, data + count);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

void StateWriter::WriteLittleEndian (uint64_t value, size_t byteCount)
{
    size_t  i = 0;



    for (i = 0; i < byteCount; i++)
    {
        m_bytes.push_back (static_cast<Byte> (value >> (i * CHAR_BIT)));
    }
}
