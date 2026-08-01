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





////////////////////////////////////////////////////////////////////////////////
//
//  Required-key schema
//
//  What theme.json must contain, as data. Presence and type are settled once
//  from these tables, which is what lets every check further down be about the
//  *value* alone -- the messages used to all read "missing or invalid X"
//  because each site was answering two questions at once and could not say
//  which one had failed.
//
//  `$cassoThemeVersion` is deliberately absent from the root table. It is
//  handled before the sweep runs, because a schema newer than this build may
//  legitimately rename or drop keys listed here; reporting those as malformed
//  would bury the answer the user actually needs, which is that their Casso is
//  too old. Version first, then contents.
//
////////////////////////////////////////////////////////////////////////////////

struct RequiredKey
{
    const char *  name;
    JsonType      type;
    bool          mustBeNonEmpty;   // strings only: "" is present but useless
};

static constexpr RequiredKey  s_kRequiredRootKeys[] =
{
    { "name",               JsonType::String, true  },
    { "familyId",           JsonType::String, true  },
    { "variantId",          JsonType::String, true  },
    { "uiTokens",           JsonType::Object, false },
    { "driveVisualProfile", JsonType::Object, false },
};

static constexpr RequiredKey  s_kRequiredDriveKeys[] =
{
    { "style",         JsonType::String, true },
    { "colorway",      JsonType::String, true },
    { "doorAnimation", JsonType::String, true },
    { "syncChannel",   JsonType::String, true },
};

// Indexed by JsonType, for the "must be a string" half of the message.
static constexpr const char *  s_kJsonTypeNames[] =
{
    "null", "a boolean", "a number", "a string", "an array", "an object",
};

static_assert (std::size (s_kJsonTypeNames) == (size_t) JsonType::Object + 1,
               "every JsonType needs a name for diagnostics");





////////////////////////////////////////////////////////////////////////////////
//
//  FindMember
//
//  JsonValue::Find is private, and the typed getters cannot tell "absent"
//  from "wrong type" -- which is the distinction the messages need.
//
////////////////////////////////////////////////////////////////////////////////

