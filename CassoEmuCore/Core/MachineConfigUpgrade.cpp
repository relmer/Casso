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
    HRESULT         hrLegacy = S_OK;
    JsonValue       root;
    JsonParseError  err;
    int             stamp   = 0;



    hr = JsonParser::Parse (content, root, err);
    CHRF (hr, stamp = 0);

    // 007-ui-overhaul : new key is "$cassoMachineVersion";
    // legacy key "$cassoDefault" is read for one upgrade cycle.
    hr = root.GetInt ("$cassoMachineVersion", stamp);

    if (FAILED (hr))
    {
        hrLegacy = root.GetInt ("$cassoDefault", stamp);
        CHRF (hrLegacy, stamp = 0);
    }


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





////////////////////////////////////////////////////////////////////////////////
//
//  MigrateUserConfig
//
//  Per 007-ui-overhaul : rewrites the legacy "$cassoDefault" JSON
//  key to the new "$cassoMachineVersion" name. The defaulting of
//  capabilityFlag on internalDevices / slots entries is performed at
//  load time by MachineConfigLoader (see LoadInternalDevices and
//  LoadSlots), so the textual migration only has to handle the rename.
//
//  Algorithm: a single pass over the source string. We look for the
//  exact byte sequence "\"$cassoDefault\"" and, if the next
//  non-whitespace character is ':' (confirming it's used as an object
//  key, not as a value), substitute "\"$cassoMachineVersion\"". The
//  pass is idempotent because input that already uses the new key
//  contains zero occurrences of the legacy token.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineConfigUpgrade::MigrateUserConfig (
    const string & content,
    string       & outMigrated)
{
    static constexpr string_view  s_kszLegacyKey = "\"$cassoDefault\"";
    static constexpr string_view  s_kszNewKey    = "\"$cassoMachineVersion\"";

    HRESULT  hr             = S_OK;
    size_t   readPos        = 0;
    size_t   matchPos       = 0;
    size_t   afterMatch     = 0;
    size_t   peek           = 0;
    bool     fAnyReplaced   = false;



    outMigrated.clear ();
    outMigrated.reserve (content.size () + 16);

    while (readPos < content.size ())
    {
        matchPos = content.find (s_kszLegacyKey, readPos);

        if (matchPos == string::npos)
        {
            outMigrated.append (content, readPos, string::npos);
            break;
        }

        // Look past the legacy key to confirm the next non-whitespace
        // character is ':' — i.e. the token is being used as an object
        // key. Bare string values that happen to read "$cassoDefault"
        // are left alone.
        afterMatch = matchPos + s_kszLegacyKey.size ();
        peek       = afterMatch;

        while (peek < content.size ()
               && (content[peek] == ' '  || content[peek] == '\t'
               ||  content[peek] == '\r' || content[peek] == '\n'))
        {
            peek++;
        }

        if (peek < content.size () && content[peek] == ':')
        {
            outMigrated.append (content, readPos, matchPos - readPos);
            outMigrated.append (s_kszNewKey);
            readPos      = afterMatch;
            fAnyReplaced = true;
        }
        else
        {
            outMigrated.append (content, readPos, afterMatch - readPos);
            readPos = afterMatch;
        }
    }

    if (!fAnyReplaced)
    {
        // Idempotent path: nothing to do. Preserve byte-for-byte
        // identity by returning the input unchanged and signalling
        // S_FALSE.
        outMigrated = content;
        hr          = S_FALSE;
    }

    return hr;
}