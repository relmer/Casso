#include "Pch.h"

#include "FileTypes/FileFormatTable.h"
#include "FileTypes/CsvWriter.h"
#include "FileTypes/RtfWriter.h"
#include "FileTypes/FileFormatRows.h"
#include "CassoExplorer/Model/PicturePreview.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::From
//
////////////////////////////////////////////////////////////////////////////////

FileFacts FileFacts::From (const FileEntry & entry, VolumeKind volume)
{
    FileFacts  facts;



    facts.volume         = volume;
    facts.type           = entry.type;
    facts.auxType        = entry.auxType;
    facts.hasAuxType     = entry.hasAuxType;
    facts.loadAddress    = entry.loadAddress;
    facts.hasLoadAddress = entry.hasLoadAddress;
    facts.name           = entry.name;

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::IsProDos
//
//  An archive's entries have ProDOS types.
//
////////////////////////////////////////////////////////////////////////////////

bool FileFacts::IsProDos() const
{
    return volume == VolumeKind::ProDos || volume == VolumeKind::Archive;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::IsDos33
//
////////////////////////////////////////////////////////////////////////////////

bool FileFacts::IsDos33() const
{
    return volume == VolumeKind::Dos33;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::IsPascal
//
////////////////////////////////////////////////////////////////////////////////

bool FileFacts::IsPascal() const
{
    return volume == VolumeKind::Pascal;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::IsNameEnding
//
////////////////////////////////////////////////////////////////////////////////

bool FileFacts::IsNameEnding (std::string_view suffix) const
{
    return name.size() >= suffix.size() && _strnicmp (name.c_str() + name.size() - suffix.size(), suffix.data(), suffix.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFacts::IsNameStarting
//
////////////////////////////////////////////////////////////////////////////////

bool FileFacts::IsNameStarting (std::string_view prefix) const
{
    return name.size() >= prefix.size() && _strnicmp (name.c_str(), prefix.data(), prefix.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::Identify
//
////////////////////////////////////////////////////////////////////////////////

FileFormat FileFormatTable::Identify (const FileEntry & entry, VolumeKind volume, std::span<const Byte> bytes)
{
    FileFacts  facts = FileFacts::From (entry, volume);



    if (entry.isDirectory)
    {
        return FileFormat::None;
    }

    for (const Row & row : GetRows())
    {
        if (row.matches != nullptr && row.matches (facts, bytes))
        {
            return row.format;
        }
    }

    return FileFormat::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::Decode
//
//  The charset is resolved for the format before the decoder is called.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileFormatTable::Decode (FileFormat format, std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT        hr       = S_OK;
    const Row    * row      = FindRow (format);
    DecodeOptions  resolved = options;



    outFile = {};
    outError.clear();

    CBREx (row != nullptr && row->decode != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    if (resolved.charset == Charset::Default)
    {
        resolved.charset = row->iigs ? Charset::MacOsRoman : Charset::AppleText;
    }

    hr = row->decode (bytes, facts, resolved, outFile, outError);
    CHR (hr);

    outFile.format  = format;
    outFile.charset = HasCharsetChoice (format) ? resolved.charset : Charset::Default;

Error:
    if (FAILED (hr))
    {
        outFile = {};
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::Export
//
//  Lines as UTF-8 text with a byte order mark and CR LF line ends, a
//  document as RTF, a grid as CSV and a picture as PNG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileFormatTable::Export (const DecodedFile & file, std::vector<Byte> & outBytes)
{
    HRESULT      hr = S_OK;
    std::string  text;



    outBytes.clear();

    switch (file.kind)
    {
        case DecodedFile::Kind::Document:
            RtfWriter::Write (file.paragraphs, outBytes);
            break;

        case DecodedFile::Kind::Grid:
            CsvWriter::Write (file.grid, outBytes);
            break;

        case DecodedFile::Kind::Picture:
            hr = PicturePreview::EncodePng (file.bgra, file.width, file.height, outBytes);
            CHR (hr);
            break;

        default:
            text = "\xEF\xBB\xBF";

            for (const std::wstring & line : file.lines)
            {
                text += TextEncoding::WideToUtf8 (line) + "\r\n";
            }

            outBytes.assign (text.begin(), text.end());
            break;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::Import
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileFormatTable::Import (FileFormat format, VolumeKind volume, std::span<const Byte> hostBytes, FilePayload & outPayload, std::wstring & outError)
{
    HRESULT      hr  = S_OK;
    const Row  * row = FindRow (format);



    outError.clear();

    CBREx (row != nullptr && row->import != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED));

    hr = row->import (hostBytes, volume, outPayload, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::GetSuffix
//
////////////////////////////////////////////////////////////////////////////////

const wchar_t * FileFormatTable::GetSuffix (FileFormat format)
{
    const Row  * row = FindRow (format);



    return (row != nullptr) ? row->suffix : L"";
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::GetImportBySuffix
//
////////////////////////////////////////////////////////////////////////////////

FileFormat FileFormatTable::GetImportBySuffix (const std::wstring & hostName)
{
    for (const Row & row : GetRows())
    {
        size_t  length = wcslen (row.suffix);

        if (row.import != nullptr && length > 0 && hostName.size() > length
            && _wcsicmp (hostName.c_str() + hostName.size() - length, row.suffix) == 0)
        {
            return row.format;
        }
    }

    return FileFormat::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::IsIigsFormat
//
////////////////////////////////////////////////////////////////////////////////

bool FileFormatTable::IsIigsFormat (FileFormat format)
{
    const Row  * row = FindRow (format);



    return row != nullptr && row->iigs;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::HasCharsetChoice
//
////////////////////////////////////////////////////////////////////////////////

bool FileFormatTable::HasCharsetChoice (FileFormat format)
{
    switch (format)
    {
        case FileFormat::PascalText:
        case FileFormat::RandomText:
        case FileFormat::Teach:
        case FileFormat::AppleWorksGsWp:
            return true;

        default:
            return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::GetRows
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<FileFormatTable::Row> & FileFormatTable::GetRows()
{
    static const std::vector<Row>  s_rows = FileFormatRows::Make();



    return s_rows;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileFormatTable::FindRow
//
////////////////////////////////////////////////////////////////////////////////

const FileFormatTable::Row * FileFormatTable::FindRow (FileFormat format)
{
    for (const Row & row : GetRows())
    {
        if (row.format == format)
        {
            return &row;
        }
    }

    return nullptr;
}