static const JsonValue * FindMember (const JsonValue & obj, const std::string & key)
{
    const JsonValue *  found = nullptr;



    for (const std::pair<std::string, JsonValue> & entry : obj.GetObjectEntries())
    {
        if (entry.first == key)
        {
            found = &entry.second;
            break;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasRequiredKeys
//
//  True when every listed key is present with the listed type. On failure
//  `outProblem` names the first offender and says which of the two things went
//  wrong with it.
//
////////////////////////////////////////////////////////////////////////////////

static bool HasRequiredKeys (const JsonValue              & obj,
                             const char                   * context,
                             std::span<const RequiredKey>   required,
                             std::string                  & outProblem)
{
    bool  result  = true;
    bool  isEmpty = false;

    outProblem.clear();



    for (const RequiredKey & key : required)
    {
        const JsonValue *  member = FindMember (obj, key.name);

        if (member == nullptr)
        {
            outProblem = std::format ("{} is missing required key `{}`", context, key.name);
            result     = false;
            break;
        }

        if (member->GetType() != key.type)
        {
            outProblem = std::format ("{} key `{}` must be {}",
                                      context, key.name, s_kJsonTypeNames[(size_t) key.type]);
            result     = false;
            break;
        }

        // Present and correctly typed is not the same as usable. Folding this
        // in here is what lets the caller stop re-testing every field it just
        // read -- and what keeps a `.empty()` call out of seven EHM guards.
        isEmpty = key.mustBeNonEmpty && member->GetString().empty();

        if (isEmpty)
        {
            outProblem = std::format ("{} key `{}` must not be empty", context, key.name);
            result     = false;
            break;
        }
    }

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



    outNames.clear();

    hr = fs.EnumerateDirectories (themesBaseDir, dirs);

    // Base directory missing: no themes to enumerate. outNames is already
    // cleared, and that emptiness is the whole answer -- no second result
    // code needed, so this reports success rather than propagating.
    BAIL_OUT_IF (FAILED (hr), S_OK);

    for (const std::wstring & dir : dirs)
    {
        std::wstring  themeJson = JoinPath (JoinPath (themesBaseDir, dir), L"theme.json");

        if (fs.Exists (themeJson))
        {
            outNames.push_back (dir);
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
    const JsonValue *   overridesObj  = nullptr;
    int                 themeVersion  = 0;
    double              versionValue  = 0.0;
    JsonType            rootType      = JsonType::Null;
    bool                present       = false;
    std::string         problem;



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

    // the version gate, before anything else is judged
    //
    // A theme written for a newer schema may legitimately not look like one
    // this build understands, so "your Casso is too old" has to be decided
    // before its contents are called malformed.

    present = root.HasNumber (s_kpszVersionKey, versionValue);
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = std::format ("theme.json is missing required key `{}`",
                                            s_kpszVersionKey));

    themeVersion = (int) versionValue;

    CBRFEx (themeVersion >= 1, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = std::format ("theme.json `{}` must be 1 or greater",
                                            s_kpszVersionKey));

    CBRFEx (themeVersion <= kCurrentThemeSchemaVersion, E_NOTIMPL,
            outError.code    = ThemeLoadResult::VersionTooNew;
            outError.message = "theme.json $cassoThemeVersion is newer than this build supports");

    outTheme.version = themeVersion;

    // presence and type, settled once from the schema tables
    //
    // Everything below this point is a question about a *value*. That is the
    // whole reason for doing it here: each message used to read "missing or
    // invalid X" because the site was answering two questions at once and had
    // no way to say which had failed.

    present = HasRequiredKeys (root, "theme.json", s_kRequiredRootKeys, problem);
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = problem);

    // Guaranteed by the sweep above; CBRA because dereferencing it needs that
    // guarantee, and a failure here means the table drifted from the reads.
    present = root.HasObject ("driveVisualProfile", driveProfile);
    CBRA (present);

    present = HasRequiredKeys (*driveProfile, "driveVisualProfile", s_kRequiredDriveKeys, problem);
    CBRFEx (present, E_INVALIDARG,
            outError.code    = ThemeLoadResult::MetadataInvalid;
            outError.message = problem);

    // optional scalars

    outTheme.author          = GetStringOpt (root, "author",      "");
    outTheme.description     = GetStringOpt (root, "description", "");
    outTheme.useMicaBackdrop = GetBoolOpt   (root, "useMicaBackdrop", false);
    outTheme.isBuiltIn       = GetBoolOpt   (root, s_kpszBuiltInKey,  false);

    // reads
    //
    // Nothing below can fail. The sweep proved every key is present, correctly
    // typed and non-empty, so these are plain reads through the *Opt getters
    // whose fallback is unreachable. There is no guard here because there is
    // no longer a question to ask.

    present = root.HasObject ("uiTokens", uiTokensObj);
    CBRA (present);

    outTheme.uiTokens = *uiTokensObj;

    outTheme.name      = GetStringOpt (root, "name",      "");
    outTheme.familyId  = GetStringOpt (root, "familyId",  "");
    outTheme.variantId = GetStringOpt (root, "variantId", "");

    outTheme.driveVisualProfile.style         = GetStringOpt (*driveProfile, "style",         "");
    outTheme.driveVisualProfile.colorway      = GetStringOpt (*driveProfile, "colorway",      "");
    outTheme.driveVisualProfile.doorAnimation = GetStringOpt (*driveProfile, "doorAnimation", "");
    outTheme.driveVisualProfile.syncChannel   = GetStringOpt (*driveProfile, "syncChannel",   "");

    // crtDefaults (all optional; clamped to schema bounds)

    if (root.HasObject ("crtDefaults", crtObj))
    {
        double  d = 0.0;

        if (crtObj->HasNumber ("brightness", d))
        {
            outTheme.crtDefaults.brightness    = (float) d;
            outTheme.crtDefaults.hasBrightness = true;
        }

        if (crtObj->HasNumber ("contrast", d))
        {
            outTheme.crtDefaults.contrast    = (float) d;
            outTheme.crtDefaults.hasContrast = true;
        }

        if (crtObj->HasObject ("scanlines", scanObj))
        {
            outTheme.crtDefaults.scanlinesEnabled   = GetBoolOpt   (*scanObj, "enabled",
                                                                    outTheme.crtDefaults.scanlinesEnabled);
            outTheme.crtDefaults.scanlinesIntensity = (float) GetNumberOpt (*scanObj, "intensity",
                                                                            outTheme.crtDefaults.scanlinesIntensity);
            outTheme.crtDefaults.hasScanlines       = true;
        }

        if (crtObj->HasObject ("bloom", bloomObj))
        {
            outTheme.crtDefaults.bloomEnabled  = GetBoolOpt (*bloomObj, "enabled",
                                                             outTheme.crtDefaults.bloomEnabled);
            outTheme.crtDefaults.bloomRadius   = (float) GetNumberOpt (*bloomObj, "radius",
                                                                       outTheme.crtDefaults.bloomRadius);
            outTheme.crtDefaults.bloomStrength = (float) GetNumberOpt (*bloomObj, "strength",
                                                                       outTheme.crtDefaults.bloomStrength);
            outTheme.crtDefaults.hasBloom      = true;
        }

        if (crtObj->HasObject ("colorBleed", bleedObj))
        {
            outTheme.crtDefaults.colorBleedEnabled = GetBoolOpt (*bleedObj, "enabled",
                                                                 outTheme.crtDefaults.colorBleedEnabled);
            outTheme.crtDefaults.colorBleedWidth   = (float) GetNumberOpt (*bleedObj, "width",
                                                                           outTheme.crtDefaults.colorBleedWidth);
            outTheme.crtDefaults.hasColorBleed     = true;
        }
    }

    // optional: variantOverrides (per-machine sparse overlays)

    if (root.HasObject ("variantOverrides", overridesObj))
    {
        for (const std::pair<std::string, JsonValue> & entry : overridesObj->GetObjectEntries())
        {
            if (entry.second.GetType() == JsonType::Object)
            {
                outTheme.variantOverrides.emplace_back (entry.first, entry.second);
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



    for (const std::pair<std::string, JsonValue> & entry : variantOverrides)
    {
        if (entry.first == machineDisplayName)
        {
            override_ = &entry.second;
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

        if (override_->HasObject ("crtDefaults", crtObj))
        {
            double  d = 0.0;

            if (crtObj->HasNumber ("brightness", d)) { result.crtDefaults.brightness = (float) d; result.crtDefaults.hasBrightness = true; }
            if (crtObj->HasNumber ("contrast",   d)) { result.crtDefaults.contrast   = (float) d; result.crtDefaults.hasContrast   = true; }

            if (crtObj->HasObject ("scanlines", scanObj))
            {
                bool  b = false;

                if (scanObj->HasBool   ("enabled",   b)) { result.crtDefaults.scanlinesEnabled   = b; }
                if (scanObj->HasNumber ("intensity", d)) { result.crtDefaults.scanlinesIntensity = (float) d; }

                result.crtDefaults.hasScanlines = true;
            }

            if (crtObj->HasObject ("bloom", bloomObj))
            {
                bool  b = false;

                if (bloomObj->HasBool   ("enabled",  b)) { result.crtDefaults.bloomEnabled  = b; }
                if (bloomObj->HasNumber ("radius",   d)) { result.crtDefaults.bloomRadius   = (float) d; }
                if (bloomObj->HasNumber ("strength", d)) { result.crtDefaults.bloomStrength = (float) d; }

                result.crtDefaults.hasBloom = true;
            }

            if (crtObj->HasObject ("colorBleed", bleedObj))
            {
                bool  b = false;

                if (bleedObj->HasBool   ("enabled", b)) { result.crtDefaults.colorBleedEnabled = b; }
                if (bleedObj->HasNumber ("width",   d)) { result.crtDefaults.colorBleedWidth   = (float) d; }

                result.crtDefaults.hasColorBleed = true;
            }
        }

        if (override_->HasObject ("driveVisualProfile", driveObj))
        {
            std::string  s;

            if (driveObj->HasString ("style",         s) && !s.empty()) { result.driveVisualProfile.style         = s; }
            if (driveObj->HasString ("colorway",      s) && !s.empty()) { result.driveVisualProfile.colorway      = s; }
            if (driveObj->HasString ("doorAnimation", s) && !s.empty()) { result.driveVisualProfile.doorAnimation = s; }
            if (driveObj->HasString ("syncChannel",   s) && !s.empty()) { result.driveVisualProfile.syncChannel   = s; }
        }

        if (override_->HasBool ("useMicaBackdrop", mica))
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

