#include "Pch.h"

#include "SettingsPanelState.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  s_kpszUiPrefsKey      = "$cassoUiPrefs";
    constexpr const char *  s_kpszVersionKey      = "$cassoMachineVersion";


    int  FindKey (
        const std::vector<std::pair<std::string, JsonValue>> & entries,
        const std::string                                    & key)
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


    bool  GetBoolOpt (
        const JsonValue   & obj,
        const std::string & key,
        bool                fallback)
    {
        bool      out = fallback;
        HRESULT   hr  = obj.GetBool (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    std::string  GetStringOpt (
        const JsonValue   & obj,
        const std::string & key,
        const std::string & fallback)
    {
        std::string  out = fallback;
        HRESULT      hr  = obj.GetString (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    int  GetIntOpt (
        const JsonValue   & obj,
        const std::string & key,
        int                 fallback)
    {
        int      out = fallback;
        HRESULT  hr  = obj.GetInt (key, out);
        if (FAILED (hr))
        {
            return fallback;
        }
        return out;
    }


    CapabilityFlag  ParseCapability (
        const std::string & str,
        CapabilityFlag      fallback)
    {
        if (str == "optional")        return CapabilityFlag::Optional;
        if (str == "required")        return CapabilityFlag::Required;
        if (str == "platform-locked") return CapabilityFlag::PlatformLocked;
        return fallback;
    }


    [[maybe_unused]] const char *  CapabilityToString (CapabilityFlag c)
    {
        switch (c)
        {
            case CapabilityFlag::Optional:       return "optional";
            case CapabilityFlag::Required:       return "required";
            case CapabilityFlag::PlatformLocked: return "platform-locked";
        }
        return "optional";
    }


    const char *  SpeedToString (SettingsSpeedMode s)
    {
        switch (s)
        {
            case SettingsSpeedMode::Authentic: return "authentic";
            case SettingsSpeedMode::Double:    return "double";
            case SettingsSpeedMode::Maximum:   return "maximum";
        }
        return "authentic";
    }


    SettingsSpeedMode  SpeedFromString (
        const std::string & s,
        SettingsSpeedMode   fallback)
    {
        if (s == "authentic") return SettingsSpeedMode::Authentic;
        if (s == "double")    return SettingsSpeedMode::Double;
        if (s == "maximum")   return SettingsSpeedMode::Maximum;
        return fallback;
    }


    const char *  ColorToString (SettingsColorMode c)
    {
        switch (c)
        {
            case SettingsColorMode::Color: return "color";
            case SettingsColorMode::Green: return "green";
            case SettingsColorMode::Amber: return "amber";
            case SettingsColorMode::White: return "white";
        }
        return "color";
    }


    SettingsColorMode  ColorFromString (
        const std::string & s,
        SettingsColorMode   fallback)
    {
        if (s == "color") return SettingsColorMode::Color;
        if (s == "green") return SettingsColorMode::Green;
        if (s == "amber") return SettingsColorMode::Amber;
        if (s == "white") return SettingsColorMode::White;
        return fallback;
    }


    // Deep-copy a JsonValue by writing+re-parsing. Cheap enough for the
    // settings panel snapshot (one-time at Show()) and avoids needing
    // a public clone API on JsonValue.
    JsonValue  CloneJson (const JsonValue & v)
    {
        std::string          text;
        JsonWriter::Options  opts;
        JsonParseError       err;
        JsonValue            out;

        opts.fPretty = false;

        if (FAILED (JsonWriter::Write (v, opts, text)))
        {
            return JsonValue();
        }
        if (FAILED (JsonParser::Parse (text, out, err)))
        {
            return JsonValue();
        }
        return out;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SettingsPanelState
//
////////////////////////////////////////////////////////////////////////////////

SettingsPanelState::SettingsPanelState()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  LoadFromMachine
//
//  Reload the snapshot from a freshly-merged machine config.
//  `mergedJson` is the result of `UserConfigStore::Load` (or
//  equivalently `MergeJson (defaultJson, userJson)`). `defaultJson`
//  is the unmerged embedded default for the machine; it is kept so
//  `Apply` can diff against it for `SaveDelta`. Both must be JSON
//  objects -- anything else returns E_INVALIDARG.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::LoadFromMachine (
    const std::string  & machineName,
    const JsonValue    & defaultJson,
    const JsonValue    & mergedJson)
{
    HRESULT  hr = S_OK;


    CBR (defaultJson.GetType() == JsonType::Object);
    CBR (mergedJson .GetType() == JsonType::Object);

    m_machineName = machineName;
    m_defaultJson = CloneJson (defaultJson);
    m_mergedJson  = CloneJson (mergedJson);

    m_original = Snapshot {};

    hr = ExtractUiPrefs (mergedJson, m_original.prefs);
    CHR (hr);

    hr = ExtractHardware (mergedJson, m_original.hardware);
    CHR (hr);

    m_current = m_original;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Cancel
//
//  Reset `current` to match `original` (used by the Cancel button
//  and implicitly by `LoadFromMachine`).
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::Cancel()
{
    m_current = m_original;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDirty
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::IsDirty() const
{
    if (! PrefsEqual (m_original.prefs, m_current.prefs))
    {
        return true;
    }
    if (! HardwareEqual (m_original.hardware, m_current.hardware))
    {
        return true;
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  RequiresReset
//
//  Per FR-010: a reset is required iff the hardware-enable bits
//  changed. Speed / color / sound / mechanism / write-protect are
//  all live-applicable.
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::RequiresReset() const
{
    size_t  i = 0;



    if (m_original.hardware.size() != m_current.hardware.size())
    {
        return true;
    }
    for (i = 0; i < m_current.hardware.size(); ++i)
    {
        if (m_original.hardware[i].enabled != m_current.hardware[i].enabled)
        {
            return true;
        }
    }
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Mutators
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetSpeedMode (SettingsSpeedMode mode)
{
    m_current.prefs.speedMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetColorMode
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetColorMode (SettingsColorMode mode)
{
    m_current.prefs.colorMode = mode;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetFloppySound
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetFloppySound (bool enabled)
{
    m_current.prefs.floppySoundEnabled = enabled;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetMechanism
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetMechanism (const std::string & mechanism)
{
    m_current.prefs.floppyMechanism = mechanism;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetWriteProtect
//
////////////////////////////////////////////////////////////////////////////////

void SettingsPanelState::SetWriteProtect (int drive, bool wp)
{
    if (drive < 0 || drive >= 2)
    {
        return;
    }
    m_current.prefs.writeProtect[drive] = wp;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetHardwareEnabled
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::SetHardwareEnabled (size_t index, bool enabled)
{
    HRESULT  hr = S_OK;



    CBR (index < m_current.hardware.size());

    if (! enabled)
    {
        // FR-007 / FR-008: required and platform-locked entries
        // cannot be turned off. User input -- non-asserting.
        CBR (m_current.hardware[index].capability == CapabilityFlag::Optional);
    }

    m_current.hardware[index].enabled = enabled;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Apply
//
//  Pushes the live-applicable diffs through `sink` and emits the
//  updated machine JSON ready for `UserConfigStore::SaveDelta`.
//  Always emits `outCurrentJson` (even when nothing changed) so
//  callers can drop the result straight into SaveDelta.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::Apply (
    ISettingsApplySink & sink,
    JsonValue          & outCurrentJson) const
{
    HRESULT  hr = S_OK;
    int      i  = 0;


    // Live-effect fields (FR-011 -- always pushed; cheap, idempotent).
    sink.ApplySpeedMode   (m_current.prefs.speedMode);
    sink.ApplyColorMode   (m_current.prefs.colorMode);
    sink.ApplyFloppySound (m_current.prefs.floppySoundEnabled);
    sink.ApplyMechanism   (m_current.prefs.floppyMechanism);
    for (i = 0; i < 2; ++i)
    {
        sink.ApplyWriteProtect (i, m_current.prefs.writeProtect[i]);
    }

    // FR-010: any hardware enable diff requires the caller to confirm
    // and the machine to be reset. Queue the reset request; the
    // production sink (EmulatorShell) gates it behind a modal confirm.
    if (RequiresReset())
    {
        sink.QueueMachineReset();
    }

    outCurrentJson = BuildJson (m_mergedJson, m_current.hardware, m_current.prefs);

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractUiPrefs
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractUiPrefs (
    const JsonValue   & mergedJson,
    SettingsUiPrefs   & outPrefs)
{
    HRESULT             hr     = S_OK;
    const JsonValue *   uiObj  = nullptr;
    const JsonValue *   wpArr  = nullptr;
    size_t              i      = 0;


    CBR (mergedJson.GetType() == JsonType::Object);

    outPrefs = SettingsUiPrefs {};

    if (FAILED (mergedJson.GetObject (s_kpszUiPrefsKey, uiObj)) || uiObj == nullptr)
    {
        // No $cassoUiPrefs in the file -- struct defaults stand.
        return S_OK;
    }

    outPrefs.speedMode = SpeedFromString (
        GetStringOpt (*uiObj, "speedMode", "authentic"),
        SettingsSpeedMode::Authentic);

    outPrefs.colorMode = ColorFromString (
        GetStringOpt (*uiObj, "colorMode", "color"),
        SettingsColorMode::Color);

    outPrefs.floppySoundEnabled = GetBoolOpt   (*uiObj, "floppySoundEnabled", true);
    outPrefs.floppyMechanism    = GetStringOpt (*uiObj, "floppyMechanism",    "shugart");

    if (SUCCEEDED (uiObj->GetArray ("writeProtect", wpArr)) && wpArr != nullptr)
    {
        for (i = 0; i < wpArr->ArraySize() && i < 2; ++i)
        {
            const JsonValue & entry = wpArr->ArrayAt (i);
            if (entry.GetType() == JsonType::Bool)
            {
                outPrefs.writeProtect[i] = entry.GetBool();
            }
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ExtractHardware
//
//  Walks a merged machine JSON object and pulls out the typed
//  hardware-tree representation. Slot/internal-device order is
//  preserved. Default capability per FR-015: internal devices ->
//  Required, slot entries -> Optional. JSON-level overrides
//  (`capabilityFlag` + `lockReason`) win where present.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT SettingsPanelState::ExtractHardware (
    const JsonValue              & mergedJson,
    std::vector<HardwareEntry>   & outEntries)
{
    static const struct { const char * id; const char * display; }  s_kDeviceDisplayNames[] =
    {
        { "disk-ii",                 "Disk ][" },
        { "smartport",               "SmartPort" },
        { "mockingboard",            "Mockingboard" },
        { "passport",                "Passport MIDI" },
        { "serial",                  "Super Serial Card" },
        { "parallel",                "Parallel Printer" },
        { "videx",                   "Videx 80-Column" },
        { "ramworks",                "RamWorks" },
        { "ramfactor",               "RAMFactor" },
        { "saturn128",               "Saturn 128K" },
        { "language-card",           "Language Card" },
        { "extended-80-column",      "Extended 80-Column Card" },
        { "cassette",                "Cassette" },
        { "speaker",                 "Speaker" },
        { "keyboard",                "Keyboard" },
        { "joystick",                "Joystick" },
        { "paddle",                  "Paddle" },
        { "monitor",                 "Monitor" },
    };

    HRESULT             hr       = S_OK;
    const JsonValue *   devArr   = nullptr;
    const JsonValue *   slotArr  = nullptr;
    size_t              i        = 0;
    size_t              j        = 0;


    CBR (mergedJson.GetType() == JsonType::Object);

    outEntries.clear();

    if (SUCCEEDED (mergedJson.GetArray ("internalDevices", devArr)) && devArr != nullptr)
    {
        for (i = 0; i < devArr->ArraySize(); ++i)
        {
            const JsonValue & entry = devArr->ArrayAt (i);
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            HardwareEntry  hw;
            std::string    devType = GetStringOpt (entry, "type", "");
            std::string    friendly = devType;

            for (j = 0; j < sizeof (s_kDeviceDisplayNames) / sizeof (s_kDeviceDisplayNames[0]); ++j)
            {
                if (devType == s_kDeviceDisplayNames[j].id)
                {
                    friendly = s_kDeviceDisplayNames[j].display;
                    break;
                }
            }

            hw.kind        = HardwareEntryKind::InternalDevice;
            hw.jsonIndex   = (int) i;
            hw.slot        = 0;
            hw.type        = devType;
            hw.displayName = friendly;
            hw.capability  = ParseCapability (
                GetStringOpt (entry, "capabilityFlag", ""),
                CapabilityFlag::Required);   // FR-015 default for internal
            hw.lockReason  = GetStringOpt (entry, "lockReason", "");
            hw.enabled     = GetBoolOpt   (entry, "enabled",    true);

            outEntries.push_back (std::move (hw));
        }
    }

    if (SUCCEEDED (mergedJson.GetArray ("slots", slotArr)) && slotArr != nullptr)
    {
        for (i = 0; i < slotArr->ArraySize(); ++i)
        {
            const JsonValue & entry = slotArr->ArrayAt (i);
            if (entry.GetType() != JsonType::Object)
            {
                continue;
            }

            HardwareEntry  hw;
            int            slotNum  = GetIntOpt (entry, "slot", 0);
            std::string    dev      = GetStringOpt (entry, "device", "");
            std::string    devNice  = dev;

            for (j = 0; j < sizeof (s_kDeviceDisplayNames) / sizeof (s_kDeviceDisplayNames[0]); ++j)
            {
                if (dev == s_kDeviceDisplayNames[j].id)
                {
                    devNice = s_kDeviceDisplayNames[j].display;
                    break;
                }
            }

            hw.kind        = HardwareEntryKind::Slot;
            hw.jsonIndex   = (int) i;
            hw.slot        = slotNum;
            hw.type        = dev;
            hw.displayName = std::string ("Slot ") + std::to_string (slotNum)
                           + ": " + (dev.empty() ? std::string ("(rom only)") : devNice);
            hw.capability  = ParseCapability (
                GetStringOpt (entry, "capabilityFlag", ""),
                CapabilityFlag::Optional);   // FR-015 default for slots
            hw.lockReason  = GetStringOpt (entry, "lockReason", "");
            hw.enabled     = GetBoolOpt   (entry, "enabled",    true);

            outEntries.push_back (std::move (hw));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildJson
//
//  Returns a brand-new object with:
//
//      * every key from `mergedJson` preserved verbatim
//        EXCEPT internalDevices / slots / $cassoUiPrefs
//      * internalDevices / slots rebuilt from the matching entries of
//        `mergedJson` with the `enabled` bit overlaid from `hw`
//      * $cassoUiPrefs emitted from `prefs`
//
////////////////////////////////////////////////////////////////////////////////

JsonValue SettingsPanelState::BuildJson (
    const JsonValue                       & mergedJson,
    const std::vector<HardwareEntry>      & hw,
    const SettingsUiPrefs                 & prefs)
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  uiObj;
    std::vector<JsonValue>                          wpArr;
    std::vector<JsonValue>                          devArr;
    std::vector<JsonValue>                          slotArr;
    const std::vector<std::pair<std::string, JsonValue>> *  entries = nullptr;
    size_t                                          i = 0;


    if (mergedJson.GetType() != JsonType::Object)
    {
        return JsonValue();
    }

    entries = &mergedJson.GetObjectEntries();

    for (i = 0; i < entries->size(); ++i)
    {
        const std::string & key = (*entries)[i].first;
        const JsonValue   & val = (*entries)[i].second;

        if (key == "internalDevices" || key == "slots" || key == s_kpszUiPrefsKey)
        {
            continue;
        }
        root.emplace_back (key, CloneJson (val));
    }

    // Rebuild internalDevices preserving original per-entry JSON
    // (so unrelated fields like "type" / "lockReason" round-trip).
    if (mergedJson.GetType() == JsonType::Object)
    {
        const JsonValue *  devSrc  = nullptr;
        const JsonValue *  slotSrc = nullptr;

        if (SUCCEEDED (mergedJson.GetArray ("internalDevices", devSrc)) && devSrc != nullptr)
        {
            for (i = 0; i < devSrc->ArraySize(); ++i)
            {
                const JsonValue & src = devSrc->ArrayAt (i);
                if (src.GetType() != JsonType::Object)
                {
                    devArr.push_back (CloneJson (src));
                    continue;
                }

                // Find matching hw entry by (kind, jsonIndex).
                bool   found        = false;
                bool   enabledFlag  = true;
                size_t h            = 0;
                for (h = 0; h < hw.size(); ++h)
                {
                    if (hw[h].kind == HardwareEntryKind::InternalDevice &&
                        hw[h].jsonIndex == (int) i)
                    {
                        enabledFlag = hw[h].enabled;
                        found       = true;
                        break;
                    }
                }
                (void) found;

                std::vector<std::pair<std::string, JsonValue>>  rebuilt;
                const auto &  srcEntries = src.GetObjectEntries();
                size_t        j          = 0;
                for (j = 0; j < srcEntries.size(); ++j)
                {
                    if (srcEntries[j].first == "enabled")
                    {
                        continue;
                    }
                    rebuilt.emplace_back (srcEntries[j].first, CloneJson (srcEntries[j].second));
                }
                rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));
                devArr.emplace_back (JsonValue (std::move (rebuilt)));
            }
        }

        if (SUCCEEDED (mergedJson.GetArray ("slots", slotSrc)) && slotSrc != nullptr)
        {
            for (i = 0; i < slotSrc->ArraySize(); ++i)
            {
                const JsonValue & src = slotSrc->ArrayAt (i);
                if (src.GetType() != JsonType::Object)
                {
                    slotArr.push_back (CloneJson (src));
                    continue;
                }

                bool   enabledFlag = true;
                size_t h           = 0;
                for (h = 0; h < hw.size(); ++h)
                {
                    if (hw[h].kind == HardwareEntryKind::Slot &&
                        hw[h].jsonIndex == (int) i)
                    {
                        enabledFlag = hw[h].enabled;
                        break;
                    }
                }

                std::vector<std::pair<std::string, JsonValue>>  rebuilt;
                const auto &  srcEntries = src.GetObjectEntries();
                size_t        j          = 0;
                for (j = 0; j < srcEntries.size(); ++j)
                {
                    if (srcEntries[j].first == "enabled")
                    {
                        continue;
                    }
                    rebuilt.emplace_back (srcEntries[j].first, CloneJson (srcEntries[j].second));
                }
                rebuilt.emplace_back ("enabled", JsonValue (enabledFlag));
                slotArr.emplace_back (JsonValue (std::move (rebuilt)));
            }
        }
    }

    if (! devArr.empty())
    {
        root.emplace_back ("internalDevices", JsonValue (std::move (devArr)));
    }
    if (! slotArr.empty())
    {
        root.emplace_back ("slots", JsonValue (std::move (slotArr)));
    }

    // $cassoUiPrefs block
    uiObj.emplace_back ("speedMode",          JsonValue (std::string (SpeedToString (prefs.speedMode))));
    uiObj.emplace_back ("colorMode",          JsonValue (std::string (ColorToString (prefs.colorMode))));
    uiObj.emplace_back ("floppySoundEnabled", JsonValue (prefs.floppySoundEnabled));
    uiObj.emplace_back ("floppyMechanism",    JsonValue (prefs.floppyMechanism));

    wpArr.emplace_back (JsonValue (prefs.writeProtect[0]));
    wpArr.emplace_back (JsonValue (prefs.writeProtect[1]));
    uiObj.emplace_back ("writeProtect", JsonValue (std::move (wpArr)));

    root.emplace_back (s_kpszUiPrefsKey, JsonValue (std::move (uiObj)));

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  PrefsEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::PrefsEqual (
    const SettingsUiPrefs & a,
    const SettingsUiPrefs & b)
{
    if (a.speedMode             != b.speedMode)             return false;
    if (a.colorMode             != b.colorMode)             return false;
    if (a.floppySoundEnabled    != b.floppySoundEnabled)    return false;
    if (a.floppyMechanism       != b.floppyMechanism)       return false;
    if (a.writeProtect[0]       != b.writeProtect[0])       return false;
    if (a.writeProtect[1]       != b.writeProtect[1])       return false;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HardwareEqual
//
////////////////////////////////////////////////////////////////////////////////

bool SettingsPanelState::HardwareEqual (
    const std::vector<HardwareEntry> & a,
    const std::vector<HardwareEntry> & b)
{
    size_t  i = 0;



    if (a.size() != b.size())
    {
        return false;
    }
    for (i = 0; i < a.size(); ++i)
    {
        if (a[i].kind       != b[i].kind)       return false;
        if (a[i].jsonIndex  != b[i].jsonIndex)  return false;
        if (a[i].slot       != b[i].slot)       return false;
        if (a[i].type       != b[i].type)       return false;
        if (a[i].capability != b[i].capability) return false;
        if (a[i].enabled    != b[i].enabled)    return false;
    }
    return true;
}
