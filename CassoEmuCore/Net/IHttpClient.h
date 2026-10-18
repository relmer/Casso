#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HttpRequest
//
//  One HTTPS GET. `extraHeaders` is CRLF-separated header lines, empty for
//  none. `displayName` appears in error text so the user sees which download
//  failed. The progress counter, when given, holds the bytes received so
//  far; the cancel flag, when given, is polled between reads.
//
////////////////////////////////////////////////////////////////////////////////

struct HttpRequest
{
    std::wstring                  host;
    std::wstring                  path;
    std::wstring                  extraHeaders;
    std::string                   displayName;
    std::atomic<std::uint64_t>  * progressBytes   = nullptr;
    std::atomic<bool>           * cancelRequested = nullptr;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HttpResponse
//
//  What came back: the HTTP status and the whole body. A response with any
//  status is a successful GET; the caller decides what a status means.
//
////////////////////////////////////////////////////////////////////////////////

struct HttpResponse
{
    DWORD               statusCode = 0;
    std::vector<Byte>   body;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IHttpClient
//
//  The seam every network fetch goes through, so unit tests can supply the
//  responses. Get fails only when no response arrived (no network, a
//  timeout, a canceled read: E_ABORT); `outError` then holds the reason.
//
////////////////////////////////////////////////////////////////////////////////

class IHttpClient
{
public:
    virtual ~IHttpClient() = default;

    virtual HRESULT Get (const HttpRequest  & request,
                         HttpResponse       & outResponse,
                         std::string        & outError) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullHttpClient
//
//  Has no network: every request fails.
//
////////////////////////////////////////////////////////////////////////////////

class NullHttpClient : public IHttpClient
{
public:
    HRESULT Get (const HttpRequest  & request,
                 HttpResponse       & outResponse,
                 std::string        & outError) override
    {
        outResponse = {};
        outError    = std::format ("No network for {}", request.displayName);

        return HRESULT_FROM_WIN32 (ERROR_NOT_SUPPORTED);
    }
};
