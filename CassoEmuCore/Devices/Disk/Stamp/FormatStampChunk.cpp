#include "Pch.h"

#include "Devices/Disk/Stamp/FormatStampChunk.h"

#include "Devices/Disk/Stamp/FormatStamp.h"
#include "Devices/Disk/Stamp/FormatStampJson.h"
#include "Machines/Apple2/Common/WozChunkWalker.h"
#include "Machines/Apple2/Common/WozMetadata.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::GetStamp
//
//  The stamp in the image's first CSSO chunk. Unreadable when the chunk is
//  there and its text is not a stamp this build reads; outStamp is then
//  empty.
//
////////////////////////////////////////////////////////////////////////////////

StampState FormatStampChunk::GetStamp (const WozMetadata & meta, FormatStamp & outStamp)
{
    HRESULT     hr    = S_OK;
    StampState  state = StampState::Absent;



    outStamp = FormatStamp();

    for (const WozChunk & chunk : meta.passThrough)
    {
        if (memcmp (chunk.id, kId, sizeof (kId)) != 0)
        {
            continue;
        }

        hr    = FormatStampJson::Parse (std::string_view ((const char *) chunk.payload.data(), chunk.payload.size()), outStamp);
        state = SUCCEEDED (hr) ? StampState::Readable : StampState::Unreadable;
        break;
    }

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::HasStampChunk
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampChunk::HasStampChunk (const WozMetadata & meta)
{
    return std::any_of (meta.passThrough.begin(), meta.passThrough.end(), [] (const WozChunk & chunk) { return memcmp (chunk.id, kId, sizeof (kId)) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::SetStamp
//
//  Replaces the first CSSO chunk's text with the stamp's, or appends a CSSO
//  chunk when the image has none.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampChunk::SetStamp (WozMetadata & ioMeta, const FormatStamp & stamp)
{
    HRESULT                     hr       = S_OK;
    std::string                 text;
    WozChunk                    chunk;
    bool                        isSmall  = false;
    vector<WozChunk>::iterator  existing;



    hr = FormatStampJson::Write (stamp, text);
    CHR (hr);

    isSmall = text.size() <= FormatStamp::kMaxBytes;
    CBRAEx (isSmall, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    memcpy (chunk.id, kId, sizeof (kId));
    chunk.payload.assign (text.begin(), text.end());

    existing = std::find_if (ioMeta.passThrough.begin(), ioMeta.passThrough.end(), [] (const WozChunk & candidate) { return memcmp (candidate.id, kId, sizeof (kId)) == 0; });

    if (existing != ioMeta.passThrough.end())
    {
        existing->payload = std::move (chunk.payload);
    }
    else
    {
        ioMeta.passThrough.push_back (std::move (chunk));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::RemoveStamp
//
//  Removes every CSSO chunk.
//
////////////////////////////////////////////////////////////////////////////////

void FormatStampChunk::RemoveStamp (WozMetadata & ioMeta)
{
    std::erase_if (ioMeta.passThrough, [] (const WozChunk & chunk) { return memcmp (chunk.id, kId, sizeof (kId)) == 0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::IsWozFile
//
////////////////////////////////////////////////////////////////////////////////

bool FormatStampChunk::IsWozFile (std::span<const Byte> file)
{
    constexpr Byte  kWoz[] = { 'W', 'O', 'Z' };



    return file.size() >= WozChunkWalker::kHeaderSize && memcmp (file.data(), kWoz, sizeof (kWoz)) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::GetStampFromFile
//
//  The first CSSO chunk of a WOZ file: its text as stored, and the stamp
//  when the text is one this build reads. ERROR_INVALID_DATA for bytes that
//  are not a WOZ file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampChunk::GetStampFromFile (std::span<const Byte> file, std::string & outText, StampState & outState, FormatStamp & outStamp)
{
    HRESULT               hr     = S_OK;
    vector<WozChunkSpan>  chunks;
    size_t                end    = 0;
    bool                  isWoz  = IsWozFile (file);



    outText.clear();
    outState = StampState::Absent;
    outStamp = FormatStamp();

    CBREx (isWoz, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    WozChunkWalker::Walk (file, chunks, end);

    for (const WozChunkSpan & chunk : chunks)
    {
        if (!WozChunkWalker::IsChunk (chunk, kId))
        {
            continue;
        }

        outText.assign ((const char *) file.data() + chunk.payload, chunk.size);
        hr       = FormatStampJson::Parse (outText, outStamp);
        outState = SUCCEEDED (hr) ? StampState::Readable : StampState::Unreadable;
        hr       = S_OK;
        break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatStampChunk::SetStampInFile
//
//  Puts the stamp in a WOZ file's bytes: removes the CSSO chunk it has,
//  appends the new one after the last chunk, and computes the header CRC
//  again. Every other byte stays where it is. Fails, leaving the bytes as
//  they were, with ERROR_CRC for a file whose CRC doesn't match, since a new
//  CRC would hide the damage; and ERROR_NOT_SUPPORTED when removing the old
//  chunk would move a FLUX chunk off the block INFO points to, or when the
//  file has bytes after its last chunk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FormatStampChunk::SetStampInFile (vector<Byte> & ioFile, const FormatStamp & stamp)
{
    constexpr Byte          kFlux[4]  = { 'F', 'L', 'U', 'X' };
    HRESULT                 hr        = S_OK;
    vector<Byte>            bytes     = ioFile;
    vector<WozChunkSpan>    chunks;
    const WozChunkSpan    * old       = nullptr;
    const WozChunkSpan    * flux      = nullptr;
    std::string             text;
    size_t                  end       = 0;
    size_t                  at        = 0;
    uint32_t                stored    = 0;
    bool                    isWoz     = IsWozFile (bytes);
    bool                    isIntact  = false;
    bool                    isMovable = false;
    bool                    isAtEnd   = false;
    bool                    isSmall   = false;



    CBREx (isWoz, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    //  Zero means the writer didn't compute a CRC.
    stored   = WozChunkWalker::ReadSize (bytes.data() + WozChunkWalker::kCrcOffset);
    isIntact = stored == 0 || stored == WozChunkWalker::ComputeCrc (bytes);
    CBREx (isIntact, HRESULT_FROM_WIN32 (ERROR_CRC));

    hr = FormatStampJson::Write (stamp, text);
    CHR (hr);

    isSmall = text.size() <= FormatStamp::kMaxBytes;
    CBRAEx (isSmall, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    WozChunkWalker::Walk (bytes, chunks, end);

    for (const WozChunkSpan & chunk : chunks)
    {
        old  = (old == nullptr && WozChunkWalker::IsChunk (chunk, kId)) ? &chunk : old;
        flux = WozChunkWalker::IsChunk (chunk, kFlux) ? &chunk : flux;
    }

    if (old != nullptr)
    {
        isMovable = flux == nullptr || flux->start < old->start;
        CBREx (isMovable, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

        bytes.erase (bytes.begin() + (ptrdiff_t) old->start, bytes.begin() + (ptrdiff_t) (old->payload + old->size));
        end -= WozChunkWalker::kChunkHeaderSize + old->size;
    }

    isAtEnd = end == bytes.size();
    CBREx (isAtEnd, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    at = bytes.size();
    bytes.resize (at + WozChunkWalker::kChunkHeaderSize + text.size());

    memcpy                      (bytes.data() + at, kId, sizeof (kId));
    WozChunkWalker::WriteSize   (bytes.data() + at + sizeof (kId), static_cast<uint32_t> (text.size()));
    memcpy                      (bytes.data() + at + WozChunkWalker::kChunkHeaderSize, text.data(), text.size());
    WozChunkWalker::UpdateCrc   (bytes);

    ioFile.swap (bytes);

Error:
    return hr;
}
