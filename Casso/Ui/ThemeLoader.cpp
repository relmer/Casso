#include "Pch.h"

#include "ThemeLoader.h"


#include "Core/JsonParser.h"






////////////////////////////////////////////////////////////////////////////////
//
//  Anonymous helpers
//
////////////////////////////////////////////////////////////////////////////////

static constexpr const char *  s_kpszVersionKey  = "$cassoThemeVersion";
static constexpr const char *  s_kpszBuiltInKey  = "$cassoBuiltIn";


std::wstring  ThemeLoader::Utf8ToWide (const std::string & s)
{
    // Theme paths in theme.json are ASCII by spec (filename
    // restrictions). A naive widen is fine for the relative
    // names we deal with here.
    return std::wstring (s.begin(), s.end());
}


//  Presence tests for optional members. Every caller used to write
//  SUCCEEDED (obj.GetNumber (key, out)) inline, which puts a call inside a
//  macro argument -- forbidden for the same reason it is in an EHM condition,
//  and doubly awkward because the macro hid a call with an out param. Wrapping
//  it turns each site into an ordinary call in an ordinary `if`, which the
//  rule has no quarrel with.

static bool  HasBool (const JsonValue & obj, const std::string & key, bool & out)
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    hr     = obj.GetBool (key, out);
    result = SUCCEEDED (hr);

    return result;
}


static bool  HasNumber (const JsonValue & obj, const std::string & key, double & out)
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    hr     = obj.GetNumber (key, out);
    result = SUCCEEDED (hr);

    return result;
}


static bool  HasString (const JsonValue & obj, const std::string & key, std::string & out)
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    hr     = obj.GetString (key, out);
    result = SUCCEEDED (hr);

    return result;
}


//  Folds in the null check every caller repeated after the SUCCEEDED test.

static bool  HasObject (const JsonValue & obj, const std::string & key, const JsonValue * & out)
{
    HRESULT  hr     = S_OK;
    bool     result = false;



    out    = nullptr;
    hr     = obj.GetObject (key, out);
    result = SUCCEEDED (hr) && out != nullptr;

    return result;
}


//  The three optional getters below swallow failure by contract -- absent or
//  wrong-typed means "use the fallback", not an error to report. They still
//  call a failable API, so they take the documented non-HRESULT EHM shape: a
//  vestigial `hr` for the macro, and the normal result returned at `Error:`.

bool  ThemeLoader::GetBoolOpt (
    const JsonValue   & obj,
    const std::string & key,
    bool                fallback)
{
    HRESULT  hr     = S_OK;
    bool     result = fallback;



    hr = obj.GetBool (key, result);
    CHRF (hr, result = fallback);

Error:
    return result;
}


double  ThemeLoader::GetNumberOpt (
    const JsonValue   & obj,
    const std::string & key,
    double              fallback)
{
    HRESULT  hr     = S_OK;
    double   result = fallback;



    hr = obj.GetNumber (key, result);
    CHRF (hr, result = fallback);

Error:
    return result;
}


std::string  ThemeLoader::GetStringOpt (
    const JsonValue   & obj,
    const std::string & key,
    const std::string & fallback)
{
    HRESULT      hr     = S_OK;
    std::string  result = fallback;



    hr = obj.GetString (key, result);
    CHRF (hr, result = fallback);

Error:
    return result;
}


