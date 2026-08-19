#include "Pch.h"

#include "MerlinFixture.h"



//  DOS 3.3 binary header: load address then length, both little-endian, ahead of
//  the payload proper. Objects keep theirs; sources were committed without one.
static const size_t  s_kBinHeaderSize = 4;





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
//  to catch. Every committed object carries a declared length matching its
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
//  A committed source is already ordinary text, so this is a read and nothing
//  else. It stays a named entry point rather than becoming an inline file read
//  at each call site, because the two halves of the corpus are stored
//  DIFFERENTLY -- objects verbatim with a header, sources transcoded without
//  one -- and which applies is a property of the fixture, not a decision for
//  whoever is writing a test.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MerlinFixture::LoadSource (
    IFixtureProvider           &  provider,
    const std::string          &  relativePath,
    std::string                &  outText)
{
    HRESULT              hr   = S_OK;
    std::vector<Byte>    raw;

    outText.clear();

    hr = provider.OpenFixture (relativePath, raw);
    CHR (hr);

    outText.assign (raw.begin(), raw.end());

Error:
    return hr;
}
