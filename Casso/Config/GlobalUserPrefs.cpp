#include "Pch.h"

#include "GlobalUserPrefs.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"





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


// crt sub-object key per monitor type, indexed by SettingsColorMode.
static constexpr const char *  s_kpszCrtModeKeys[GlobalUserPrefs::kCrtModeCount] = {
    "color", "green", "amber", "white"
};


// Known top-level keys recognized by this version of GlobalUserPrefs.
// Anything not in this list is preserved in `unknownPassthrough`.
static const std::set<std::string>  s_kKnownTopLevel = {
    "$cassoGlobalPrefsVersion",
    "activeTheme",
    "lastSelectedMachine",
    "audioDownloadConsent",
    "inputMappingMode",
    "mapArrowsToJoystick",
    "colorMonitorTextMode",
    "colorMonitorTextCustom",
    "recentDisks",
    "recentDiskLoadedAt",
    "crt",
    "window"
};


// Serialized string tokens for InputMappingMode.
static constexpr const char *  s_kpszInputModeOff      = "off";
static constexpr const char *  s_kpszInputModeJoystick = "joystick";
static constexpr const char *  s_kpszInputModePaddle   = "paddle";
static constexpr const char *  s_kpszInputModeMouse    = "mouse";

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
// a pointer-to-member into GlobalUserPrefs::Crt, and (floats only) the
// inclusive clamp range applied on load so a hand-edited prefs file can't
// drive the shaders out of range. The unused member pointer is null. Row
// order within a group is the serialized key order.
struct CrtFieldDesc
{
    CrtGroup                       group;
    const char *                   key;
    CrtScalar                      type;
    bool  GlobalUserPrefs::Crt::*  boolMember;
    float GlobalUserPrefs::Crt::*  floatMember;
    float                          lo;
    float                          hi;
};


