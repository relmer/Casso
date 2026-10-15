#include "Pch.h"

#include "Devices/SerialPortDevice.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortDevice::Parse
//
//  An empty device is loopback. Anything else must be one of the two forms in
//  full; Parse fails on a port number of zero, a port name with nothing in
//  it, or a speed that is not a number, rather than reading it as loopback.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SerialPortDevice::Parse (const std::string & device, SerialPortDevice & outDevice)
{
    HRESULT           hr          = S_OK;
    std::string_view  text        = device;
    std::string_view  name;
    size_t            at          = 0;
    uint32_t          number      = 0;
    bool              isCom       = false;
    bool              hasPortName = false;



    outDevice = SerialPortDevice();

    BAIL_OUT_IF (text.empty(), S_OK);

    if (text.starts_with (s_kTcpPrefix))
    {
        hr = ParseNumber (text.substr (s_kTcpPrefix.size()), UINT16_MAX, number);
        CHR (hr);
        CBREx (number != 0, E_INVALIDARG);

        outDevice.kind    = Kind::Tcp;
        outDevice.tcpPort = (uint16_t) number;
    }
    else
    {
        isCom = text.starts_with (s_kComPrefix);
        CBREx (isCom, E_INVALIDARG);

        name = text.substr (s_kComPrefix.size());
        at   = name.find ('@');

        if (at != std::string_view::npos)
        {
            hr = ParseNumber (name.substr (at + 1), UINT32_MAX, number);
            CHR (hr);
            CBREx (number != 0, E_INVALIDARG);

            outDevice.baud = number;
            name           = name.substr (0, at);
        }

        hasPortName = !name.empty();
        CBREx (hasPortName, E_INVALIDARG);

        outDevice.kind    = Kind::Com;
        outDevice.comPort = TextEncoding::NarrowToWide (std::string (name));
    }

Error:
    if (FAILED (hr))
    {
        outDevice = SerialPortDevice();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortDevice::Format
//
//  The speed is written only when it is not the default, so a device written
//  back reads as it was typed.
//
////////////////////////////////////////////////////////////////////////////////

std::string SerialPortDevice::Format (const SerialPortDevice & device)
{
    std::string  text;



    switch (device.kind)
    {
        case Kind::Tcp:
            text = std::string (s_kTcpPrefix) + std::to_string (device.tcpPort);
            break;

        case Kind::Com:
            text = std::string (s_kComPrefix) + TextEncoding::WideToNarrow (device.comPort);

            if (device.baud != kDefaultBaud)
            {
                text += "@" + std::to_string (device.baud);
            }

            break;

        default:
            break;
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SerialPortDevice::ParseNumber
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SerialPortDevice::ParseNumber (std::string_view text, uint32_t maximum, uint32_t & outValue)
{
    HRESULT                 hr       = S_OK;
    uint64_t                value    = 0;
    std::from_chars_result  result   = {};
    bool                    present  = !text.empty();
    bool                    complete = false;



    outValue = 0;

    CBREx (present, E_INVALIDARG);

    result = std::from_chars (text.data(), text.data() + text.size(), value);
    complete = result.ec == std::errc{} && result.ptr == text.data() + text.size();
    CBREx (complete, E_INVALIDARG);
    CBREx (value <= maximum, E_INVALIDARG);

    outValue = (uint32_t) value;

Error:
    return hr;
}
