#include "Pch.h"

#include "GlobalUserPrefs.h"


#include "MachineInputPrefs.h"
#include "Capture/ScreenshotMode.h"
#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"

#include "CrtResolver.h"





////////////////////////////////////////////////////////////////////////////////
//
//  File-local aliases, constants, and CRT field tables
//
////////////////////////////////////////////////////////////////////////////////

// A JSON object body is an ordered list of key/value members. Insertion
// order is preserved so serialized output is deterministic across
// roundtrips.
using KeyValuePair = std::pair<std::string, JsonValue>;
using JsonObject   = std::vector<KeyValuePair>;


static constexpr const char *  s_kpszVersionKey  = "$cassoGlobalPrefsVersion";
static constexpr int           s_kCurrentVersion = 1;


// Legacy v1 crt sub-object key per monitor type, indexed by SettingsColorMode.
// Read by the v1 conversion only; nothing writes these any more.
static constexpr const char *  s_kpszCrtModeKeys[kCrtModeCount] = {
    "color", "green", "amber", "white"
};


// The monitors the v1-era catalog held, frozen against that history. The
// conversion fans an adopted v1 block onto both, and deliberately does NOT
// read s_kMonitors: an eight-monitor build must not spread a //e-era user's
// tuning onto a Commodore tube they have never booted.
static constexpr const char *  s_kpszV1Monitors[] = { "AppleMonitorII", "AppleMonitorIIc" };


// Known top-level keys recognized by this version of GlobalUserPrefs.
// Anything not in this list is preserved in `unknownPassthrough`.
static const std::set<std::string>  s_kKnownTopLevel = {
    "$cassoGlobalPrefsVersion",
    "activeTheme",
    "crtMonitor",
    "showFrameRate",
    "showSceneView",
    "sceneAntiAliasing",
    "lastSelectedMachine",
    "lastDiskCreateFolder",
    "audioDownloadConsent",
    "inputMappingMode",
    "arrowsToJoystick",
    "pointerMapping",
    "mapArrowsToJoystick",
    "colorMonitorTextMode",
    "colorMonitorTextCustom",
    "recentDisks",
    "recentDiskLoadedAt",
    "crt",                        // legacy v1 block; consumed by the conversion, no longer emitted
    "crtOverrides",
    "monitorTilt",
    "window",
    "printOutputDpi",
    "printDotStyle",
    "printerAudioEnabled",
    "printerAudioMuted",          // legacy (pre-toggle); consumed, no longer emitted
    "printerAudioVolume",
    "printerAudioPanOverride",
    "printerAudioPan",
    "masterVolume",
    "masterMuted",
    "screenshotMode",
    "screenshotSaveFile",
    "screenshotFolder"
};


// Serialized string tokens for ColorMonitorTextMode.
static constexpr const char *  s_kpszTextModeWhite  = "white";
static constexpr const char *  s_kpszTextModeGreen  = "green";
static constexpr const char *  s_kpszTextModeAmber  = "amber";
static constexpr const char *  s_kpszTextModeCustom = "custom";


enum class CrtScalar
{
    Bool,
    Float
};


// Which slice of the serialized mode object a field belongs to. Top fields
// sit directly on the mode object; the rest live in a nested sub-object
// named by s_kpszCrtGroupKeys.
enum class CrtGroup
{
    Top,
    Scanlines,
    Bloom,
    ColorBleed
};




// JSON sub-object key per group, indexed by CrtGroup. Top has no key
// because its fields serialize directly onto the mode object.
static constexpr const char *  s_kpszCrtGroupKeys[] = {
    nullptr, "scanlines", "bloom", "colorBleed"
};

static constexpr size_t  s_kcCrtGroup = _countof (s_kpszCrtGroupKeys);



// One scalar CRT field: which group it serializes into, its JSON key, type,
// a pointer-to-member into CrtOverrides, and (floats only) the
// inclusive clamp range applied on load so a hand-edited prefs file can't
// drive the shaders out of range. The unused member pointer is null. Row
// order within a group is the serialized key order.
struct CrtFieldDesc
{
    CrtGroup                                group;
    const char                            * key;
    CrtScalar                               type;
    std::optional<bool> CrtOverrides::*     boolMember;
    std::optional<float> CrtOverrides::*    floatMember;
    float                                   lo;
    float                                   hi;
};


