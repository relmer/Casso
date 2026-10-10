#pragma once

#include "Pch.h"

#include "Net/IHttpClient.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockHttpClient
//
//  An IHttpClient that answers from a table keyed by host and path, and
//  records every request. A request for anything not in the table gets a
//  404. `onGet` runs before the answer, so a test can act mid-request (a
//  cancel, say).
//
//  Header-only on purpose; lives only in the test binary.
//
////////////////////////////////////////////////////////////////////////////////

class MockHttpClient : public IHttpClient
{
public:
    static constexpr DWORD  kStatusOk       = 200;
    static constexpr DWORD  kStatusNotFound = 404;

    struct Reply
    {
        HRESULT            hr     = S_OK;
        DWORD              status = kStatusOk;
        std::vector<Byte>  body;
    };

    std::map<std::wstring, Reply>               replies;
    std::vector<HttpRequest>                    requests;
    std::function<void (const HttpRequest &)>   onGet;
    std::mutex                                  mutex;

    void SetText (const std::wstring & host, const std::wstring & path, DWORD status, const std::string & body)
    {
        Reply  reply;

        reply.status = status;
        reply.body.assign (body.begin(), body.end());
        replies[host + path] = std::move (reply);
    }

    void SetBytes (const std::wstring & host, const std::wstring & path, const std::vector<Byte> & body)
    {
        Reply  reply;

        reply.body           = body;
        replies[host + path] = std::move (reply);
    }

    void SetFailure (const std::wstring & host, const std::wstring & path, HRESULT hr)
    {
        Reply  reply;

        reply.hr             = hr;
        replies[host + path] = std::move (reply);
    }

    size_t GetRequestCount()
    {
        std::lock_guard<std::mutex>  lock (mutex);

        return requests.size();
    }

    HRESULT Get (const HttpRequest  & request,
                 HttpResponse       & outResponse,
                 std::string        & outError) override
    {
        std::lock_guard<std::mutex>  lock (mutex);
        auto                         it   = replies.find (request.host + request.path);

        requests.push_back (request);

        if (onGet)
        {
            onGet (request);
        }

        outResponse = {};

        if (it == replies.end())
        {
            outResponse.statusCode = kStatusNotFound;
            return S_OK;
        }

        if (FAILED (it->second.hr))
        {
            outError = "no network";
            return it->second.hr;
        }

        outResponse.statusCode = it->second.status;
        outResponse.body       = it->second.body;

        if (request.progressBytes != nullptr)
        {
            *request.progressBytes = outResponse.body.size();
        }

        return S_OK;
    }
};
