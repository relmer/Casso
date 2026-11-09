#pragma once

#include "Pch.h"

#include "Seams/ByteStreamSettings.h"
#include "Seams/IByteStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IByteStreamFactory
//
//  Lists COM ports and opens each kind of stream, so the code that searches
//  ports or starts a link runs against a fake in tests.
//
//  A listening TCP stream is open before any client connects: reads return
//  nothing and writes are discarded until one does, and after that client
//  disconnects the next one is accepted.
//
////////////////////////////////////////////////////////////////////////////////

class IByteStreamFactory
{
public:
    virtual ~IByteStreamFactory() = default;

    virtual HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) = 0;
    virtual HRESULT  OpenComPort  (const std::wstring & port, const ComSettings & settings, std::unique_ptr<IByteStream> & outStream) = 0;
    virtual HRESULT  ListenTcp    (uint16_t port, LinkTransport transport, std::unique_ptr<IByteStream> & outStream) = 0;
    virtual HRESULT  ConnectTcp   (const std::string & host, uint16_t port, std::unique_ptr<IByteStream> & outStream) = 0;

    //  ConnectTcp that ends with ERROR_CANCELLED once `stop` is requested,
    //  for a connect to an address that does not answer. A factory whose
    //  connect returns at once does not override it.
    virtual HRESULT  ConnectTcp   (const std::string & host, uint16_t port, std::stop_token stop, std::unique_ptr<IByteStream> & outStream)
    {
        (void) stop;

        return ConnectTcp (host, port, outStream);
    }
};
