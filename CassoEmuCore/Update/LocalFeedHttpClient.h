#pragma once

#include "Pch.h"

#include "Net/IHttpClient.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFeedFileReader
//
//  Reads a whole file, for the local update feed. Fails with
//  ERROR_FILE_NOT_FOUND (as an HRESULT) when the file is not there.
//
////////////////////////////////////////////////////////////////////////////////

class IFeedFileReader
{
public:
    virtual ~IFeedFileReader() = default;

    virtual HRESULT ReadAllBytes (const std::wstring & path, std::vector<Byte> & outBytes) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  Win32FeedFileReader
//
//  IFeedFileReader on the real file system.
//
////////////////////////////////////////////////////////////////////////////////

class Win32FeedFileReader : public IFeedFileReader
{
public:
    HRESULT ReadAllBytes (const std::wstring & path, std::vector<Byte> & outBytes) override;
};





////////////////////////////////////////////////////////////////////////////////
//
//  LocalFeedHttpClient
//
//  Serves the update check from a release JSON on disk, for an end-to-end
//  test of a signed update without publishing a release. The release
//  request reads the JSON file; the CHANGELOG, the README and relative
//  images read the files beside it; every asset download reads the file its
//  browser_download_url gives, a path relative to the JSON's folder or a
//  file:/// URI. Any other host goes to `fallback`, or answers 404 without
//  one. Chosen at startup only when CASSO_UPDATE_FEED holds the JSON's path;
//  everything after the fetch -- digest, signature, version, install -- is
//  the ordinary path.
//
////////////////////////////////////////////////////////////////////////////////

class LocalFeedHttpClient : public IHttpClient
{
public:
    static constexpr LPCWSTR  kpszFeedVariable = L"CASSO_UPDATE_FEED";
    static constexpr LPCWSTR  kpszFeedHost     = L"casso-local-feed.invalid";
    static constexpr DWORD    kStatusOk        = 200;
    static constexpr DWORD    kStatusNotFound  = 404;

    LocalFeedHttpClient (std::wstring feedPath, IFeedFileReader & reader, IHttpClient * fallback);

    HRESULT Get (const HttpRequest  & request,
                 HttpResponse       & outResponse,
                 std::string        & outError) override;

    static std::optional<std::wstring>  SelectFeedPath       (const wchar_t * variableValue);
    static std::wstring                 GetFolder            (const std::wstring & feedPath);
    static bool                         TryResolve           (const HttpRequest   & request,
                                                              const std::wstring  & feedPath,
                                                              std::wstring        & outFilePath,
                                                              bool                & outIsFeed);
    static std::string                  RewriteDownloadUrls  (const std::string & json);

private:
    static bool  TryMakeLocalPath (std::wstring_view path, const std::wstring & folder, std::wstring & outFilePath);

    std::wstring       m_feedPath;
    IFeedFileReader  & m_reader;
    IHttpClient      * m_fallback = nullptr;
};
