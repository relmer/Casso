#include "Pch.h"

#include "CassoExplorer/Model/BasicConversion.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "AppleTextCodec.h"
#include "ApplesoftTokenizer.h"
#include "IntegerBasicDetokenizer.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BasicConversion::GetKind
//
////////////////////////////////////////////////////////////////////////////////

BasicConversion::Kind BasicConversion::GetKind (VolumeKind volume, Byte type)
{
    bool  proDos = volume == VolumeKind::ProDos;



    if (type == (proDos ? kProDosApplesoft : kDos33Applesoft) || type == (proDos ? kProDosInteger : kDos33Integer))
    {
        return Kind::ToListing;
    }

    if (type == (proDos ? kProDosText : kDos33Text))
    {
        return Kind::ToApplesoft;
    }

    return Kind::None;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicConversion::Convert
//
//  Text goes on the disk as high-bit ASCII with carriage returns, the form
//  text files on both file systems have.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BasicConversion::Convert (const FilePayload & source, VolumeKind volume, Kind kind, FilePayload & outPayload, std::wstring & outError)
{
    HRESULT                   hr        = S_OK;
    bool                      proDos    = volume == VolumeKind::ProDos;
    bool                      integer   = source.type == (proDos ? kProDosInteger : kDos33Integer);
    std::string               text;
    size_t                    badOffset = 0;
    ApplesoftListingError     listingError;
    IntegerBasicListingError  integerError;



    outPayload = FilePayload();
    outError.clear();

    CBRAEx (kind != Kind::None, E_INVALIDARG);

    if (kind == Kind::ToListing)
    {
        hr = integer ? IntegerBasicDetokenizer::Detokenize (source.bytes, text, integerError)
                     : ApplesoftTokenizer::Detokenize (source.bytes, text, listingError);

        if (FAILED (hr))
        {
            outError = std::format (L"It can't be read as {} BASIC program: {}.", integer ? L"an Integer" : L"an Applesoft",
                                    TextEncoding::NarrowToWide (integer ? integerError.reason : listingError.reason));
        }

        CHR (hr);

        hr = AppleTextCodec::Encode (text, AppleTextConvention::HighAscii, outPayload.bytes, badOffset);
        CHR (hr);

        outPayload.type = proDos ? kProDosText : kDos33Text;
    }
    else
    {
        AppleTextCodec::Decode (source.bytes, AppleTextConvention::HighAscii, text);

        hr = ApplesoftTokenizer::Tokenize (text, outPayload.bytes, listingError);

        if (FAILED (hr))
        {
            outError = std::format (L"It isn't an Applesoft BASIC listing: line {}, {}.",
                                    listingError.hasLineNumber ? (size_t) listingError.lineNumber : listingError.sourceLineIndex,
                                    TextEncoding::NarrowToWide (listingError.reason));
        }

        CHR (hr);

        outPayload.type       = proDos ? kProDosApplesoft : kDos33Applesoft;
        outPayload.auxType    = ApplesoftTokenizer::kProgramBase;
        outPayload.hasAuxType = true;
    }

    outPayload.encoding = PayloadEncoding::Verbatim;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BasicConversion::MakeName
//
////////////////////////////////////////////////////////////////////////////////

std::string BasicConversion::MakeName (const std::string & original, VolumeKind volume, Kind kind, const std::vector<std::string> & taken)
{
    bool         proDos    = volume == VolumeKind::ProDos;
    size_t       maxLength = proDos ? CassoExplorerActions::kProDosNameMax : CassoExplorerActions::kDos33NameMax;
    std::string  suffix    = (kind == Kind::ToListing) ? ".LIST" : ".BAS";
    std::string  base      = original.substr (0, (std::min) (original.size(), maxLength - suffix.size()));



    return CassoExplorerActions::MakeFreeEntryName (base + suffix, taken, volume, false);
}
