#include "Pch.h"

#include "ThemeLoader.h"


#include "Core/JsonParser.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    constexpr const char *  s_kpszVersionKey  = "$cassoThemeVersion";
    constexpr const char *  s_kpszBuiltInKey  = "$cassoBuiltIn";


    std::wstring  Utf8ToWide (const std::string & s)
    {
        // Theme paths in theme.json are ASCII by spec (filename
        // restrictions). A naive widen is fine for the relative
        // names we deal with here.
        return std::wstring (s.begin(), s.end());
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


    std::wstring  StripTrailingSep (const std::wstring & p)
    {
        std::wstring  r = p;
        while (!r.empty() && (r.back() == L'\\' || r.back() == L'/'))
        {
            r.pop_back();
        }
        return r;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::JoinPath
//
//  Join `dir` + L'/' + leaf, normalising trailing separators.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring ThemeLoader::JoinPath (
    const std::wstring  & dir,
    const std::wstring  & leaf)
{
    std::wstring  result = StripTrailingSep (dir);



    if (!result.empty() && !leaf.empty())
    {
        result += L'\\';
    }
    result += leaf;
    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::EnumerateCandidateDirs
//
//  Walks `<themesBaseDir>` and returns the subset of sub-directory
//  names that look like candidate themes (theme.json exists). The
//  returned names are bare directory names — caller composes the
//  absolute path. Returns S_FALSE (with empty list) if
//  `themesBaseDir` itself doesn't exist.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeLoader::EnumerateCandidateDirs (
    IFileSystem                & fs,
    const std::wstring         & themesBaseDir,
    std::vector<std::wstring>  & outNames)
{
    HRESULT                     hr     = S_OK;
    std::vector<std::wstring>   dirs;
    size_t                      i      = 0;



    outNames.clear();

    hr = fs.EnumerateDirectories (themesBaseDir, dirs);

    if (FAILED (hr))
    {
        // Base directory missing → S_FALSE with empty list.
        return S_FALSE;
    }

    for (i = 0; i < dirs.size(); ++i)
    {
        std::wstring  themeJson = JoinPath (JoinPath (themesBaseDir, dirs[i]),
                                            L"theme.json");
        if (fs.Exists (themeJson))
        {
            outNames.push_back (dirs[i]);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::ParseMetadata
//
//  Parses + validates raw theme.json text. Doesn't touch the
//  filesystem.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeLoader::ParseMetadata (
    const std::string  & jsonText,
    LoadedTheme        & outTheme,
    ThemeLoadError     & outError)
{
    HRESULT             hr            = S_OK;
    JsonValue           root;
    JsonParseError      perr;
    const JsonValue *   uiTokensObj   = nullptr;
    const JsonValue *   driveProfile  = nullptr;
    const JsonValue *   crtObj        = nullptr;
    const JsonValue *   scanObj       = nullptr;
    const JsonValue *   bloomObj      = nullptr;
    const JsonValue *   bleedObj      = nullptr;
    int                 themeVersion  = 0;



    outTheme = LoadedTheme {};

    hr = JsonParser::Parse (jsonText, root, perr);

    if (FAILED (hr))
    {
        outError.code       = ThemeLoadResult::MetadataInvalid;
        outError.message    = perr.message;
        outError.jsonLine   = perr.line;
        outError.jsonColumn = perr.column;
        return hr;
    }

    if (root.GetType() != JsonType::Object)
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json root is not a JSON object";
        return E_INVALIDARG;
    }

    // ---- required: $cassoThemeVersion + name + family/variant ids ---------

    hr = root.GetInt (s_kpszVersionKey, themeVersion);

    if (FAILED (hr) || themeVersion < 1)
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing or invalid $cassoThemeVersion";
        return FAILED (hr) ? hr : E_INVALIDARG;
    }

    if (themeVersion > kCurrentThemeSchemaVersion)
    {
        outError.code    = ThemeLoadResult::VersionTooNew;
        outError.message = "theme.json $cassoThemeVersion is newer than this build supports";
        return E_NOTIMPL;
    }

    outTheme.version = themeVersion;

    hr = root.GetString ("name", outTheme.name);

    if (FAILED (hr) || outTheme.name.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing or empty `name`";
        return FAILED (hr) ? hr : E_INVALIDARG;
    }

    hr = root.GetString ("familyId", outTheme.familyId);

    if (FAILED (hr) || outTheme.familyId.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing or empty `familyId`";
        return FAILED (hr) ? hr : E_INVALIDARG;
    }

    hr = root.GetString ("variantId", outTheme.variantId);

    if (FAILED (hr) || outTheme.variantId.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing or empty `variantId`";
        return FAILED (hr) ? hr : E_INVALIDARG;
    }

    // ---- optional scalars --------------------------------------------------

    outTheme.author          = GetStringOpt (root, "author",      "");
    outTheme.description     = GetStringOpt (root, "description", "");
    outTheme.useMicaBackdrop = GetBoolOpt   (root, "useMicaBackdrop", false);
    outTheme.isBuiltIn       = GetBoolOpt   (root, s_kpszBuiltInKey,  false);

    // ---- required: uiTokens + driveVisualProfile --------------------------

    if (FAILED (root.GetObject ("uiTokens", uiTokensObj)) || uiTokensObj == nullptr)
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing required `uiTokens` object";
        return E_INVALIDARG;
    }
    outTheme.uiTokens = *uiTokensObj;

    if (FAILED (root.GetObject ("driveVisualProfile", driveProfile)) || driveProfile == nullptr)
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "theme.json missing required `driveVisualProfile` object";
        return E_INVALIDARG;
    }
    if (FAILED (driveProfile->GetString ("style", outTheme.driveVisualProfile.style)) ||
        outTheme.driveVisualProfile.style.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "driveVisualProfile.style is required";
        return E_INVALIDARG;
    }
    if (FAILED (driveProfile->GetString ("colorway", outTheme.driveVisualProfile.colorway)) ||
        outTheme.driveVisualProfile.colorway.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "driveVisualProfile.colorway is required";
        return E_INVALIDARG;
    }
    if (FAILED (driveProfile->GetString ("doorAnimation", outTheme.driveVisualProfile.doorAnimation)) ||
        outTheme.driveVisualProfile.doorAnimation.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "driveVisualProfile.doorAnimation is required";
        return E_INVALIDARG;
    }
    if (FAILED (driveProfile->GetString ("syncChannel", outTheme.driveVisualProfile.syncChannel)) ||
        outTheme.driveVisualProfile.syncChannel.empty())
    {
        outError.code    = ThemeLoadResult::MetadataInvalid;
        outError.message = "driveVisualProfile.syncChannel is required";
        return E_INVALIDARG;
    }

    // ---- crtDefaults (all optional; clamped to schema bounds) -------------

    if (SUCCEEDED (root.GetObject ("crtDefaults", crtObj)) && crtObj != nullptr)
    {
        double  d = 0.0;
        if (SUCCEEDED (crtObj->GetNumber ("brightness", d)))
        {
            outTheme.crtDefaults.brightness    = (float) d;
            outTheme.crtDefaults.hasBrightness = true;
        }
        if (SUCCEEDED (crtObj->GetNumber ("contrast", d)))
        {
            outTheme.crtDefaults.contrast    = (float) d;
            outTheme.crtDefaults.hasContrast = true;
        }

        if (SUCCEEDED (crtObj->GetObject ("scanlines", scanObj)) && scanObj != nullptr)
        {
            outTheme.crtDefaults.scanlinesEnabled   = GetBoolOpt   (*scanObj, "enabled",
                                                                    outTheme.crtDefaults.scanlinesEnabled);
            outTheme.crtDefaults.scanlinesIntensity = (float) GetNumberOpt (*scanObj, "intensity",
                                                                            outTheme.crtDefaults.scanlinesIntensity);
            outTheme.crtDefaults.hasScanlines       = true;
        }
        if (SUCCEEDED (crtObj->GetObject ("bloom", bloomObj)) && bloomObj != nullptr)
        {
            outTheme.crtDefaults.bloomEnabled  = GetBoolOpt (*bloomObj, "enabled",
                                                             outTheme.crtDefaults.bloomEnabled);
            outTheme.crtDefaults.bloomRadius   = (float) GetNumberOpt (*bloomObj, "radius",
                                                                       outTheme.crtDefaults.bloomRadius);
            outTheme.crtDefaults.bloomStrength = (float) GetNumberOpt (*bloomObj, "strength",
                                                                       outTheme.crtDefaults.bloomStrength);
            outTheme.crtDefaults.hasBloom      = true;
        }
        if (SUCCEEDED (crtObj->GetObject ("colorBleed", bleedObj)) && bleedObj != nullptr)
        {
            outTheme.crtDefaults.colorBleedEnabled = GetBoolOpt (*bleedObj, "enabled",
                                                                 outTheme.crtDefaults.colorBleedEnabled);
            outTheme.crtDefaults.colorBleedWidth   = (float) GetNumberOpt (*bleedObj, "width",
                                                                           outTheme.crtDefaults.colorBleedWidth);
            outTheme.crtDefaults.hasColorBleed     = true;
        }
    }

    // ---- optional: variantOverrides (per-machine sparse overlays) --------

    {
        const JsonValue *  overridesObj = nullptr;

        if (SUCCEEDED (root.GetObject ("variantOverrides", overridesObj)) && overridesObj != nullptr)
        {
            const auto &  entries = overridesObj->GetObjectEntries();
            size_t        i       = 0;

            for (i = 0; i < entries.size(); i++)
            {
                if (entries[i].second.GetType() == JsonType::Object)
                {
                    outTheme.variantOverrides.emplace_back (entries[i].first, entries[i].second);
                }
            }
        }
    }

    return S_OK;
}




