#include "Pch.h"

#include "MachineConfigUpgrade.h"
#include "JsonParser.h"
#include "JsonValue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeBytes
//
////////////////////////////////////////////////////////////////////////////////

string MachineConfigUpgrade::NormalizeBytes (const string & content)
{
    string  normalized;
    size_t  start = 0;



    normalized.reserve (content.size());

    if (content.size() >= 3
        && static_cast<uint8_t> (content[0]) == 0xEF
        && static_cast<uint8_t> (content[1]) == 0xBB
        && static_cast<uint8_t> (content[2]) == 0xBF)
    {
        start = 3;
    }

    for (size_t i = start; i < content.size(); i++)
    {
        if (content[i] == '\r' && i + 1 < content.size() && content[i + 1] == '\n')
        {
            normalized.push_back ('\n');
            i++;
        }
        else
        {
            normalized.push_back (content[i]);
        }
    }

    return normalized;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParseStamp
//
////////////////////////////////////////////////////////////////////////////////

int MachineConfigUpgrade::ParseStamp (const string & content)
{
    HRESULT         hr      = S_OK;
    JsonValue       root;
    JsonParseError  err;
    int             stamp   = 0;



    hr = JsonParser::Parse (content, root, err);
    CHRF (hr, stamp = 0);

    hr = root.GetInt ("$cassoDefault", stamp);
    CHRF (hr, stamp = 0);


Error:
    return stamp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Plan
//
////////////////////////////////////////////////////////////////////////////////

MachineConfigUpgradeAction MachineConfigUpgrade::Plan (
    string_view                                machineName,
    int                                        embeddedVersion,
    const string                             * diskContent,
    string_view                                diskNormalizedHashHex,
    span<const MachineConfigPriorHash>         priorHashes)
{
    HRESULT                     hr          = S_OK;
    MachineConfigUpgradeAction  action      = MachineConfigUpgradeAction::BackupAndReplace;
    int                         diskVersion = 0;



    CBRF (diskContent != nullptr, action = MachineConfigUpgradeAction::Extract);

    diskVersion = ParseStamp (*diskContent);
    CBRF (diskVersion < embeddedVersion, action = MachineConfigUpgradeAction::Skip);

    // Stamped but stale: extracted by an older Casso release — safe
    // to overwrite, no backup.
    CBRF (diskVersion <= 0, action = MachineConfigUpgradeAction::OverwriteSilent);

    // Unstamped: hash-match against known historical defaults; any
    // match means the file is an untouched extract and is safe to
    // refresh.
    CBR (!diskNormalizedHashHex.empty());

    for (const MachineConfigPriorHash & p : priorHashes)
    {
        CBRF (!(p.machineName == machineName && p.hashHex == diskNormalizedHashHex),
              action = MachineConfigUpgradeAction::OverwriteSilent);
    }


Error:
    return action;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BytesToHex
//
////////////////////////////////////////////////////////////////////////////////

string MachineConfigUpgrade::BytesToHex (span<const uint8_t> bytes)
{
    static const char  s_kchHexDigits[] = "0123456789abcdef";
    string             out;



    out.reserve (bytes.size() * 2);

    for (uint8_t b : bytes)
    {
        out.push_back (s_kchHexDigits[(b >> 4) & 0x0F]);
        out.push_back (s_kchHexDigits[ b       & 0x0F]);
    }

    return out;
}