#pragma once

#include "Pch.h"

#include "Core/JsonValue.h"
#include "Update/ReleaseVersion.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseArch
//
//  The processor a release zip is built for.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReleaseArch
{
    X64,
    Arm64,
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseAsset
//
//  One downloadable file of a release. `sha256` is the 32-byte digest GitHub
//  reports, empty when it reported none.
//
////////////////////////////////////////////////////////////////////////////////

struct ReleaseAsset
{
    std::string         name;
    std::uint64_t       sizeBytes = 0;
    std::wstring        downloadUrl;
    std::vector<Byte>   sha256;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfo
//
//  The fields Casso reads from GitHub's "latest release" JSON.
//
////////////////////////////////////////////////////////////////////////////////

struct ReleaseInfo
{
    static constexpr size_t  kSha256Bytes = 32;

    ReleaseVersion              version;
    std::string                 tag;
    std::string                 publishedDate;
    std::wstring                pageUrl;
    bool                        isPrerelease = false;
    std::vector<ReleaseAsset>   assets;

    static HRESULT        Parse              (const std::string & json, ReleaseInfo & outInfo);

    const ReleaseAsset  * FindZipAsset       (ReleaseArch arch) const;
    const ReleaseAsset  * FindBundleAsset    () const;

    static std::string    GetZipAssetName    (const ReleaseVersion & version, ReleaseArch arch);
    static std::string    GetBundleAssetName (const ReleaseVersion & version);

private:
    static HRESULT        ParseAsset         (const JsonValue & json, ReleaseAsset & outAsset);
    static void           ParseDigest        (const std::string & digest, std::vector<Byte> & outSha256);
    static int            GetHexDigitValue   (char ch);

    const ReleaseAsset  * FindAsset          (const std::string & name) const;
};