////////////////////////////////////////////////////////////////////////////////
//
//  LoadedTheme::ResolveForMachine
//
//  Returns a copy of this theme with the variantOverrides entry for
//  `machineDisplayName` (if any) applied on top of the base. Match is
//  exact and case-sensitive against the machine JSON's "name" field.
//  Missing entries produce a verbatim copy.
//
//  Only the fields actually consumed downstream are merged today:
//   - crtDefaults (sub-objects: scanlines, bloom, colorBleed)
//   - driveVisualProfile (scalar replacement)
//   - useMicaBackdrop
//
//  uiTokens overrides are stored on the LoadedTheme but not deep-merged
//  yet because CassoTheme is keyed by name only (see CassoTheme::ForName).
//  When CassoTheme grows a JSON-driven path the merge will land here.
//
////////////////////////////////////////////////////////////////////////////////

LoadedTheme LoadedTheme::ResolveForMachine (const std::string & machineDisplayName) const
{
    LoadedTheme         result    = *this;
    const JsonValue *   override_ = nullptr;
    size_t              i         = 0;

    for (i = 0; i < variantOverrides.size(); i++)
    {
        if (variantOverrides[i].first == machineDisplayName)
        {
            override_ = &variantOverrides[i].second;
            break;
        }
    }

    if (override_ == nullptr)
    {
        return result;
    }

    {
        const JsonValue *  crtObj   = nullptr;
        const JsonValue *  scanObj  = nullptr;
        const JsonValue *  bloomObj = nullptr;
        const JsonValue *  bleedObj = nullptr;
        const JsonValue *  driveObj = nullptr;
        bool               mica     = false;

        if (SUCCEEDED (override_->GetObject ("crtDefaults", crtObj)) && crtObj != nullptr)
        {
            double  d = 0.0;
            if (SUCCEEDED (crtObj->GetNumber ("brightness", d))) { result.crtDefaults.brightness = (float) d; result.crtDefaults.hasBrightness = true; }
            if (SUCCEEDED (crtObj->GetNumber ("contrast",   d))) { result.crtDefaults.contrast   = (float) d; result.crtDefaults.hasContrast   = true; }

            if (SUCCEEDED (crtObj->GetObject ("scanlines", scanObj)) && scanObj != nullptr)
            {
                bool  b = false;
                if (SUCCEEDED (scanObj->GetBool   ("enabled",   b))) { result.crtDefaults.scanlinesEnabled   = b; }
                if (SUCCEEDED (scanObj->GetNumber ("intensity", d))) { result.crtDefaults.scanlinesIntensity = (float) d; }
                result.crtDefaults.hasScanlines = true;
            }
            if (SUCCEEDED (crtObj->GetObject ("bloom", bloomObj)) && bloomObj != nullptr)
            {
                bool  b = false;
                if (SUCCEEDED (bloomObj->GetBool   ("enabled",  b))) { result.crtDefaults.bloomEnabled  = b; }
                if (SUCCEEDED (bloomObj->GetNumber ("radius",   d))) { result.crtDefaults.bloomRadius   = (float) d; }
                if (SUCCEEDED (bloomObj->GetNumber ("strength", d))) { result.crtDefaults.bloomStrength = (float) d; }
                result.crtDefaults.hasBloom = true;
            }
            if (SUCCEEDED (crtObj->GetObject ("colorBleed", bleedObj)) && bleedObj != nullptr)
            {
                bool  b = false;
                if (SUCCEEDED (bleedObj->GetBool   ("enabled", b))) { result.crtDefaults.colorBleedEnabled = b; }
                if (SUCCEEDED (bleedObj->GetNumber ("width",   d))) { result.crtDefaults.colorBleedWidth   = (float) d; }
                result.crtDefaults.hasColorBleed = true;
            }
        }

        if (SUCCEEDED (override_->GetObject ("driveVisualProfile", driveObj)) && driveObj != nullptr)
        {
            std::string  s;
            if (SUCCEEDED (driveObj->GetString ("style",         s)) && !s.empty()) { result.driveVisualProfile.style         = s; }
            if (SUCCEEDED (driveObj->GetString ("colorway",      s)) && !s.empty()) { result.driveVisualProfile.colorway      = s; }
            if (SUCCEEDED (driveObj->GetString ("doorAnimation", s)) && !s.empty()) { result.driveVisualProfile.doorAnimation = s; }
            if (SUCCEEDED (driveObj->GetString ("syncChannel",   s)) && !s.empty()) { result.driveVisualProfile.syncChannel   = s; }
        }

        if (SUCCEEDED (override_->GetBool ("useMicaBackdrop", mica)))
        {
            result.useMicaBackdrop = mica;
        }
    }

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::Load
//
//  Loads `<themeDir>/theme.json` and validates it.
//
//  * On success returns S_OK and fills `outTheme`. `outError`
//    is left untouched.
//
//  * On any validation failure returns a failure HRESULT, fills
//    `outError`, and leaves `outTheme` in a default-constructed
//    state. The caller logs the structured error and excludes
//    the theme from the available list (FR-036).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeLoader::Load (
    IFileSystem                & fs,
    const std::wstring         & themeDir,
    LoadedTheme                & outTheme,
    ThemeLoadError             & outError)
{
    HRESULT       hr        = S_OK;
    std::wstring  themePath = JoinPath (themeDir, L"theme.json");
    std::string   text;



    outError = ThemeLoadError {};
    outError.themeDir = themeDir;
    outTheme = LoadedTheme {};

    if (!fs.Exists (themePath))
    {
        outError.code          = ThemeLoadResult::MetadataMissing;
        outError.offendingPath = themePath;
        outError.message       = "theme.json not found in theme directory";
        return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    }

    hr = fs.ReadAllText (themePath, text);

    if (FAILED (hr))
    {
        outError.code          = ThemeLoadResult::MetadataInvalid;
        outError.offendingPath = themePath;
        outError.message       = "failed to read theme.json";
        return hr;
    }

    hr = ParseMetadata (text, outTheme, outError);

    if (FAILED (hr))
    {
        outError.offendingPath = themePath;
        return hr;
    }

    outTheme.directoryPath = StripTrailingSep (themeDir);

    return S_OK;
}