static constexpr CrtFieldDesc  s_kCrtFields[] = {
    { CrtGroup::Top,        "userOverride", CrtScalar::Bool,  &GlobalUserPrefs::Crt::userOverride,      nullptr,                                   0.0f, 0.0f  },
    { CrtGroup::Top,        "brightness",   CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::brightness,         0.0f, 2.0f  },
    { CrtGroup::Top,        "contrast",     CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::contrast,           0.0f, 2.0f  },
    { CrtGroup::Top,        "gamma",        CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::gamma,              0.5f, 2.5f  },
    { CrtGroup::Top,        "persistence",  CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::persistence,        0.0f, 0.99f },
    { CrtGroup::Scanlines,  "enabled",      CrtScalar::Bool,  &GlobalUserPrefs::Crt::scanlinesEnabled,  nullptr,                                   0.0f, 0.0f  },
    { CrtGroup::Scanlines,  "intensity",    CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::scanlinesIntensity, 0.0f, 1.0f  },
    { CrtGroup::Bloom,      "enabled",      CrtScalar::Bool,  &GlobalUserPrefs::Crt::bloomEnabled,      nullptr,                                   0.0f, 0.0f  },
    { CrtGroup::Bloom,      "radius",       CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::bloomRadius,        0.0f, 10.0f },
    { CrtGroup::Bloom,      "strength",     CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::bloomStrength,      0.0f, 1.0f  },
    { CrtGroup::ColorBleed, "enabled",      CrtScalar::Bool,  &GlobalUserPrefs::Crt::colorBleedEnabled, nullptr,                                   0.0f, 0.0f  },
    { CrtGroup::ColorBleed, "width",        CrtScalar::Float, nullptr,                                  &GlobalUserPrefs::Crt::colorBleedWidth,    0.0f, 8.0f  },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::GetBoolOpt
//
//  Read an optional boolean leaf, falling back to `fallback` when absent or
//  not a boolean.
//
////////////////////////////////////////////////////////////////////////////////

bool GlobalUserPrefs::GetBoolOpt (
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
//  GlobalUserPrefs::CrtToJson
//
//  Serialize one monitor's CRT block, table-driven so the emitted key order
//  matches s_kCrtGroups exactly.
//
////////////////////////////////////////////////////////////////////////////////

JsonValue GlobalUserPrefs::CrtToJson (const Crt & c)
{
    JsonObject  groups[s_kcCrtGroup];
    JsonObject  modeObj;



    for (const CrtFieldDesc & field : s_kCrtFields)
    {
        JsonObject &  target = groups[(size_t) field.group];

        if (field.type == CrtScalar::Bool)
        {
            target.emplace_back (field.key, JsonValue (c.*field.boolMember));
        }
        else
        {
            target.emplace_back (field.key, JsonValue ((double) (c.*field.floatMember)));
        }
    }

    // Top-group fields serialize directly onto the mode object; each named
    // group becomes a nested sub-object, in CrtGroup order.
    modeObj = std::move (groups[(size_t) CrtGroup::Top]);
    for (size_t i = 1; i < s_kcCrtGroup; i++)
    {
        modeObj.emplace_back (s_kpszCrtGroupKeys[i], JsonValue (std::move (groups[i])));
    }

    return JsonValue (std::move (modeObj));
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::InputMappingModeToString
//
////////////////////////////////////////////////////////////////////////////////

const char * GlobalUserPrefs::InputMappingModeToString (InputMappingMode mode)
{
    switch (mode)
    {
        case InputMappingMode::Joystick:
            return s_kpszInputModeJoystick;

        case InputMappingMode::Paddle:
            return s_kpszInputModePaddle;

        case InputMappingMode::Mouse:
            return s_kpszInputModeMouse;

        case InputMappingMode::Off:
        default:
            return s_kpszInputModeOff;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::InputMappingModeFromString
//
//  Parses a serialized mode token, returning `fallback` for an empty or
//  unrecognized string so an unknown future value degrades gracefully.
//
////////////////////////////////////////////////////////////////////////////////

InputMappingMode GlobalUserPrefs::InputMappingModeFromString (const std::string & s, InputMappingMode fallback)
{
    if (s == s_kpszInputModeJoystick)
    {
        return InputMappingMode::Joystick;
    }

    if (s == s_kpszInputModePaddle)
    {
        return InputMappingMode::Paddle;
    }

    if (s == s_kpszInputModeMouse)
    {
        return InputMappingMode::Mouse;
    }

    if (s == s_kpszInputModeOff)
    {
        return InputMappingMode::Off;
    }

    return fallback;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::ColorTextModeToString
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
//  GlobalUserPrefs::CrtModeFromJson
//
//  Parse one monitor's CRT block, table-driven and clamping each numeric
//  field to its documented range. Absent fields keep their struct defaults.
//
////////////////////////////////////////////////////////////////////////////////

void GlobalUserPrefs::CrtModeFromJson (const JsonValue & modeObj, Crt & c)
{
    HRESULT           hr                      = S_OK;
    const JsonValue * sources[s_kcCrtGroup] = {};



    // Resolve each group's source object once: the mode object itself for
    // the top group, and the matching sub-object (when present) otherwise.
    sources[(size_t) CrtGroup::Top] = &modeObj;
    for (size_t i = 1; i < s_kcCrtGroup; i++)
    {
        const JsonValue * sub = nullptr;

        hr = modeObj.GetObject (s_kpszCrtGroupKeys[i], sub);
        if (FAILED (hr))
        {
            continue;
        }
         
        sources[i] = sub;

    }

    for (const CrtFieldDesc & field : s_kCrtFields)
    {
        const JsonValue * source = sources[(size_t) field.group];

        if (source == nullptr)
        {
            continue;
        }

        if (field.type == CrtScalar::Bool)
        {
            c.*field.boolMember = GetBoolOpt (*source, field.key, c.*field.boolMember);
        }
        else
        {
            float  value = (float) GetNumberOpt (*source, field.key, c.*field.floatMember);

            c.*field.floatMember = std::clamp (value, field.lo, field.hi);
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
        if (kv.second.GetType() != JsonType::Object)
        {
            continue;
        }

        WindowBounds  b;

        b.x = GetIntOpt (kv.second, "x", 0);
        b.y = GetIntOpt (kv.second, "y", 0);
        b.w = GetIntOpt (kv.second, "w", 0);
        b.h = GetIntOpt (kv.second, "h", 0);
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



    recentDisks.reserve (recentArr.ArraySize());
    for (ri = 0; ri < recentArr.ArraySize(); ri++)
    {
        const JsonValue &  entry = recentArr.ArrayAt (ri);

        if (entry.GetType() != JsonType::String)
        {
            continue;
        }

        const std::string &  s = entry.GetString();

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



    loadedAtUnix.reserve (loadedArr.ArraySize());
    for (ti = 0; ti < loadedArr.ArraySize(); ti++)
    {
        const JsonValue &  entry = loadedArr.ArrayAt (ti);
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
//  GlobalUserPrefs::FilePath
//
////////////////////////////////////////////////////////////////////////////////

std::wstring GlobalUserPrefs::FilePath (const std::wstring & baseDir)
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

void GlobalUserPrefs::ResetColorMonitorTextToDefault ()
{
    // White is the shipped default (matches the field initialiser). Leave
    // colorMonitorTextCustomArgb untouched so re-picking "Custom" restores
    // the user's last colour.
    colorMonitorTextMode = ColorMonitorTextMode::White;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::Load
//
//  Read the unified preferences file under `baseDir`. If absent, leaves
//  `*this` at struct defaults and returns S_FALSE so the caller can treat
//  it as first run.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT GlobalUserPrefs::Load (
    const std::wstring  & baseDir,
    IFileSystem         & fs)
{
    HRESULT          hr      = S_OK;
    std::wstring     path    = FilePath (baseDir);
    std::string      text;
    JsonValue        root;
    JsonParseError   err;



    if (!fs.Exists (path))
    {
        // File absent — keep struct defaults.
        return S_FALSE;
    }

    hr = fs.ReadAllText (path, text);
    CHR (hr);

    hr = JsonParser::Parse (text, root, err);
    CHR (hr);

    if (root.GetType() == JsonType::Object)
    {
        const JsonValue *  global = nullptr;


        if (SUCCEEDED (root.GetObject ("global", global)) && global != nullptr)
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
    std::wstring         path             = FilePath (baseDir);
    std::string          text;
    std::string          existingText;
    JsonValue            existing;
    JsonParseError       err;
    JsonWriter::Options  opts;
    JsonValue            global           = ToJson();
    JsonObject           rootEntries;
    JsonObject           machines;



    // Preserve any machines section the file already has on disk so that
    // this "global only" save path doesn't clobber per-machine user prefs
    // written by UserConfigStore. Without this, every Main.cpp pre-flight
    // save wipes the disk path the user mounted last session.
    if (fs.Exists (path))
    {
        hr = fs.ReadAllText (path, existingText);
        if (SUCCEEDED (hr))
        {
            hr = JsonParser::Parse (existingText, existing, err);
            if (SUCCEEDED (hr) && existing.GetType() == JsonType::Object)
            {
                for (const auto & kv : existing.GetObjectEntries())
                {
                    if (kv.first == "machines" && kv.second.GetType() == JsonType::Object)
                    {
                        machines = kv.second.GetObjectEntries();
                        break;
                    }
                }
            }
        }
        hr = S_OK;
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
    root.emplace_back ("lastSelectedMachine",  JsonValue (lastSelectedMachine));
    root.emplace_back ("audioDownloadConsent", JsonValue (audioDownloadConsent));
    root.emplace_back ("inputMappingMode",     JsonValue (std::string (InputMappingModeToString (inputMappingMode))));
    root.emplace_back ("colorMonitorTextMode", JsonValue (std::string (ColorTextModeToString (colorMonitorTextMode))));
    root.emplace_back ("colorMonitorTextCustom", JsonValue ((double) (colorMonitorTextCustomArgb & 0x00FFFFFFu)));

    // crt: one sub-object per monitor type. Persist every block even
    // when userOverride is false so a roundtrip is deterministic; the
    // override flag controls whether the values are APPLIED, not
    // whether they're written.
    for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
    {
        crtObj.emplace_back (s_kpszCrtModeKeys[i], CrtToJson (crtByMode[i]));
    }

    root.emplace_back ("crt", JsonValue (std::move (crtObj)));

    // window
    windowObj.emplace_back ("placements", PlacementsToJson (window.placements));
    windowObj.emplace_back ("fullscreen", JsonValue (window.fullscreen));

    root.emplace_back ("window", JsonValue (std::move (windowObj)));

    // recentDisks: most-recent-first absolute paths, cap enforced by
    // DiskMru itself before we get here.
    root.emplace_back ("recentDisks", RecentDisksToJson (recentDisks));
    root.emplace_back ("recentDiskLoadedAt", RecentDiskTimesToJson (recentDiskLoadedAt));

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
////////////////////////////////////////////////////////////////////////////////

HRESULT GlobalUserPrefs::FromJson (const JsonValue & v)
{
    HRESULT             hr            = S_OK;
    const JsonValue *   crtSub        = nullptr;
    const JsonValue *   windowSub     = nullptr;
    const JsonValue *   placementsObj = nullptr;
    const JsonValue *   recentArr     = nullptr;
    const JsonValue *   loadedArr     = nullptr;
    std::string         inputModeStr;
    std::string         textModeStr;
    bool                legacyArrows  = false;
    size_t              i             = 0;



    if (v.GetType() != JsonType::Object)
    {
        hr = E_INVALIDARG;
        CHR (hr);
    }

    // Reset to defaults so partial JSON doesn't leak old state across loads.
    *this = GlobalUserPrefs {};

    version              = GetIntOpt    (v, s_kpszVersionKey,        s_kCurrentVersion);
    activeTheme          = GetStringOpt (v, "activeTheme",            activeTheme);
    lastSelectedMachine  = GetStringOpt (v, "lastSelectedMachine",    lastSelectedMachine);
    audioDownloadConsent = GetStringOpt (v, "audioDownloadConsent",   audioDownloadConsent);

    // inputMappingMode supersedes the legacy bool "mapArrowsToJoystick";
    // when the new key is absent, a true legacy bool migrates to Joystick.
    inputModeStr = GetStringOpt (v, "inputMappingMode",   "");
    legacyArrows = GetBoolOpt   (v, "mapArrowsToJoystick", false);

    if (!inputModeStr.empty())
    {
        inputMappingMode = InputMappingModeFromString (inputModeStr, inputMappingMode);
    }
    else if (legacyArrows)
    {
        inputMappingMode = InputMappingMode::Joystick;
    }

    textModeStr          = GetStringOpt (v, "colorMonitorTextMode", "");
    colorMonitorTextMode = ColorTextModeFromString (textModeStr, colorMonitorTextMode);
    colorMonitorTextCustomArgb =
        0xFF000000u | ((uint32_t) GetIntOpt (v, "colorMonitorTextCustom",
                                             (int) (colorMonitorTextCustomArgb & 0x00FFFFFFu)) & 0x00FFFFFFu);

    if (SUCCEEDED (v.GetObject ("crt", crtSub)) && crtSub != nullptr)
    {
        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            const JsonValue *  modeObj = nullptr;

            if (SUCCEEDED (crtSub->GetObject (s_kpszCrtModeKeys[i], modeObj)) && modeObj != nullptr)
            {
                CrtModeFromJson (*modeObj, crtByMode[i]);
            }
        }
    }

    if (SUCCEEDED (v.GetObject ("window", windowSub)) && windowSub != nullptr)
    {
        if (SUCCEEDED (windowSub->GetObject ("placements", placementsObj)) && placementsObj != nullptr)
        {
            PlacementsFromJson (*placementsObj, window.placements);
        }
        window.fullscreen = GetBoolOpt (*windowSub, "fullscreen", window.fullscreen);
    }

    // recentDisks: drop non-string and empty entries silently per
    // data-model.md §1; cap is enforced by DiskMru on use.
    recentDisks.clear();
    if (SUCCEEDED (v.GetArray ("recentDisks", recentArr)) && recentArr != nullptr)
    {
        RecentDisksFromJson (*recentArr, recentDisks);
    }

    // recentDiskLoadedAt: parallel Unix-second load times. Absent in a
    // legacy prefs file, leaving every recent disk with an unknown time.
    recentDiskLoadedAt.clear();
    if (SUCCEEDED (v.GetArray ("recentDiskLoadedAt", loadedArr)) && loadedArr != nullptr)
    {
        RecentDiskTimesFromJson (*loadedArr, recentDiskLoadedAt);
    }

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
