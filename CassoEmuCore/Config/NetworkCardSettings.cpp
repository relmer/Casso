#include "Pch.h"

#include "Config/NetworkCardSettings.h"
#include "Core/JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::FromJson
//
//  Keys: "mac" (string), "lanAccess" (bool), "portForwards" (array of
//  { "protocol", "hostPort", "guestPort" }). A missing key takes its
//  default; a bad value takes its default and adds a warning, so the machine
//  loads despite a typo.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT NetworkCardSettings::FromJson (
    const JsonValue           & object,
    NetworkCardSettings       & outSettings,
    std::vector<std::string>  & outWarnings)
{
    HRESULT             hr       = S_OK;
    std::string         macText;
    const JsonValue   * forwards = nullptr;
    bool                isObject = object.GetType() == JsonType::Object;
    size_t              i        = 0;
    MacAddress          parsed;
    PortForward         forward;
    std::string         problem;



    outSettings = {};

    CBREx (isObject, E_INVALIDARG);

    if (object.HasString ("mac", macText))
    {
        if (MacAddress::TryParse (macText, parsed) && parsed.IsUnicast())
        {
            outSettings.mac = parsed;
        }
        else
        {
            outWarnings.push_back (std::format ("The network card's MAC address \"{}\" is not valid and was ignored.", macText));
        }
    }

    object.HasBool ("lanAccess", outSettings.lanAccess);

    if (object.HasArray ("portForwards", forwards))
    {
        for (i = 0; i < forwards->GetArraySize(); i++)
        {
            if (!TryReadPortForward (forwards->GetArrayElement (i), forward))
            {
                outWarnings.push_back (std::format ("Port forward {} is not valid and was ignored.", i + 1));
                continue;
            }

            problem = ValidatePortForward (forward, outSettings.portForwards);

            if (!problem.empty())
            {
                outWarnings.push_back (problem);
                continue;
            }

            outSettings.portForwards.push_back (forward);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::TryReadPortForward
//
////////////////////////////////////////////////////////////////////////////////

bool NetworkCardSettings::TryReadPortForward (
    const JsonValue  & entry,
    PortForward      & outForward)
{
    std::string  token;
    int          hostPort  = 0;
    int          guestPort = 0;



    if (entry.GetType() != JsonType::Object
        || !entry.HasString ("protocol", token)
        || !TryParseProtocol (token, outForward.protocol)
        || !entry.HasInt ("hostPort", hostPort)
        || !entry.HasInt ("guestPort", guestPort))
    {
        return false;
    }

    if (hostPort < 1 || hostPort > UINT16_MAX || guestPort < 1 || guestPort > UINT16_MAX)
    {
        return false;
    }

    outForward.hostPort  = (uint16_t) hostPort;
    outForward.guestPort = (uint16_t) guestPort;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::ToJson
//
//  An unset MAC is omitted rather than written empty.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue NetworkCardSettings::ToJson() const
{
    vector<pair<string, JsonValue>>  root;
    vector<JsonValue>                forwards;



    if (mac.IsSet())
    {
        root.emplace_back ("mac", JsonValue (mac.Format()));
    }

    root.emplace_back ("lanAccess", JsonValue (lanAccess));

    for (const PortForward & forward : portForwards)
    {
        vector<pair<string, JsonValue>>  entry;

        entry.emplace_back ("protocol",  JsonValue (std::string (ProtocolToToken (forward.protocol))));
        entry.emplace_back ("hostPort",  JsonValue ((double) forward.hostPort));
        entry.emplace_back ("guestPort", JsonValue ((double) forward.guestPort));

        forwards.emplace_back (std::move (entry));
    }

    root.emplace_back ("portForwards", JsonValue (std::move (forwards)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::ValidatePortForward
//
//  Ports 1-65535, and no two forwards on the same protocol and host port.
//
////////////////////////////////////////////////////////////////////////////////

std::string NetworkCardSettings::ValidatePortForward (
    const PortForward               & forward,
    const std::vector<PortForward>  & existing)
{
    if (forward.hostPort == 0 || forward.guestPort == 0)
    {
        return "Ports must be from 1 to 65535.";
    }

    for (const PortForward & other : existing)
    {
        if (other.protocol == forward.protocol && other.hostPort == forward.hostPort)
        {
            return std::format ("Host port {} already has a {} port forward.",
                                forward.hostPort,
                                forward.protocol == PortForwardProtocol::Tcp ? "TCP" : "UDP");
        }
    }

    return {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::ProtocolToToken
//
////////////////////////////////////////////////////////////////////////////////

const char * NetworkCardSettings::ProtocolToToken (PortForwardProtocol protocol)
{
    return protocol == PortForwardProtocol::Udp ? "udp" : "tcp";
}





////////////////////////////////////////////////////////////////////////////////
//
//  NetworkCardSettings::TryParseProtocol
//
////////////////////////////////////////////////////////////////////////////////

bool NetworkCardSettings::TryParseProtocol (
    std::string_view       token,
    PortForwardProtocol  & outProtocol)
{
    if (token == "tcp")
    {
        outProtocol = PortForwardProtocol::Tcp;
        return true;
    }

    if (token == "udp")
    {
        outProtocol = PortForwardProtocol::Udp;
        return true;
    }

    return false;
}
