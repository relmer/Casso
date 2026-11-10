#include "Pch.h"

#include "CassoExplorer/Model/CopyOutConversion.h"
#include "Core/TextEncoding.h"
#include "FileTypes/TextCharset.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOutConversion::Find
//
////////////////////////////////////////////////////////////////////////////////

FileFormat CopyOutConversion::Find (
    DiskOperations     & operations,
    const std::string  & image,
    const std::string  & catalogPath,
    const FileEntry    & entry,
    VolumeKind           kind)
{
    FilePayload             payload;
    DiskOperations::Result  result;
    FileFormat              format = FileFormat::None;



    if (!entry.isDirectory)
    {
        result = operations.Read (image, catalogPath, payload, entry.catalogIndex);

        if (result.Succeeded())
        {
            format = FileFormatTable::Identify (entry, kind, payload.bytes);
        }
    }

    return format;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOutConversion::Applies
//
////////////////////////////////////////////////////////////////////////////////

bool CopyOutConversion::Applies (FileFormat format, DiskOperations::Encoding encoding, const DecodeOptions & options)
{
    bool  inCharset = encoding == DiskOperations::Encoding::Text && TextCharset::Resolve (options.charset, false) != Charset::AppleText;



    return format != FileFormat::None || inCharset;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CopyOutConversion::Get
//
//  Text whose bytes aren't Apple text on the volume, as on a Pascal volume,
//  has no character set choice and is read by DiskOperations::Get.
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result CopyOutConversion::Get (
    DiskOperations            & operations,
    const DecodeOptions       & options,
    const std::string         & image,
    const std::string         & catalogPath,
    const FileEntry           & entry,
    VolumeKind                  kind,
    FileFormat                  format,
    DiskOperations::Encoding    encoding,
    std::wstring              & outDecodeFailure)
{
    DiskOperations::Result  result;
    FilePayload             payload;
    DecodedFile             decoded;
    FileFacts               facts   = FileFacts::From (entry, kind);
    std::wstring            error;
    bool                    decodes = Applies (format, encoding, options);
    HRESULT                 hr      = S_OK;



    outDecodeFailure.clear();

    if (decodes)
    {
        result  = operations.Read (image, catalogPath, payload, entry.catalogIndex);
        decodes = format != FileFormat::None || payload.textForm == PayloadTextForm::AppleHighAscii;
    }

    if (!decodes)
    {
        result = operations.Get (image, catalogPath, encoding, "", entry.catalogIndex);
    }
    else if (result.Succeeded())
    {
        if (format == FileFormat::None)
        {
            decoded.lines = TextCharset::DecodeLines (payload.bytes, TextCharset::Resolve (options.charset, false));
        }
        else
        {
            facts.resourceFork = payload.resourceFork;
            hr                 = FileFormatTable::Decode (format, payload.bytes, facts, options, decoded, error);
        }

        if (SUCCEEDED (hr))
        {
            hr = FileFormatTable::Export (decoded, result.payload);
        }

        if (FAILED (hr))
        {
            outDecodeFailure = TextEncoding::NarrowToWide (std::format ("Unable to copy {}: ", entry.name)) + (error.empty() ? std::wstring (L"it could not be decoded") : error);
            result.hr        = hr;
            result.message   = TextEncoding::WideToNarrow (outDecodeFailure);
            result.payload.clear();
        }
    }

    return result;
}




////////////////////////////////////////////////////////////////////////////////
//
//  CopyOutConversion::GetToFile
//
////////////////////////////////////////////////////////////////////////////////

DiskOperations::Result CopyOutConversion::GetToFile (
    DiskOperations            & operations,
    IFileSystem               & fs,
    const DecodeOptions       & options,
    const std::string         & image,
    const std::string         & catalogPath,
    const FileEntry           & entry,
    VolumeKind                  kind,
    FileFormat                  format,
    DiskOperations::Encoding    encoding,
    const std::wstring        & hostPath)
{
    DiskOperations::Result  result;
    std::wstring            failure;
    HRESULT                 hr = S_OK;



    if (!Applies (format, encoding, options))
    {
        return operations.Get (image, catalogPath, encoding, TextEncoding::WideToNarrow (hostPath), entry.catalogIndex);
    }

    result = Get (operations, options, image, catalogPath, entry, kind, format, encoding, failure);

    if (result.Succeeded())
    {
        hr = fs.WriteAllText (hostPath, std::string (result.payload.begin(), result.payload.end()));

        if (FAILED (hr))
        {
            result.hr      = hr;
            result.message = std::format ("{} could not be written.", TextEncoding::WideToNarrow (hostPath.substr (hostPath.find_last_of (L"\\/") + 1)));
        }
    }

    return result;
}
