#include "Pch.h"

#include "Shell/SavedHistory.h"

#include "Core/StateHash.h"
#include "Core/StateReader.h"
#include "Core/StateWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::Build
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::Build (
    const SavedHistoryContents  & contents,
    std::vector<Byte>           & outSection)
{
    constexpr size_t     kTagBytes  = sizeof (uint32_t);
    constexpr size_t     kWordBytes = sizeof (uint16_t);
    constexpr size_t     kQuadBytes = sizeof (uint64_t);
    HRESULT              hr         = S_OK;
    StateWriter          body;



    outSection.clear();

    hr = WriteBody (contents, body);
    CHR (hr);

    outSection.reserve (kHeaderSize + body.GetBytes().size());

    AppendLittleEndian (kTag,                    kTagBytes,  outSection);
    AppendLittleEndian (kVersion,                kWordBytes, outSection);
    AppendLittleEndian (0,                       kWordBytes, outSection);
    AppendLittleEndian (body.GetBytes().size(),  kQuadBytes, outSection);
    AppendLittleEndian (StateHash::Hash (body.GetBytes().data(), body.GetBytes().size()), kQuadBytes, outSection);

    outSection.insert (outSection.end(), body.GetBytes().begin(), body.GetBytes().end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::WriteBody
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::WriteBody (
    const SavedHistoryContents  & contents,
    StateWriter                 & body)
{
    HRESULT                 hr        = S_OK;
    const SavedKeyframes  & keyframes = contents.keyframes;



    body.BeginSection (kFormatTag, kSectionVersion);
    body.WriteUInt64  (contents.fingerprint);
    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kSettingsTag, kSectionVersion);
    body.WriteUInt64  (keyframes.settings.intervalCycles);
    body.WriteUInt32  (keyframes.settings.wholeEvery);
    body.WriteUInt32  (keyframes.settings.longestGroup);
    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kSegmentsTag, kSectionVersion);
    body.WriteUInt32  (static_cast<uint32_t> (keyframes.segments.size()));

    for (const SavedKeyframeSegment & segment : keyframes.segments)
    {
        body.WriteUInt32 (segment.info.id);
        body.WriteUInt32 (segment.info.parent);
        body.WriteUInt64 (segment.info.forkPosition);
        body.WriteUInt64 (segment.info.forkCycle);
        body.WriteUInt64 (segment.firstJournalIndex);
        body.WriteUInt64 (segment.info.leftAtWallTime);
        body.WriteUInt64 (segment.info.liveEndPosition);
        body.WriteUInt64 (segment.info.liveEndCycle);
        body.WriteUInt32 (static_cast<uint32_t> (segment.infos.size()));
    }

    body.WriteUInt32 (keyframes.nextBranchId);
    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kKeyframesTag, kSectionVersion);

    for (const SavedKeyframeSegment & segment : keyframes.segments)
    {
        for (size_t i = 0; i < segment.infos.size(); i++)
        {
            WriteInfo (segment.infos[i], body);
            body.WriteUInt64 (segment.offsets[i]);
        }
    }

    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kArenaTag, kSectionVersion);
    body.WriteUInt64  (keyframes.arena.size());
    body.WriteBytes   (keyframes.arena.data(), keyframes.arena.size());
    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kJournalTag, kSectionVersion);

    for (const SavedKeyframeSegment & segment : keyframes.segments)
    {
        body.WriteUInt32 (static_cast<uint32_t> (segment.records.size()));

        for (const InputRecord & record : segment.records)
        {
            WriteRecord (record, body);
        }
    }

    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kMediaTag, kSectionVersion);
    body.WriteUInt32  (static_cast<uint32_t> (contents.media.size()));

    for (const SavedBayMedium & medium : contents.media)
    {
        body.WriteByte   (static_cast<Byte> (medium.slot));
        body.WriteByte   (static_cast<Byte> (medium.drive));
        body.WriteUInt64 (medium.mediaId);
    }

    hr = body.EndSection();
    CHRA (hr);

    body.BeginSection (kResumeTag, kSectionVersion);
    body.WriteUInt64  (contents.resumePosition);
    body.WriteUInt64  (contents.resumeCycle);
    hr = body.EndSection();
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::Parse
//
//  The header's tag, version, size and hash are checked before the body is
//  read, and the body must be read to its last byte.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::Parse (
    const Byte            * data,
    size_t                  size,
    SavedHistoryContents  & outContents)
{
    HRESULT   hr         = S_OK;
    uint64_t  tag        = 0;
    uint64_t  version    = 0;
    uint64_t  bodySize   = 0;
    uint64_t  hash       = 0;
    bool      isIntact   = false;



    outContents = SavedHistoryContents();

    CBREx (size >= kHeaderSize, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    tag      = ReadLittleEndian (data,      sizeof (uint32_t));
    version  = ReadLittleEndian (data + 4,  sizeof (uint16_t));
    bodySize = ReadLittleEndian (data + 8,  sizeof (uint64_t));
    hash     = ReadLittleEndian (data + 16, sizeof (uint64_t));

    CBREx (tag == kTag && version == kVersion,  HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    CBREx (bodySize == size - kHeaderSize,      HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isIntact = hash == StateHash::Hash (data + kHeaderSize, size - kHeaderSize);
    CBREx (isIntact, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReadBody (data + kHeaderSize, size - kHeaderSize, outContents);
    CHR (hr);

Error:
    if (FAILED (hr))
    {
        outContents = SavedHistoryContents();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadBody
//
//  Every count is checked against the body's size before anything is sized
//  from it.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadBody (
    const Byte            * data,
    size_t                  size,
    SavedHistoryContents  & outContents)
{
    HRESULT          hr        = S_OK;
    StateReader      reader (data, size);
    SavedKeyframes & keyframes = outContents.keyframes;
    uint16_t         version   = 0;
    uint64_t         count     = 0;
    bool             fits      = false;
    bool             isAtEnd   = false;



    hr = reader.BeginSection (kFormatTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt64 (outContents.fingerprint);

    hr = reader.EndSection();
    CHR (hr);

    hr = reader.BeginSection (kSettingsTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt64 (keyframes.settings.intervalCycles);
    reader.ReadUInt32 (keyframes.settings.wholeEvery);
    reader.ReadUInt32 (keyframes.settings.longestGroup);

    hr = reader.EndSection();
    CHR (hr);

    hr = ReadSegments (reader, size, keyframes);
    CHR (hr);

    hr = ReadKeyframes (reader, keyframes);
    CHR (hr);

    hr = reader.BeginSection (kArenaTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt64 (count);
    fits = count <= size;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    keyframes.arena.resize (static_cast<size_t> (count));
    reader.ReadBytes (keyframes.arena.data(), keyframes.arena.size());

    hr = reader.EndSection();
    CHR (hr);

    hr = ReadJournal (reader, size, keyframes);
    CHR (hr);

    hr = ReadMedia (reader, outContents.media);
    CHR (hr);

    hr = reader.BeginSection (kResumeTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt64 (outContents.resumePosition);
    reader.ReadUInt64 (outContents.resumeCycle);

    hr = reader.EndSection();
    CHR (hr);

    isAtEnd = reader.IsAtEnd();
    CBREx (isAtEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadSegments
//
//  Each segment's keyframe count sizes its infos, which ReadKeyframes
//  fills.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadSegments (
    StateReader     & reader,
    size_t            size,
    SavedKeyframes  & outKeyframes)
{
    constexpr size_t      kSegmentBytes  = 56;
    constexpr size_t      kKeyframeBytes = 84;
    HRESULT               hr             = S_OK;
    uint16_t              version        = 0;
    uint32_t              count          = 0;
    uint32_t              keyframes      = 0;
    uint64_t              value          = 0;
    bool                  fits           = false;
    SavedKeyframeSegment  segment;



    hr = reader.BeginSection (kSegmentsTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt32 (count);
    fits = count <= size / kSegmentBytes;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (uint32_t i = 0; i < count; i++)
    {
        segment = SavedKeyframeSegment();

        reader.ReadUInt32 (segment.info.id);
        reader.ReadUInt32 (segment.info.parent);
        reader.ReadUInt64 (segment.info.forkPosition);
        reader.ReadUInt64 (segment.info.forkCycle);
        reader.ReadUInt64 (value);
        reader.ReadUInt64 (segment.info.leftAtWallTime);
        reader.ReadUInt64 (segment.info.liveEndPosition);
        reader.ReadUInt64 (segment.info.liveEndCycle);
        reader.ReadUInt32 (keyframes);

        fits = keyframes <= size / kKeyframeBytes;
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        segment.firstJournalIndex  = static_cast<size_t> (value);
        segment.info.keyframeCount = keyframes;
        segment.infos.resize   (keyframes);
        segment.offsets.resize (keyframes);

        outKeyframes.segments.push_back (std::move (segment));
    }

    reader.ReadUInt32 (outKeyframes.nextBranchId);

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadKeyframes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadKeyframes (
    StateReader     & reader,
    SavedKeyframes  & ioKeyframes)
{
    HRESULT   hr      = S_OK;
    uint16_t  version = 0;
    uint64_t  offset  = 0;



    hr = reader.BeginSection (kKeyframesTag, kSectionVersion, version);
    CHR (hr);

    for (SavedKeyframeSegment & segment : ioKeyframes.segments)
    {
        for (size_t i = 0; i < segment.infos.size(); i++)
        {
            ReadInfo (reader, segment.infos[i]);
            reader.ReadUInt64 (offset);

            segment.offsets[i] = static_cast<size_t> (offset);
        }

        hr = reader.GetResult();
        CHR (hr);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadJournal
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadJournal (
    StateReader     & reader,
    size_t            size,
    SavedKeyframes  & ioKeyframes)
{
    constexpr size_t  kRecordBytes = 33;
    HRESULT           hr           = S_OK;
    uint16_t          version      = 0;
    uint32_t          count        = 0;
    bool              fits         = false;
    InputRecord       record;



    hr = reader.BeginSection (kJournalTag, kSectionVersion, version);
    CHR (hr);

    for (SavedKeyframeSegment & segment : ioKeyframes.segments)
    {
        reader.ReadUInt32 (count);
        fits = count <= size / kRecordBytes;
        CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        segment.records.reserve (count);

        for (uint32_t i = 0; i < count; i++)
        {
            hr = ReadRecord (reader, size, record);
            CHR (hr);

            segment.records.push_back (std::move (record));
        }
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadMedia
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadMedia (
    StateReader                  & reader,
    std::vector<SavedBayMedium>  & outMedia)
{
    constexpr uint32_t  kMostBays = 64;
    HRESULT             hr        = S_OK;
    uint16_t            version   = 0;
    uint32_t            count     = 0;
    Byte                value     = 0;
    bool                fits      = false;
    SavedBayMedium      medium;



    hr = reader.BeginSection (kMediaTag, kSectionVersion, version);
    CHR (hr);

    reader.ReadUInt32 (count);
    fits = count <= kMostBays;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (uint32_t i = 0; i < count; i++)
    {
        reader.ReadByte (value);
        medium.slot = value;
        reader.ReadByte (value);
        medium.drive = value;
        reader.ReadUInt64 (medium.mediaId);

        outMedia.push_back (medium);
    }

    hr = reader.EndSection();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::WriteInfo
//
////////////////////////////////////////////////////////////////////////////////

void SavedHistory::WriteInfo (
    const KeyframeInfo  & info,
    StateWriter         & writer)
{
    writer.WriteUInt64 (info.position);
    writer.WriteUInt64 (info.cycle);
    writer.WriteUInt64 (info.journalIndex);
    writer.WriteUInt64 (info.checksum);
    writer.WriteUInt64 (info.stateBytes);
    writer.WriteUInt64 (info.storedBytes);
    writer.WriteUInt64 (info.sideBytes);
    writer.WriteUInt64 (info.wallTime);
    writer.WriteUInt64 (info.gapStart);
    writer.WriteBool   (info.isWhole);
    writer.WriteBool   (info.isBoundary);
    writer.WriteBool   (info.isLiveEnd);
    writer.WriteBool   (info.hasGapBefore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadInfo
//
////////////////////////////////////////////////////////////////////////////////

void SavedHistory::ReadInfo (
    StateReader   & reader,
    KeyframeInfo  & outInfo)
{
    uint64_t  value = 0;



    reader.ReadUInt64 (outInfo.position);
    reader.ReadUInt64 (outInfo.cycle);
    reader.ReadUInt64 (value);
    outInfo.journalIndex = static_cast<size_t> (value);
    reader.ReadUInt64 (outInfo.checksum);
    reader.ReadUInt64 (value);
    outInfo.stateBytes = static_cast<size_t> (value);
    reader.ReadUInt64 (value);
    outInfo.storedBytes = static_cast<size_t> (value);
    reader.ReadUInt64 (value);
    outInfo.sideBytes = static_cast<size_t> (value);
    reader.ReadUInt64 (outInfo.wallTime);
    reader.ReadUInt64 (outInfo.gapStart);
    reader.ReadBool   (outInfo.isWhole);
    reader.ReadBool   (outInfo.isBoundary);
    reader.ReadBool   (outInfo.isLiveEnd);
    reader.ReadBool   (outInfo.hasGapBefore);
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::WriteRecord
//
////////////////////////////////////////////////////////////////////////////////

void SavedHistory::WriteRecord (
    const InputRecord  & record,
    StateWriter        & writer)
{
    writer.WriteUInt64 (record.position);
    writer.WriteUInt64 (record.cycle);
    writer.WriteByte   (static_cast<Byte> (record.kind));
    writer.WriteByte   (record.value);
    writer.WriteWord   (record.detail);
    writer.WriteUInt64 (record.data);
    writer.WriteBool   (record.isObserved);
    writer.WriteUInt32 (static_cast<uint32_t> (record.payload.size()));
    writer.WriteBytes  (reinterpret_cast<const Byte *> (record.payload.data()), record.payload.size());
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadRecord
//
//  An input kind past InputKind::StateLoad is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SavedHistory::ReadRecord (
    StateReader  & reader,
    size_t         size,
    InputRecord  & outRecord)
{
    HRESULT   hr     = S_OK;
    Byte      kind   = 0;
    uint32_t  length = 0;
    bool      fits   = false;



    reader.ReadUInt64 (outRecord.position);
    reader.ReadUInt64 (outRecord.cycle);
    reader.ReadByte   (kind);
    reader.ReadByte   (outRecord.value);
    reader.ReadWord   (outRecord.detail);
    reader.ReadUInt64 (outRecord.data);
    reader.ReadBool   (outRecord.isObserved);
    reader.ReadUInt32 (length);

    fits = kind <= static_cast<Byte> (InputKind::StateLoad) && length <= size;
    CBREx (fits, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outRecord.kind = static_cast<InputKind> (kind);
    outRecord.payload.resize (length);
    reader.ReadBytes (reinterpret_cast<Byte *> (outRecord.payload.data()), length);

    hr = reader.GetResult();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::ReadLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

uint64_t SavedHistory::ReadLittleEndian (
    const Byte  * data,
    size_t        byteCount)
{
    uint64_t  value = 0;



    for (size_t i = byteCount; i > 0; i--)
    {
        value = (value << 8) | data[i - 1];
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SavedHistory::AppendLittleEndian
//
////////////////////////////////////////////////////////////////////////////////

void SavedHistory::AppendLittleEndian (
    uint64_t             value,
    size_t               byteCount,
    std::vector<Byte>  & outBytes)
{
    for (size_t i = 0; i < byteCount; i++)
    {
        outBytes.push_back (static_cast<Byte> (value >> (8 * i)));
    }
}
