#include "Pch.h"

#include "GlobalUserPrefs.h"


#include "Core/JsonParser.h"
#include "Core/JsonWriter.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  s_kpszVersionKey    = "$cassoGlobalPrefsVersion";
    constexpr int           s_kCurrentVersion   = 1;


    // Known top-level keys recognized by this version of GlobalUserPrefs.
    // Anything not in this list is preserved in `unknownPassthrough`.
    const std::set<std::string>  s_knownTopLevel = {
        "$cassoGlobalPrefsVersion",
        "activeTheme",
        "lastSelectedMachine",
        "crt",
        "window"
    };


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
        bool      result = fallback;
        HRESULT   hr     = obj.GetBool (key, result);
        if (FAILED (hr))
        {
            return fallback;
        }
        return result;
    }


    double  GetNumberOpt (
        const JsonValue   & obj,
        const std::string & key,
        double              fallback)
    {
        double    result = fallback;
        HRESULT   hr     = obj.GetNumber (key, result);
        if (FAILED (hr))
        {
            return fallback;
        }
        return result;
    }


    int  GetIntOpt (
        const JsonValue   & obj,
        const std::string & key,
        int                 fallback)
    {
        int      result = fallback;
        HRESULT  hr     = obj.GetInt (key, result);
        if (FAILED (hr))
        {
            return fallback;
        }
        return result;
    }


    std::string  GetStringOpt (
        const JsonValue   & obj,
        const std::string & key,
        const std::string & fallback)
    {
        std::string  result = fallback;
        HRESULT      hr     = obj.GetString (key, result);
        if (FAILED (hr))
        {
            return fallback;
        }
        return result;
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
    HRESULT              hr      = S_OK;
    std::wstring         path    = FilePath (baseDir);
    std::string          text;
    JsonWriter::Options  opts;
    JsonValue            global  = ToJson();
    std::vector<std::pair<std::string, JsonValue>>  rootEntries;
    std::vector<std::pair<std::string, JsonValue>>  machines;



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
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  crtObj;
    std::vector<std::pair<std::string, JsonValue>>  bounds;
    std::vector<std::pair<std::string, JsonValue>>  windowObj;
    size_t                                          i = 0;
    static const char *  s_kModeKeys[GlobalUserPrefs::kCrtModeCount] = {
        "color", "green", "amber", "white"
    };



    // $cassoGlobalPrefsVersion (always first for human readability).
    root.emplace_back (s_kpszVersionKey, JsonValue ((double) version));

    root.emplace_back ("activeTheme",         JsonValue (activeTheme));
    root.emplace_back ("lastSelectedMachine", JsonValue (lastSelectedMachine));

    // crt: one sub-object per monitor type. Persist every block even
    // when userOverride is false so a roundtrip is deterministic; the
    // override flag controls whether the values are APPLIED, not
    // whether they're written.
    for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
    {
        const Crt &                                     c = crtByMode[i];
        std::vector<std::pair<std::string, JsonValue>>  modeObj;
        std::vector<std::pair<std::string, JsonValue>>  scanlines;
        std::vector<std::pair<std::string, JsonValue>>  bloom;
        std::vector<std::pair<std::string, JsonValue>>  colorBleed;

        scanlines.emplace_back ("enabled",   JsonValue (c.scanlinesEnabled));
        scanlines.emplace_back ("intensity", JsonValue ((double) c.scanlinesIntensity));

        bloom.emplace_back ("enabled",  JsonValue (c.bloomEnabled));
        bloom.emplace_back ("radius",   JsonValue ((double) c.bloomRadius));
        bloom.emplace_back ("strength", JsonValue ((double) c.bloomStrength));

        colorBleed.emplace_back ("enabled", JsonValue (c.colorBleedEnabled));
        colorBleed.emplace_back ("width",   JsonValue ((double) c.colorBleedWidth));

        modeObj.emplace_back ("userOverride", JsonValue (c.userOverride));
        modeObj.emplace_back ("brightness",   JsonValue ((double) c.brightness));
        modeObj.emplace_back ("contrast",     JsonValue ((double) c.contrast));
        modeObj.emplace_back ("gamma",        JsonValue ((double) c.gamma));
        modeObj.emplace_back ("persistence",  JsonValue ((double) c.persistence));
        modeObj.emplace_back ("scanlines",    JsonValue (std::move (scanlines)));
        modeObj.emplace_back ("bloom",        JsonValue (std::move (bloom)));
        modeObj.emplace_back ("colorBleed",   JsonValue (std::move (colorBleed)));

        crtObj.emplace_back (s_kModeKeys[i], JsonValue (std::move (modeObj)));
    }

    root.emplace_back ("crt", JsonValue (std::move (crtObj)));

    // window
    if (window.fHaveLastBounds)
    {
        bounds.emplace_back ("x", JsonValue ((double) window.x));
        bounds.emplace_back ("y", JsonValue ((double) window.y));
        bounds.emplace_back ("w", JsonValue ((double) window.w));
        bounds.emplace_back ("h", JsonValue ((double) window.h));
        windowObj.emplace_back ("lastBounds", JsonValue (std::move (bounds)));
    }
    windowObj.emplace_back ("fullscreen", JsonValue (window.fullscreen));

    root.emplace_back ("window", JsonValue (std::move (windowObj)));

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
    const JsonValue *   bounds        = nullptr;
    const auto *        rootEntries   = (const std::vector<std::pair<std::string, JsonValue>> *) nullptr;
    size_t              i             = 0;
    static const char *  s_kModeKeys[GlobalUserPrefs::kCrtModeCount] = {
        "color", "green", "amber", "white"
    };



    if (v.GetType() != JsonType::Object)
    {
        hr = E_INVALIDARG;
        CHR (hr);
    }

    // Reset to defaults so partial JSON doesn't leak old state across loads.
    *this = GlobalUserPrefs {};

    version             = GetIntOpt    (v, s_kpszVersionKey,        s_kCurrentVersion);
    activeTheme         = GetStringOpt (v, "activeTheme",           activeTheme);
    lastSelectedMachine = GetStringOpt (v, "lastSelectedMachine",   lastSelectedMachine);

    if (SUCCEEDED (v.GetObject ("crt", crtSub)) && crtSub != nullptr)
    {
        for (i = 0; i < GlobalUserPrefs::kCrtModeCount; i++)
        {
            const JsonValue *  modeObj    = nullptr;
            const JsonValue *  scanlines  = nullptr;
            const JsonValue *  bloom      = nullptr;
            const JsonValue *  colorBleed = nullptr;
            Crt &              c          = crtByMode[i];

            if (FAILED (crtSub->GetObject (s_kModeKeys[i], modeObj)) || modeObj == nullptr)
            {
                continue;
            }

            c.userOverride = GetBoolOpt (*modeObj, "userOverride", c.userOverride);
            c.brightness   = (float) GetNumberOpt (*modeObj, "brightness",   c.brightness);
            c.contrast     = (float) GetNumberOpt (*modeObj, "contrast",     c.contrast);
            c.gamma        = (float) GetNumberOpt (*modeObj, "gamma",        c.gamma);
            c.persistence  = (float) GetNumberOpt (*modeObj, "persistence",  c.persistence);

            if (SUCCEEDED (modeObj->GetObject ("scanlines", scanlines)) && scanlines != nullptr)
            {
                c.scanlinesEnabled   = GetBoolOpt   (*scanlines, "enabled",   c.scanlinesEnabled);
                c.scanlinesIntensity = (float) GetNumberOpt (*scanlines, "intensity", c.scanlinesIntensity);
            }
            if (SUCCEEDED (modeObj->GetObject ("bloom", bloom)) && bloom != nullptr)
            {
                c.bloomEnabled  = GetBoolOpt (*bloom, "enabled", c.bloomEnabled);
                c.bloomRadius   = (float) GetNumberOpt (*bloom, "radius",   c.bloomRadius);
                c.bloomStrength = (float) GetNumberOpt (*bloom, "strength", c.bloomStrength);
            }
            if (SUCCEEDED (modeObj->GetObject ("colorBleed", colorBleed)) && colorBleed != nullptr)
            {
                c.colorBleedEnabled = GetBoolOpt (*colorBleed, "enabled", c.colorBleedEnabled);
                c.colorBleedWidth   = (float) GetNumberOpt (*colorBleed, "width",   c.colorBleedWidth);
            }

            // Clamp every numeric CRT field to its documented range so a
            // hand-edited prefs file with out-of-range values can't drive
            // the shaders into nonsense territory.
            if (c.brightness         < 0.0f)  c.brightness         = 0.0f;
            if (c.brightness         > 2.0f)  c.brightness         = 2.0f;
            if (c.contrast           < 0.0f)  c.contrast           = 0.0f;
            if (c.contrast           > 2.0f)  c.contrast           = 2.0f;
            if (c.gamma              < 0.5f)  c.gamma              = 0.5f;
            if (c.gamma              > 2.5f)  c.gamma              = 2.5f;
            if (c.scanlinesIntensity < 0.0f)  c.scanlinesIntensity = 0.0f;
            if (c.scanlinesIntensity > 1.0f)  c.scanlinesIntensity = 1.0f;
            if (c.bloomRadius        < 0.0f)  c.bloomRadius        = 0.0f;
            if (c.bloomRadius        > 10.0f) c.bloomRadius        = 10.0f;
            if (c.bloomStrength      < 0.0f)  c.bloomStrength      = 0.0f;
            if (c.bloomStrength      > 1.0f)  c.bloomStrength      = 1.0f;
            if (c.colorBleedWidth    < 0.0f)  c.colorBleedWidth    = 0.0f;
            if (c.colorBleedWidth    > 8.0f)  c.colorBleedWidth    = 8.0f;
            if (c.persistence        < 0.0f)  c.persistence        = 0.0f;
            if (c.persistence        > 0.99f) c.persistence        = 0.99f;
        }
    }

    if (SUCCEEDED (v.GetObject ("window", windowSub)) && windowSub != nullptr)
    {
        if (SUCCEEDED (windowSub->GetObject ("lastBounds", bounds)) && bounds != nullptr)
        {
            window.fHaveLastBounds = true;
            window.x = GetIntOpt (*bounds, "x", window.x);
            window.y = GetIntOpt (*bounds, "y", window.y);
            window.w = GetIntOpt (*bounds, "w", window.w);
            window.h = GetIntOpt (*bounds, "h", window.h);
        }
        window.fullscreen = GetBoolOpt (*windowSub, "fullscreen", window.fullscreen);
    }

    // Capture unknown top-level keys for round-tripping.
    rootEntries = &v.GetObjectEntries();
    for (i = 0; i < rootEntries->size(); ++i)
    {
        const std::string & key = (*rootEntries)[i].first;
        if (s_knownTopLevel.find (key) == s_knownTopLevel.end())
        {
            unknownPassthrough.emplace_back (key, (*rootEntries)[i].second);
        }
    }

Error:
    return hr;
}