std::wstring  ThemeLoader::StripTrailingSep (const std::wstring & p)
{
    std::wstring  r = p;
    while (!r.empty() && (r.back() == L'\\' || r.back() == L'/'))
    {
        r.pop_back();
    }
    return r;
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
//  absolute path. Returns S_OK with an empty list if
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

    // Base directory missing: no themes to enumerate. outNames is already
    // cleared, and that emptiness is the whole answer -- no second result
    // code needed, so this reports success rather than propagating.
    BAIL_OUT_IF (FAILED (hr), S_OK);

    for (i = 0; i < dirs.size(); ++i)
    {
        std::wstring  themeJson = JoinPath (JoinPath (themesBaseDir, dirs[i]),
                                            L"theme.json");
        if (fs.Exists (themeJson))
        {
            outNames.push_back (dirs[i]);
        }
    }

Error:
    return hr;
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
    JsonType            rootType      = JsonType::Null;
    bool                present       = false;



    outTheme = LoadedTheme {};

    hr = JsonParser::Parse (jsonText, root, perr);
    CHRF (hr,
          outError.code       = ThemeLoadResult::MetadataInvalid;
          outError.message    = perr.message;
          outError.jsonLine   = perr.line;
          outError.jsonColumn = perr.column);

    rootType = root.GetType();
    CBRFEx (rootType == JsonType::Object, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "theme.json root is not a JSON object");

    // ---- required: $cassoThemeVersion + name + family/variant ids ---------
    //
    // Each of these can fail two ways -- the getter failed, or it succeeded
    // and produced something unusable. The original returned
    // `FAILED (hr) ? hr : E_INVALIDARG` at every one of them; folding the
    // second case into hr first says the same thing once.

    hr = root.GetInt (s_kpszVersionKey, themeVersion);

    if (SUCCEEDED (hr) && themeVersion < 1)
    {
        hr = E_INVALIDARG;
    }

    CHRF (hr,
          outError.code    = ThemeLoadResult::MetadataInvalid;
          outError.message = "theme.json missing or invalid $cassoThemeVersion");

    CBRFEx (themeVersion <= kCurrentThemeSchemaVersion, E_NOTIMPL,
            outError.code    = ThemeLoadResult::VersionTooNew;
            outError.message = "theme.json $cassoThemeVersion is newer than this build supports");

    outTheme.version = themeVersion;

    hr = root.GetString ("name", outTheme.name);

    if (SUCCEEDED (hr) && outTheme.name.empty())
    {
        hr = E_INVALIDARG;
    }

    CHRF (hr,
          outError.code    = ThemeLoadResult::MetadataInvalid;
          outError.message = "theme.json missing or empty `name`");

    hr = root.GetString ("familyId", outTheme.familyId);

    if (SUCCEEDED (hr) && outTheme.familyId.empty())
    {
        hr = E_INVALIDARG;
    }

    CHRF (hr,
          outError.code    = ThemeLoadResult::MetadataInvalid;
          outError.message = "theme.json missing or empty `familyId`");

    hr = root.GetString ("variantId", outTheme.variantId);

    if (SUCCEEDED (hr) && outTheme.variantId.empty())
    {
        hr = E_INVALIDARG;
    }

    CHRF (hr,
          outError.code    = ThemeLoadResult::MetadataInvalid;
          outError.message = "theme.json missing or empty `variantId`");

    // ---- optional scalars --------------------------------------------------

    outTheme.author          = GetStringOpt (root, "author",      "");
    outTheme.description     = GetStringOpt (root, "description", "");
    outTheme.useMicaBackdrop = GetBoolOpt   (root, "useMicaBackdrop", false);
    outTheme.isBuiltIn       = GetBoolOpt   (root, s_kpszBuiltInKey,  false);

    // ---- required: uiTokens + driveVisualProfile --------------------------

    // These six report E_INVALIDARG whatever the getter said, so the getter's
    // own code never has to be carried -- a presence flag says it all. Missing
    // and present-but-empty are the same answer to the caller.

    present = HasObject (root, "uiTokens", uiTokensObj);
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "theme.json missing required `uiTokens` object");

    outTheme.uiTokens = *uiTokensObj;

    present = HasObject (root, "driveVisualProfile", driveProfile);
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "theme.json missing required `driveVisualProfile` object");

    present = HasString (*driveProfile, "style", outTheme.driveVisualProfile.style)
              && !outTheme.driveVisualProfile.style.empty();
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "driveVisualProfile.style is required");

    present = HasString (*driveProfile, "colorway", outTheme.driveVisualProfile.colorway)
              && !outTheme.driveVisualProfile.colorway.empty();
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "driveVisualProfile.colorway is required");

    present = HasString (*driveProfile, "doorAnimation", outTheme.driveVisualProfile.doorAnimation)
              && !outTheme.driveVisualProfile.doorAnimation.empty();
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "driveVisualProfile.doorAnimation is required");

    present = HasString (*driveProfile, "syncChannel", outTheme.driveVisualProfile.syncChannel)
              && !outTheme.driveVisualProfile.syncChannel.empty();
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = "driveVisualProfile.syncChannel is required");

    // ---- crtDefaults (all optional; clamped to schema bounds) -------------

    if (HasObject (root, "crtDefaults", crtObj))
    {
        double  d = 0.0;

        if (HasNumber (*crtObj, "brightness", d))
        {
            outTheme.crtDefaults.brightness    = (float) d;
            outTheme.crtDefaults.hasBrightness = true;
        }

        if (HasNumber (*crtObj, "contrast", d))
        {
            outTheme.crtDefaults.contrast    = (float) d;
            outTheme.crtDefaults.hasContrast = true;
        }

        if (HasObject (*crtObj, "scanlines", scanObj))
        {
            outTheme.crtDefaults.scanlinesEnabled   = GetBoolOpt   (*scanObj, "enabled",
                                                                    outTheme.crtDefaults.scanlinesEnabled);
            outTheme.crtDefaults.scanlinesIntensity = (float) GetNumberOpt (*scanObj, "intensity",
                                                                            outTheme.crtDefaults.scanlinesIntensity);
            outTheme.crtDefaults.hasScanlines       = true;
        }

        if (HasObject (*crtObj, "bloom", bloomObj))
        {
            outTheme.crtDefaults.bloomEnabled  = GetBoolOpt (*bloomObj, "enabled",
                                                             outTheme.crtDefaults.bloomEnabled);
            outTheme.crtDefaults.bloomRadius   = (float) GetNumberOpt (*bloomObj, "radius",
                                                                       outTheme.crtDefaults.bloomRadius);
            outTheme.crtDefaults.bloomStrength = (float) GetNumberOpt (*bloomObj, "strength",
                                                                       outTheme.crtDefaults.bloomStrength);
            outTheme.crtDefaults.hasBloom      = true;
        }

        if (HasObject (*crtObj, "colorBleed", bleedObj))
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

        if (HasObject (root, "variantOverrides", overridesObj))
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

Error:
    return hr;
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

        if (HasObject (*override_, "crtDefaults", crtObj))
        {
            double  d = 0.0;

            if (HasNumber (*crtObj, "brightness", d)) { result.crtDefaults.brightness = (float) d; result.crtDefaults.hasBrightness = true; }
            if (HasNumber (*crtObj, "contrast",   d)) { result.crtDefaults.contrast   = (float) d; result.crtDefaults.hasContrast   = true; }

            if (HasObject (*crtObj, "scanlines", scanObj))
            {
                bool  b = false;

                if (HasBool   (*scanObj, "enabled",   b)) { result.crtDefaults.scanlinesEnabled   = b; }
                if (HasNumber (*scanObj, "intensity", d)) { result.crtDefaults.scanlinesIntensity = (float) d; }

                result.crtDefaults.hasScanlines = true;
            }

            if (HasObject (*crtObj, "bloom", bloomObj))
            {
                bool  b = false;

                if (HasBool   (*bloomObj, "enabled",  b)) { result.crtDefaults.bloomEnabled  = b; }
                if (HasNumber (*bloomObj, "radius",   d)) { result.crtDefaults.bloomRadius   = (float) d; }
                if (HasNumber (*bloomObj, "strength", d)) { result.crtDefaults.bloomStrength = (float) d; }

                result.crtDefaults.hasBloom = true;
            }

            if (HasObject (*crtObj, "colorBleed", bleedObj))
            {
                bool  b = false;

                if (HasBool   (*bleedObj, "enabled", b)) { result.crtDefaults.colorBleedEnabled = b; }
                if (HasNumber (*bleedObj, "width",   d)) { result.crtDefaults.colorBleedWidth   = (float) d; }

                result.crtDefaults.hasColorBleed = true;
            }
        }

        if (HasObject (*override_, "driveVisualProfile", driveObj))
        {
            std::string  s;

            if (HasString (*driveObj, "style",         s) && !s.empty()) { result.driveVisualProfile.style         = s; }
            if (HasString (*driveObj, "colorway",      s) && !s.empty()) { result.driveVisualProfile.colorway      = s; }
            if (HasString (*driveObj, "doorAnimation", s) && !s.empty()) { result.driveVisualProfile.doorAnimation = s; }
            if (HasString (*driveObj, "syncChannel",   s) && !s.empty()) { result.driveVisualProfile.syncChannel   = s; }
        }

        if (HasBool (*override_, "useMicaBackdrop", mica))
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
    bool          exists    = false;



    outError = ThemeLoadError {};
    outError.themeDir = themeDir;
    outTheme = LoadedTheme {};

    exists = fs.Exists (themePath);
    CBRFEx (exists, HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND),
            outError.code          = ThemeLoadResult::MetadataMissing;
            outError.offendingPath = themePath;
            outError.message       = "theme.json not found in theme directory");

    hr = fs.ReadAllText (themePath, text);
    CHRF (hr,
          outError.code          = ThemeLoadResult::MetadataInvalid;
          outError.offendingPath = themePath;
          outError.message       = "failed to read theme.json");

    // ParseMetadata has already filled outError; only the path is missing,
    // because it is the one thing that function does not know.
    hr = ParseMetadata (text, outTheme, outError);
    CHRF (hr, outError.offendingPath = themePath);

    outTheme.directoryPath = StripTrailingSep (themeDir);

Error:
    return hr;
}

