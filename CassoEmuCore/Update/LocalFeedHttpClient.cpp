#include "Pch.h"

#include "Update/LocalFeedHttpClient.h"
#include "Update/UpdateService.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FeedFileReader::ReadAllBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32FeedFileReader::ReadAllBytes (const std::wstring & path, std::vector<Byte> & outBytes)
{
    HRESULT        hr     = S_OK;
    HANDLE         hFile  = INVALID_HANDLE_VALUE;
    LARGE_INTEGER  size   = {};
    DWORD          read   = 0;
    BOOL           fOk    = FALSE;
    bool           isAll  = false;



    outBytes.clear();

    hFile = CreateFileW (path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CWR (hFile != INVALID_HANDLE_VALUE);

    fOk = GetFileSizeEx (hFile, &size);
    CWR (fOk);

    CBREx (size.QuadPart < (LONGLONG) MAXDWORD, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));
    outBytes.resize ((size_t) size.QuadPart);

    fOk = ::ReadFile (hFile, outBytes.data(), (DWORD) outBytes.size(), &read, nullptr);
    CWR (fOk);

    isAll = read == outBytes.size();
    CBREx (isAll, HRESULT_FROM_WIN32 (ERROR_READ_FAULT));

Error:
    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle (hFile);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::LocalFeedHttpClient
//
////////////////////////////////////////////////////////////////////////////////

LocalFeedHttpClient::LocalFeedHttpClient (std::wstring feedPath, IFeedFileReader & reader, IHttpClient * fallback) :
    m_feedPath (std::move (feedPath)),
    m_reader   (reader),
    m_fallback (fallback)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::SelectFeedPath
//
//  The feed's path from the variable's value, without the quotes a shell
//  may leave around it. Unset or blank means no local feed.
//
////////////////////////////////////////////////////////////////////////////////

std::optional<std::wstring> LocalFeedHttpClient::SelectFeedPath (const wchar_t * variableValue)
{
    std::wstring_view            value;
    std::optional<std::wstring>  path;



    if (variableValue != nullptr)
    {
        value = variableValue;

        while (!value.empty() && (value.front() == L' ' || value.front() == L'"'))
        {
            value.remove_prefix (1);
        }

        while (!value.empty() && (value.back() == L' ' || value.back() == L'"'))
        {
            value.remove_suffix (1);
        }

        if (!value.empty())
        {
            path = std::wstring (value);
        }
    }

    return path;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::GetFolder
//
//  The folder holding the feed, with a trailing separator; empty for a
//  bare file name, which then resolves against the current directory.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring LocalFeedHttpClient::GetFolder (const std::wstring & feedPath)
{
    size_t  slash = feedPath.find_last_of (L"\\/");



    return (slash == std::wstring::npos) ? std::wstring() : feedPath.substr (0, slash + 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::TryMakeLocalPath
//
//  A URL path ("/sub/name.zip" or "/C:/dir/name.zip") as a file path: a
//  drive-letter path stands as it is, anything else is under `folder`. A
//  path that climbs with ".." is not served.
//
////////////////////////////////////////////////////////////////////////////////

bool LocalFeedHttpClient::TryMakeLocalPath (std::wstring_view path, const std::wstring & folder, std::wstring & outFilePath)
{
    constexpr size_t  kDriveChars = 3;



    bool  isAbsolute = false;
    bool  isMade     = false;



    while (path.starts_with (L'/'))
    {
        path.remove_prefix (1);
    }

    isAbsolute = path.size() >= kDriveChars && iswalpha (path[0]) && path[1] == L':' && (path[2] == L'/' || path[2] == L'\\');

    if (!path.empty() && path.find (L"..") == std::wstring_view::npos)
    {
        outFilePath = isAbsolute ? std::wstring (path) : folder + std::wstring (path);
        std::replace (outFilePath.begin(), outFilePath.end(), L'/', L'\\');
        isMade = true;
    }

    return isMade;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::TryResolve
//
//  The file a request reads: the feed itself for the release request
//  (`outIsFeed`), the file under the feed's folder for a raw tag path such
//  as "/relmer/Casso/v1.31.91/CHANGELOG.md", and the asset's file for the
//  feed host. False for any other request.
//
////////////////////////////////////////////////////////////////////////////////

bool LocalFeedHttpClient::TryResolve (
    const HttpRequest   & request,
    const std::wstring  & feedPath,
    std::wstring        & outFilePath,
    bool                & outIsFeed)
{
    std::wstring_view  path       = request.path;
    std::wstring       folder     = GetFolder (feedPath);
    size_t             tagEnd     = 0;
    bool               isResolved = false;



    outIsFeed = false;

    if (request.host == UpdateService::kpszApiHost && request.path == UpdateService::kpszLatestPath)
    {
        outFilePath = feedPath;
        outIsFeed   = true;
        isResolved  = true;
    }
    else if (request.host == UpdateService::kpszRawHost && path.starts_with (UpdateService::kpszRepoRawPath))
    {
        path.remove_prefix (std::wstring_view (UpdateService::kpszRepoRawPath).size());
        tagEnd = path.find (L'/');

        if (tagEnd != std::wstring_view::npos)
        {
            isResolved = TryMakeLocalPath (path.substr (tagEnd), folder, outFilePath);
        }
    }
    else if (request.host == kpszFeedHost)
    {
        isResolved = TryMakeLocalPath (path, folder, outFilePath);
    }

    return isResolved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::RewriteDownloadUrls
//
//  The service downloads only https URLs, so each browser_download_url that
//  is a path or a file:/// URI becomes an https URL on the feed host, which
//  this client then reads from disk. https URLs are left as written.
//
////////////////////////////////////////////////////////////////////////////////

std::string LocalFeedHttpClient::RewriteDownloadUrls (const std::string & json)
{
    constexpr std::string_view  kKey      = "\"browser_download_url\"";
    constexpr std::string_view  kHttps    = "https://";
    constexpr std::string_view  kFileUri  = "file:///";



    std::string  out;
    std::string  value;
    std::string  host;
    size_t       at     = 0;
    size_t       key    = 0;
    size_t       open   = 0;
    size_t       close  = 0;



    // The feed host is plain ASCII.
    for (const wchar_t * pch = kpszFeedHost; *pch != L'\0'; pch++)
    {
        host += (char) *pch;
    }

    for (key = json.find (kKey, at); key != std::string::npos; key = json.find (kKey, at))
    {
        open = json.find ('"', json.find (':', key + kKey.size()));

        close = open;

        do
        {
            close = json.find ('"', close + 1);
        } while (close != std::string::npos && json[close - 1] == '\\' && json[close - 2] != '\\');

        if (open == std::string::npos || close == std::string::npos)
        {
            break;
        }

        out.append (json, at, open + 1 - at);
        value = json.substr (open + 1, close - open - 1);

        if (!value.starts_with (kHttps))
        {
            if (value.starts_with (kFileUri))
            {
                value.erase (0, kFileUri.size());
            }

            for (size_t i = value.find ("\\\\"); i != std::string::npos; i = value.find ("\\\\", i))
            {
                value.replace (i, 2, "/");
            }

            for (size_t i = value.find ("\\/"); i != std::string::npos; i = value.find ("\\/", i))
            {
                value.replace (i, 2, "/");
            }

            std::replace (value.begin(), value.end(), '\\', '/');

            while (value.starts_with ('/'))
            {
                value.erase (0, 1);
            }

            value = std::string (kHttps) + host + "/" + value;
        }

        out += value;
        at   = close;
    }

    out.append (json, at, std::string::npos);

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient::Get
//
//  A file that is not there answers 404, as a missing release file would,
//  and so does any other host when there is no fallback.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT LocalFeedHttpClient::Get (
    const HttpRequest  & request,
    HttpResponse       & outResponse,
    std::string        & outError)
{
    HRESULT       hr         = S_OK;
    std::wstring  filePath;
    std::string   json;
    bool          isFeed     = false;
    bool          isResolved = TryResolve (request, m_feedPath, filePath, isFeed);
    bool          isMissing  = false;



    outResponse = {};
    outError.clear();

    // Not a feed request: the real network, when there is one.
    if (!isResolved && m_fallback != nullptr)
    {
        hr = m_fallback->Get (request, outResponse, outError);
        CHR (hr);
    }

    if (isResolved)
    {
        hr = m_reader.ReadAllBytes (filePath, outResponse.body);
    }

    isMissing = !isResolved || hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32 (ERROR_PATH_NOT_FOUND);

    if (isMissing && (isResolved || m_fallback == nullptr))
    {
        hr                     = S_OK;
        outResponse.statusCode = kStatusNotFound;
        outResponse.body.clear();
    }

    CHR (hr);

    if (isResolved && !isMissing)
    {
        if (isFeed)
        {
            json.assign (outResponse.body.begin(), outResponse.body.end());
            json = RewriteDownloadUrls (json);
            outResponse.body.assign (json.begin(), json.end());
        }

        outResponse.statusCode = kStatusOk;

        if (request.progressBytes != nullptr)
        {
            request.progressBytes->store (outResponse.body.size());
        }
    }

Error:
    if (FAILED (hr) && outError.empty())
    {
        outError = std::format ("Could not read {} from the local update feed", request.displayName);
    }

    return hr;
}
