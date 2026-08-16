#include "Pch.h"

#include "MerlinFixture.h"



//  DOS 3.3 binary header: load address then length, both little-endian, ahead of
//  the payload proper.
static const size_t  s_kBinHeaderSize = 4;

//  Merlin terminates a source line with a carriage return, not a newline. The
//  high bit is already masked off by the time this is compared.
static const Byte    s_kSourceLineTerminator = 0x0D;





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture::StripHeader
//
//  Splits a raw DOS 3.3 file into its load address and its payload.
//
//  The declared length is VERIFIED against what was actually read rather than
//  merely skipped past. A fixture that was truncated, padded to a sector
//  boundary, or extracted with the wrong length still decodes into plausible
//  bytes, and plausible-but-wrong is the whole failure mode this corpus is built
//  to catch. Every committed fixture carries a declared length matching its
//  payload exactly, so the strict form costs nothing and turns a silent
//  extraction bug into a named failure.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinFixture::StripHeader (
    const std::vector<Byte>    &  raw,
    MerlinFixtureFile          &  outFile)
{
    HRESULT   hr              = S_OK;
    size_t    declaredLength  = 0;
    size_t    actualLength    = 0;
    bool      hasHeader       = false;
    bool      lengthAgrees    = false;

    outFile.loadAddress = 0;
    outFile.payload.clear();

    hasHeader = (raw.size() >= s_kBinHeaderSize);
    CBRAEx (hasHeader, E_INVALIDARG);

    outFile.loadAddress = static_cast<Word> (raw[0] | (raw[1] << 8));

    declaredLength = static_cast<size_t> (raw[2] | (raw[3] << 8));
    actualLength   = raw.size() - s_kBinHeaderSize;

    lengthAgrees = (declaredLength == actualLength);
    CBRAEx (lengthAgrees, E_UNEXPECTED);

    outFile.payload.assign (raw.begin() + s_kBinHeaderSize, raw.end());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture::LoadObject
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinFixture::LoadObject (
    IFixtureProvider           &  provider,
    const std::string          &  relativePath,
    MerlinFixtureFile          &  outFile)
{
    HRESULT              hr   = S_OK;
    std::vector<Byte>    raw;

    hr = provider.OpenFixture (relativePath, raw);
    CHR (hr);

    hr = StripHeader (raw, outFile);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture::LoadSource
//
//  Decodes stored Merlin source into ordinary text.
//
//  Every byte is masked to seven bits and every carriage return becomes a
//  newline. Nothing else is done to it: no trailing-blank trimming, no tab
//  expansion, no dropping of a final terminator. The corpus compares what Merlin
//  assembled, so a decoder that tidied its input would be testing the tidied
//  text rather than the vendor's -- and the field model this dialect turns on is
//  made of exactly the whitespace such a step would remove.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinFixture::LoadSource (
    IFixtureProvider           &  provider,
    const std::string          &  relativePath,
    std::string                &  outText)
{
    HRESULT              hr    = S_OK;
    MerlinFixtureFile    file;

    outText.clear();

    hr = LoadObject (provider, relativePath, file);
    CHR (hr);

    DecodeText (file.payload, outText);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture::LoadTextSource
//
//  A DOS 3.3 type-T file, which carries no header at all.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinFixture::LoadTextSource (
    IFixtureProvider           &  provider,
    const std::string          &  relativePath,
    std::string                &  outText)
{
    HRESULT              hr   = S_OK;
    std::vector<Byte>    raw;

    outText.clear();

    hr = provider.OpenFixture (relativePath, raw);
    CHR (hr);

    DecodeText (raw, outText);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MerlinFixture::DecodeText
//
//  The text decoding both source paths share, so the type-B and type-T forms
//  cannot drift into decoding the same characters differently.
//
////////////////////////////////////////////////////////////////////////////////

void MerlinFixture::DecodeText (
    const std::vector<Byte>    &  payload,
    std::string                &  outText)
{
    Byte  masked = 0;

    outText.reserve (payload.size());

    for (size_t i = 0; i < payload.size(); i++)
    {
        masked = static_cast<Byte> (payload[i] & 0x7F);

        if (masked == s_kSourceLineTerminator)
        {
            outText.push_back ('\n');
        }
        else
        {
            outText.push_back (static_cast<char> (masked));
        }
    }
}
