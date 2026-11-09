#include "Pch.h"

#include "Seams/Win32ByteStreamFactory.h"
#include "Seams/Win32ComPort.h"
#include "Seams/Win32TcpStream.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory::ListComPorts
//
//  Each value under the serial device map is one port, its data the port's
//  name. No key at all means no ports.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ByteStreamFactory::ListComPorts (std::vector<std::wstring> & outPorts)
{
    HRESULT  hr     = S_OK;
    HKEY     key    = nullptr;
    LSTATUS  status = ERROR_SUCCESS;



    outPorts.clear();

    status = RegOpenKeyExW (HKEY_LOCAL_MACHINE, s_kpszSerialMapKey, 0, KEY_READ, &key);
    BAIL_OUT_IF (status == ERROR_FILE_NOT_FOUND, S_OK);
    CBREx (status == ERROR_SUCCESS, HRESULT_FROM_WIN32 (status));

    for (DWORD index = 0; status == ERROR_SUCCESS; index++)
    {
        wchar_t  name[256]  = {};
        wchar_t  value[64]  = {};
        DWORD    nameLength = (DWORD) std::size (name);
        DWORD    valueBytes = (DWORD) sizeof (value);
        DWORD    type       = 0;

        status = RegEnumValueW (key, index, name, &nameLength, nullptr, &type, (LPBYTE) value, &valueBytes);

        if (status == ERROR_SUCCESS && type == REG_SZ)
        {
            outPorts.push_back (value);
        }
    }

    std::sort (outPorts.begin(), outPorts.end());

Error:
    if (key != nullptr)
    {
        RegCloseKey (key);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory::OpenComPort
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ByteStreamFactory::OpenComPort (const std::wstring & port, const ComSettings & settings, std::unique_ptr<IByteStream> & outStream)
{
    HRESULT                        hr     = S_OK;
    std::unique_ptr<Win32ComPort>  stream = std::make_unique<Win32ComPort>();



    hr = stream->Open (port, settings);
    CHR (hr);

    outStream = std::move (stream);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory::ListenTcp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ByteStreamFactory::ListenTcp (uint16_t port, LinkTransport transport, std::unique_ptr<IByteStream> & outStream)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<Win32TcpStream>  stream = std::make_unique<Win32TcpStream>();



    //  An emulated machine's serial port writes on its CPU thread.
    stream->SetDropWhenFull (transport == LinkTransport::Emulated);

    hr = stream->Listen (ByteStreamSettings::GetBindAddress (transport), port);
    CHR (hr);

    outStream = std::move (stream);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory::ConnectTcp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ByteStreamFactory::ConnectTcp (const std::string & host, uint16_t port, std::unique_ptr<IByteStream> & outStream)
{
    return ConnectTcp (host, port, std::stop_token(), outStream);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Win32ByteStreamFactory::ConnectTcp
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Win32ByteStreamFactory::ConnectTcp (const std::string & host, uint16_t port, std::stop_token stop, std::unique_ptr<IByteStream> & outStream)
{
    HRESULT                          hr     = S_OK;
    std::unique_ptr<Win32TcpStream>  stream = std::make_unique<Win32TcpStream>();



    hr = stream->Connect (host, port, stop);
    CHR (hr);

    outStream = std::move (stream);

Error:
    return hr;
}
