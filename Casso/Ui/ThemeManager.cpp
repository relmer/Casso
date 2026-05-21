#include "Pch.h"

#include "ThemeManager.h"

#include "Win11DwmHelpers.h"


#include "Core/JsonParser.h"






////////////////////////////////////////////////////////////////////////////////
//
//  ThemeBootstrapPlanner::Plan
//
////////////////////////////////////////////////////////////////////////////////

ThemeBootstrapAction ThemeBootstrapPlanner::Plan (
    const std::string  * themeJsonOnDisk,
    int                  currentVersion)
{
    HRESULT          hr            = S_OK;
    JsonValue        root;
    JsonParseError   perr;
    bool             isBuiltIn     = false;
    int              diskVersion   = 0;


    // Rule 1: nothing on disk -> install.
    if (themeJsonOnDisk == nullptr || themeJsonOnDisk->empty())
    {
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    // Rule 2: unparseable on-disk theme.json -> install (treated as
    // a half-baked previous extraction).
    hr = JsonParser::Parse (*themeJsonOnDisk, root, perr);

    if (FAILED (hr) || root.GetType() != JsonType::Object)
    {
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    // Rule 3: user-authored theme (no built-in marker) -> never touch.
    hr = root.GetBool ("$cassoBuiltIn", isBuiltIn);

    if (FAILED (hr) || !isBuiltIn)
    {
        return ThemeBootstrapAction::Skip;
    }

    // Rule 4 / 5: built-in theme; upgrade if older.
    hr = root.GetInt ("$cassoThemeVersion", diskVersion);

    if (FAILED (hr))
    {
        // Built-in marker present but version missing — install to fix.
        return ThemeBootstrapAction::InstallBuiltIn;
    }

    if (diskVersion < currentVersion)
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
    : m_fs              (fs)
    , m_themesBaseDir   (themesBaseDir)
{
    // The shared fallback directory lives next to the theme dirs as
    // `Themes/_shared/`. Per data-model.md §Theme an absent
    // `entryDocuments.<entry>` falls back to
    // `Themes/_shared/<entry>.rml`. The directory itself may legally
    // be empty (no shared overrides yet) — ResolveOne handles that.
    m_sharedDir = ThemeLoader::JoinPath (m_themesBaseDir, L"_shared");
}





////////////////////////////////////////////////////////////////////////////////
//
//  BindRml
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::BindRml (Rml::Context * pContext, HWND hwnd)
{
    // Re-binding: tear down anything attached to the old context
    // first so we don't leak ElementDocuments.
    if (m_context != nullptr && pContext != m_context)
    {
        UnloadActiveDocuments();
    }

    m_context = pContext;
    m_hwnd    = hwnd;

    // If a theme was activated before the binding fired, attach its
    // documents now. Ignore failure here — bringing up the binding
    // shouldn't crash the shell if a theme's RML happens to be
    // malformed (UiShell stays usable with no chrome).
    const LoadedTheme * theme = GetActiveTheme();

    if (theme != nullptr && m_context != nullptr)
    {
        HRESULT hr = ReattachDocuments (*theme);
        IGNORE_RETURN_VALUE (hr, S_OK);
        ApplyDwm (*theme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Discover
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Discover ()
{
    std::vector<std::wstring>  dirNames;
    size_t                     i      = 0;
    HRESULT                    hr     = S_OK;


    m_available.clear();

    hr = ThemeLoader::EnumerateCandidateDirs (m_fs, m_themesBaseDir, dirNames);

    // S_FALSE (themesBaseDir missing) is acceptable — list stays empty.
    if (FAILED (hr))
    {
        return hr;
    }

    for (i = 0; i < dirNames.size(); ++i)
    {
        std::wstring     dir     = ThemeLoader::JoinPath (m_themesBaseDir, dirNames[i]);
        LoadedTheme      theme;
        ThemeLoadError   err;
        HRESULT          hrLoad  = ThemeLoader::Load (m_fs, dir, m_sharedDir, theme, err);

        if (SUCCEEDED (hrLoad))
        {
            m_available.push_back (std::move (theme));
        }
        else
        {
            // FR-036: invalid themes are logged and excluded.
            char  buf[512] = {};
            snprintf (buf, sizeof (buf),
                      "[ThemeManager] excluded theme dir '%ls': %s\n",
                      dir.c_str(),
                      err.message.c_str());
            OutputDebugStringA (buf);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActiveTheme
//
////////////////////////////////////////////////////////////////////////////////

const LoadedTheme * ThemeManager::GetActiveTheme () const
{
    size_t  i = 0;

    if (m_activeName.empty())
    {
        return nullptr;
    }

    for (i = 0; i < m_available.size(); ++i)
    {
        if (m_available[i].name == m_activeName)
        {
            return &m_available[i];
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Activate
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::Activate (const std::string & themeName)
{
    const LoadedTheme  * theme    = nullptr;
    size_t               i        = 0;
    HRESULT              hr       = S_OK;
    std::string          previous = m_activeName;


    for (i = 0; i < m_available.size(); ++i)
    {
        if (m_available[i].name == themeName)
        {
            theme = &m_available[i];
            break;
        }
    }

    if (theme == nullptr)
    {
        return S_FALSE;
    }

    // Tentatively flip the active name so GetActiveTheme() inside
    // ReattachDocuments reports the new one. Roll back on failure
    // to preserve the FR-036 "previous theme remains active"
    // invariant.
    m_activeName = themeName;

    hr = ReattachDocuments (*theme);

    if (FAILED (hr))
    {
        m_activeName = previous;
        return hr;
    }

    ApplyDwm (*theme);
    NotifyListeners (*theme);

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReloadCurrent
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ReloadCurrent ()
{
    std::string  name = m_activeName;
    HRESULT      hr   = S_OK;


    if (name.empty())
    {
        return S_FALSE;
    }

    hr = Discover();
    CHRA (hr);

    hr = Activate (name);
    CHRA (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AddChangeListener
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::AddChangeListener (ChangeListener listener)
{
    if (listener)
    {
        m_listeners.push_back (std::move (listener));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReattachDocuments
//
//  Internal: load every entry document of `theme` into m_context.
//  No-op when m_context is null (BindRml will retry later).
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ThemeManager::ReattachDocuments (const LoadedTheme & theme)
{
    const std::wstring * paths[] = {
        &theme.entryDocs.titleBar,
        &theme.entryDocs.navLayer,
        &theme.entryDocs.settings,
        &theme.entryDocs.driveWidgets,
    };
    size_t  i = 0;


    if (m_context == nullptr)
    {
        return S_OK;
    }

    UnloadActiveDocuments();

    // Per FR-033 hot-swap must complete within one frame. Clearing
    // RmlUi's stylesheet cache here makes sure the new theme's
    // .rcss is parsed against fresh state rather than the previous
    // theme's interned rules.
    Rml::Factory::ClearStyleSheetCache();

    for (i = 0; i < std::size (paths); ++i)
    {
        if (paths[i]->empty())
        {
            continue;
        }

        // RmlUi takes UTF-8; theme entry paths are ASCII by spec so
        // a narrow widen-back is sufficient. Cast each wchar_t to
        // char explicitly to silence C4244 — anything non-ASCII in
        // a theme path is a theme-author bug and will surface as
        // LoadDocument failure below.
        std::string  utf8;
        utf8.reserve (paths[i]->size());
        for (wchar_t wc : *paths[i])
        {
            utf8.push_back (static_cast<char> (wc));
        }

        Rml::ElementDocument * doc = m_context->LoadDocument (utf8);

        if (doc != nullptr)
        {
            doc->Show();
            m_activeDocs.push_back (doc);
        }
        else
        {
            char  buf[512] = {};
            snprintf (buf, sizeof (buf),
                      "[ThemeManager] LoadDocument failed for '%s'\n",
                      utf8.c_str());
            OutputDebugStringA (buf);
        }
    }

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UnloadActiveDocuments
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::UnloadActiveDocuments ()
{
    size_t  i = 0;

    if (m_context == nullptr)
    {
        m_activeDocs.clear();
        return;
    }

    for (i = 0; i < m_activeDocs.size(); ++i)
    {
        if (m_activeDocs[i] != nullptr)
        {
            m_context->UnloadDocument (m_activeDocs[i]);
        }
    }

    m_activeDocs.clear();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ApplyDwm
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::ApplyDwm (const LoadedTheme & theme)
{
    if (m_hwnd == nullptr)
    {
        return;
    }

    // Per P4-T3 deferred wiring: Mica is theme-driven. Pre-Win11 the
    // helper is a no-op so this is safe on every supported OS.
    Win11DwmHelpers::ApplyMicaBackdrop (m_hwnd, theme.useMicaBackdrop);
}





////////////////////////////////////////////////////////////////////////////////
//
//  NotifyListeners
//
////////////////////////////////////////////////////////////////////////////////

void ThemeManager::NotifyListeners (const LoadedTheme & theme)
{
    size_t  i = 0;

    for (i = 0; i < m_listeners.size(); ++i)
    {
        if (m_listeners[i])
        {
            m_listeners[i] (theme);
        }
    }
}
