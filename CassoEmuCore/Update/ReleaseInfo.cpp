#include "Pch.h"

#include "Update/ReleaseInfo.h"
#include "Core/JsonParser.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::Parse
//
//  Reads GitHub's "latest release" JSON. Missing or unparsable tag_name, a
//  document that is not JSON, and a prerelease are all ERROR_INVALID_DATA:
//  none of them is a release Casso may offer. Assets are optional; a release
//  without them parses, and FindZipAsset then finds nothing.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReleaseInfo::Parse (const std::string & json, ReleaseInfo & outInfo)
{
    static constexpr size_t    kDateLength = 10;   // YYYY-MM-DD
    HRESULT                    hr          = S_OK;
    JsonValue                  root;
    JsonParseError             parseError;
    ReleaseInfo                info;
    std::string                publishedAt;
    std::string                pageUrl;
    const JsonValue          * assetsArr   = nullptr;
    bool                       isObject    = false;
    ReleaseAsset               asset;



    outInfo = {};

    hr = JsonParser::Parse (json, root, parseError);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isObject = root.GetType() == JsonType::Object;
    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = root.GetString ("tag_name", info.tag);
    CHREx (hr, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = ReleaseVersion::Parse (info.tag, info.version);
    CHR (hr);

    info.isPrerelease = false;
    if (root.HasBool ("prerelease", info.isPrerelease))
    {
        CBREx (!info.isPrerelease, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));
    }

    if (root.HasString ("published_at", publishedAt))
    {
        info.publishedDate = publishedAt.substr (0, kDateLength);
    }

    if (root.HasString ("html_url", pageUrl))
    {
        info.pageUrl = TextEncoding::Utf8ToWide (pageUrl);
    }

    if (root.HasArray ("assets", assetsArr))
    {
        for (size_t i = 0; i < assetsArr->GetArraySize(); i++)
        {
            hr = ParseAsset (assetsArr->GetArrayElement (i), asset);

            if (FAILED (hr))
            {
                // One malformed asset costs that asset, not the release.
                hr = S_OK;
                continue;
            }

            info.assets.push_back (std::move (asset));
        }
    }

    outInfo = std::move (info);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::ParseAsset
//
//  One entry of `assets`. A name and a download URL are required; the size
//  and the digest are optional.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReleaseInfo::ParseAsset (const JsonValue & json, ReleaseAsset & outAsset)
{
    HRESULT      hr       = S_OK;
    std::string  url;
    std::string  digest;
    double       size     = 0;
    bool         isObject = false;



    outAsset = {};

    isObject = json.GetType() == JsonType::Object;
    CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    hr = json.GetString ("name", outAsset.name);
    CHR (hr);

    hr = json.GetString ("browser_download_url", url);
    CHR (hr);

    outAsset.downloadUrl = TextEncoding::Utf8ToWide (url);

    if (json.HasNumber ("size", size) && size > 0)
    {
        outAsset.sizeBytes = (std::uint64_t) size;
    }

    if (json.HasString ("digest", digest))
    {
        ParseDigest (digest, outAsset.sha256);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::ParseDigest
//
//  "sha256:<64 hex digits>" to 32 bytes. Any other form leaves the digest
//  empty, which the installer treats exactly like a missing one.
//
////////////////////////////////////////////////////////////////////////////////

void ReleaseInfo::ParseDigest (const std::string & digest, std::vector<Byte> & outSha256)
{
    static constexpr std::string_view  kPrefix      = "sha256:";
    static constexpr int  kNibbleShift = 4;
    std::string_view      hex;
    std::vector<Byte>     bytes;
    int                   high         = 0;
    int                   low          = 0;



    outSha256.clear();

    if (!digest.starts_with (kPrefix))
    {
        return;
    }

    hex = std::string_view (digest).substr (kPrefix.size());

    if (hex.size() != kSha256Bytes * 2)
    {
        return;
    }

    for (size_t i = 0; i < kSha256Bytes; i++)
    {
        high = GetHexDigitValue (hex[i * 2]);
        low  = GetHexDigitValue (hex[i * 2 + 1]);

        if (high < 0 || low < 0)
        {
            return;
        }

        bytes.push_back ((Byte) ((high << kNibbleShift) | low));
    }

    outSha256 = std::move (bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::GetHexDigitValue
//
//  0-15 for a hex digit of either case, -1 for anything else.
//
////////////////////////////////////////////////////////////////////////////////

int ReleaseInfo::GetHexDigitValue (char ch)
{
    static constexpr int  kTen  = 10;
    int                   value = -1;



    if (ch >= '0' && ch <= '9')
    {
        value = ch - '0';
    }
    else if (ch >= 'a' && ch <= 'f')
    {
        value = ch - 'a' + kTen;
    }
    else if (ch >= 'A' && ch <= 'F')
    {
        value = ch - 'A' + kTen;
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::GetZipAssetName
//
//  The release zip's file name as the release workflow writes it:
//  Casso-<version>-x64.zip or Casso-<version>-ARM64.zip.
//
////////////////////////////////////////////////////////////////////////////////

std::string ReleaseInfo::GetZipAssetName (const ReleaseVersion & version, ReleaseArch arch)
{
    const char  * pszArch = (arch == ReleaseArch::Arm64) ? "ARM64" : "x64";



    return std::format ("Casso-{}-{}.zip", version.ToString(), pszArch);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::GetBundleAssetName
//
////////////////////////////////////////////////////////////////////////////////

std::string ReleaseInfo::GetBundleAssetName (const ReleaseVersion & version)
{
    return std::format ("Casso-{}.msixbundle", version.ToString());
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::FindZipAsset
//
////////////////////////////////////////////////////////////////////////////////

const ReleaseAsset * ReleaseInfo::FindZipAsset (ReleaseArch arch) const
{
    return FindAsset (GetZipAssetName (version, arch));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::FindBundleAsset
//
////////////////////////////////////////////////////////////////////////////////

const ReleaseAsset * ReleaseInfo::FindBundleAsset() const
{
    return FindAsset (GetBundleAssetName (version));
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo::FindAsset
//
//  The asset with this file name, ignoring case; null when there is none.
//
////////////////////////////////////////////////////////////////////////////////

const ReleaseAsset * ReleaseInfo::FindAsset (const std::string & name) const
{
    const ReleaseAsset  * found = nullptr;



    for (const ReleaseAsset & asset : assets)
    {
        if (_stricmp (asset.name.c_str(), name.c_str()) == 0)
        {
            found = &asset;
            break;
        }
    }

    return found;
}
