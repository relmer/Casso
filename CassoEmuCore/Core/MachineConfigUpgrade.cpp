#include "Pch.h"

#include "MachineConfigUpgrade.h"
#include "MachineConfig.h"
#include "JsonParser.h"
#include "JsonValue.h"
#include "JsonWriter.h"





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizeBytes
//
//  Strips a UTF-8 BOM and folds CRLF to LF, so a config's content hash
//  identifies its MEANING rather than its byte encoding.
//
//  Both differences are introduced by editors and version-control checkout
//  rules without anyone changing a character of the config. Hashing the raw
//  bytes would classify a file that Notepad merely opened and saved as
//  user-modified, and the upgrade planner would then back it up and refuse to
//  refresh it forever after.
//
//  A bare CR is deliberately left alone: it is not a line ending anything
//  writes these days, and converting it would change the meaning of a config
//  that genuinely contains one.
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
//  Reads a config's version stamp, accepting the current key and, for one
//  upgrade cycle, the legacy one.
//
//  Reading both is what lets a user upgrading from an older release keep their
//  files: a config written before the rename carries only the old key, and
//  treating it as unstamped would send it down the hash-matching path
//  unnecessarily.
//
//  Every failure -- unparseable JSON, neither key present, wrong type --
//  answers 0, which the planner reads as "unstamped" and handles safely. So a
//  corrupt file is never mistaken for a specific version.
//
////////////////////////////////////////////////////////////////////////////////

