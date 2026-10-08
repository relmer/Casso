#pragma once

#include "Pch.h"

#include "Net/IHttpClient.h"
#include "Version.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WinHttpClient
//
//  The production IHttpClient over WinHTTP. Owns one session, opened on the
//  first request and closed with the object. GetWithSession is the same GET
//  over a session the caller owns, for code that shares one session across
//  several downloads.
//
////////////////////////////////////////////////////////////////////////////////

class WinHttpClient : public IHttpClient
{
public:
    static constexpr LPCWSTR kpszUserAgent = L"Casso/" VERSION_STRING;

    WinHttpClient() = default;
    ~WinHttpClient() override;

    WinHttpClient (const WinHttpClient &)             = delete;
    WinHttpClient & operator= (const WinHttpClient &) = delete;

    HRESULT         Get            (const HttpRequest  & request,
                                    HttpResponse       & outResponse,
                                    std::string        & outError) override;

    static HRESULT  OpenSession    (HINTERNET & outSession);
    static HRESULT  GetWithSession (HINTERNET            hSession,
                                    const HttpRequest  & request,
                                    HttpResponse       & outResponse,
                                    std::string        & outError);

private:
    static HRESULT  ReadBody       (HINTERNET            hRequest,
                                    const HttpRequest  & request,
                                    std::vector<Byte>  & outBody,
                                    std::string        & outError);

    HINTERNET  m_hSession = nullptr;
};
