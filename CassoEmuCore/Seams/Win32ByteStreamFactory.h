#pragma once

#include "Pch.h"

#include "Seams/IByteStreamFactory.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory
//
//  COM ports are listed from the system's serial device map, the list
//  Device Manager's Ports section is drawn from; TCP streams are
//  Win32TcpStream, bound for the transport by ByteStreamSettings.
//
////////////////////////////////////////////////////////////////////////////////

class Win32ByteStreamFactory : public IByteStreamFactory
{
public:
    HRESULT  ListComPorts (std::vector<std::wstring> & outPorts) override;
    HRESULT  OpenComPort  (const std::wstring & port, const ComSettings & settings, std::unique_ptr<IByteStream> & outStream) override;
    HRESULT  ListenTcp    (uint16_t port, LinkTransport transport, std::unique_ptr<IByteStream> & outStream) override;
    HRESULT  ConnectTcp   (const std::string & host, uint16_t port, std::unique_ptr<IByteStream> & outStream) override;
    HRESULT  ConnectTcp   (const std::string & host, uint16_t port, std::stop_token stop, std::unique_ptr<IByteStream> & outStream) override;

private:
    static constexpr const wchar_t *  s_kpszSerialMapKey = L"HARDWARE\\DEVICEMAP\\SERIALCOMM";
};
