#include "Pch.h"

#include "Update/ReleaseInfo.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReleaseInfoTests
//
//  GitHub's "latest release" document: the fields read, the asset lookup by
//  release-workflow file name, and the documents that must not parse.
//
////////////////////////////////////////////////////////////////////////////////

static const std::string s_kFullRelease = R"({
  "url": "https://api.github.com/repos/relmer/Casso/releases/1",
  "tag_name": "v1.31.0",
  "name": "Casso 1.31.0",
  "html_url": "https://github.com/relmer/Casso/releases/tag/v1.31.0",
  "draft": false,
  "prerelease": false,
  "published_at": "2026-10-20T18:04:11Z",
  "assets": [
    { "name": "Casso-1.31.0-x64.zip", "size": 1234567,
      "browser_download_url": "https://github.com/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0-x64.zip",
      "digest": "sha256:00112233445566778899aabbccddeeff00112233445566778899AABBCCDDEEFF" },
    { "name": "Casso-1.31.0-ARM64.zip", "size": 1200000,
      "browser_download_url": "https://github.com/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0-ARM64.zip" },
    { "name": "Casso-1.31.0.msixbundle", "size": 5000000,
      "browser_download_url": "https://github.com/relmer/Casso/releases/download/v1.31.0/Casso-1.31.0.msixbundle",
      "digest": "md5:abc" },
    { "size": 1 }
  ]
})";



TEST_CLASS (ReleaseInfoTests)
{
public:

    TEST_METHOD (Parse_ReadsEveryField)
    {
        ReleaseInfo           info;
        const ReleaseAsset  * zip = nullptr;
        HRESULT               hr  = S_OK;



        hr = ReleaseInfo::Parse (s_kFullRelease, info);
        AssertSucceeded (hr);

        Assert::IsTrue   (info.version == ReleaseVersion { 1, 31, 0 });
        Assert::AreEqual (std::string ("v1.31.0"),    info.tag);
        Assert::AreEqual (std::string ("2026-10-20"), info.publishedDate);
        Assert::AreEqual (std::wstring (L"https://github.com/relmer/Casso/releases/tag/v1.31.0"), info.pageUrl);
        Assert::IsFalse  (info.isPrerelease);
        Assert::AreEqual ((size_t) 3, info.assets.size(), L"the asset without a name is dropped, the rest kept");

        zip = info.FindZipAsset (ReleaseArch::X64);
        Assert::IsNotNull (zip);
        Assert::AreEqual ((std::uint64_t) 1234567, zip->sizeBytes);
        Assert::AreEqual ((size_t) 32, zip->sha256.size());
        Assert::AreEqual ((Byte) 0x00, zip->sha256[0]);
        Assert::AreEqual ((Byte) 0x11, zip->sha256[1]);
        Assert::AreEqual ((Byte) 0xFF, zip->sha256[31], L"upper-case hex digits parse");
        Assert::IsTrue   (zip->downloadUrl.ends_with (L"Casso-1.31.0-x64.zip"));
    }



    TEST_METHOD (FindAssets_ByWorkflowNames)
    {
        ReleaseInfo           info;
        const ReleaseAsset  * arm    = nullptr;
        const ReleaseAsset  * bundle = nullptr;
        HRESULT               hr     = S_OK;



        hr = ReleaseInfo::Parse (s_kFullRelease, info);
        AssertSucceeded (hr);

        arm    = info.FindZipAsset (ReleaseArch::Arm64);
        bundle = info.FindBundleAsset();

        Assert::IsNotNull (arm);
        Assert::AreEqual  (std::string ("Casso-1.31.0-ARM64.zip"), arm->name);
        Assert::IsTrue    (arm->sha256.empty(), L"no digest reported");

        Assert::IsNotNull (bundle);
        Assert::AreEqual  (std::string ("Casso-1.31.0.msixbundle"), bundle->name);
        Assert::IsTrue    (bundle->sha256.empty(), L"a digest that is not sha256 counts as missing");
    }



    TEST_METHOD (Parse_NoAssets_FindsNothing)
    {
        ReleaseInfo  info;
        HRESULT      hr   = S_OK;



        hr = ReleaseInfo::Parse (R"({ "tag_name": "1.31.0" })", info);
        AssertSucceeded (hr);

        Assert::IsTrue (info.assets.empty());
        Assert::IsNull (info.FindZipAsset (ReleaseArch::X64));
        Assert::IsNull (info.FindBundleAsset());
    }



    TEST_METHOD (Parse_RejectsMissingTagPrereleaseAndJunk)
    {
        static constexpr const char * kBad[] =
        {
            R"({ "html_url": "x" })",
            R"({ "tag_name": "nightly" })",
            R"({ "tag_name": 131 })",
            R"({ "tag_name": "v1.31.0", "prerelease": true })",
            R"([ "v1.31.0" ])",
            "<html>rate limited</html>",
            "",
        };

        ReleaseInfo  info;
        HRESULT      hr   = S_OK;



        for (const char * json : kBad)
        {
            hr = ReleaseInfo::Parse (json, info);
            Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), hr, std::wstring (json, json + strlen (json)).c_str());
            Assert::IsTrue   (info.tag.empty(), L"a rejected document leaves no partial release");
        }
    }



    TEST_METHOD (AssetNames_MatchReleaseWorkflow)
    {
        ReleaseVersion  version { 1, 31, 2 };



        Assert::AreEqual (std::string ("Casso-1.31.2-x64.zip"),    ReleaseInfo::GetZipAssetName (version, ReleaseArch::X64));
        Assert::AreEqual (std::string ("Casso-1.31.2-ARM64.zip"),  ReleaseInfo::GetZipAssetName (version, ReleaseArch::Arm64));
        Assert::AreEqual (std::string ("Casso-1.31.2.msixbundle"), ReleaseInfo::GetBundleAssetName (version));
    }
};
