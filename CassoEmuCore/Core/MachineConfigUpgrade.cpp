#include "Pch.h"

#include "MachineConfigUpgrade.h"
#include "JsonParser.h"
#include "JsonValue.h"
#include "JsonWriter.h"





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
//  JSON-aware single-pass migration of a per-machine `<Machine>_user.json`
//  document. Handles two distinct schema concerns:
//
//      1.  Version-stamp rename / canonicalization. The legacy key name
//          was `$cassoDefault`. The canonical key is now
//          `$cassoMachineVersion`. If only the legacy key is present, it
//          is renamed. If both are present (a partially-migrated file),
//          the canonical key wins and the legacy key is dropped — the
//          authoritative source of truth is `$cassoMachineVersion`.
//
//      2.  `capabilityFlag` default injection on every object entry of
//          `internalDevices[]` (default `"required"`) and `slots[]`
//          (default `"optional"`). Existing flags are preserved.
//
//  The operation is idempotent: running it on an already-canonical
//  document returns S_FALSE with `outMigrated` set to the input bytes
//  verbatim. Returns S_OK when at least one change was applied (output
//  is freshly serialized JSON, key order otherwise preserved). Returns
//  E_INVALIDARG when the input fails to parse as JSON; `outMigrated`
//  is left empty.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  s_kpszVersionKey         = "$cassoMachineVersion";
    constexpr const char *  s_kpszLegacyVersionKey   = "$cassoDefault";
    constexpr const char *  s_kpszCapabilityFlagKey  = "capabilityFlag";
    constexpr const char *  s_kpszInternalDevicesKey = "internalDevices";
    constexpr const char *  s_kpszSlotsKey           = "slots";
    constexpr const char *  s_kpszInternalDefault    = "required";
    constexpr const char *  s_kpszSlotDefault        = "optional";


    int  FindKey (
        const vector<pair<string, JsonValue>> & entries,
        const string                          & key)
    {
        int  i = 0;

        for (i = 0; i < (int) entries.size(); ++i)
        {
            if (entries[(size_t) i].first == key)
            {
                return i;
            }
        }
        return -1;
    }


    bool  EntryHasKey (
        const JsonValue & entry,
        const string    & key)
    {
        if (entry.GetType() != JsonType::Object)
        {
            return false;
        }
        return FindKey (entry.GetObjectEntries(), key) >= 0;
    }


    // Insert `capabilityFlag` on every object element of `arr` that
    // lacks one. Returns true if any element was changed.
    bool  InjectCapabilityFlag (
        JsonValue   & arr,
        const char  * defaultFlag)
    {
        vector<JsonValue>  rebuiltArr;
        bool               fChanged = false;
        size_t             i        = 0;



        if (arr.GetType() != JsonType::Array)
        {
            return false;
        }

        rebuiltArr.reserve (arr.ArraySize());

        for (i = 0; i < arr.ArraySize(); ++i)
        {
            const JsonValue & elem = arr.ArrayAt (i);

            if (elem.GetType() != JsonType::Object ||
                EntryHasKey (elem, s_kpszCapabilityFlagKey))
            {
                rebuiltArr.push_back (elem);
                continue;
            }

            vector<pair<string, JsonValue>>  rebuilt = elem.GetObjectEntries();
            rebuilt.emplace_back (s_kpszCapabilityFlagKey,
                                  JsonValue (string (defaultFlag)));
            rebuiltArr.emplace_back (JsonValue (std::move (rebuilt)));
            fChanged = true;
        }

        if (fChanged)
        {
            arr = JsonValue (std::move (rebuiltArr));
        }
        return fChanged;
    }


    // Build a new top-level object, applying the version canonicalization
    // rule in place. `outChanged` is set to true if anything moved.
    JsonValue  RewriteTopLevel (
        const JsonValue & root,
        bool            & outChanged)
    {
        vector<pair<string, JsonValue>>  rebuilt;
        const auto                     * entries        = &root.GetObjectEntries();
        int                              idxCanonical   = -1;
        int                              idxLegacy      = -1;
        bool                             fHaveCanonical = false;
        size_t                           i              = 0;



        idxCanonical = FindKey (*entries, s_kpszVersionKey);
        idxLegacy    = FindKey (*entries, s_kpszLegacyVersionKey);

        // Canonicalization: drop legacy when canonical already present,
        // rename legacy to canonical when only legacy is present.
        fHaveCanonical = (idxCanonical >= 0);

        rebuilt.reserve (entries->size());

        for (i = 0; i < entries->size(); ++i)
        {
            const string    & key = (*entries)[i].first;
            const JsonValue & val = (*entries)[i].second;

            if (key == s_kpszVersionKey)
            {
                rebuilt.emplace_back (key, val);
                continue;
            }

            if (key == s_kpszLegacyVersionKey)
            {
                if (fHaveCanonical)
                {
                    // Canonical already wrote — drop the legacy entry.
                    outChanged = true;
                    continue;
                }

                // Promote the legacy entry to the canonical key in place.
                rebuilt.emplace_back (s_kpszVersionKey, val);
                outChanged = true;
                continue;
            }

            rebuilt.emplace_back (key, val);
        }

        return JsonValue (std::move (rebuilt));
    }
}


HRESULT MachineConfigUpgrade::MigrateUserConfig (
    const string & content,
    string       & outMigrated)
{
    HRESULT              hr           = S_OK;
    JsonValue            root;
    JsonParseError       err;
    JsonValue            rewritten;
    bool                 fChanged     = false;
    JsonWriter::Options  opts;
    string               serialized;
    int                  idxInternal  = -1;
    int                  idxSlots     = -1;



    outMigrated.clear();

    hr = JsonParser::Parse (content, root, err);
    BAIL_OUT_IF (FAILED (hr), E_INVALIDARG);

    if (root.GetType() != JsonType::Object)
    {
        hr = E_INVALIDARG;
        CHR (hr);
    }

    rewritten = RewriteTopLevel (root, fChanged);

    {
        vector<pair<string, JsonValue>>  rebuilt = rewritten.GetObjectEntries();

        idxInternal = FindKey (rebuilt, s_kpszInternalDevicesKey);
        if (idxInternal >= 0)
        {
            if (InjectCapabilityFlag (rebuilt[(size_t) idxInternal].second,
                                      s_kpszInternalDefault))
            {
                fChanged = true;
            }
        }

        idxSlots = FindKey (rebuilt, s_kpszSlotsKey);
        if (idxSlots >= 0)
        {
            if (InjectCapabilityFlag (rebuilt[(size_t) idxSlots].second,
                                      s_kpszSlotDefault))
            {
                fChanged = true;
            }
        }

        rewritten = JsonValue (std::move (rebuilt));
    }

    if (!fChanged)
    {
        // No-op: hand the input bytes back verbatim so callers can
        // detect "unchanged" via byte equality.
        outMigrated = content;
        hr          = S_FALSE;
        goto Error;
    }

    opts.fPretty = true;
    hr = JsonWriter::Write (rewritten, opts, serialized);
    CHR (hr);

    outMigrated = std::move (serialized);
    hr          = S_OK;

Error:
    return hr;
}