static constexpr CrtFieldDesc  s_kCrtFields[] = {
    { CrtGroup::Top,        "brightness",   CrtScalar::Float, nullptr,                             &CrtOverrides::brightness,         0.0f, 2.0f  },
    { CrtGroup::Top,        "contrast",     CrtScalar::Float, nullptr,                             &CrtOverrides::contrast,           0.0f, 2.0f  },
    { CrtGroup::Top,        "gamma",        CrtScalar::Float, nullptr,                             &CrtOverrides::gamma,              0.5f, 2.5f  },
    { CrtGroup::Top,        "persistence",  CrtScalar::Float, nullptr,                             &CrtOverrides::persistence,        0.0f, 0.99f },
    { CrtGroup::Scanlines,  "enabled",      CrtScalar::Bool,  &CrtOverrides::scanlinesEnabled,     nullptr,                           0.0f, 0.0f  },
    { CrtGroup::Scanlines,  "intensity",    CrtScalar::Float, nullptr,                             &CrtOverrides::scanlinesIntensity, 0.0f, 1.0f  },
    { CrtGroup::Bloom,      "enabled",      CrtScalar::Bool,  &CrtOverrides::bloomEnabled,         nullptr,                           0.0f, 0.0f  },
    { CrtGroup::Bloom,      "radius",       CrtScalar::Float, nullptr,                             &CrtOverrides::bloomRadius,        0.0f, 4.0f  },
    { CrtGroup::Bloom,      "strength",     CrtScalar::Float, nullptr,                             &CrtOverrides::bloomStrength,      0.0f, 1.0f  },
    { CrtGroup::ColorBleed, "enabled",      CrtScalar::Bool,  &CrtOverrides::colorBleedEnabled,    nullptr,                           0.0f, 0.0f  },
    { CrtGroup::ColorBleed, "width",        CrtScalar::Float, nullptr,                             &CrtOverrides::colorBleedWidth,    0.0f, 8.0f  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::TryGetBoolOpt
//
//  Read an optional boolean leaf, falling back to `fallback` when absent or
//  not a boolean.
//
////////////////////////////////////////////////////////////////////////////////

bool GlobalUserPrefs::TryGetBoolOpt (
    const JsonValue   & obj,
    const std::string & key,
    bool                fallback)
{
    HRESULT  hr     = S_OK;
    bool     result = fallback;



    hr = obj.GetBool (key, result);
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::GetNumberOpt
//
//  Read an optional numeric leaf, falling back to `fallback` when absent or
//  not a number.
//
////////////////////////////////////////////////////////////////////////////////

double GlobalUserPrefs::GetNumberOpt (
    const JsonValue   & obj,
    const std::string & key,
    double              fallback)
{
    HRESULT  hr     = S_OK;
    double   result = fallback;



    hr = obj.GetNumber (key, result);
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::GetIntOpt
//
//  Read an optional integer leaf, falling back to `fallback` when absent or
//  not an integer.
//
////////////////////////////////////////////////////////////////////////////////

int GlobalUserPrefs::GetIntOpt (
    const JsonValue   & obj,
    const std::string & key,
    int                 fallback)
{
    HRESULT  hr     = S_OK;
    int      result = fallback;



    hr = obj.GetInt (key, result);
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::GetStringOpt
//
//  Read an optional string leaf, falling back to `fallback` when absent or
//  not a string.
//
////////////////////////////////////////////////////////////////////////////////

std::string GlobalUserPrefs::GetStringOpt (
    const JsonValue   & obj,
    const std::string & key,
    const std::string & fallback)
{
    HRESULT      hr     = S_OK;
    std::string  result = fallback;



    hr = obj.GetString (key, result);
    CHR (hr);

Error:
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::CrtOverridesToJson
//
//  Serialize one pair's overrides, table-driven so the emitted key order
//  matches s_kCrtFields exactly.
//
//  SPARSE: a field the user has not set is omitted rather than written with
//  a placeholder, and a group with nothing set is omitted entirely. Absent
//  is the encoding for "no opinion", so writing a default-valued field would
//  claim an adjustment the user never made.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::CrtOverridesToJson (const CrtOverrides & o)
{
    JsonObject  groups[s_kcCrtGroup];
    JsonObject  pairObj;
    size_t      i        = 0;



    for (const CrtFieldDesc & field : s_kCrtFields)
    {
        JsonObject &  target = groups[(size_t) field.group];

        if (field.type == CrtScalar::Bool)
        {
            const std::optional<bool> &  slot = o.*field.boolMember;

            if (slot.has_value())
            {
                target.emplace_back (field.key, JsonValue (slot.value()));
            }
        }
        else
        {
            const std::optional<float> &  slot = o.*field.floatMember;

            if (slot.has_value())
            {
                target.emplace_back (field.key, JsonValue ((double) slot.value()));
            }
        }
    }

    // Top-group fields serialize directly onto the pair object; each named
    // group becomes a nested sub-object, in CrtGroup order, and only when it
    // actually holds something.
    pairObj = std::move (groups[(size_t) CrtGroup::Top]);
    for (i = 1; i < s_kcCrtGroup; i++)
    {
        if (!groups[i].empty())
        {
            pairObj.emplace_back (s_kpszCrtGroupKeys[i], JsonValue (std::move (groups[i])));
        }
    }

    return JsonValue (std::move (pairObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ColorTextModeToString
//
//  Maps the text-color mode to its persisted spelling.
//
//  Prefs are stored as NAMES rather than as enum ordinals, so inserting a mode
//  later cannot silently reinterpret everyone's saved setting as a different
//  one. That is the whole reason this function exists instead of a cast.
//
//  Unknown values fall back to white -- the default -- so a value written by a
//  newer build round-trips through an older one as something sensible rather
//  than as an empty string that would fail to parse.
//
////////////////////////////////////////////////////////////////////////////////

const char * GlobalUserPrefs::ColorTextModeToString (ColorMonitorTextMode mode)
{
    const char *  result = s_kpszTextModeWhite;



    switch (mode)
    {
        case ColorMonitorTextMode::Green:
            result = s_kpszTextModeGreen;
            break;

        case ColorMonitorTextMode::Amber:
            result = s_kpszTextModeAmber;
            break;

        case ColorMonitorTextMode::Custom:
            result = s_kpszTextModeCustom;
            break;

        case ColorMonitorTextMode::White:
        default:
            result = s_kpszTextModeWhite;
            break;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ColorTextModeFromString
//
//  Parses a serialized text-mode token, returning `fallback` for an empty
//  or unrecognized string so an unknown future value degrades gracefully.
//
////////////////////////////////////////////////////////////////////////////////

ColorMonitorTextMode GlobalUserPrefs::ColorTextModeFromString (const std::string & s, ColorMonitorTextMode fallback)
{
    ColorMonitorTextMode  result = fallback;



    if (s == s_kpszTextModeGreen)
    {
        result = ColorMonitorTextMode::Green;
    }
    else if (s == s_kpszTextModeAmber)
    {
        result = ColorMonitorTextMode::Amber;
    }
    else if (s == s_kpszTextModeCustom)
    {
        result = ColorMonitorTextMode::Custom;
    }
    else if (s == s_kpszTextModeWhite)
    {
        result = ColorMonitorTextMode::White;
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::PlacementsToJson
//
//  Serialize the per-topology window placement map.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::PlacementsToJson (const std::map<std::string, WindowBounds> & placements)
{
    JsonObject  placementsObj;



    for (const auto & kv : placements)
    {
        JsonObject  bounds;

        bounds.emplace_back ("x", JsonValue ((double) kv.second.x));
        bounds.emplace_back ("y", JsonValue ((double) kv.second.y));
        bounds.emplace_back ("w", JsonValue ((double) kv.second.w));
        bounds.emplace_back ("h", JsonValue ((double) kv.second.h));
        bounds.emplace_back ("max", JsonValue (kv.second.maximized));
        placementsObj.emplace_back (kv.first, JsonValue (std::move (bounds)));
    }

    return JsonValue (std::move (placementsObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::RecentDisksToJson
//
//  Serialize the recent-disks list as a JSON array of paths.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::RecentDisksToJson (const std::vector<std::string> & recentDisks)
{
    std::vector<JsonValue>  recentArr;



    recentArr.reserve (recentDisks.size());
    for (const std::string & path : recentDisks)
    {
        recentArr.emplace_back (JsonValue (path));
    }

    return JsonValue (std::move (recentArr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::RecentDiskTimesToJson
//
//  Serialize the parallel recent-disk load-time list as a JSON array of
//  Unix-second numbers. Stored as JSON numbers (double-backed, exact for
//  any realistic Unix second count).
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::RecentDiskTimesToJson (const std::vector<std::int64_t> & loadedAtUnix)
{
    std::vector<JsonValue>  timesArr;



    timesArr.reserve (loadedAtUnix.size());
    for (std::int64_t when : loadedAtUnix)
    {
        timesArr.emplace_back (JsonValue ((double) when));
    }

    return JsonValue (std::move (timesArr));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ReadCrtOverrides
//
//  Fills the override map from a document, converting a v1 "crt" block when
//  that is what the document carries.
//
//  The trigger is SHAPE, never $cassoGlobalPrefsVersion. Nothing branches on
//  that stamp, and one UserPrefs.json is shared by builds of different ages,
//  so an older build reads a stamped file, finds no key it recognizes,
//  and write its own defaults back over the top while leaving the stamp
//  claiming otherwise.
//
//  Absence is tested by scanning the members rather than with HasObject,
//  because HasObject is type-checked
//  and a hand-edited "crtOverrides": null would read as absent
//  and re-run the conversion over data already converted.
//
//  The v1 blocks were monitor-independent, since the render path indexed by
//  color mode alone. The two monitors in the v1-era catalog are therefore the
//  only tubes the user could have been looking at, so an overridden block
//  becomes an entry under BOTH. Those two names are literals frozen against
//  that catalog and are deliberately not read from s_kMonitors: an
//  eight-monitor build must not fan a //e-era user's tuning onto a Commodore
//  tube they have never booted.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::ReadCrtOverrides (const JsonValue & v, std::map<std::string, CrtOverrides> & out)
{
    const JsonValue *  overridesVal = nullptr;
    const JsonValue *  overridesObj = nullptr;
    const JsonValue *  crtSub       = nullptr;
    size_t             i            = 0;
    size_t             m            = 0;



    out.clear();

    for (const auto & kv : v.GetObjectEntries())
    {
        if (kv.first == "crtOverrides")
        {
            overridesVal = &kv.second;
            break;
        }
    }

    if (overridesVal != nullptr)
    {
        // Present. Whatever its type, the conversion has already run over
        // this document and must not run again.
        if (v.HasObject ("crtOverrides", overridesObj))
        {
            for (const auto & kv : overridesObj->GetObjectEntries())
            {
                CrtOverrides  parsed;

                if (kv.second.GetType() == JsonType::Object)
                {
                    CrtOverridesFromJson (kv.second, parsed);
                }

                if (!parsed.IsEmpty())
                {
                    out[kv.first] = parsed;
                }
            }
        }

        return;
    }

    // No new key: convert a v1 block if one is there.
    if (!v.HasObject ("crt", crtSub))
    {
        return;
    }

    for (i = 0; i < kCrtModeCount; i++)
    {
        const JsonValue *  modeObj = nullptr;
        CrtOverrides       carried;

        if (!crtSub->HasObject (s_kpszCrtModeKeys[i], modeObj))
        {
            continue;
        }

        // A block the user never adopted was never applied, because the old
        // renderer gated the whole user tier on this flag. Its stored numbers
        // carry no intent, so they convert to nothing.
        if (!TryGetBoolOpt (*modeObj, "userOverride", false))
        {
            continue;
        }

        CrtOverridesFromJson (*modeObj, carried);

        if (carried.IsEmpty())
        {
            continue;
        }

        for (m = 0; m < _countof (s_kpszV1Monitors); m++)
        {
            out[CrtResolver::MakeKey (s_kpszV1Monitors[m], i)] = carried;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::CrtOverridesFromJson
//
//  Parse one pair's overrides, table-driven and clamping each numeric field
//  to its documented range.
//
//  A field the document does not carry stays absent rather than taking a
//  default, because absent means the user has no opinion about it. An
//  out-of-range value in a hand-edited file is clamped rather than refused:
//  the file is meant to be readable and editable, and refusing to load would
//  leave the user unable to fix it by changing a setting.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::CrtOverridesFromJson (const JsonValue & obj, CrtOverrides & o)
{
    HRESULT            hr                    = S_OK;
    const JsonValue  * sources[s_kcCrtGroup] = {};
    size_t             i                     = 0;



    // Resolve each group's source object once: the pair object itself for
    // the top group, and the matching sub-object (when present) otherwise.
    sources[(size_t) CrtGroup::Top] = &obj;
    for (i = 1; i < s_kcCrtGroup; i++)
    {
        const JsonValue *  sub = nullptr;

        hr = obj.GetObject (s_kpszCrtGroupKeys[i], sub);
        if (SUCCEEDED (hr))
        {
            sources[i] = sub;
        }
    }

    for (const CrtFieldDesc & field : s_kCrtFields)
    {
        const JsonValue *  source = sources[(size_t) field.group];

        if (source == nullptr)
        {
            continue;
        }

        if (field.type == CrtScalar::Bool)
        {
            bool  value = false;

            hr = source->GetBool (field.key, value);
            if (SUCCEEDED (hr))
            {
                o.*field.boolMember = value;
            }
        }
        else
        {
            double  value = 0.0;

            hr = source->GetNumber (field.key, value);
            if (SUCCEEDED (hr))
            {
                o.*field.floatMember = std::clamp ((float) value, field.lo, field.hi);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::PlacementsFromJson
//
//  Parse the per-topology window placement map, skipping non-object entries.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::PlacementsFromJson (
    const JsonValue                     & placementsObj,
    std::map<std::string, WindowBounds> & placements)
{
    const auto &  entries = placementsObj.GetObjectEntries();



    for (const auto & kv : entries)
    {
        WindowBounds  b;

        if (kv.second.GetType() != JsonType::Object)
        {
            continue;
        }


        b.x = GetIntOpt (kv.second, "x", 0);
        b.y = GetIntOpt (kv.second, "y", 0);
        b.w = GetIntOpt (kv.second, "w", 0);
        b.h         = GetIntOpt (kv.second, "h", 0);
        b.maximized = TryGetBoolOpt (kv.second, "max", false);
        placements[kv.first] = b;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::RecentDisksFromJson
//
//  Parse the recent-disks array, dropping non-string and empty entries.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::RecentDisksFromJson (
    const JsonValue          & recentArr,
    std::vector<std::string> & recentDisks)
{
    size_t  ri = 0;



    recentDisks.reserve (recentArr.GetArraySize());
    for (ri = 0; ri < recentArr.GetArraySize(); ri++)
    {
        const JsonValue   &  entry = recentArr.GetArrayElement (ri);
        // GetString is a plain accessor (empty for non-strings), so the
        // binding is safe before the type test.
        const std::string &  s     = entry.GetString();

        if (entry.GetType() != JsonType::String)
        {
            continue;
        }

        if (s.empty())
        {
            continue;
        }

        recentDisks.push_back (s);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::RecentDiskTimesFromJson
//
//  Parse the parallel recent-disk load-time array, keeping numeric
//  entries as Unix seconds and substituting 0 (unknown) for any
//  non-numeric element so the array stays index-aligned with recentDisks.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::RecentDiskTimesFromJson (
    const JsonValue           & loadedArr,
    std::vector<std::int64_t> & loadedAtUnix)
{
    size_t  ti = 0;



    loadedAtUnix.reserve (loadedArr.GetArraySize());
    for (ti = 0; ti < loadedArr.GetArraySize(); ti++)
    {
        const JsonValue &  entry = loadedArr.GetArrayElement (ti);
        std::int64_t       when  = 0;

        if (entry.GetType() == JsonType::Number)
        {
            when = (std::int64_t) entry.GetNumber();
        }

        loadedAtUnix.push_back (when);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::GetFilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GlobalUserPrefs::GetFilePath (const std::wstring & baseDir)
{
    std::wstring  result = baseDir;



    if (!result.empty() &&
        result.back() != L'\\' &&
        result.back() != L'/')
    {
        result += L'\\';
    }

    result += L"UserPrefs.json";

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ResetColorMonitorTextToDefault
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::ResetColorMonitorTextToDefault()
{
    // White is the shipped default (matches the field initializer). Leave
    // colorMonitorTextCustomArgb untouched so re-picking "Custom" restores
    // the user's last color.
    colorMonitorTextMode = ColorMonitorTextMode::White;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::Load
//
//  Read the unified preferences file under `baseDir`. If absent, leaves
//  `*this` at struct defaults and reports
//  HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND) -- callers that treat first
//  run as normal test for exactly that code, so a genuine read or parse
//  failure can no longer be mistaken for "no prefs yet."
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GlobalUserPrefs::Load (
    const std::wstring  & baseDir,
    IFileSystem         & fs)
{
    HRESULT          hr      = S_OK;
    std::wstring     path    = GetFilePath (baseDir);
    std::string      text;
    JsonValue        root;
    JsonParseError   err;



    // File absent -- keep struct defaults and say so precisely.
    BAIL_OUT_IF (!fs.Exists (path), HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND));

    hr = fs.ReadAllText (path, text);
    CHR (hr);

    hr = JsonParser::Parse (text, root, err);
    CHR (hr);

    if (root.GetType() == JsonType::Object)
    {
        const JsonValue *  global = nullptr;


        if (root.HasObject ("global", global))
        {
            hr = FromJson (*global);
            CHR (hr);
        }
        else
        {
            hr = FromJson (root);
            CHR (hr);
        }
    }
    else
    {
        hr = FromJson (root);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::Save
//
//  Atomically write the unified preferences file under `baseDir`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GlobalUserPrefs::Save (
    const std::wstring  & baseDir,
    IFileSystem         & fs) const
{
    HRESULT              hr               = S_OK;
    std::wstring         path             = GetFilePath (baseDir);
    std::string          text;
    std::string          existingText;
    JsonValue            existing;
    JsonParseError       err;
    JsonWriter::Options  opts;
    JsonValue            global           = ToJson();
    JsonObject           rootEntries;
    JsonObject           machines;
    bool                 isObject         = false;



    // Preserve any machines section the file already has on disk so that
    // this "global only" save path doesn't clobber per-machine user prefs
    // written by UserConfigStore. Without this, every Main.cpp pre-flight
    // save wipes the disk path the user mounted last session.
    //
    // A file that will not read or parse REFUSES THE SAVE, exactly as
    // UserConfigStore::BuildCombinedJson does, because this is the second
    // writer of the same document and the weaker of two contracts is the one
    // that holds. Main.cpp:219 runs this on every launch before the shell
    // exists, so swallowing the failure here overwrote an unreadable prefs
    // file with defaults before anything could set it aside -- which made the
    // whole recovery path unreachable in a shipped build.
    if (fs.Exists (path))
    {
        hr = fs.ReadAllText (path, existingText);
        CHR (hr);

        hr = JsonParser::Parse (existingText, existing, err);
        CHREx (hr, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        isObject = (existing.GetType() == JsonType::Object);
        CBREx (isObject, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));

        for (const auto & kv : existing.GetObjectEntries())
        {
            if (kv.first == "machines" && kv.second.GetType() == JsonType::Object)
            {
                machines = kv.second.GetObjectEntries();
                break;
            }
        }
    }

    rootEntries.emplace_back ("global",   std::move (global));
    rootEntries.emplace_back ("machines", JsonValue (std::move (machines)));
    opts.fPretty = true;

    hr = JsonWriter::Write (JsonValue (std::move (rootEntries)), opts, text);
    CHR (hr);

    hr = fs.WriteAllText (path, text);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ToJson
//
//  Serializes the global prefs. Every field is written unconditionally, so the
//  document is complete and a round trip is deterministic.
//
//  The version key is written FIRST because this file is meant to be read and
//  hand-edited; the reader should see what schema they are looking at before
//  anything else.
//
//  CRT overrides go out per monitor-and-mode pair, and within a pair only the
//  fields the user adjusted. A pair holding nothing is omitted entirely, so a
//  monitor the user never touched leaves no trace in the file -- which is what
//  lets a later preset or theme change reach it. Writing the pairs that DO
//  hold something keeps a user's adjustments for a mode they have temporarily
//  switched away from.
//
//  Enum-valued fields go out as names via their ToString helpers, so inserting
//  an enumerator later cannot reinterpret an existing file.
//
//  The custom text color is written WITHOUT its alpha byte: it is always
//  opaque, so persisting the alpha would only invite a hand edit that makes
//  the text invisible. FromJson masks it back on.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::ToJson() const
{
    JsonObject  root;
    JsonObject  crtObj;
    JsonObject  windowObj;
    size_t      i = 0;



    // $cassoGlobalPrefsVersion (always first for human readability).
    root.emplace_back (s_kpszVersionKey, JsonValue ((double) version));

    root.emplace_back ("activeTheme",          JsonValue (activeTheme));
    root.emplace_back ("crtMonitor",            JsonValue (crtMonitor));
    root.emplace_back ("showFrameRate",         JsonValue (showFrameRate));
    root.emplace_back ("showSceneView",         JsonValue (showSceneView));
    root.emplace_back ("sceneAntiAliasing",     JsonValue ((double) sceneAntiAliasing));
    root.emplace_back ("lastSelectedMachine",  JsonValue (lastSelectedMachine));
    root.emplace_back ("lastDiskCreateFolder", JsonValue (lastDiskCreateFolder));
    root.emplace_back ("audioDownloadConsent", JsonValue (audioDownloadConsent));
    root.emplace_back ("inputMappingMode",     JsonValue (std::string (MachineInputPrefs::ModeToToken (inputMappingMode))));
    root.emplace_back ("arrowsToJoystick",     JsonValue (arrowsToJoystick));
    root.emplace_back ("pointerMapping",       JsonValue (std::string (MachineInputPrefs::ModeToToken (pointerMapping))));
    root.emplace_back ("colorMonitorTextMode", JsonValue (std::string (ColorTextModeToString (colorMonitorTextMode))));
    root.emplace_back ("colorMonitorTextCustom", JsonValue ((double) (colorMonitorTextCustomArgb & 0x00FFFFFFu)));

    // crtOverrides: only pairs the user has actually adjusted, and within
    // each pair only the fields they set. std::map already gives sorted
    // keys, so the file does not churn between saves.
    //
    // The object is emitted even when the map is empty. That is what
    // retires the v1 conversion: its trigger is the legacy block present
    // and this key absent, and most files convert to nothing at all.
    for (const auto & kv : crtOverrides)
    {
        if (!kv.second.IsEmpty())
        {
            crtObj.emplace_back (kv.first, CrtOverridesToJson (kv.second));
        }
    }

    root.emplace_back ("crtOverrides", JsonValue (std::move (crtObj)));

    // window
    windowObj.emplace_back ("placements", PlacementsToJson (window.placements));
    windowObj.emplace_back ("fullscreen", JsonValue (window.fullscreen));

    root.emplace_back ("window", JsonValue (std::move (windowObj)));

    // monitorTilt: radians by monitor name. Written only for monitors the
    // user has actually moved, so an untouched install carries none.
    {
        JsonObject  tiltObj;

        for (const auto & kv : monitorTilt)
        {
            tiltObj.emplace_back (kv.first, JsonValue ((double) kv.second));
        }

        root.emplace_back ("monitorTilt", JsonValue (std::move (tiltObj)));
    }

    // recentDisks: most-recent-first absolute paths, cap enforced by
    // DiskMru itself before we get here.
    root.emplace_back ("recentDisks", RecentDisksToJson (recentDisks));
    root.emplace_back ("recentDiskLoadedAt", RecentDiskTimesToJson (recentDiskLoadedAt));

    // Printing (host print services, FR-011).
    root.emplace_back ("printOutputDpi",   JsonValue ((double) printOutputDpi));
    root.emplace_back ("printDotStyle",    JsonValue (printDotStyle));

    // Screenshots. The mode is normalized on the way out: a value this build
    // does not recognize was already read as the default, and writing it back
    // verbatim would leave the file disagreeing with the running setting.
    root.emplace_back ("screenshotMode",     JsonValue (string (ScreenshotModeToken::Format (
                                                 ScreenshotModeToken::Parse (screenshotMode)))));
    root.emplace_back ("screenshotSaveFile", JsonValue (screenshotSaveFile));
    root.emplace_back ("screenshotFolder",   JsonValue (screenshotFolder));

    // Printer mechanical-audio prefs (FR-034).
    root.emplace_back ("printerAudioEnabled",     JsonValue (printerAudioEnabled));
    root.emplace_back ("printerAudioVolume",      JsonValue ((double) printerAudioVolume));
    root.emplace_back ("printerAudioPanOverride", JsonValue (printerAudioPanOverride));
    root.emplace_back ("printerAudioPan",         JsonValue ((double) printerAudioPan));

    // Master output volume (chrome toolbar).
    root.emplace_back ("masterVolume", JsonValue ((double) masterVolume));
    root.emplace_back ("masterMuted",  JsonValue (masterMuted));

    // Round-trip unknown keys verbatim.
    for (const auto & kv : unknownPassthrough)
    {
        root.emplace_back (kv.first, kv.second);
    }

    return JsonValue (std::move (root));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::FromJson
//
//  Reads the global prefs object, migrating older key shapes as it goes.
//
//  The whole struct is RESET to defaults first, so a partial or truncated JSON
//  object cannot leak the previous load's values into the gaps -- a second
//  Load would otherwise inherit fields the new document never mentioned.
//
//  Every key is read optionally, defaulting to what the reset just installed.
//  A prefs file is user-writable and version-skewed by nature, so a missing or
//  malformed key must cost one setting rather than the whole file.
//
//  Two live migrations run here, both following the same rule -- the NEW key
//  wins, and the legacy shape is consulted only when it is absent:
//
//    mapArrowsToJoystick  the old bool becomes inputMappingMode::Joystick
//    inputMappingMode     the old single mode splits into arrowsToJoystick
//                         (keys) plus pointerMapping (paddle / mouse)
//
//  The split is what makes the two independent: arrow-to-joystick mapping and
//  a pointer mode are unrelated choices that the single field forced into one.
//  A legacy Joystick value therefore migrates to the keys half and leaves the
//  pointer Off.
//
//  pointerMapping additionally rejects Joystick outright, since it is not a
//  pointer mode -- a hand-edited or migrated file that names it there falls
//  back to Off rather than producing a state the UI cannot represent.
//
//  The custom text color is masked back to opaque, so an alpha byte that a
//  hand edit dropped or zeroed cannot yield invisible text.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GlobalUserPrefs::FromJson (const JsonValue & v)
{
    HRESULT             hr            = S_OK;
    const JsonValue *   windowSub     = nullptr;
    const JsonValue *   placementsObj = nullptr;
    const JsonValue *   recentArr     = nullptr;
    const JsonValue *   loadedArr     = nullptr;
    std::string         inputModeStr;
    std::string         textModeStr;
    bool                legacyArrows  = false;



    if (v.GetType() != JsonType::Object)
    {
        hr = E_INVALIDARG;
        CHR (hr);
    }

    // Reset to defaults so partial JSON doesn't leak old state across loads.
    *this = GlobalUserPrefs {};

    version              = GetIntOpt    (v, s_kpszVersionKey,        s_kCurrentVersion);
    activeTheme          = GetStringOpt (v, "activeTheme",            activeTheme);
    crtMonitor            = TryGetBoolOpt   (v, "crtMonitor",              crtMonitor);
    showFrameRate         = TryGetBoolOpt   (v, "showFrameRate",           showFrameRate);
    showSceneView         = TryGetBoolOpt   (v, "showSceneView",           showSceneView);

    // Samples, not a quality index: 1, 2 or 4 only. Anything else is a
    // hand-edited file or a value from a build that knows more counts than
    // this one, and rounding DOWN to a supported count is the safe direction
    // -- it can only ever cost less than what was asked for.
    {
        int  aa = GetIntOpt (v, "sceneAntiAliasing", sceneAntiAliasing);

        sceneAntiAliasing = (aa >= 4) ? 4 : ((aa >= 2) ? 2 : 1);
    }

    lastSelectedMachine  = GetStringOpt (v, "lastSelectedMachine",    lastSelectedMachine);
    lastDiskCreateFolder = GetStringOpt (v, "lastDiskCreateFolder",   lastDiskCreateFolder);
    audioDownloadConsent = GetStringOpt (v, "audioDownloadConsent",   audioDownloadConsent);

    // inputMappingMode supersedes the legacy bool "mapArrowsToJoystick";
    // when the new key is absent, a true legacy bool migrates to Joystick.
    inputModeStr = GetStringOpt (v, "inputMappingMode",   "");
    legacyArrows = TryGetBoolOpt   (v, "mapArrowsToJoystick", false);

    if (!inputModeStr.empty())
    {
        inputMappingMode = MachineInputPrefs::ModeFromToken (inputModeStr, inputMappingMode);
    }
    else if (legacyArrows)
    {
        inputMappingMode = InputMappingMode::Joystick;
    }

    // Split model. New keys win; absent keys migrate from the
    // legacy single mode (joystick -> Keys on; paddle/mouse -> Pointer).
    arrowsToJoystick = TryGetBoolOpt (v, "arrowsToJoystick",
                                   inputMappingMode == InputMappingMode::Joystick);
    {
        std::string  pointerStr = GetStringOpt (v, "pointerMapping", "");

        if (!pointerStr.empty())
        {
            pointerMapping = MachineInputPrefs::ModeFromToken (pointerStr, InputMappingMode::Off);
            if (pointerMapping == InputMappingMode::Joystick)
            {
                pointerMapping = InputMappingMode::Off;    // not a pointer mode
            }
        }
        else
        {
            pointerMapping = (inputMappingMode == InputMappingMode::Paddle ||
                              inputMappingMode == InputMappingMode::Mouse)
                                 ? inputMappingMode
                                 : InputMappingMode::Off;
        }
    }

    textModeStr          = GetStringOpt (v, "colorMonitorTextMode", "");
    colorMonitorTextMode = ColorTextModeFromString (textModeStr, colorMonitorTextMode);
    colorMonitorTextCustomArgb =
        0xFF000000u | ((uint32_t) GetIntOpt (v, "colorMonitorTextCustom",
                                             (int) (colorMonitorTextCustomArgb & 0x00FFFFFFu)) & 0x00FFFFFFu);

    ReadCrtOverrides (v, crtOverrides);

    {
        const JsonValue *  tiltObj = nullptr;

        if (v.HasObject ("monitorTilt", tiltObj) && tiltObj != nullptr)
        {
            monitorTilt.clear();

            for (const auto & kv : tiltObj->GetObjectEntries())
            {
                if (kv.second.GetType() == JsonType::Number)
                {
                    monitorTilt[kv.first] = (float) kv.second.GetNumber();
                }
            }
        }
    }

    if (v.HasObject ("window", windowSub))
    {
        if (windowSub->HasObject ("placements", placementsObj))
        {
            PlacementsFromJson (*placementsObj, window.placements);
        }

        window.fullscreen = TryGetBoolOpt (*windowSub, "fullscreen", window.fullscreen);
    }

    // recentDisks: drop non-string and empty entries silently per
    // data-model.md §1; cap is enforced by DiskMru on use.
    recentDisks.clear();
    if (v.HasArray ("recentDisks", recentArr))
    {
        RecentDisksFromJson (*recentArr, recentDisks);
    }

    // recentDiskLoadedAt: parallel Unix-second load times. Absent in a
    // legacy prefs file, leaving every recent disk with an unknown time.
    recentDiskLoadedAt.clear();
    if (v.HasArray ("recentDiskLoadedAt", loadedArr))
    {
        RecentDiskTimesFromJson (*loadedArr, recentDiskLoadedAt);
    }

    // Printing (host print services, FR-011); absent keys keep struct defaults.
    printOutputDpi   = GetIntOpt    (v, "printOutputDpi",   printOutputDpi);
    printDotStyle    = GetStringOpt (v, "printDotStyle",    printDotStyle);

    // Screenshots; absent keys keep struct defaults.
    screenshotMode     = GetStringOpt   (v, "screenshotMode",     screenshotMode);
    screenshotSaveFile = TryGetBoolOpt  (v, "screenshotSaveFile", screenshotSaveFile);
    screenshotFolder   = GetStringOpt   (v, "screenshotFolder",   screenshotFolder);

    // Printer mechanical-audio prefs (FR-034); absent keys keep struct defaults.
    // Legacy pre-toggle files stored the inverse `printerAudioMuted`; fall back
    // to it (inverted) so an older mute survives the rename to `enabled`.
    printerAudioEnabled     = TryGetBoolOpt   (v, "printerAudioEnabled",
                                            !TryGetBoolOpt (v, "printerAudioMuted", !printerAudioEnabled));
    printerAudioVolume      = (float) GetNumberOpt (v, "printerAudioVolume",      printerAudioVolume);
    printerAudioPanOverride = TryGetBoolOpt   (v, "printerAudioPanOverride", printerAudioPanOverride);
    printerAudioPan         = (float) GetNumberOpt (v, "printerAudioPan",         printerAudioPan);
    printerAudioVolume      = std::clamp (printerAudioVolume, 0.0f, 1.0f);
    printerAudioPan         = std::clamp (printerAudioPan,   -1.0f, 1.0f);

    // Master output volume (chrome toolbar); absent keys keep struct defaults.
    masterVolume = (float) GetNumberOpt (v, "masterVolume", masterVolume);
    masterMuted  = TryGetBoolOpt (v, "masterMuted", masterMuted);
    masterVolume = std::clamp (masterVolume, 0.0f, 1.0f);

    // Capture unknown top-level keys for round-tripping.
    for (const auto & entry : v.GetObjectEntries())
    {
        if (s_kKnownTopLevel.find (entry.first) == s_kKnownTopLevel.end())
        {
            unknownPassthrough.emplace_back (entry.first, entry.second);
        }
    }

Error:
    return hr;
}