int MachineConfigUpgrade::ParseStamp (const string & content)
{
    HRESULT         hr       = S_OK;
    HRESULT         hrLegacy = S_OK;
    JsonValue       root;
    JsonParseError  err;
    int             stamp    = 0;



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
//  Decides what to do with a machine config already on disk: leave it, refresh
//  it silently, back it up and replace it, or extract it fresh.
//
//  The whole function answers one question -- HAS THE USER EDITED THIS FILE?
//  An untouched extract can be refreshed silently so improvements reach
//  everyone, while an edited file must never be overwritten without a backup.
//  Version stamps alone cannot answer that, which is why the tests stack up:
//
//    no file            extract
//    newer stamp        skip -- never downgrade a user who is somehow ahead
//    equal stamp AND    skip -- genuinely up to date
//      matching content
//    stamp <= 0, or     overwrite silently -- extracted by an older release
//      an older stamp
//    hash matches a     overwrite silently -- an untouched extract from some
//      known prior          past version, just not this one
//    anything else      back up, then replace
//
//  An equal stamp with DIFFERENT content is the case worth understanding.
//  Parallel feature branches can each ship a different config under the same
//  version number, so an equal stamp proves nothing on its own; such a file
//  falls through to the hash checks rather than being trusted.
//
//  The historical-hash list is what makes silent refresh safe at all. Without
//  it, any file whose stamp does not line up would have to be treated as
//  user-edited, and every user would accumulate backups of files they never
//  touched.
//
//  The default is the most conservative action, so an unanticipated
//  combination costs a backup rather than a user's edits.
//
////////////////////////////////////////////////////////////////////////////////

MachineConfigUpgradeAction MachineConfigUpgrade::Plan (
    string_view                                machineName,
    int                                        embeddedVersion,
    string_view                                embeddedNormalizedHashHex,
    const string                             * diskContent,
    string_view                                diskNormalizedHashHex,
    span<const MachineConfigPriorHash>         priorHashes)
{
    HRESULT                     hr          = S_OK;
    MachineConfigUpgradeAction  action      = MachineConfigUpgradeAction::BackupAndReplace;
    int                         diskVersion = 0;
    bool                        hasDiskHash = false;



    CBRF (diskContent != nullptr, action = MachineConfigUpgradeAction::Extract);

    diskVersion = ParseStamp (*diskContent);

    // Strictly newer stamp: never downgrade the user.
    CBRF (diskVersion <= embeddedVersion, action = MachineConfigUpgradeAction::Skip);

    // Equal stamp: up to date only if the CONTENT matches this build's
    // embedded default. Parallel feature branches can each ship a
    // different config under the same version number, so an equal stamp
    // alone proves nothing; a mismatched file falls through to the hash
    // checks below (known prior extract -> silent refresh, else backup
    // first).
    CBRF (!(diskVersion == embeddedVersion &&
            diskNormalizedHashHex == embeddedNormalizedHashHex),
          action = MachineConfigUpgradeAction::Skip);

    // Stamped but stale: extracted by an older Casso release — safe
    // to overwrite, no backup.
    CBRF (diskVersion <= 0 || diskVersion == embeddedVersion,
          action = MachineConfigUpgradeAction::OverwriteSilent);

    // Unstamped or stamp-collided: hash-match against known historical
    // defaults; any match means the file is an untouched extract and is
    // safe to refresh.
    hasDiskHash = !diskNormalizedHashHex.empty();
    CBR (hasDiskHash);

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
    string             out;



    static const char  s_kchHexDigits[] = "0123456789abcdef";



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
//  JSON-aware single-pass migration of a per-machine `per-machine user JSON`
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
//      3.  `ports[]` injection on a `disk-ii` slot entry that predates the
//          key, giving it the two occupied drive connectors it has always
//          behaved as though it had. Needed because a user delta replaces
//          an array wholesale, so `slots[]` in a user file never picks up
//          the embedded default's new ports through the merge.
//
//  The operation is idempotent: running it on an already-canonical
//  document leaves `outChanged` false with `outMigrated` set to the input
//  bytes verbatim. `outChanged` is true when at least one change was
//  applied (output is freshly serialized JSON, key order otherwise
//  preserved). Returns E_INVALIDARG when the input fails to parse as
//  JSON; `outMigrated` is left empty.
//
////////////////////////////////////////////////////////////////////////////////

int  MachineConfigUpgrade::FindKey (
    const vector<pair<string, JsonValue>> & entries,
    const string                          & key)
{
    int  found = -1;      // -1 == absent
    int  i     = 0;

    for (i = 0; found < 0 && i < (int) entries.size(); ++i)
    {
        if (entries[(size_t) i].first == key)
        {
            found = i;
        }
    }

    return found;
}


bool  MachineConfigUpgrade::EntryHasKey (
    const JsonValue & entry,
    const string    & key)
{
    // A non-object entry has no keys at all -- the short-circuit is what
    // keeps GetObjectEntries() off a non-object.
    return entry.GetType() == JsonType::Object
        && FindKey (entry.GetObjectEntries(), key) >= 0;
}


// Insert `capabilityFlag` on every object element of `arr` that
// lacks one. Returns true if any element was changed.
bool  MachineConfigUpgrade::TryInjectCapabilityFlag (
    JsonValue   & arr,
    const char  * defaultFlag)
{
    vector<JsonValue>  rebuiltArr;
    bool               fChanged = false;
    size_t             i        = 0;



    // A non-array is not a schema error here -- the caller passes whatever the
    // document had under the key, and a missing section arrives as Null.
    if (arr.GetType() == JsonType::Array)
    {
        rebuiltArr.reserve (arr.ArraySize());

        for (i = 0; i < arr.ArraySize(); ++i)
        {
            const JsonValue & elem = arr.ArrayAt (i);

            // Non-objects and entries that already carry a flag pass through
            // untouched -- an existing flag is the user's, not ours to reset.
            if (elem.GetType() != JsonType::Object ||
                EntryHasKey (elem, kpszCapabilityFlagKey))
            {
                rebuiltArr.push_back (elem);
            }
            else
            {
                vector<pair<string, JsonValue>>  rebuilt = elem.GetObjectEntries();

                rebuilt.emplace_back (kpszCapabilityFlagKey,
                                      JsonValue (string (defaultFlag)));
                rebuiltArr.emplace_back (JsonValue (std::move (rebuilt)));
                fChanged = true;
            }
        }

        // Only swap in the rebuild when something actually changed, so an
        // already-canonical document keeps its original JsonValue identity.
        if (fChanged)
        {
            arr = JsonValue (std::move (rebuiltArr));
        }
    }

    return fChanged;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryInjectPrinterSlot
//
//  Add a default slot-1 parallel-printer entry to `arr` when slot 1 has
//  no entry at all. An existing slot-1 entry -- even a disabled one -- is
//  left untouched, so a slot the user turned off is never resurrected
//  (FR-001). Returns true if an entry was appended.
//
////////////////////////////////////////////////////////////////////////////////

bool  MachineConfigUpgrade::TryInjectPrinterSlot (JsonValue & arr)
{
    vector<JsonValue>  rebuilt;
    size_t             i        = 0;
    int                slot     = 0;
    bool               occupied = (arr.GetType() != JsonType::Array);



    // A non-array counts as occupied so nothing is appended to it. Otherwise
    // ANY existing slot-1 entry blocks the injection -- including a disabled
    // one, so a slot the user turned off is never resurrected (FR-001).
    for (i = 0; !occupied && i < arr.ArraySize(); ++i)
    {
        const JsonValue & elem = arr.ArrayAt (i);

        occupied = elem.GetType() == JsonType::Object
                   && elem.HasInt (kpszSlotNumberKey, slot)
                   && slot == kPrinterDefaultSlot;
    }

    if (!occupied)
    {
        vector<pair<string, JsonValue>>  entry;

        rebuilt.reserve (arr.ArraySize() + 1);

        for (i = 0; i < arr.ArraySize(); ++i)
        {
            rebuilt.push_back (arr.ArrayAt (i));
        }

        entry.emplace_back (kpszSlotNumberKey,     JsonValue ((double) kPrinterDefaultSlot));
        entry.emplace_back (kpszDeviceKey,         JsonValue (string (kpszPrinterDevice)));
        entry.emplace_back (kpszCapabilityFlagKey, JsonValue (string (kpszSlotDefault)));
        rebuilt.emplace_back (JsonValue (std::move (entry)));

        arr = JsonValue (std::move (rebuilt));
    }

    return !occupied;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryInjectDiskPorts
//
//  Give a Disk ][ Interface entry that predates `ports` the two drive
//  connectors the real card has, both occupied -- which is the hardware every
//  such config has been emulating all along, now written down instead of
//  assumed. Returns true if any entry was changed.
//
//  THIS EXISTS BECAUSE A USER DELTA REPLACES AN ARRAY WHOLESALE. A user file
//  carrying its own `slots[]` never receives the embedded default's new
//  `ports` through the merge, so without this migration everyone who has
//  touched their slots would silently lose their second drive.
//
//  An entry that already has `ports` is left alone even if the list is empty
//  or disagrees with the card: that list is the user's statement about their
//  own hardware, and a migration that "corrects" it would reattach a drive
//  they detached on purpose -- the same rule TryInjectPrinterSlot follows for
//  a slot the user turned off.
//
////////////////////////////////////////////////////////////////////////////////

bool  MachineConfigUpgrade::TryInjectDiskPorts (JsonValue & arr)
{
    vector<JsonValue>  rebuilt;
    bool               fChanged = false;
    size_t             i        = 0;
    string             device;



    if (arr.GetType() != JsonType::Array)
    {
        return false;
    }

    rebuilt.reserve (arr.ArraySize());

    for (i = 0; i < arr.ArraySize(); ++i)
    {
        const JsonValue & elem     = arr.ArrayAt (i);
        bool              fIsObj   = (elem.GetType() == JsonType::Object);
        HRESULT           hrDevice = E_FAIL;

        device.clear();

        if (fIsObj)
        {
            hrDevice = elem.GetString (kpszDeviceKey, device);
        }

        // Only a Disk ][ card gets drive ports, and only if it has not
        // already spoken for itself.
        if (!fIsObj ||
            EntryHasKey (elem, kpszPortsKey) ||
            FAILED (hrDevice) ||
            device != kpszDiskIiDevice)
        {
            rebuilt.push_back (elem);
        }
        else
        {
            vector<pair<string, JsonValue>>  entry = elem.GetObjectEntries();
            vector<JsonValue>                ports;
            int                              p     = 0;

            for (p = 0; p < kDiskIiPortCount; ++p)
            {
                ports.emplace_back (string (kpszDiskIiDrive));
            }

            entry.emplace_back (kpszPortsKey, JsonValue (std::move (ports)));
            rebuilt.emplace_back (JsonValue (std::move (entry)));
            fChanged = true;
        }
    }

    if (fChanged)
    {
        arr = JsonValue (std::move (rebuilt));
    }

    return fChanged;
}


// Build a new top-level object, applying the version canonicalization
// rule in place. `outChanged` is set to true if anything moved.
JsonValue  MachineConfigUpgrade::RewriteTopLevel (
    const JsonValue & root,
    bool            & outChanged)
{
    vector<pair<string, JsonValue>>  rebuilt;
    const auto                     * entries        = &root.GetObjectEntries();
    int                              idxCanonical   = -1;
    int                              idxLegacy      = -1;
    bool                             fHaveCanonical = false;
    size_t                           i              = 0;



    idxCanonical = FindKey (*entries, kpszVersionKey);
    idxLegacy    = FindKey (*entries, kpszLegacyVersionKey);

    // Canonicalization: drop legacy when canonical already present,
    // rename legacy to canonical when only legacy is present.
    fHaveCanonical = (idxCanonical >= 0);

    rebuilt.reserve (entries->size());

    for (i = 0; i < entries->size(); ++i)
    {
        const string    & key = (*entries)[i].first;
        const JsonValue & val = (*entries)[i].second;

        if (key == kpszVersionKey)
        {
            rebuilt.emplace_back (key, val);
            continue;
        }

        if (key == kpszLegacyVersionKey)
        {
            if (fHaveCanonical)
            {
                // Canonical already wrote — drop the legacy entry.
                outChanged = true;
                continue;
            }

            // Promote the legacy entry to the canonical key in place.
            rebuilt.emplace_back (kpszVersionKey, val);
            outChanged = true;
            continue;
        }

        rebuilt.emplace_back (key, val);
    }

    return JsonValue (std::move (rebuilt));
}


HRESULT MachineConfigUpgrade::MigrateUserConfig (
    const string & content,
    string       & outMigrated,
    bool         & outChanged)
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
    outChanged = false;

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

        idxInternal = FindKey (rebuilt, kpszInternalDevicesKey);
        if (idxInternal >= 0)
        {
            if (TryInjectCapabilityFlag (rebuilt[(size_t) idxInternal].second,
                                      kpszInternalDefault))
            {
                fChanged = true;
            }
        }

        idxSlots = FindKey (rebuilt, kpszSlotsKey);
        if (idxSlots >= 0)
        {
            if (TryInjectCapabilityFlag (rebuilt[(size_t) idxSlots].second,
                                      kpszSlotDefault))
            {
                fChanged = true;
            }

            if (TryInjectPrinterSlot (rebuilt[(size_t) idxSlots].second))
            {
                fChanged = true;
            }

            if (TryInjectDiskPorts (rebuilt[(size_t) idxSlots].second))
            {
                fChanged = true;
            }
        }

        rewritten = JsonValue (std::move (rebuilt));
    }

    if (!fChanged)
    {
        // No-op: hand the input bytes back verbatim, and say so through
        // outChanged rather than through the result code.
        outMigrated = content;
        BAIL_OUT_IF (true, S_OK);
    }

    opts.fPretty = true;
    hr = JsonWriter::Write (rewritten, opts, serialized);
    CHR (hr);

    outMigrated = std::move (serialized);
    outChanged  = true;
    hr          = S_OK;

Error:
    return hr;
}
