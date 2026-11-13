#include "Pch.h"

#include "Devices/Tape/TapeRecordingVolume.h"
#include "Devices/Tape/AiffCodec.h"
#include "Devices/Tape/MfTapeAudioDecoder.h"
#include "Devices/Tape/OggVorbisCodec.h"
#include "Devices/Tape/TapeImageLoader.h"
#include "Devices/Tape/WavCodec.h"
#include "Machines/Apple2/Common/VolumeIntegrityReport.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::TapeRecordingVolume
//
////////////////////////////////////////////////////////////////////////////////

TapeRecordingVolume::TapeRecordingVolume (const vector<Byte> & bytes)
    : m_bytes (bytes)
{
    MfTapeAudioDecoder  decoder;



    Decode (decoder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::TapeRecordingVolume
//
////////////////////////////////////////////////////////////////////////////////

TapeRecordingVolume::TapeRecordingVolume (const vector<Byte> & bytes, ITapeAudioDecoder & decoder)
    : m_bytes (bytes)
{
    Decode (decoder);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool TapeRecordingVolume::IsFound (const vector<Byte> & bytes)
{
    return WavCodec::IsWav (bytes) || AiffCodec::IsAiff (bytes) || OggVorbisCodec::IsOgg (bytes)
        || TapeImageLoader::IsFlac (bytes) || TapeImageLoader::IsMp3 (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Decode
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingVolume::Decode (ITapeAudioDecoder & decoder)
{
    HRESULT                  hr = S_OK;
    TapeImage                image;
    std::vector<TapeRecord>  records;



    hr = TapeImageLoader::Load (m_bytes, std::string(), true, decoder, image, m_error);

    if (SUCCEEDED (hr))
    {
        TapeRecordScanner::Scan (image.signal.transitions, image.signal.sampleRate, records);
        GroupRecords (records, m_files);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::GroupRecords
//
////////////////////////////////////////////////////////////////////////////////

void TapeRecordingVolume::GroupRecords (const std::vector<TapeRecord> & records, std::vector<TapeFile> & outFiles)
{
    outFiles.clear();

    for (size_t i = 0; i < records.size(); i++)
    {
        const TapeRecord  & record  = records[i];
        bool                hasNext = i + 1 < records.size() && record.isValid;
        size_t              length  = (record.bytes.size() >= kIntegerHeader) ? (size_t) (record.bytes[0] | (record.bytes[1] << kByteShift)) : 0;
        bool                isApple = hasNext && record.bytes.size() == kApplesoftHeader && records[i + 1].bytes.size() == length + 1;
        bool                isInt   = hasNext && record.bytes.size() == kIntegerHeader   && records[i + 1].bytes.size() == length;
        TapeFile            file;

        file.name = std::format ("FILE{}", outFiles.size() + 1);

        if (isApple || isInt)
        {
            file.type    = isApple ? kTypeApplesoft : kTypeInteger;
            file.bytes.assign (records[i + 1].bytes.begin(), records[i + 1].bytes.begin() + (ptrdiff_t) length);
            file.isValid = records[i + 1].isValid;
            i++;
        }
        else
        {
            file.type    = kTypeBinary;
            file.bytes   = record.bytes;
            file.isValid = record.isValid;
        }

        outFiles.push_back (file);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::TryFindFile
//
////////////////////////////////////////////////////////////////////////////////

bool TapeRecordingVolume::TryFindFile (const std::vector<TapeFile> & files, const std::string & name, size_t & outIndex)
{
    bool  found = false;



    for (size_t i = 0; !found && i < files.size(); i++)
    {
        found    = files[i].name == name;
        outIndex = found ? i : outIndex;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Enumerate
//
//  A recording that couldn't be decoded lists nothing, with the reason.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Enumerate (VolumeListing & outListing) const
{
    outListing = VolumeListing();

    if (!m_error.empty())
    {
        outListing.damage.push_back (m_error);
    }

    for (size_t i = 0; i < m_files.size(); i++)
    {
        FileEntry  entry;

        entry.name         = m_files[i].name;
        entry.type         = m_files[i].type;
        entry.sizeUnits    = 1;
        entry.eofBytes     = (uint32_t) m_files[i].bytes.size();
        entry.hasEofBytes  = true;
        entry.catalogIndex = i;

        if (m_files[i].type == kTypeApplesoft)
        {
            entry.auxType        = kApplesoftLoad;
            entry.loadAddress    = kApplesoftLoad;
            entry.hasAuxType     = true;
            entry.hasLoadAddress = true;
        }

        outListing.entries.push_back (entry);

        if (!m_files[i].isValid)
        {
            outListing.damage.push_back (std::format ("{} fails its checksum", m_files[i].name));
        }
    }

    outListing.totalUnits = (uint32_t) m_files.size();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Read
//
//  A file whose checksum fails is ERROR_CRC.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Read (const FilePath & path, FilePayload & outPayload) const
{
    HRESULT  hr    = S_OK;
    size_t   index = 0;
    bool     found = TryFindFile (m_files, path.ToString(), index);



    outPayload = FilePayload();

    CBREx (found, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
    CBREx (m_files[index].isValid, HRESULT_FROM_WIN32 (ERROR_CRC));

    outPayload.bytes = m_files[index].bytes;
    outPayload.type  = m_files[index].type;

    if (m_files[index].type == kTypeApplesoft)
    {
        outPayload.auxType        = kApplesoftLoad;
        outPayload.loadAddress    = kApplesoftLoad;
        outPayload.hasAuxType     = true;
        outPayload.hasLoadAddress = true;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::BuildIntegrityReport
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::BuildIntegrityReport (VolumeIntegrityReport & outReport) const
{
    outReport.Reset ((uint32_t) m_files.size());
    outReport.SetCatalogFullyParsed (m_error.empty());

    for (size_t i = 0; i < m_files.size(); i++)
    {
        outReport.AddClaim ((uint32_t) i, (uint16_t) i);
        outReport.SetAllocatedInFreeMap ((uint32_t) i, true);
    }

    outReport.Finish();

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Write
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Write (const FilePath & path, const FilePayload & payload, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (payload);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Delete (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Delete
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Delete (const FilePath & path, vector<Byte> & outBuffer, DeleteOutcome & outOutcome) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);
    UNREFERENCED_PARAMETER (outOutcome);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::SetStartupProgram
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::SetStartupProgram (const FilePath & path, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::Rename
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::Rename (const FilePath & from, const std::string & to, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (from);
    UNREFERENCED_PARAMETER (to);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TapeRecordingVolume::SetAttributes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT TapeRecordingVolume::SetAttributes (const FilePath & path, const FileAttributes & attributes, vector<Byte> & outBuffer) const
{
    UNREFERENCED_PARAMETER (path);
    UNREFERENCED_PARAMETER (attributes);
    UNREFERENCED_PARAMETER (outBuffer);

    return kReadOnly;
}
