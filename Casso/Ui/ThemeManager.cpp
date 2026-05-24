#include "Pch.h"

#include "ThemeManager.h"

#include "Core/JsonValue.h"
#include "Core/JsonParser.h"






////////////////////////////////////////////////////////////////////////////////
//
//  ThemeBootstrapPlanner::Plan
//
////////////////////////////////////////////////////////////////////////////////

ThemeBootstrapAction ThemeBootstrapPlanner::Plan (
    const std::string * themeJsonOnDisk,
    int                 currentVersion)
{
    HRESULT         hr        = S_OK;
    JsonValue       parsed;
    JsonParseError  err;
    bool            isBuiltIn = false;
    int             version   = 0;



    if (themeJsonOnDisk == nullptr)
    {
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    hr = JsonParser::Parse (*themeJsonOnDisk, parsed, err);

    if (FAILED (hr) || parsed.GetType() != JsonType::Object)
    {
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    hr = parsed.GetBool ("$cassoBuiltIn", isBuiltIn);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (!isBuiltIn)
    {
        return ThemeBootstrapAction::Skip;
    }

    hr = parsed.GetInt ("$cassoThemeVersion", version);
    IGNORE_RETURN_VALUE (hr, S_OK);

    if (version < currentVersion)
    {
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    return ThemeBootstrapAction::Skip;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ThemeManager
//
////////////////////////////////////////////////////////////////////////////////

ThemeManager::ThemeManager (
    IFileSystem        & fs,
    const std::wstring & themesBaseDir)
    : m_fs (fs)
    , m_themesBaseDir (themesBaseDir)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Discover
//
//  Enumerates every theme directory beneath `m_themesBaseDir` and
//  parses each `theme.json` via `ThemeLoader::Load`. Themes whose
//  metadata fails to validate are silently excluded; the remainder
//  populate `m_available`.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Discover ()
{
    HRESULT                    hr      = S_OK;
    std::vector<std::wstring>  candidates;



    m_available.clear();

    hr = ThemeLoader::EnumerateCandidateDirs (m_fs, m_themesBaseDir, candidates);

    if (hr == S_FALSE)
    {
        return S_OK;
    }

    CHRA (hr);

    for (const std::wstring & name : candidates)
    {
        LoadedTheme     theme;
        ThemeLoadError  err;
        std::wstring    dir    = ThemeLoader::JoinPath (m_themesBaseDir, name);
        HRESULT         hrLoad = ThemeLoader::Load (m_fs, dir, theme, err);

        if (SUCCEEDED (hrLoad))
        {
            m_available.push_back (std::move (theme));
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Activate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Activate (const std::string & themeName)
{
    for (const LoadedTheme & t : m_available)
    {
        if (t.name == themeName)
        {
            m_activeName      = t.name;
            m_activeFamilyId  = t.familyId;
            m_activeVariantId = t.variantId;
            NotifyListeners (t);
            return S_OK;
        }
    }

    return S_FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ActivateByFamilyVariant
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ActivateByFamilyVariant (
    const std::string & familyId,
    const std::string & variantId)
{
    for (const LoadedTheme & t : m_available)
    {
        if (t.familyId == familyId && t.variantId == variantId)
        {
            return Activate (t.name);
        }
    }

    return S_FALSE;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadCurrent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ReloadCurrent ()
{
    HRESULT      hr             = S_OK;
    std::string  previousActive = m_activeName;



    hr = Discover();
    CHR (hr);

    if (!previousActive.empty())
    {
        hr = Activate (previousActive);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveTheme
//
////////////////////////////////////////////////////////////////////////////////

const LoadedTheme * ThemeManager::GetActiveTheme () const
{
    if (m_activeName.empty())
    {
        return nullptr;
    }

    for (const LoadedTheme & t : m_available)
    {
        if (t.name == m_activeName)
        {
            return &t;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChangeListener
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::AddChangeListener (ChangeListener listener)
{
    m_listeners.push_back (std::move (listener));
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyListeners
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::NotifyListeners (const LoadedTheme & theme)
{
    for (const ChangeListener & listener : m_listeners)
    {
        if (listener)
        {
            listener (theme);
        }
    }
}
