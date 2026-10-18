#include "Pch.h"

#include "Update/LocalFeedHttpClient.h"
#include "Update/UpdateService.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeFeedFileReader
//
//  Files held in memory by path; any other path is not found.
//
////////////////////////////////////////////////////////////////////////////////

class FakeFeedFileReader : public IFeedFileReader
{
public:
    std::map<std::wstring, std::string>  files;
    std::vector<std::wstring>            reads;

    HRESULT ReadAllBytes (const std::wstring & path, std::vector<Byte> & outBytes) override
    {
        HRESULT  hr      = S_OK;
        auto     it      = files.find (path);
        bool     isFound = it != files.end();



        reads.push_back (path);
        outBytes.clear();

        CBREx (isFound, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));
        outBytes.assign (it->second.begin(), it->second.end());

    Error:
        return hr;
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClientTests
//
//  How a local update feed is chosen, and which file each request reads.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LocalFeedHttpClientTests)
{
public:

    static HttpRequest MakeRequest (LPCWSTR host, const std::wstring & path)
    {
        HttpRequest  request;



        request.host = host;
        request.path = path;

        return request;
    }



    TEST_METHOD (SelectFeedPath_OnlyWhenSetAndNotBlank)
    {
        Assert::IsFalse  (LocalFeedHttpClient::SelectFeedPath (nullptr).has_value(), L"unset: GitHub as usual");
        Assert::IsFalse  (LocalFeedHttpClient::SelectFeedPath (L"").has_value());
        Assert::IsFalse  (LocalFeedHttpClient::SelectFeedPath (L"  \"\" ").has_value(), L"blank");
        Assert::AreEqual (std::wstring (L"C:\\feed\\release.json"), *LocalFeedHttpClient::SelectFeedPath (L"\"C:\\feed\\release.json\""),
                          L"the quotes a shell leaves are dropped");
    }



    TEST_METHOD (TryResolve_MapsEachRequestToAFileBesideTheFeed)
    {
        std::wstring  feed     = L"C:\\feed\\release.json";
        std::wstring  file;
        bool          isFeed   = false;



        Assert::IsTrue   (LocalFeedHttpClient::TryResolve (MakeRequest (UpdateService::kpszApiHost, UpdateService::kpszLatestPath), feed, file, isFeed));
        Assert::AreEqual (feed, file);
        Assert::IsTrue   (isFeed, L"the release request reads the feed");

        Assert::IsTrue   (LocalFeedHttpClient::TryResolve (MakeRequest (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.91/CHANGELOG.md"), feed, file, isFeed));
        Assert::AreEqual (std::wstring (L"C:\\feed\\CHANGELOG.md"), file);
        Assert::IsFalse  (isFeed);

        Assert::IsTrue   (LocalFeedHttpClient::TryResolve (MakeRequest (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.91/docs/shot.png"), feed, file, isFeed));
        Assert::AreEqual (std::wstring (L"C:\\feed\\docs\\shot.png"), file, L"a relative image, under the feed's folder");

        Assert::IsTrue   (LocalFeedHttpClient::TryResolve (MakeRequest (LocalFeedHttpClient::kpszFeedHost, L"/Casso-1.31.91-x64.zip"), feed, file, isFeed));
        Assert::AreEqual (std::wstring (L"C:\\feed\\Casso-1.31.91-x64.zip"), file);

        Assert::IsTrue   (LocalFeedHttpClient::TryResolve (MakeRequest (LocalFeedHttpClient::kpszFeedHost, L"/D:/drops/Casso-1.31.91.msixbundle"), feed, file, isFeed));
        Assert::AreEqual (std::wstring (L"D:\\drops\\Casso-1.31.91.msixbundle"), file, L"an absolute path stands as it is");

        Assert::IsFalse  (LocalFeedHttpClient::TryResolve (MakeRequest (LocalFeedHttpClient::kpszFeedHost, L"/../secret.txt"), feed, file, isFeed), L"no climbing out");
        Assert::IsFalse  (LocalFeedHttpClient::TryResolve (MakeRequest (UpdateService::kpszApiHost, L"/repos/relmer/Casso/releases"), feed, file, isFeed));
        Assert::IsFalse  (LocalFeedHttpClient::TryResolve (MakeRequest (L"example.com", L"/image.png"), feed, file, isFeed), L"another host is not the feed's");
    }



    TEST_METHOD (RewriteDownloadUrls_TurnsPathsAndFileUrisIntoFeedUrls)
    {
        std::string  json = "{\"assets\":[{\"name\":\"a.zip\",\"browser_download_url\":\"Casso-1.31.91-x64.zip\"},"
                            "{\"browser_download_url\": \"file:///D:/drops/b.msixbundle\"},"
                            "{\"browser_download_url\":\"sub\\\\c.zip\"},"
                            "{\"browser_download_url\":\"https://github.com/x/d.zip\"}]}";
        std::string  out  = LocalFeedHttpClient::RewriteDownloadUrls (json);



        Assert::IsTrue (out.find ("\"https://casso-local-feed.invalid/Casso-1.31.91-x64.zip\"") != std::string::npos, L"a relative path");
        Assert::IsTrue (out.find ("\"https://casso-local-feed.invalid/D:/drops/b.msixbundle\"") != std::string::npos, L"a file:/// URI");
        Assert::IsTrue (out.find ("\"https://casso-local-feed.invalid/sub/c.zip\"") != std::string::npos, L"escaped backslashes");
        Assert::IsTrue (out.find ("\"https://github.com/x/d.zip\"") != std::string::npos, L"https left as written");
        Assert::IsTrue (out.find ("\"name\":\"a.zip\"") != std::string::npos, L"everything else untouched");
    }



    TEST_METHOD (Get_ReadsFilesAndAnswers404ForMissingOnes)
    {
        FakeFeedFileReader         reader;
        LocalFeedHttpClient        client (L"C:\\feed\\release.json", reader, nullptr);
        HttpResponse               response;
        std::string                error;
        std::atomic<std::uint64_t> progress = 0;
        HttpRequest                request  = MakeRequest (LocalFeedHttpClient::kpszFeedHost, L"/Casso-1.31.91-x64.zip");
        std::string                body;



        reader.files[L"C:\\feed\\release.json"]         = "{\"browser_download_url\":\"Casso-1.31.91-x64.zip\"}";
        reader.files[L"C:\\feed\\Casso-1.31.91-x64.zip"] = "PK-zip";

        Assert::AreEqual (S_OK, client.Get (MakeRequest (UpdateService::kpszApiHost, UpdateService::kpszLatestPath), response, error));
        Assert::AreEqual ((DWORD) 200, response.statusCode);
        body.assign (response.body.begin(), response.body.end());
        Assert::IsTrue   (body.find ("https://casso-local-feed.invalid/Casso-1.31.91-x64.zip") != std::string::npos, L"served with its URLs rewritten");

        request.progressBytes = &progress;
        Assert::AreEqual (S_OK, client.Get (request, response, error));
        Assert::AreEqual ((DWORD) 200, response.statusCode);
        Assert::AreEqual ((std::uint64_t) 6, progress.load(), L"the download reports its size");

        Assert::AreEqual (S_OK, client.Get (MakeRequest (UpdateService::kpszRawHost, L"/relmer/Casso/v1.31.91/README.md"), response, error));
        Assert::AreEqual ((DWORD) 404, response.statusCode, L"a missing file is a 404");

        Assert::AreEqual (S_OK, client.Get (MakeRequest (L"example.com", L"/image.png"), response, error));
        Assert::AreEqual ((DWORD) 404, response.statusCode, L"another host, with no fallback");
    }
};
