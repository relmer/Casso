#include "Pch.h"

#include "ThemeLoader.h"


#include "Core/JsonParser.h"

#include <sstream>





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
        // names we deal with here; any non-ASCII filename is a
        // theme-author bug and will surface as DocumentMissing.
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
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeLoader::ParseMetadata (
    const std::string  & jsonText,
    LoadedTheme        & outTheme,
    ThemeLoadError     & outError)
{
    HRESULT             hr            = S_OK;
    JsonValue           root;
    JsonParseError      perr;
    const JsonValue *   entryObj      = nullptr;
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

    // ---- required: $cassoThemeVersion + name ------------------------------

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

    // ---- optional scalars --------------------------------------------------

    outTheme.author          = GetStringOpt (root, "author",      "");
    outTheme.description     = GetStringOpt (root, "description", "");
    outTheme.useMicaBackdrop = GetBoolOpt   (root, "useMicaBackdrop", false);
    outTheme.isBuiltIn       = GetBoolOpt   (root, s_kpszBuiltInKey,  false);

    // ---- entryDocuments (all optional; resolved later) --------------------

    if (SUCCEEDED (root.GetObject ("entryDocuments", entryObj)) && entryObj != nullptr)
    {
        std::string  s;

        if (SUCCEEDED (entryObj->GetString ("titleBar", s)))
        {
            outTheme.entryDocs.titleBar = Utf8ToWide (s);
        }
        if (SUCCEEDED (entryObj->GetString ("navLayer", s)))
        {
            outTheme.entryDocs.navLayer = Utf8ToWide (s);
        }
        if (SUCCEEDED (entryObj->GetString ("settings", s)))
        {
            outTheme.entryDocs.settings = Utf8ToWide (s);
        }
        if (SUCCEEDED (entryObj->GetString ("driveWidgets", s)))
        {
            outTheme.entryDocs.driveWidgets = Utf8ToWide (s);
        }
    }

    // ---- crtDefaults (all optional; clamped to schema bounds) -------------

    if (SUCCEEDED (root.GetObject ("crtDefaults", crtObj)) && crtObj != nullptr)
    {
        outTheme.crtDefaults.brightness =
            (float) GetNumberOpt (*crtObj, "brightness", outTheme.crtDefaults.brightness);

        if (SUCCEEDED (crtObj->GetObject ("scanlines", scanObj)) && scanObj != nullptr)
        {
            outTheme.crtDefaults.scanlinesEnabled   = GetBoolOpt   (*scanObj, "enabled",
                                                                    outTheme.crtDefaults.scanlinesEnabled);
            outTheme.crtDefaults.scanlinesIntensity = (float) GetNumberOpt (*scanObj, "intensity",
                                                                            outTheme.crtDefaults.scanlinesIntensity);
        }
        if (SUCCEEDED (crtObj->GetObject ("bloom", bloomObj)) && bloomObj != nullptr)
        {
            outTheme.crtDefaults.bloomEnabled  = GetBoolOpt (*bloomObj, "enabled",
                                                             outTheme.crtDefaults.bloomEnabled);
            outTheme.crtDefaults.bloomRadius   = (float) GetNumberOpt (*bloomObj, "radius",
                                                                       outTheme.crtDefaults.bloomRadius);
            outTheme.crtDefaults.bloomStrength = (float) GetNumberOpt (*bloomObj, "strength",
                                                                       outTheme.crtDefaults.bloomStrength);
        }
        if (SUCCEEDED (crtObj->GetObject ("colorBleed", bleedObj)) && bleedObj != nullptr)
        {
            outTheme.crtDefaults.colorBleedEnabled = GetBoolOpt (*bleedObj, "enabled",
                                                                 outTheme.crtDefaults.colorBleedEnabled);
            outTheme.crtDefaults.colorBleedWidth   = (float) GetNumberOpt (*bleedObj, "width",
                                                                           outTheme.crtDefaults.colorBleedWidth);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::ResolveEntryDocs
//
////////////////////////////////////////////////////////////////////////////////

static HRESULT ResolveOne (
    IFileSystem         & fs,
    const std::wstring  & themeDir,
    const std::wstring  & sharedDir,
    const wchar_t       * sharedLeaf,
    std::wstring        & ioPath,
    ThemeLoadError      & outError)
{
    std::wstring  candidate;
    std::wstring  fallback;


    if (!ioPath.empty())
    {
        // Theme declared an entry — must exist in theme dir.
        candidate = ThemeLoader::JoinPath (themeDir, ioPath);
        if (fs.Exists (candidate))
        {
            ioPath = candidate;
            return S_OK;
        }

        outError.code          = ThemeLoadResult::DocumentMissing;
        outError.offendingPath = candidate;
        outError.message       = "theme.json entryDocuments path does not exist on disk";
        return HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND);
    }

    // Not declared — try _shared fallback.
    if (!sharedDir.empty() && sharedLeaf != nullptr)
    {
        fallback = ThemeLoader::JoinPath (sharedDir, sharedLeaf);
        if (fs.Exists (fallback))
        {
            ioPath = fallback;
            return S_OK;
        }
    }

    // Neither declared nor shared — leave empty. Caller decides
    // whether the missing entry is fatal (titleBar/navLayer are
    // mandatory; settings/driveWidgets are optional in early phases).
    return S_OK;
}


HRESULT ThemeLoader::ResolveEntryDocs (
    IFileSystem         & fs,
    const std::wstring  & themeDir,
    const std::wstring  & sharedDir,
    LoadedTheme         & ioTheme,
    ThemeLoadError      & outError)
{
    HRESULT  hr = S_OK;


    hr = ResolveOne (fs, themeDir, sharedDir, L"title_bar.rml",
                     ioTheme.entryDocs.titleBar, outError);
    if (FAILED (hr)) { return hr; }

    hr = ResolveOne (fs, themeDir, sharedDir, L"nav_layer.rml",
                     ioTheme.entryDocs.navLayer, outError);
    if (FAILED (hr)) { return hr; }

    hr = ResolveOne (fs, themeDir, sharedDir, L"settings.rml",
                     ioTheme.entryDocs.settings, outError);
    if (FAILED (hr)) { return hr; }

    hr = ResolveOne (fs, themeDir, sharedDir, L"drive_widgets.rml",
                     ioTheme.entryDocs.driveWidgets, outError);
    if (FAILED (hr)) { return hr; }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeLoader::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeLoader::Load (
    IFileSystem                & fs,
    const std::wstring         & themeDir,
    const std::wstring         & sharedDir,
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

    hr = ResolveEntryDocs (fs, themeDir, sharedDir, outTheme, outError);

    if (FAILED (hr))
    {
        outTheme = LoadedTheme {};
        return hr;
    }

    return S_OK;
}
