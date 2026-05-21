#include "Pch.h"

#include "GlobalUserPrefs.h"

#include "../../CassoCore/Ehm.h"

#include "../../CassoEmuCore/Core/JsonParser.h"
#include "../../CassoEmuCore/Core/JsonWriter.h"

#include <set>





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
        for (i = 0; i < (int) entries.size (); ++i)
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

    if (!result.empty () &&
        result.back () != L'\\' &&
        result.back () != L'/')
    {
        result += L'\\';
    }
    result += L"GlobalUserPrefs.json";

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::Load
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

    hr = FromJson (root);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GlobalUserPrefs::Save
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
    JsonValue            root    = ToJson ();


    opts.fPretty = true;

    hr = JsonWriter::Write (root, opts, text);
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

JsonValue GlobalUserPrefs::ToJson () const
{
    std::vector<std::pair<std::string, JsonValue>>  root;
    std::vector<std::pair<std::string, JsonValue>>  scanlines;
    std::vector<std::pair<std::string, JsonValue>>  bloom;
    std::vector<std::pair<std::string, JsonValue>>  colorBleed;
    std::vector<std::pair<std::string, JsonValue>>  crtObj;
    std::vector<std::pair<std::string, JsonValue>>  bounds;
    std::vector<std::pair<std::string, JsonValue>>  windowObj;


    // $cassoGlobalPrefsVersion (always first for human readability).
    root.emplace_back (s_kpszVersionKey, JsonValue ((double) version));

    root.emplace_back ("activeTheme",         JsonValue (activeTheme));
    root.emplace_back ("lastSelectedMachine", JsonValue (lastSelectedMachine));

    // crt
    scanlines.emplace_back ("enabled",   JsonValue (crt.scanlinesEnabled));
    scanlines.emplace_back ("intensity", JsonValue ((double) crt.scanlinesIntensity));

    bloom.emplace_back ("enabled",  JsonValue (crt.bloomEnabled));
    bloom.emplace_back ("radius",   JsonValue ((double) crt.bloomRadius));
    bloom.emplace_back ("strength", JsonValue ((double) crt.bloomStrength));

    colorBleed.emplace_back ("enabled", JsonValue (crt.colorBleedEnabled));
    colorBleed.emplace_back ("width",   JsonValue ((double) crt.colorBleedWidth));

    crtObj.emplace_back ("brightness", JsonValue ((double) crt.brightness));
    crtObj.emplace_back ("scanlines",  JsonValue (std::move (scanlines)));
    crtObj.emplace_back ("bloom",      JsonValue (std::move (bloom)));
    crtObj.emplace_back ("colorBleed", JsonValue (std::move (colorBleed)));

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
    const JsonValue *   scanlines     = nullptr;
    const JsonValue *   bloom         = nullptr;
    const JsonValue *   colorBleed    = nullptr;
    const JsonValue *   windowSub     = nullptr;
    const JsonValue *   bounds        = nullptr;
    const auto *        rootEntries   = (const std::vector<std::pair<std::string, JsonValue>> *) nullptr;
    size_t              i             = 0;


    if (v.GetType () != JsonType::Object)
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
        crt.brightness = (float) GetNumberOpt (*crtSub, "brightness", crt.brightness);

        if (SUCCEEDED (crtSub->GetObject ("scanlines", scanlines)) && scanlines != nullptr)
        {
            crt.scanlinesEnabled   = GetBoolOpt   (*scanlines, "enabled",   crt.scanlinesEnabled);
            crt.scanlinesIntensity = (float) GetNumberOpt (*scanlines, "intensity", crt.scanlinesIntensity);
        }
        if (SUCCEEDED (crtSub->GetObject ("bloom", bloom)) && bloom != nullptr)
        {
            crt.bloomEnabled  = GetBoolOpt (*bloom, "enabled", crt.bloomEnabled);
            crt.bloomRadius   = (float) GetNumberOpt (*bloom, "radius",   crt.bloomRadius);
            crt.bloomStrength = (float) GetNumberOpt (*bloom, "strength", crt.bloomStrength);
        }
        if (SUCCEEDED (crtSub->GetObject ("colorBleed", colorBleed)) && colorBleed != nullptr)
        {
            crt.colorBleedEnabled = GetBoolOpt (*colorBleed, "enabled", crt.colorBleedEnabled);
            crt.colorBleedWidth   = (float) GetNumberOpt (*colorBleed, "width",   crt.colorBleedWidth);
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
    rootEntries = &v.GetObjectEntries ();
    for (i = 0; i < rootEntries->size (); ++i)
    {
        const std::string & key = (*rootEntries)[i].first;
        if (s_knownTopLevel.find (key) == s_knownTopLevel.end ())
        {
            unknownPassthrough.emplace_back (key, (*rootEntries)[i].second);
        }
    }

Error:
    return hr;
}
