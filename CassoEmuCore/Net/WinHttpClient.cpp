#include "Pch.h"

#include "Net/WinHttpClient.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient::~WinHttpClient
//
////////////////////////////////////////////////////////////////////////////////

WinHttpClient::~WinHttpClient()
{
    if (m_hSession != nullptr)
    {
        WinHttpCloseHandle (m_hSession);
        m_hSession = nullptr;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient::OpenSession
//
//  A WinHTTP session with Casso's User-Agent and the system proxy settings.
//  The caller closes it with WinHttpCloseHandle.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WinHttpClient::OpenSession (HINTERNET & outSession)
{
    HRESULT  hr = S_OK;



    outSession = WinHttpOpen (kpszUserAgent,
                              WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                              WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS,
                              0);
    CWR (outSession);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient::Get
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WinHttpClient::Get (
    const HttpRequest  & request,
    HttpResponse       & outResponse,
    std::string        & outError)
{
    HRESULT  hr = S_OK;



    if (m_hSession == nullptr)
    {
        hr = OpenSession (m_hSession);
        CHRF (hr, outError = "Cannot initialize WinHTTP session");
    }

    hr = GetWithSession (m_hSession, request, outResponse, outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient::GetWithSession
//
//  Fetches `request.path` from `request.host` over HTTPS. Succeeds once a
//  response arrives, whatever its status; the body is read in full either
//  way.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WinHttpClient::GetWithSession (
    HINTERNET            hSession,
    const HttpRequest  & request,
    HttpResponse       & outResponse,
    std::string        & outError)
{
    HRESULT    hr           = S_OK;
    HINTERNET  hConnect     = nullptr;
    HINTERNET  hRequest     = nullptr;
    BOOL       fOk          = FALSE;
    DWORD      statusSize   = sizeof (outResponse.statusCode);
    LPCWSTR    pszHeaders   = WINHTTP_NO_ADDITIONAL_HEADERS;
    DWORD      headerLength = 0;
    string     narrowHost;



    outResponse = {};

    if (request.progressBytes != nullptr)
    {
        request.progressBytes->store (0, std::memory_order_relaxed);
    }

    narrowHost = TextEncoding::WideToNarrow (request.host);

    if (!request.extraHeaders.empty())
    {
        pszHeaders   = request.extraHeaders.c_str();
        headerLength = (DWORD) -1;
    }

    hConnect = WinHttpConnect (hSession, request.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    CBRF (hConnect != nullptr,
          outError = format ("Cannot connect to {}", narrowHost));

    hRequest = WinHttpOpenRequest (hConnect,
                                   L"GET",
                                   request.path.c_str(),
                                   nullptr,
                                   WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   WINHTTP_FLAG_SECURE);
    CBRF (hRequest != nullptr,
          outError = format ("Cannot open HTTPS request for {}", request.displayName));

    fOk = WinHttpSendRequest (hRequest,
                              pszHeaders,
                              headerLength,
                              WINHTTP_NO_REQUEST_DATA,
                              0,
                              0,
                              0);
    CBRF (fOk,
          outError = format ("Network send failed for {}", request.displayName));

    fOk = WinHttpReceiveResponse (hRequest, nullptr);
    CBRF (fOk,
          outError = format ("No response from server for {}", request.displayName));

    fOk = WinHttpQueryHeaders (hRequest,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &outResponse.statusCode,
                               &statusSize,
                               WINHTTP_NO_HEADER_INDEX);
    CBRF (fOk,
          outError = format ("HTTP {} fetching {}", outResponse.statusCode, request.displayName));

    hr = ReadBody (hRequest, request, outResponse.body, outError);
    CHR (hr);

Error:
    if (hRequest != nullptr)
    {
        WinHttpCloseHandle (hRequest);
    }

    if (hConnect != nullptr)
    {
        WinHttpCloseHandle (hConnect);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient::ReadBody
//
//  Reads the response body to the end, polling the cancel flag before each
//  read (E_ABORT when set) and publishing the running byte count.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WinHttpClient::ReadBody (
    HINTERNET            hRequest,
    const HttpRequest  & request,
    std::vector<Byte>  & outBody,
    std::string        & outError)
{
    HRESULT       hr         = S_OK;
    BOOL          fOk        = FALSE;
    DWORD         bytesAvail = 0;
    DWORD         bytesRead  = 0;
    bool          fCanceled  = false;
    vector<Byte>  chunk;



    outBody.clear();

    while (true)
    {
        fCanceled = (request.cancelRequested != nullptr) &&
                    request.cancelRequested->load (std::memory_order_relaxed);
        CBRFEx (!fCanceled, E_ABORT, outError = format ("{} canceled", request.displayName));

        bytesAvail = 0;
        fOk = WinHttpQueryDataAvailable (hRequest, &bytesAvail);
        CBRF (fOk,
              outError = format ("Read failed for {}", request.displayName));

        if (bytesAvail == 0)
        {
            break;
        }

        chunk.resize (bytesAvail);
        bytesRead = 0;
        fOk = WinHttpReadData (hRequest, chunk.data(), bytesAvail, &bytesRead);
        CBRF (fOk,
              outError = format ("Read failed for {}", request.displayName));

        if (bytesRead == 0)
        {
            break;
        }

        outBody.insert (outBody.end(), chunk.begin(), chunk.begin() + bytesRead);

        if (request.progressBytes != nullptr)
        {
            request.progressBytes->store ((std::uint64_t) outBody.size(), std::memory_order_relaxed);
        }
    }

Error:
    return hr;
}
