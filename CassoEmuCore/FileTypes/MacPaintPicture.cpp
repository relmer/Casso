#include "Pch.h"

#include "FileTypes/MacPaintPicture.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool MacPaintPicture::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    std::span<const Byte>  document;
    std::vector<Byte>      dots;
    bool                   byName   = facts.IsNameEnding (".PNTG") || facts.IsNameEnding (".MAC");



    if (!byName && !IsMacBinary (bytes))
    {
        return false;
    }

    return GetDocument (bytes, document) && Unpack (document, dots);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MacPaintPicture::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                hr = S_OK;
    std::span<const Byte>  document;
    std::vector<Byte>      dots;



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    CBREx (GetDocument (bytes, document) && Unpack (document, dots), HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFile.kind   = DecodedFile::Kind::Picture;
    outFile.width  = kWidth;
    outFile.height = kHeight;
    outFile.bgra.assign ((size_t) kWidth * kHeight, 0xFFFFFFFFu);

    for (size_t dot = 0; dot < outFile.bgra.size(); dot++)
    {
        if ((dots[dot / 8] >> (7 - (dot % 8))) & 1)
        {
            outFile.bgra[dot] = 0xFF000000u;
        }
    }

Error:
    if (FAILED (hr))
    {
        outError = L"The MacPaint picture doesn't unpack.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture::IsMacBinary
//
//  A MacBinary header's first byte is 0, and its Finder type is at 65.
//
////////////////////////////////////////////////////////////////////////////////

bool MacPaintPicture::IsMacBinary (std::span<const Byte> bytes)
{
    static constexpr Byte  kType[] = { 'P', 'N', 'T', 'G' };



    return bytes.size() > kMacBinaryBytes && bytes[0] == 0
        && std::equal (std::begin (kType), std::end (kType), bytes.begin() + kMacBinaryTypeAt);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture::GetDocument
//
//  The document after its MacBinary header, at the data fork's length, and
//  then only when it is longer than its header and has a version of 0 or 2.
//
////////////////////////////////////////////////////////////////////////////////

bool MacPaintPicture::GetDocument (std::span<const Byte> bytes, std::span<const Byte> & outDocument)
{
    size_t  length = 0;



    outDocument = bytes;

    if (IsMacBinary (bytes))
    {
        length = ((size_t) bytes[kMacBinaryLengthAt]     << 24) | ((size_t) bytes[kMacBinaryLengthAt + 1] << 16)
               | ((size_t) bytes[kMacBinaryLengthAt + 2] <<  8) |  (size_t) bytes[kMacBinaryLengthAt + 3];

        outDocument = bytes.subspan (kMacBinaryBytes, (std::min) (length, bytes.size() - kMacBinaryBytes));
    }

    return outDocument.size() > kHeaderBytes
        && outDocument[0] == 0 && outDocument[1] == 0 && outDocument[2] == 0
        && (outDocument[3] == 0 || outDocument[3] == 2);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MacPaintPicture::Unpack
//
//  The rows are unpacked as one run of bytes, since no run crosses the end
//  of a row; the document is whole when all 720 rows are out before its
//  bytes end.
//
////////////////////////////////////////////////////////////////////////////////

bool MacPaintPicture::Unpack (std::span<const Byte> document, std::vector<Byte> & outDots)
{
    size_t  total = kRowBytes * kHeight;
    size_t  at    = kHeaderBytes;
    int     flag  = 0;
    size_t  count = 0;



    outDots.clear();
    outDots.reserve (total);

    while (outDots.size() < total && at < document.size())
    {
        flag = (int) (int8_t) document[at++];

        if (flag == -128)
        {
            continue;
        }

        if (flag >= 0)
        {
            count = (size_t) flag + 1;

            if (at + count > document.size() || outDots.size() + count > total)
            {
                return false;
            }

            outDots.insert (outDots.end(), document.begin() + (ptrdiff_t) at, document.begin() + (ptrdiff_t) (at + count));
            at += count;
        }
        else
        {
            count = (size_t) (1 - flag);

            if (at >= document.size() || outDots.size() + count > total)
            {
                return false;
            }

            outDots.insert (outDots.end(), count, document[at++]);
        }
    }

    return outDots.size() == total;
}
