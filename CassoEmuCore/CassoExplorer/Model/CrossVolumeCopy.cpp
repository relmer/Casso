#include "Pch.h"

#include "CassoExplorer/Model/CrossVolumeCopy.h"
#include "AppleTextCodec.h"
#include "CassoExplorer/Model/HostFileNaming.h"
#include "CassoExplorer/Model/PreviewDecoder.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/CpmVolume.h"
#include "Machines/Apple2/Common/Dos33Volume.h"
#include "Machines/Apple2/Common/PascalVolume.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::EndsWith
//
////////////////////////////////////////////////////////////////////////////////

bool CrossVolumeCopy::EndsWith (const std::string & name, const char * suffix)
{
    size_t  length = strlen (suffix);



    return name.size() > length && _stricmp (name.c_str() + (name.size() - length), suffix) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::IsText
//
////////////////////////////////////////////////////////////////////////////////

bool CrossVolumeCopy::IsText (VolumeKind kind, const std::string & name, const FilePayload & payload)
{
    bool  isText = false;



    switch (kind)
    {
        case VolumeKind::Pascal: isText = payload.type == PascalVolume::kKindText;  break;
        case VolumeKind::Dos33:  isText = payload.type == Dos33Volume::kTypeText;   break;
        case VolumeKind::ProDos: isText = payload.type == ProDosVolume::kTypeText;  break;
        case VolumeKind::Cpm:    isText = EndsWith (name, ".TXT") || PreviewDecoder::DecideCpm (payload) == PreviewContent::Kind::Text; break;
        default:                 break;
    }

    return isText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::DecodeToHostText
//
//  Seven-bit text with its line ends, and none of the padding, control
//  characters or leftover high bits of the file system it came from.
//
////////////////////////////////////////////////////////////////////////////////

void CrossVolumeCopy::DecodeToHostText (VolumeKind kind, const FilePayload & payload, std::string & outText)
{
    std::string  decoded;



    if (kind == VolumeKind::Pascal)
    {
        PascalVolume::DecodeText (payload.bytes, decoded);
    }
    else if (kind == VolumeKind::Cpm)
    {
        CpmVolume::DecodeText (payload.bytes, decoded);
    }
    else
    {
        AppleTextCodec::Decode (payload.bytes, AppleTextConvention::HighAscii, decoded);
    }

    outText.clear();

    for (char raw : decoded)
    {
        char  ch = (char) (raw & 0x7F);

        if (ch == '\r' || ch == '\n' || ch == '\t' || (ch >= ' ' && ch < 0x7F))
        {
            outText.push_back (ch);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::TryEncodeText
//
////////////////////////////////////////////////////////////////////////////////

bool CrossVolumeCopy::TryEncodeText (const std::string & text, VolumeKind target, FilePayload & inOutPayload)
{
    HRESULT       hr        = S_OK;
    size_t        badOffset = 0;
    vector<Byte>  encoded;



    switch (target)
    {
        case VolumeKind::Pascal:
            hr = PascalVolume::EncodeText (text, encoded, badOffset);
            inOutPayload.type     = PascalVolume::kKindText;
            inOutPayload.textForm = PayloadTextForm::PascalPages;
            break;

        case VolumeKind::Cpm:
            CpmVolume::EncodeText (text, encoded);
            inOutPayload.type     = 0;
            inOutPayload.textForm = PayloadTextForm::CpmRecords;
            break;

        case VolumeKind::Dos33:
        case VolumeKind::ProDos:
            hr = AppleTextCodec::Encode (text, AppleTextConvention::HighAscii, encoded, badOffset);
            inOutPayload.type     = (target == VolumeKind::Dos33) ? Dos33Volume::kTypeText : ProDosVolume::kTypeText;
            inOutPayload.textForm = PayloadTextForm::AppleHighAscii;
            break;

        default:
            hr = E_INVALIDARG;
            break;
    }

    if (SUCCEEDED (hr))
    {
        inOutPayload.bytes          = std::move (encoded);
        inOutPayload.hasLoadAddress = false;
        inOutPayload.hasAuxType     = false;
    }

    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::ConvertBinaryType
//
//  An Apple Pascal picture is a hi-res screen, so it does have an address.
//
////////////////////////////////////////////////////////////////////////////////

void CrossVolumeCopy::ConvertBinaryType (VolumeKind source, VolumeKind target, FilePayload & inOutPayload)
{
    constexpr Word  kHiResPage = 0x2000;
    bool            isFlat     = source == VolumeKind::Pascal || source == VolumeKind::Cpm;
    bool            isPicture  = source == VolumeKind::Pascal && inOutPayload.type == PascalVolume::kKindFoto;



    HostFileNaming::ConvertPayloadType (source, target, inOutPayload);

    if (isFlat && target == VolumeKind::Dos33)
    {
        inOutPayload.type           = isPicture ? Dos33Volume::kTypeBinary : Dos33Volume::kTypeSpecial;
        inOutPayload.hasLoadAddress = isPicture;
        inOutPayload.loadAddress    = isPicture ? kHiResPage : (Word) 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::Convert
//
////////////////////////////////////////////////////////////////////////////////

void CrossVolumeCopy::Convert (VolumeKind source, VolumeKind target, const std::string & name, FilePayload & inOutPayload, std::wstring & outNote)
{
    bool         storesNoAddress = target == VolumeKind::Pascal || target == VolumeKind::Cpm;
    bool         lostAddress     = storesNoAddress && inOutPayload.hasLoadAddress;
    bool         storesForks     = target == VolumeKind::ProDos;
    bool         lostFork        = !storesForks && inOutPayload.hasResourceFork;
    Word         address         = inOutPayload.loadAddress;
    std::string  text;



    outNote.clear();

    //  Only ProDOS has a resource fork. Elsewhere the data fork is written
    //  alone, and the note says so.
    if (lostFork)
    {
        inOutPayload.resourceFork.clear();
        inOutPayload.finderInfo.clear();
        inOutPayload.hasResourceFork = false;

        outNote = std::format (L"{}: {} has no resource fork, so only the data fork was copied.",
                               TextEncoding::NarrowToWide (name),
                               TextEncoding::NarrowToWide (VolumeImage::GetSystemName (target)));
    }

    if (source == target)
    {
        return;
    }

    if (IsText (source, name, inOutPayload))
    {
        DecodeToHostText (source, inOutPayload, text);

        if (TryEncodeText (text, target, inOutPayload))
        {
            return;
        }
    }

    ConvertBinaryType (source, target, inOutPayload);

    if (lostAddress)
    {
        outNote += std::format (L"{}: {} has no load address, so ${:04X} is not on the copy.",
                               TextEncoding::NarrowToWide (name),
                               TextEncoding::NarrowToWide (VolumeImage::GetSystemName (target)),
                               address);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CrossVolumeCopy::FitTextName
//
////////////////////////////////////////////////////////////////////////////////

std::string CrossVolumeCopy::FitTextName (const std::string & name, VolumeKind target)
{
    constexpr size_t  kPascalNameMax = 15;
    std::string       stem           = name;



    if (target == VolumeKind::Pascal && !EndsWith (name, ".TEXT"))
    {
        stem = EndsWith (name, ".TXT") ? name.substr (0, name.size() - strlen (".TXT")) : name;

        return (stem.size() + strlen (".TEXT") <= kPascalNameMax) ? stem + ".TEXT" : name;
    }

    if (target == VolumeKind::Cpm && EndsWith (name, ".TEX"))
    {
        return name.substr (0, name.size() - strlen (".TEX")) + ".TXT";
    }

    return name;
}
